// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// FbReadback: copy a VDP1 offscreen image to a host-visible buffer so the
// Memory Viewer can read raw pixel bytes. Used only during pause; allocates
// once on demand and reuses the staging buffer across step changes.
//
// ASCII-only.
#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

class FbReadback {
public:
    FbReadback() = default;
    ~FbReadback();
    FbReadback(const FbReadback&) = delete;
    FbReadback& operator=(const FbReadback&) = delete;

    // Capture the given image (currentLayout must be the layout the image
    // is in right now; the readback transitions to TRANSFER_SRC_OPTIMAL and
    // back). RGBA8 / extent2D / device must match across consecutive calls
    // for buffer reuse. Returns true on success.
    bool capture(VkDevice device,
                 VkPhysicalDevice physDev,
                 VkQueue queue,
                 uint32_t queueFamily,
                 VkImage image,
                 VkImageLayout currentLayout,
                 uint32_t width,
                 uint32_t height);

    // Width/height of the most recent successful capture.
    uint32_t width()  const { return _width; }
    uint32_t height() const { return _height; }
    // Read-only view into the cached host-visible RGBA8 pixel buffer.
    // Empty before the first successful capture or after release().
    const std::vector<uint8_t>& pixels() const { return _pixels; }

    // Free Vulkan resources (call on shutdown / device-lost).
    void release(VkDevice device);

private:
    VkBuffer       _staging       = VK_NULL_HANDLE;
    VkDeviceMemory _stagingMem    = VK_NULL_HANDLE;
    VkDeviceSize   _stagingSize   = 0;
    VkCommandPool  _cmdPool       = VK_NULL_HANDLE;
    uint32_t       _cmdPoolFamily = 0xFFFFFFFFu;
    uint32_t       _width  = 0;
    uint32_t       _height = 0;
    std::vector<uint8_t> _pixels;  // host copy of last capture (RGBA8)

    bool ensureStaging(VkDevice device, VkPhysicalDevice physDev,
                       VkDeviceSize sizeBytes);
    bool ensureCmdPool(VkDevice device, uint32_t queueFamily);
};
