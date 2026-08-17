// Copyright 2026 devMiyax
//
// Vdp2GBuffer implementation. See Vdp2GBuffer.h for the design summary and
// docs/feature/issue-22/01-design.md section 2.5 for the architecture.

#include "Vdp2GBuffer.h"
#include "VIDVulkan.h"
#include "VulkanTools.h"

#include <array>

Vdp2GBuffer::Vdp2GBuffer(VIDVulkan* vulkan) : vulkan(vulkan) {}

Vdp2GBuffer::~Vdp2GBuffer() {
  free();
}

void Vdp2GBuffer::resize(int newWidth, int newHeight) {
  if (newWidth <= 0 || newHeight <= 0) {
    return;
  }
  if (isAllocated() && width == newWidth && height == newHeight) {
    return;
  }
  free();
  allocate(newWidth, newHeight);
}

void Vdp2GBuffer::createRenderPass() {
  VkDevice device = vulkan->getDevice();

  // Two color attachments: color slice (RGBA8) + attr slice (R32_UINT).
  // No blending and no depth: each layer is decoded independently and the
  // compositor sorts by the per-pixel priority stored in attr.
  std::array<VkAttachmentDescription, 2> attachments = {};

  attachments[0].format = kColorFormat;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  attachments[1].format = kAttrFormat;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  std::array<VkAttachmentReference, 2> colorRefs = {};
  colorRefs[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  colorRefs[1] = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
  subpass.pColorAttachments = colorRefs.data();

  // Layout transitions: written as color attachments, then read in the
  // compositor as sampled images.
  std::array<VkSubpassDependency, 2> dependencies = {};

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
  vkDebugNameObject(device, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)renderPass, "Vdp2GBuffer.renderPass");
}

void Vdp2GBuffer::allocate(int newWidth, int newHeight) {
  VkDevice device = vulkan->getDevice();

  width = newWidth;
  height = newHeight;

  VkMemoryAllocateInfo memAlloc = {};
  memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  VkMemoryRequirements memReqs;

  // Helper to create one array image (kSliceCount layers) with the given
  // format, used as both a color attachment and a sampled image.
  auto createArrayImage = [&](VkFormat format, VkImage& outImage, VkDeviceMemory& outMem, const char* name) {
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = format;
    image.extent.width = static_cast<uint32_t>(width);
    image.extent.height = static_cast<uint32_t>(height);
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = kSliceCount;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &outImage));
    vkDebugNameObject(device, VK_OBJECT_TYPE_IMAGE, (uint64_t)outImage, name);
    vkGetImageMemoryRequirements(device, outImage, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkan->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &outMem));
    VK_CHECK_RESULT(vkBindImageMemory(device, outImage, outMem, 0));
  };

  createArrayImage(kColorFormat, colorImage, colorMem, "Vdp2GBuffer.colorImage");
  createArrayImage(kAttrFormat, attrImage, attrMem, "Vdp2GBuffer.attrImage");

  // Array views (all slices) for sampling in the compositor.
  auto createArrayView = [&](VkImage img, VkFormat format, VkImageView& outView, const char* name) {
    VkImageViewCreateInfo view = {};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view.format = format;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = kSliceCount;
    view.image = img;
    VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &outView));
    vkDebugNameObject(device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)outView, name);
  };

  createArrayView(colorImage, kColorFormat, colorArrayView, "Vdp2GBuffer.colorArrayView");
  createArrayView(attrImage, kAttrFormat, attrArrayView, "Vdp2GBuffer.attrArrayView");

  // Per-slice single-layer views (framebuffer attachments).
  auto createSliceView = [&](VkImage img, VkFormat format, uint32_t slice, VkImageView& outView, const char* name) {
    VkImageViewCreateInfo view = {};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = slice;
    view.subresourceRange.layerCount = 1;
    view.image = img;
    VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &outView));
    vkDebugNameObject(device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)outView, name);
  };

  for (uint32_t i = 0; i < kSliceCount; i++) {
    createSliceView(colorImage, kColorFormat, i, colorSliceView[i], "Vdp2GBuffer.colorSliceView");
    createSliceView(attrImage, kAttrFormat, i, attrSliceView[i], "Vdp2GBuffer.attrSliceView");
  }

  // Shared sampler for reading slices (nearest, clamp to edge).
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = samplerInfo.addressModeU;
  samplerInfo.addressModeW = samplerInfo.addressModeU;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));
  vkDebugNameObject(device, VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler, "Vdp2GBuffer.sampler");

  createRenderPass();

  // One framebuffer per slice, binding that slice's color + attr views.
  for (uint32_t i = 0; i < kSliceCount; i++) {
    VkImageView attachments[2] = {colorSliceView[i], attrSliceView[i]};

    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments = attachments;
    fbInfo.width = static_cast<uint32_t>(width);
    fbInfo.height = static_cast<uint32_t>(height);
    fbInfo.layers = 1;
    VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbInfo, nullptr, &frameBuffer[i]));
    vkDebugNameObject(device, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)frameBuffer[i], "Vdp2GBuffer.frameBuffer");
  }
}

void Vdp2GBuffer::free() {
  VkDevice device = vulkan->getDevice();

  for (uint32_t i = 0; i < kSliceCount; i++) {
    if (frameBuffer[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, frameBuffer[i], nullptr);
      frameBuffer[i] = VK_NULL_HANDLE;
    }
    if (colorSliceView[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device, colorSliceView[i], nullptr);
      colorSliceView[i] = VK_NULL_HANDLE;
    }
    if (attrSliceView[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device, attrSliceView[i], nullptr);
      attrSliceView[i] = VK_NULL_HANDLE;
    }
  }

  if (renderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;
  }
  if (sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
  }

  if (colorArrayView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, colorArrayView, nullptr);
    colorArrayView = VK_NULL_HANDLE;
  }
  if (attrArrayView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, attrArrayView, nullptr);
    attrArrayView = VK_NULL_HANDLE;
  }

  if (colorImage != VK_NULL_HANDLE) {
    vkDestroyImage(device, colorImage, nullptr);
    colorImage = VK_NULL_HANDLE;
  }
  if (colorMem != VK_NULL_HANDLE) {
    vkFreeMemory(device, colorMem, nullptr);
    colorMem = VK_NULL_HANDLE;
  }
  if (attrImage != VK_NULL_HANDLE) {
    vkDestroyImage(device, attrImage, nullptr);
    attrImage = VK_NULL_HANDLE;
  }
  if (attrMem != VK_NULL_HANDLE) {
    vkFreeMemory(device, attrMem, nullptr);
    attrMem = VK_NULL_HANDLE;
  }

  width = -1;
  height = -1;
}
