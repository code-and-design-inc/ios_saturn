// Copyright 2026 devMiyax
#pragma once

// Vdp2Compositor: the new per-pixel VDP2 compositor pass (issue #22, task
// T-008). It reads every Vdp2GBuffer slice (color RGBA8 + packed attr R32_UINT)
// per-pixel, sorts the layers by priority and runs the VDP2 color calculation,
// then writes the result into the main framebuffer (offscreenPass / window
// render pass) with a single fullscreen triangle.
//
// T-008 scope is the compositor skeleton + the BASIC pattern (no color calc):
// pick the highest-priority opaque layer color, else the back screen. Normal /
// extended color calc and LNCL are added by later tasks (T-012 / T-014 / T-015)
// inside the same fragment shader. The selection logic mirrors the host-port
// reference vdp2_color_oracle.h compositeBasic() 1:1 (see the GLSL comments).
//
// Style follows FramebufferRenderer (the existing per-pixel VDP1 sprite
// compositor): inline GLSL compiled via shaderc, a small UBO, one descriptor
// set per in-flight composite, a fullscreen quad. ASCII-only comments
// (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#include <cstdint>
#include <vulkan/vulkan.h>

extern "C" {
#include "ygl.h"
}

class VIDVulkan;
class Vdp2GBuffer;

class Vdp2Compositor {
public:
  explicit Vdp2Compositor(VIDVulkan* vulkan);
  ~Vdp2Compositor();

  Vdp2Compositor(const Vdp2Compositor&) = delete;
  Vdp2Compositor& operator=(const Vdp2Compositor&) = delete;

  // One-time GPU resource setup (vertex buffer, descriptor layout/pool, UBO,
  // sampler). Must be called once the Vulkan device exists. The graphics
  // pipeline is built lazily on the first composite() because it needs the
  // target render pass (which can change on swapchain rebuild).
  void setup();

  // Release all GPU resources. Idempotent.
  void release();

  // The render pass the compositor draws into changed (swapchain rebuild /
  // resolution change). Drops the cached pipeline so the next composite()
  // rebuilds it against the new render pass.
  void invalidatePipeline();

