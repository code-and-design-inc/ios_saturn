// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
// ASCII-only.
#include "FbReadback.h"

#include <cstring>

namespace {

uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeBits,
                        VkMemoryPropertyFlags wantedProps) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(physDev, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & wantedProps) == wantedProps) {
            return i;
        }
    }
    return 0xFFFFFFFFu;
}

}  // namespace

FbReadback::~FbReadback() {
    // device must already have been waited idle by the owner before dtor.
    // We can't free GPU resources here without a device handle; release()
    // must be called explicitly while the device is alive.
}

void FbReadback::release(VkDevice device) {
    if (_cmdPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, _cmdPool, nullptr);
        _cmdPool = VK_NULL_HANDLE;
    }
    if (_staging != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, _staging, nullptr);
        _staging = VK_NULL_HANDLE;
    }
    if (_stagingMem != VK_NULL_HANDLE) {
        vkFreeMemory(device, _stagingMem, nullptr);
        _stagingMem = VK_NULL_HANDLE;
    }
    _stagingSize = 0;
    _pixels.clear();
    _width = _height = 0;
}

bool FbReadback::ensureStaging(VkDevice device, VkPhysicalDevice physDev,
                                VkDeviceSize sizeBytes) {
    if (_staging != VK_NULL_HANDLE && _stagingSize >= sizeBytes) return true;
    if (_staging != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, _staging, nullptr);
        vkFreeMemory(device, _stagingMem, nullptr);
        _staging = VK_NULL_HANDLE;
        _stagingMem = VK_NULL_HANDLE;
    }

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size  = sizeBytes;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bci, nullptr, &_staging) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, _staging, &req);
    const uint32_t memType = findMemoryType(
        physDev, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == 0xFFFFFFFFu) {
        vkDestroyBuffer(device, _staging, nullptr);
        _staging = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = memType;
    if (vkAllocateMemory(device, &ai, nullptr, &_stagingMem) != VK_SUCCESS) {
        vkDestroyBuffer(device, _staging, nullptr);
        _staging = VK_NULL_HANDLE;
        return false;
    }
    if (vkBindBufferMemory(device, _staging, _stagingMem, 0) != VK_SUCCESS) {
        vkFreeMemory(device, _stagingMem, nullptr);
        vkDestroyBuffer(device, _staging, nullptr);
        _staging = VK_NULL_HANDLE;
        _stagingMem = VK_NULL_HANDLE;
        return false;
    }
    _stagingSize = req.size;
    return true;
}

bool FbReadback::ensureCmdPool(VkDevice device, uint32_t queueFamily) {
    if (_cmdPool != VK_NULL_HANDLE && _cmdPoolFamily == queueFamily) return true;
    if (_cmdPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, _cmdPool, nullptr);
        _cmdPool = VK_NULL_HANDLE;
    }
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = queueFamily;
    if (vkCreateCommandPool(device, &pci, nullptr, &_cmdPool) != VK_SUCCESS) {
        return false;
    }
    _cmdPoolFamily = queueFamily;
    return true;
}

bool FbReadback::capture(VkDevice device,
                         VkPhysicalDevice physDev,
                         VkQueue queue,
                         uint32_t queueFamily,
                         VkImage image,
                         VkImageLayout currentLayout,
                         uint32_t width,
                         uint32_t height) {
    const VkDeviceSize sizeBytes = VkDeviceSize(width) * height * 4u;
    if (sizeBytes == 0) return false;
    if (!ensureStaging(device, physDev, sizeBytes)) return false;
    if (!ensureCmdPool(device, queueFamily)) return false;

    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool        = _cmdPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &cbai, &cb) != VK_SUCCESS) return false;

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cb, &bi) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, _cmdPool, 1, &cb);
        return false;
    }

    // Layout: currentLayout -> TRANSFER_SRC_OPTIMAL.
    VkImageMemoryBarrier b1{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b1.oldLayout = currentLayout;
    b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    b1.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b1.image = image;
    b1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &b1);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(cb, image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _staging, 1, &region);

    VkImageMemoryBarrier b2 = b1;
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b2.newLayout = currentLayout;
    b2.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &b2);

    if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, _cmdPool, 1, &cb);
        return false;
    }

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fci, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, _cmdPool, 1, &cb);
        return false;
    }

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cb;
    if (vkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS) {
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, _cmdPool, 1, &cb);
        return false;
    }
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, _cmdPool, 1, &cb);

    void* mapped = nullptr;
    if (vkMapMemory(device, _stagingMem, 0, sizeBytes, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    if (mapped == nullptr) {
        vkUnmapMemory(device, _stagingMem);
        return false;
    }
    _pixels.resize(static_cast<size_t>(sizeBytes));
    std::memcpy(_pixels.data(), mapped, static_cast<size_t>(sizeBytes));
    vkUnmapMemory(device, _stagingMem);

    _width  = width;
    _height = height;
    return true;
}
