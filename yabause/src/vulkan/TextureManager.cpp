/*
        Copyright 2021 devMiyax(smiyaxdev@gmail.com)

This file is part of YabaSanshiro.

        YabaSanshiro is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

YabaSanshiro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
along with YabaSanshiro; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "TextureManager.h"
#include "VIDVulkan.h"
#include <vulkan/Shared.h>
#include "vulkan/vulkan.hpp"

TextureManager::~TextureManager() {
  const VkDevice device = vulkan->getDevice();

  vkQueueWaitIdle(vulkan->getVulkanQueue());

  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);
  vkDestroySampler(device, _textureSampler, nullptr);
  vkDestroyImageView(device, _textureImageView, nullptr);
  vkDestroyImage(device, _textureImage, nullptr);
  vkFreeMemory(device, _textureImageMemory, nullptr);


  vkFreeCommandBuffers(device, commandPool, commandBuffers.size(), commandBuffers.data());
  vkDestroyCommandPool(device, commandPool, nullptr);

  if (copyFence != VK_NULL_HANDLE) {
    vkDestroyFence(device, copyFence, nullptr);
  }
}

int TextureManager::init(unsigned int w, unsigned int h) {

  const VkDevice device = vulkan->getDevice();
  vk::Device d(device);

  _texture = (uint32_t*)createTextureImage(w, h); //(uint32_t*)malloc( w*h*4);
  _width = w;
  _height = h;
  reset();

  VkCommandPoolCreateInfo pool_create_info{};
  pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_create_info.flags = /*VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |*/ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_create_info.queueFamilyIndex = vulkan->getVulkanGraphicsQueueFamilyIndex();
  vkCreateCommandPool(device, &pool_create_info, nullptr, &commandPool);

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  commandBuffers.resize(TX_COMMANDBUFFER_COUNT);

  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = commandBuffers.size();

  vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());

  VkSemaphoreCreateInfo semaphore_create_info{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkCreateSemaphore(device, &semaphore_create_info, nullptr, &complete);

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  ErrorCheck(vkCreateFence(device, &fenceInfo, nullptr, &copyFence));

  return 0;
}

void TextureManager::reset() {
  _currentX = 0;
  _currentY = 0;
  _yMax = 0;
  resetCache();
}

void TextureManager::realloc(unsigned int width, unsigned int height) {
  uint32_t * newtexture = (uint32_t*)reallocTextureImage(_texture, width, height);
  _width = width;
  _height = height;
  _texture = newtexture;
}

void TextureManager::allocate(CharTexture * output, unsigned int w, unsigned int h, unsigned int * x, unsigned int * y) {
  if (_width < w) {
    //YGLDEBUG("can't allocate texture: %dx%d\n", w, h);
    this->realloc(w, _height);
    this->allocate(output, w, h, x, y);
    return;
  }
  if ((_height - _currentY) < h) {
    //YGLDEBUG("can't allocate texture: %dx%d\n", w, h);
    this->realloc(_width, _height + 512);
    this->allocate(output, w, h, x, y);
    return;
  }

  if ((_width - _currentX) >= w) {
    *x = _currentX;
    *y = _currentY;
    output->w = _width - w;
    output->textdata = _texture + _currentY * _width + _currentX;
    //yprintf("allocate %lx", (uintptr_t)output->textdata );
    _currentX += w;

    if ((_currentY + h) > _yMax) {
      _yMax = _currentY + h;
    }
  }
  else {
    _currentX = 0;
    _currentY = _yMax;
    this->allocate(output, w, h, x, y);
  }
}

void TextureManager::push() {

}

void TextureManager::pull() {

}

int TextureManager::isCached(uint64_t addr, TextureCache * c) {
  auto it = _texCache.find(addr);
  if (it == _texCache.end()) {
    return 0; // not found
  }

  //*c = _texCache[addr];
  c->addr = it->second.addr;
  c->x = it->second.x;
  c->y = it->second.y;

  return 1; // found
}

void TextureManager::addCache(uint64_t addr, TextureCache * c) {
  _texCache[addr] = *c;
}

void TextureManager::resetCache() {
  _texCache.clear();
}


