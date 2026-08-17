// Copyright 2026 devMiyax
#pragma once

// Vdp2GBuffer: per-layer intermediate G-buffer for the new VDP2 emulation
// (issue #22). Each VDP2 layer (NBG0-3 / RBG0 / Sprite) is decoded into its
// own slice. A later compositor pass reads all slices per-pixel and performs
// priority sort + VDP2 color calculation.
//
// Storage layout (see docs/feature/issue-22/01-design.md section 2.5):
//   - color array image  : RGBA8 (final color after color offset)
//   - attr  array image   : R32_UINT (priority / ccEnable / ccRatio /
//                           transparent bit-packed; layout owned by the
//                           compositor side, T-006)
//
// Each layer slice has a framebuffer that binds (color slice + attr slice) as
// MRT attachments. The render pass is shared across slices because all slices
// use the same attachment formats. Allocated at full HD resolution; resize()
// reallocates on resolution change.

#include <cstdint>
#include <vulkan/vulkan.h>

extern "C" {
#include "ygl.h"
}

class VIDVulkan;

class Vdp2GBuffer {
public:
  // Number of layer slices: NBG0-3 / RBG0 / SPRITE (enBGMAX from ygl.h).
  static constexpr uint32_t kSliceCount = enBGMAX;

  // Attachment formats. color is the final per-layer color, attr is a packed
  // attribute word (priority / ccEnable / ccRatio / transparent).
  static constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
  static constexpr VkFormat kAttrFormat = VK_FORMAT_R32_UINT;

  explicit Vdp2GBuffer(VIDVulkan* vulkan);
  ~Vdp2GBuffer();

  Vdp2GBuffer(const Vdp2GBuffer&) = delete;
  Vdp2GBuffer& operator=(const Vdp2GBuffer&) = delete;

  // Allocate (or reallocate) all slices at the given resolution. Safe to call
  // repeatedly; a no-op when the size is unchanged. Frees the previous
  // allocation first when the size differs.
  void resize(int width, int height);

  // Release all GPU resources. Idempotent.
  void free();

  bool isAllocated() const { return frameBuffer[0] != VK_NULL_HANDLE; }
  int getWidth() const { return width; }
  int getHeight() const { return height; }

  // Shared render pass for all slices (2 color attachments: color + attr).
  VkRenderPass getRenderPass() const { return renderPass; }

  // Per-slice framebuffer (binds the color/attr views for that slice).
  VkFramebuffer getFramebuffer(uint32_t slice) const { return frameBuffer[slice]; }

  // Sampler shared for reading slices in the compositor.
  VkSampler getSampler() const { return sampler; }

  // Array image views (all slices) for binding as a sampler array in the
  // compositor shader.
  VkImageView getColorArrayView() const { return colorArrayView; }
  VkImageView getAttrArrayView() const { return attrArrayView; }

  // Per-slice single-layer views (used as framebuffer attachments).
  VkImageView getColorSliceView(uint32_t slice) const { return colorSliceView[slice]; }
  VkImageView getAttrSliceView(uint32_t slice) const { return attrSliceView[slice]; }

private:
  void allocate(int width, int height);
  void createRenderPass();

  VIDVulkan* vulkan = nullptr;

  int width = -1;
  int height = -1;

  // Color array (RGBA8) and attr array (R32_UINT), kSliceCount layers each.
  VkImage colorImage = VK_NULL_HANDLE;
  VkDeviceMemory colorMem = VK_NULL_HANDLE;
  VkImageView colorArrayView = VK_NULL_HANDLE;
  VkImageView colorSliceView[kSliceCount] = {};

  VkImage attrImage = VK_NULL_HANDLE;
  VkDeviceMemory attrMem = VK_NULL_HANDLE;
  VkImageView attrArrayView = VK_NULL_HANDLE;
  VkImageView attrSliceView[kSliceCount] = {};

  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkFramebuffer frameBuffer[kSliceCount] = {};
  VkSampler sampler = VK_NULL_HANDLE;
};