  // Run the basic-pattern composite. Reads `gbuffer` slices, writes the chosen
  // color into the currently bound framebuffer. Must be called inside an active
  // render pass on `commandBuffer`. `targetRenderPass` is the render pass the
  // command buffer is currently in (offscreenPass / subRenderTarget / window);
  // the pipeline is (re)built for it on demand. `backColor` is the resolved
  // back-screen color used when every layer is transparent (RGB in the low 24
  // bits). `viewport` / `scissor` size the fullscreen draw.
  //
  // T-012 normal color calc: `colorCalcMode` is CCMD (0 = ratio, 1 = add) and
  // `ratioFromSecond` is CCRTMD (0 = ratio from top layer, 1 = from second).
  // They drive the per-pixel two-layer blend in the fragment shader. The
  // per-layer ccEnable / ccRatio already arrive packed in the G-buffer attr.
  //
  // T-014 extended color calc: `exccEnable` is EXCCEN (CCCTL bit 10). When set,
  // the fragment shader selects top..fourth and builds the "extended second"
  // from the second/third screens per the ccEnable chain + ratio table before
  // the top<->second blend (else it stays the normal two-layer path).
  // `lineColorInserted` selects the table-12.2 branch (T-015 / LNCL).
  //
  // T-015 LNCL: when `lineColorInserted` is set, the line color supplies the
  // folded operand in the extended chain (manual figure 12.3). `lineColorView` /
  // `lineColorSampler` are the per-line line color texture (line color table in
  // row 0, indexed by scan line); `vheight` is the Saturn line count used to
  // recover the scan line in the HD-scaled fullscreen draw. Pass VK_NULL_HANDLE
  // for the view/sampler and 0 for lineColorInserted when LNCL is inactive.
  void composite(VkCommandBuffer commandBuffer,
                 Vdp2GBuffer* gbuffer,
                 VkRenderPass targetRenderPass,
                 const VkViewport& viewport,
                 const VkRect2D& scissor,
                 uint32_t backColorRGB,
                 int colorCalcMode,
                 int ratioFromSecond,
                 int exccEnable,
                 int lineColorInserted,
                 VkImageView lineColorView,
                 VkSampler lineColorSampler,
                 int vheight,
                 int lineScreenMask,
                 int lineColorAlpha,
                 // CRAM color mode (0/1/2) and per-slice palette-format bitmask
                 // (bit[slice]=1 => palette). Table 12.2 extended-cc ratio gate.
                 int cramMode,
                 int paletteFormatMask,
                 // Hi-res CRAM mode 1/2 (the SW reference MIXIT_SPECIAL_HIRES_CRAM12):
                 // 1 when (HRes & 0x6) && CRAM mode != 0. Suppresses the blend
                 // (second = top) for a palette-format second image.
                 int hiresCram12,
                 // Color gradation / blur (BOKEN). gradEnable on; gradSlice =
                 // blur-source slice (-1 none); vwidth = Saturn dot width.
                 int gradEnable,
                 int gradSlice,
                 int vwidth,
                 // Color offset (ch.13): 6 layers x 3 (RGB) signed -255..255,
                 // slice-indexed (vdp2cc LayerId order); 0 where CLOFEN is clear.
                 // backColorOffsetRGB is the 3-element back-screen offset.
                 const int* layerColorOffsetRGB,
                 const int* backColorOffsetRGB,
                 int debugViewSlice = -1,
                 // Color calc window (ch.12, WCTLD>>8): winmask / winflag /
                 // winmode (-1 = none). Outside the cc-window valid area the
                 // compositor skips color calc (top passes through opaque).
                 int ccWinMask = 0,
                 int ccWinFlag = 0,
                 int ccWinMode = -1,
                 // issue #22 mosaic (MZCTL): global block size (mosaicX/mosaicY,
                 // 1 = none) + per-slice enable mask (bit[slice]=1 => mosaic).
                 // The compositor snaps each enabled slice's sample to the block
                 // top-left; slices are rendered un-mosaic'd.
                 int mosaicX = 1,
                 int mosaicY = 1,
                 int mosaicMask = 0,
                 // issue #22 landscape fix: (cos, sin) of the net pre-rotation
                 // angle the fullscreen triangle must apply to match the legacy
                 // pre_rotate_mat (Android swapchain pre-transform + rotate_screen).
                 // Default (1, 0) = identity (no rotation).
                 float rotCos = 1.0f,
                 float rotSin = 0.0f,
                 // Per-line back screen (BKCLMD, BKTAU bit 15; VDP2 manual
                 // section 7.2). When 1, the fragment shader reads the back
                 // screen color per scan line from row 4 of the line color
                 // texture (packed by updatePerLineColorCalc) instead of the
                 // single backColorRGB (Space Harrier horizon gradient).
                 int backPerLine = 0);

private:
  // UBO consumed by the fragment shader. backColor is RGBA (0..1); future
  // color-calc tasks extend this struct (mode / ratios / LNCL) -- keep new
  // fields appended so the GLSL offsets stay stable.
  //
  // T-012 (normal color calc) appends two ints after backColor:
  //   colorCalcMode  : 0 = ratio blend (titan TOP / BOTTOM by CCRTMD), 1 = add.
  //   ratioFromSecond: CCRTMD. 0 = ratio from the top layer (TITAN_BLEND_TOP),
  //                    1 = ratio from the second layer (TITAN_BLEND_BOTTOM).
  // T-014 (extended color calc) reuses the two trailing padding ints:
  //   exccEnable        : EXCCEN (CCCTL bit 10). When set, the second operand is
  //                       the "extended second" built from second/third per the
  //                       ccEnable chain (Sega VDP2 manual section 12.12) before
  //                       the top<->second blend; else normal color calc.
  //   lineColorInserted : LNCL inserted (selects the table-12.2 branch that lets
  //                       the fourth screen participate). Driven by T-015; 0 for
  //                       the T-014 MVP (extended second reaches at most
  //                       second+third).
  // All fields are appended (not inserted) so the std140 backColor offset is
  // stable.
  // T-015 (LNCL) appends vdp2height (Saturn line count) so the fragment shader
  // can recover the scan line for the line color table row index, plus one pad
  // int to keep std140 16-byte alignment. Appended (not inserted) so existing
  // offsets stay stable.
  struct UniformBufferObject {
    float backColor[4];
    int colorCalcMode;
    int ratioFromSecond;
    int exccEnable;
    int lineColorInserted;
    int vheight;
    // issue #22 debug: when >= 0, the fragment shader bypasses color calc and
    // outputs G-buffer slice `debugViewSlice` directly (raw color, or green for
    // transparent/empty texels). -1 = normal composite. Reuses the former pad
    // int, so the std140 layout is unchanged.
    int debugViewSlice;
    // T-015 normal-path LNCL line color screen. lineScreenMask carries one bit
    // per G-buffer slice (set when LNCLEN selects that layer); lineColorAlpha is
    // (CCRLB & 0x1F) << 1, the line color cc ratio used by the CCRTMD=second
    // (Bottom) blend. Appended so existing std140 offsets stay stable.
    int lineScreenMask;
    int lineColorAlpha;
    // CRAM color mode (RAMCTL bits 13:12) + per-slice palette-format mask, used
    // by the extended-cc ratio (Table 12.2): in CRAM mode 1/2 a palette-format
    // second image suppresses the fold (4:0:0). pad0/pad1 keep the following
    // ivec4 array on a 16-byte std140 boundary (offset 64). issue #22.
    int cramMode;
    int paletteFormatMask;
    // Hi-res CRAM12 (the SW reference MIXIT_SPECIAL_HIRES_CRAM12): suppress blend when the
    // second image is palette format. Reuses the former pad0 (std140 unchanged).
    int hiresCram12;
    // Color gradation / blur (BOKEN, CCCTL bit 15, CRAM mode 0). gradEnable: on.
    // gradSlice: blur-source G-buffer slice (-1 = none). vwidth: Saturn dot width
    // for the blur tap spacing. Two 16-byte std140 rows precede the ivec4 array.
    int gradEnable;
    int gradSlice;
    int vwidth;
    // Per-line back screen (BKCLMD, BKTAU bit 15). 1 = the shader samples the
    // back color per scan line from row 4 of the line color texture; 0 = the
    // single backColor above. Reuses the former pad1 (std140 unchanged).
    int backPerLine;
    int pad2;
    // Color offset (Sega VDP2 manual ch.13). Per G-buffer slice signed RGB offset
    // (-255..255) applied to the top image's final color after color calc; the
    // [3] lane is std140 padding. backColorOffset is used when every layer is
    // transparent. Appended so existing std140 offsets stay stable (this block
    // starts at a 16-byte boundary: 48 bytes precede it).
    int32_t layerColorOffset[6][4];
    int32_t backColorOffset[4];
    // Color calc window (Sega VDP2 manual ch.12, WCTLD>>8). [0]=winmask,
    // [1]=winflag, [2]=winmode (-1 = no cc window), [3]=std140 pad. When the
    // pixel is outside the cc-window valid area the compositor skips color calc
    // (matches vidsoft.c forcing alpha 0x3F). Appended so existing std140
    // offsets stay stable. issue #22.
    int32_t ccWindow[4];
    // Mosaic (Sega VDP2 manual ch.10, MZCTL). [0]=global block width (>=1),
    // [1]=global block height (>=1), [2]=per-slice enable mask, [3]=std140 pad.
    // The compositor snaps each enabled slice's sample UV to the block
    // top-left. Appended so existing std140 offsets stay stable. issue #22.
    int32_t mosaic[4];
  };

  void createVertexBuffer();
  void createDescriptors();
  void createPipeline(VkRenderPass targetRenderPass);
  VkShaderModule compileGlsl(const char* code, int shaderKind, uint32_t cacheKey);

  VIDVulkan* vulkan = nullptr;

  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  static constexpr int kFrames = 4;  // round-robin descriptor sets
  VkDescriptorSet descriptorSet[kFrames] = {};
  VkBuffer uboBuffer[kFrames] = {};
  VkDeviceMemory uboMemory[kFrames] = {};
  int frameIndex = 0;

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkRenderPass pipelineRenderPass = VK_NULL_HANDLE;  // pass the pipeline was built for

  VkShaderModule vertModule = VK_NULL_HANDLE;
  VkShaderModule fragModule = VK_NULL_HANDLE;

  bool setupDone = false;
};