void * TextureManager::createTextureImage(int texWidth, int texHeight) {

  const VkDevice device = vulkan->getDevice();
  if (device == VK_NULL_HANDLE) return NULL;

  VkDeviceSize imageSize = texWidth * texHeight * 4;

  char * pixels = (char*)malloc(texWidth * texHeight * 4);
  memset(pixels, 0, texWidth * texHeight * 4);

  vulkan->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
  vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &imageBuffer);
  memcpy(imageBuffer, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(device, stagingBufferMemory);
  //stbi_image_free(pixels);

  vulkan->createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageMemory);
  //printf("_textureImage = %llx\n", _textureImage);
  vulkan->transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vulkan->copyBufferToImage(stagingBuffer, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
  vulkan->transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  free(pixels);

  vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &imageBuffer);
  //yprintf("createTextureImage vkMapMemory %lx", (uintptr_t)imageBuffer);

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = _textureImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewInfo, nullptr, &_textureImageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }

  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  //samplerInfo.maxAnisotropy = 16;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;
  if (vkCreateSampler(device, &samplerInfo, nullptr, &_textureSampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }

  this->texwidth = texWidth;
  this->texheight = texHeight;

  return imageBuffer;

}

#include <iostream>


void * TextureManager::reallocTextureImage(void * pixels, int texWidth, int texHeight) {


  std::cout << "reallocTextureImage height:" << texHeight << std::endl;

  const VkDevice device = vulkan->getDevice();
  if (device == VK_NULL_HANDLE) return NULL;
  vkDeviceWaitIdle(device);

  VkDeviceSize imageSize = texWidth * texHeight * 4;

  char * newpixels = (char*)malloc(texWidth * texHeight * 4);
  for (int i = 0; i < this->texheight; i++) {
    char * newrow = &newpixels[i*texWidth * 4];
    char * oldrow = &((char*)pixels)[i*this->texwidth * 4];
    for (int j = 0; j < this->texwidth * 4; j++) {
      newrow[j] = oldrow[j];
    }
  }

  vkUnmapMemory(device, stagingBufferMemory);
  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);


  vulkan->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
  vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &imageBuffer);
  memcpy(imageBuffer, newpixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(device, stagingBufferMemory);

  vkDestroyImage(device, _textureImage, nullptr);
  vkFreeMemory(device, _textureImageMemory, nullptr);
  vulkan->createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageMemory);
  //printf("_textureImage = %llx\n", _textureImage);
  vulkan->transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vulkan->copyBufferToImage(stagingBuffer, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
  vulkan->transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  free(newpixels);

  vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &imageBuffer);
  //yprintf("reallocTextureImage vkMapMemory %lx", (uintptr_t)imageBuffer);

  vkDestroyImageView(device, _textureImageView, nullptr);
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = _textureImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewInfo, nullptr, &_textureImageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }

  this->texwidth = texWidth;
  this->texheight = texHeight;


  return imageBuffer;

}


