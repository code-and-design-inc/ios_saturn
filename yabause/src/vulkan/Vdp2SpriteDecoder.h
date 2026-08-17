// Copyright 2026 devMiyax
#pragma once

// Vdp2SpriteDecoder: populates the Vdp2GBuffer sprite slice (kSprite) for the
// new VDP2 compositor (issue #22, task T-009). It samples the VDP1 framebuffer
// per-pixel, decodes the sprite (priority slot -> priority table, color-calc
// code -> sprite ratio table, palette/direct color), and writes the color slice
// (RGBA8, location 0) plus the packed attr slice (R32_UINT, location 1) of the
// kSprite G-buffer slice via a single fullscreen-triangle draw into the
// Vdp2GBuffer render pass.
//
// This is the GLSL/Vulkan counterpart of the host-port reference
// vulkan/test/vdp2_sprite_decode.h decodeSprite(); the fragment shader mirrors
// it 1:1 (see the GLSL comments). After this pass runs, the compositor sorts the
// sprite slice alongside NBG0-3 / RBG0 in the same per-pixel priority loop.
//
// Style follows Vdp2Compositor (the basic-pattern compositor): inline GLSL
// compiled via shaderc, a small UBO, round-robin descriptor sets, a fullscreen
// triangle, lazily-built pipeline keyed on the G-buffer render pass. Shadow
// (normal/MSB) is phase2 (01-design.md section 2.6) and not applied here.
// ASCII-only comments (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#include <cstdint>
#include <vulkan/vulkan.h>

extern "C" {
#include "ygl.h"
}

class VIDVulkan;
class Vdp2GBuffer;

class Vdp2SpriteDecoder {
public:
  explicit Vdp2SpriteDecoder(VIDVulkan* vulkan);
  ~Vdp2SpriteDecoder();

  Vdp2SpriteDecoder(const Vdp2SpriteDecoder&) = delete;
  Vdp2SpriteDecoder& operator=(const Vdp2SpriteDecoder&) = delete;

  // One-time GPU resource setup (descriptor layout/pool, UBO, sampler). The
  // graphics pipeline is built lazily because it needs the G-buffer render pass.
  void setup();

  // Release all GPU resources. Idempotent.
  void release();

  // Drop the cached pipeline (G-buffer render pass changed on resize).
  void invalidatePipeline();

  // Per-pixel sprite register inputs (mirrors vdp2sprite::SpriteRegs). Filled by
  // VIDVulkan::renderSpriteToGBuffer from the VDP2 register struct + decoded
  // color-calc state each frame.
  struct SpriteParams {
    int32_t priorityTable[8];    // PRISA..D nibbles & 0x7
    int32_t spriteRatioTable[8]; // decoded sprite color-calc ratios 0..0x3F
    int32_t spcccs;              // (SPCTL >> 12) & 0x3
    int32_t spccn;               // (SPCTL >> 8) & 0x7
    int32_t ccWindowOn;          // (CCCTL & 0x40) != 0
    int32_t spriteWindow;        // sprite-window enable
    int32_t colorRamOffset;      // (CRAOFB & 0x70) << 4
    int32_t colorMode;           // unused by GLSL (CRAM image is pre-decoded)
    // Per-line raster override (Vdp2External.perline_alpha_draw & 0x40): games
    // like Bio Hazard rewrite PRISA / CCRS* mid-frame (menu rows get sprite
    // priority > 0 only on those scanlines). The legacy FramebufferRenderer
    // handles this with a per-line texture (updateVdp2Reg); the new composite
    // packs the same data here, indexed by DISPLAY line (line_shift already
    // applied by the CPU fill). lines[ln][0] = 8 priorities, 4 bits each;
    // lines[ln][1] = decoded cc ratios for cc codes 0..3, 8 bits each;
    // lines[ln][2] = cc ratios for codes 4..7; lines[ln][3] = reserved.
    int32_t perLine;             // 1 = use `lines`, 0 = frame-uniform tables
    int32_t vdp2Height;          // display height (maps v_uv.y -> line index)
    uint32_t lines[512][4];
  };

  // Render the sprite slice. `gbuffer` must be allocated; this begins the
  // G-buffer render pass on the kSprite framebuffer, clears it, decodes the
  // VDP1 framebuffer into color/attr, and ends the pass. Must be called BEFORE
  // the main pass (same place as renderLayersToGBuffer). `vdp1FbView` is the
  // VDP1 framebuffer image view (already transitioned to shader-read).
  void renderToSlice(VkCommandBuffer commandBuffer,
                     Vdp2GBuffer* gbuffer,
                     VkImageView vdp1FbView,
                     VkImageView cramView,
                     VkSampler cramSampler,
                     const SpriteParams& params,
                     int vdp2Width, int vdp2Height);

private:
  // UBO consumed by the fragment shader (matches the GLSL std140 layout: int
  // arrays are vec4-aligned, so each table entry occupies its own 16 bytes).
  struct UniformBufferObject {
    int32_t priorityTable[8][4];
    int32_t spriteRatioTable[8][4];
    int32_t spcccs;
    int32_t spccn;
    int32_t ccWindowOn;
    int32_t spriteWindow;
    int32_t colorRamOffset;
    int32_t perLine;
    int32_t vdp2height;
    int32_t pad2;
    // Per-line priority / cc-ratio table (see SpriteParams::lines). uvec4 in
    // GLSL; starts 16-byte aligned (preceded by 8 ivec4[8] + 8 ints).
    uint32_t lines[512][4];
  };

  void createDescriptors();
  void createPipeline(VkRenderPass targetRenderPass);
  VkShaderModule compileGlsl(const char* code, int shaderKind);

  VIDVulkan* vulkan = nullptr;

  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  static constexpr int kFrames = 4;
  VkDescriptorSet descriptorSet[kFrames] = {};
  VkBuffer uboBuffer[kFrames] = {};
  VkDeviceMemory uboMemory[kFrames] = {};
  int frameIndex = 0;

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkRenderPass pipelineRenderPass = VK_NULL_HANDLE;

  VkShaderModule vertModule = VK_NULL_HANDLE;
  VkShaderModule fragModule = VK_NULL_HANDLE;

  // Nearest / clamp sampler for the VDP1 framebuffer (one-to-one texel read).
  VkSampler fbSampler = VK_NULL_HANDLE;

  bool setupDone = false;
};