void TextureManager::updateTextureImage(const std::function<void(VkCommandBuffer commandBuffer)>& f) {

  const VkDevice device = vulkan->getDevice();
  if (device == VK_NULL_HANDLE) return;
  //if (this->_yMax <= 0 ) return;

  // Wait for the previous texture upload to finish before recycling the
  // shared copyFence and the command buffer slot we are about to reset.
  // Without this the next vkResetCommandBuffer() / vkResetFences() pair
  // triggers VUID-vkResetCommandBuffer-00045 / VUID-vkResetFences-01123 on
  // Adreno when texture uploads queue faster than the GPU drains them.
  // copyFence is created SIGNALED so the very first call returns instantly.
  if (copyFence != VK_NULL_HANDLE) {
    vkWaitForFences(device, 1, &copyFence, VK_TRUE, UINT64_MAX);
  }

  int ci = updateCount & (TX_COMMANDBUFFER_COUNT-1);
  updateCount++;

  VkDeviceSize imageSize = _width * _height * 4;

  vkUnmapMemory(device, stagingBufferMemory);


  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  //vkResetCommandPool(device, this->commandPool ,0);
  vkResetCommandBuffer(commandBuffers[ci],0);
  vkBeginCommandBuffer(commandBuffers[ci], &beginInfo);

  // WAR hazard fix (root cause of the COMPUTE_RASTERIZER pattern noise). Every
  // per-frame VDP2 resource the composite fragment shader samples -- this
  // texture atlas, plus the CRAM / line / back / window / vertex uploads the
  // f() callback records into THIS same command buffer -- is host-rebuilt and
  // single/few-buffered. With up to MAX_COMMANDBUFFER_COUNT(=4) frames in
  // flight (COMPUTE_RASTERIZER path has no graphics renderFences serialization),
  // the next frame's transfers here can overwrite those resources while a prior
  // frame's composite is still sampling them -> garbage texels. This command
  // buffer is submitted before the composite every frame, so a single global
  // FRAGMENT_SHADER(read) -> TRANSFER(write) barrier at its head orders every
  // upload after the previous frame's composite reads (same queue, submission
  // order), covering all of those resources at once -- no CPU stall, no extra
  // memory, full pipelining preserved. (Confirmed cause: debugger replay and a
  // full vkDeviceWaitIdle were both clean; per-resource analysis traced it to
  // these single-buffered composite inputs.)
  {
    VkMemoryBarrier war{};
    war.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    war.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    war.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffers[ci],
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &war, 0, nullptr, 0, nullptr);
  }

  //------------------------------------------------------------------------
  if (this->_yMax > 0) {

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = _textureImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    // WAR hazard fix: this texture atlas is a SINGLE image, re-uploaded every
    // frame and sampled by the VDP2 composite fragment shader. With up to
    // MAX_COMMANDBUFFER_COUNT(=4) frames in flight (COMPUTE_RASTERIZER path,
    // which unlike the graphics path has no renderFences serialization), the
    // previous frame's composite may still be sampling _textureImage when this
    // frame's buffer->image copy overwrites it. A TOP_OF_PIPE / srcAccess=0
    // source scope imposes NO execution dependency on those prior fragment
    // reads, so the transfer write races them -> garbage texels (the
    // COMPUTE_RASTERIZER pattern noise; replay was clean, GPU-serialized was
    // clean). Order the transfer AFTER prior fragment-shader reads instead.
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(
      commandBuffers[ci],
      sourceStage, destinationStage,
      0,
      0, nullptr,
      0, nullptr,
      1, &barrier
    );

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
      _width,
      this->_yMax,
      1
    };
    vkCmdCopyBufferToImage(
      commandBuffers[ci],
      stagingBuffer,
      _textureImage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region
    );

    VkImageMemoryBarrier destbarrier = {};
    destbarrier = barrier;
    destbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    destbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    destbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    destbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    destbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    vkCmdPipelineBarrier(
      commandBuffers[ci],
      sourceStage, destinationStage,
      0,
      0, nullptr,
      0, nullptr,
      1, &destbarrier
    );
  }

  f(commandBuffers[ci]);


  vkEndCommandBuffer(commandBuffers[ci]);
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffers[ci];
  submitInfo.signalSemaphoreCount = 0; //1;
  submitInfo.pSignalSemaphores = nullptr; // (VkSemaphore*)&complete;
  // Wait for the previous frame's VDP2 composite to finish sampling the shared
  // per-frame resources before this submit's transfers overwrite them. This is
  // the Adreno-reliable (GPU-GPU) form of the WAR ordering that the in-buffer
  // pipeline barrier provides on desktop; see VIDVulkan::vdp2ReadDoneSignalSem.
  // No-op (VK_NULL_HANDLE) when no composite signal is pending.
  VkSemaphore upWaitSem = vulkan->vdp2ReadDoneWaitSem();
  VkPipelineStageFlags upWaitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  if (upWaitSem != VK_NULL_HANDLE) {
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &upWaitSem;
    submitInfo.pWaitDstStageMask = &upWaitStage;
  }
  vkResetFences(device, 1, &copyFence);
  ErrorCheck(vkQueueSubmit(vulkan->getVulkanQueue(), 1, &submitInfo, copyFence));
  //vkDeviceWaitIdle(device);

  vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &imageBuffer);
  //yprintf("vkMapMemory %lx to %lx", (uintptr_t)imageBuffer, (uintptr_t)imageBuffer + imageSize);

  _texture = (unsigned int *)imageBuffer;

}

void TextureManager::waitForCopy() {
  if (copyFence != VK_NULL_HANDLE) {
    vkWaitForFences(vulkan->getDevice(), 1, &copyFence, VK_TRUE, UINT64_MAX);
  }
}

