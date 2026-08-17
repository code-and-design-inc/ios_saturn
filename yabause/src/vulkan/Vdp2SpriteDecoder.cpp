// Copyright 2026 devMiyax
//
// Vdp2SpriteDecoder implementation (issue #22, task T-009). See
// Vdp2SpriteDecoder.h for the design summary and 01-design.md section 2.5.
//
// The fragment shader decodes a VDP1 framebuffer pixel into the kSprite slice's
// color + packed attr, mirroring the host-port reference
// vulkan/test/vdp2_sprite_decode.h decodeSprite() 1:1 (UT-010 pins it). Keep the
// two in sync. ASCII-only comments (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#include "Vdp2SpriteDecoder.h"
#include "Vdp2GBuffer.h"
#include "VIDVulkan.h"
#include "VulkanTools.h"

#include "shaderc/shaderc.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// Fullscreen triangle generated from gl_VertexIndex; no vertex buffer. v_uv is
// the [0,1] texture coordinate used to sample the VDP1 framebuffer.
//
// The VDP1 framebuffer must be sampled with the same Y orientation as the
// canonical drawfb path (FramebufferRenderer): there the quad maps clip-space
// bottom (gl_Position.y = -1) to texcoord.y = 1 and clip-space top (+1) to
// texcoord.y = 0 (see FramebufferRenderer.cpp vertex/texcoord pairs feeding
// Yglprg_vdp2_drawfb_cram_vulkan_f's `addr = v_texcoord`). The raw
// gl_VertexIndex UV has the opposite Y, which flips the sprite vertically, so
// we flip v_uv.y here to match the canonical path while keeping the fullscreen
// coverage. The background slices (renderLayersToGBuffer) are already correct,
// so only the sprite sampling needs this alignment.
static const char* kVertSrc = R"S(
layout(location = 0) out vec2 v_uv;
void main() {
  vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
  v_uv = vec2(pos.x, 1.0 - pos.y);
}
)S";

// Sprite decode fragment shader. Mirrors vdp2_sprite_decode.h decodeSprite() and
// the existing FramebufferRenderer drawfb shader (Yglprg_vdp2_drawfb_cram_vulkan_f).
//
// Alpha-byte encoding (encodeAlphaByte): bit7 show, bit6 palette, bits5:3
// color-calc code, bits2:0 priority slot. Index-color: R/G = low/high palette
// byte, B = shadow flag; the CRAM image is already decoded RGBA so we texelFetch
// the color directly (same as the legacy drawfb shader). Direct-color: R/G/B is
// the color. Attr packing matches packGBufferAttr / oracle packAttr: bits 0-4
// priority, bit 5 ccEnable, bits 6-11 ccRatio, bit 12 transparent.
//
// Any transparent path uses `discard`, so the slice keeps its render-pass clear
// value (color 0, attr with the transparent bit set) -- exactly the "undrawn
// sprite is transparent" contract the compositor relies on (UT-E03).
static const char* kFragSrc = R"S(
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 outColor;
layout(location = 1) out uint outAttr;

layout(binding = 0) uniform Ubo {
  ivec4 priorityTable[8];    // .x = priority value 0..7
  ivec4 spriteRatioTable[8]; // .x = ratio 0..0x3F
  int spcccs;
  int spccn;
  int ccWindowOn;
  int spriteWindow;
  int colorRamOffset;
  int perLine;               // 1 = per-line raster tables in `lines`
  int vdp2height;            // display height for v_uv.y -> line mapping
  int pad2;
  // Per-display-line packed tables (perLine != 0): .x = 8 priorities x 4 bits,
  // .y = cc ratios for cc codes 0..3 (8 bits each), .z = codes 4..7.
  uvec4 lines[512];
} ubo;

layout(binding = 1) uniform sampler2D s_vdp1FrameBuffer;
layout(binding = 2) uniform sampler2D s_color;

uint packAttr(uint priority, uint ccEnable, uint ccRatio, bool doShadow) {
  return (priority & 0x1Fu)
       | ((ccEnable != 0u) ? (1u << 5) : 0u)
       | ((ccRatio & 0x3Fu) << 6)
       | (doShadow ? (1u << 13) : 0u);   // G4: sprite shadow caster
}

void main() {
  vec4 fbColor = texture(s_vdp1FrameBuffer, v_uv);
  int additional = int(fbColor.a * 255.0 + 0.5);

  // show bit clear -> transparent.
  if ((additional & 0x80) == 0) { discard; }

  int slot = additional & 0x07;
  int ccCode = (additional >> 3) & 0x07;

  vec3 color = vec3(0.0);
  bool shadow = false;

  if ((additional & 0x40) != 0) {
    // Index-color (palette) path.
    if (fbColor.b >= 0.45) { shadow = true; }
    int colindex = (int(fbColor.g * 255.0 + 0.5) << 8) | int(fbColor.r * 255.0 + 0.5);
    if (colindex == 0) {
      // hard/vdp1/hon/p02_11.htm: index 0 ignored when sprite window on or slot 0.
      if (ubo.spriteWindow != 0 || slot == 0) { discard; }
    }
    colindex += ubo.colorRamOffset;
    color = texelFetch(s_color, ivec2(colindex, 0), 0).rgb;
  } else {
    // Direct-color path.
    if (ubo.spriteWindow != 0 && fbColor.r == 0.0 && fbColor.g == 0.0 && fbColor.b == 0.0) {
      discard;
    }
    color = fbColor.rgb;
  }

  // Per-line raster override (perline_alpha_draw & 0x40): PRISA / CCRS* are
  // rewritten mid-frame by some games (Bio Hazard raises slot-0 priority only
  // on the menu scanlines), so read this fragment's line entry instead of the
  // frame-uniform tables. Mirrors the legacy FramebufferRenderer per-line
  // texture (updateVdp2Reg rows 1..16).
  int line = 0;
  int priority;
  if (ubo.perLine != 0) {
    // v_uv is the VDP1 FB sampling coordinate, which is vertically flipped
    // relative to Saturn display lines (see the vertex shader comment):
    // Saturn line L lives at v_uv.y = 1 - L / vdp2height.
    line = clamp(int((1.0 - v_uv.y) * float(ubo.vdp2height)), 0, 511);
    priority = int((ubo.lines[line].x >> (slot * 4)) & 0x7u);
  } else {
    priority = ubo.priorityTable[slot].x & 0x7;
  }
  // Priority 0 -> not drawn (titan skips priority 0).
  if (priority == 0) { discard; }

  // Sprite color calculation (SPCCCS transparency test).
  uint ccEnable = 0u;
  uint ccRatio = 0x3Fu;
  if (ubo.ccWindowOn != 0) {
    bool ccApplies = false;
    if (ubo.spcccs == 0) { ccApplies = (priority <= ubo.spccn); }
    else if (ubo.spcccs == 1) { ccApplies = (priority == ubo.spccn); }
    else if (ubo.spcccs == 2) { ccApplies = (priority >= ubo.spccn); }
    else { ccApplies = shadow; } // case 3: MSB (approx)
    if (ccApplies) {
      ccEnable = 1u;
      if (ubo.perLine != 0) {
        uint packed = (ccCode < 4) ? ubo.lines[line].y : ubo.lines[line].z;
        ccRatio = (packed >> ((ccCode & 3) * 8)) & 0x3Fu;
      } else {
        ccRatio = uint(ubo.spriteRatioTable[ccCode].x) & 0x3Fu;
      }
    }
  }

  // Per-line sprite color offset (CLOFEN bit 6 rastered mid-frame). Packed by
  // renderSpriteToGBuffer into lines[line].w: bit 31 = enabled, bits 0-8 /
  // 9-17 / 18-26 = 9-bit two's-complement R/G/B. When perLine is active the
  // compositor skips its blanket register offset for the sprite slice
  // (_gbSlicePerLine[kSprite]), so this is the only place it is applied.
  if (ubo.perLine != 0) {
    uint co = ubo.lines[line].w;
    if ((co & 0x80000000u) != 0u) {
      int or9 = int(co & 0x1FFu);
      int og9 = int((co >> 9) & 0x1FFu);
      int ob9 = int((co >> 18) & 0x1FFu);
      if (or9 >= 256) { or9 -= 512; }
      if (og9 >= 256) { og9 -= 512; }
      if (ob9 >= 256) { ob9 -= 512; }
      color = clamp(color + vec3(float(or9), float(og9), float(ob9)) / 255.0,
                    vec3(0.0), vec3(1.0));
    }
  }

  // G4: a shadow-flagged sprite pixel casts a shadow on the layer below (the
  // compositor drops it and halves the lower SHADEN layer). The VDP1 framebuffer
  // only carries a single shadow flag, so self-shadow vs cast-shadow are not
  // distinguished here; the cast-shadow-onto-layer-below case is modelled.
  outColor = vec4(color, 1.0);
  outAttr = packAttr(uint(priority), ccEnable, ccRatio, shadow);
}
)S";

Vdp2SpriteDecoder::Vdp2SpriteDecoder(VIDVulkan* vulkan) : vulkan(vulkan) {}

Vdp2SpriteDecoder::~Vdp2SpriteDecoder() {
  release();
}

void Vdp2SpriteDecoder::setup() {
  if (setupDone) {
    return;
  }
  createDescriptors();
  setupDone = true;
}

void Vdp2SpriteDecoder::createDescriptors() {
  VkDevice device = vulkan->getDevice();

  VkDeviceSize uboSize = sizeof(UniformBufferObject);
  for (int i = 0; i < kFrames; i++) {
    vulkan->createBuffer(uboSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uboBuffer[i], uboMemory[i]);
  }

  std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));

  std::array<VkDescriptorPoolSize, 2> poolSizes = {};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = kFrames;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = kFrames * 2;

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = kFrames;
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

  for (int i = 0; i < kFrames; i++) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet[i]));
  }

  VkPipelineLayoutCreateInfo plInfo = {};
  plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  plInfo.setLayoutCount = 1;
  plInfo.pSetLayouts = &descriptorSetLayout;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &plInfo, nullptr, &pipelineLayout));

  // Nearest / clamp sampler for the VDP1 framebuffer: the alpha byte is bit
  // data, so no interpolation between texels is allowed.
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &fbSampler));
}

VkShaderModule Vdp2SpriteDecoder::compileGlsl(const char* code, int shaderKind) {
  VkDevice device = vulkan->getDevice();

  std::string target = std::string("#version 450\n") + std::string(code);

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
      target, static_cast<shaderc_shader_kind>(shaderKind), "vdp2_sprite_decode", options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    char msg[512];
    snprintf(msg, sizeof(msg), "Vdp2SpriteDecoder shader compile failed: %s",
             result.GetErrorMessage().c_str());
    throw std::runtime_error(msg);
  }

  std::vector<uint32_t> data(result.cbegin(), result.cend());
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = data.size() * sizeof(uint32_t);
  createInfo.pCode = data.data();
  VkShaderModule module = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &module));
  return module;
}

void Vdp2SpriteDecoder::createPipeline(VkRenderPass targetRenderPass) {
  VkDevice device = vulkan->getDevice();

  if (vertModule == VK_NULL_HANDLE) {
    vertModule = compileGlsl(kVertSrc, shaderc_vertex_shader);
  }
  if (fragModule == VK_NULL_HANDLE) {
    fragModule = compileGlsl(kFragSrc, shaderc_fragment_shader);
  }

  VkPipelineShaderStageCreateInfo vertStage = {};
  vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertModule;
  vertStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragStage = {};
  fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragModule;
  fragStage.pName = "main";

  VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

  VkPipelineVertexInputStateCreateInfo vertexInput = {};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

  // Two color attachments: color (location 0) + attr (location 1). Blend off;
  // a surviving fragment overwrites both, a discarded one leaves the clear value.
  VkPipelineColorBlendAttachmentState colorAttachment = {};
  colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorAttachment.blendEnable = VK_FALSE;
  VkPipelineColorBlendAttachmentState attachments[2] = {colorAttachment, colorAttachment};

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 2;
  colorBlending.pAttachments = attachments;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = stages;
  pipelineInfo.pVertexInputState = &vertexInput;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = targetRenderPass;
  pipelineInfo.subpass = 0;

  VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
  pipelineRenderPass = targetRenderPass;
}

void Vdp2SpriteDecoder::invalidatePipeline() {
  if (pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(vulkan->getDevice(), pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;
  }
  pipelineRenderPass = VK_NULL_HANDLE;
}

void Vdp2SpriteDecoder::renderToSlice(VkCommandBuffer commandBuffer,
                                      Vdp2GBuffer* gbuffer,
                                      VkImageView vdp1FbView,
                                      VkImageView cramView,
                                      VkSampler cramSampler,
                                      const SpriteParams& params,
                                      int vdp2Width, int vdp2Height) {
  (void)vdp2Width;
  (void)vdp2Height;
  if (!setupDone || gbuffer == nullptr || !gbuffer->isAllocated() ||
      vdp1FbView == VK_NULL_HANDLE) {
    return;
  }

  VkDevice device = vulkan->getDevice();
  VkRenderPass gbPass = gbuffer->getRenderPass();

  if (pipeline == VK_NULL_HANDLE || pipelineRenderPass != gbPass) {
    invalidatePipeline();
    createPipeline(gbPass);
  }

  // Fill the UBO (vec4-aligned int arrays for std140).
  UniformBufferObject ubo = {};
  for (int i = 0; i < 8; i++) {
    ubo.priorityTable[i][0] = params.priorityTable[i];
    ubo.spriteRatioTable[i][0] = params.spriteRatioTable[i];
  }
  ubo.spcccs = params.spcccs;
  ubo.spccn = params.spccn;
  ubo.ccWindowOn = params.ccWindowOn;
  ubo.spriteWindow = params.spriteWindow;
  ubo.colorRamOffset = params.colorRamOffset;
  ubo.perLine = params.perLine;
  ubo.vdp2height = params.vdp2Height;
  if (params.perLine != 0) {
    memcpy(ubo.lines, params.lines, sizeof(ubo.lines));
  }

  int fi = frameIndex;
  frameIndex = (frameIndex + 1) % kFrames;

  void* data = nullptr;
  vkMapMemory(device, uboMemory[fi], 0, sizeof(ubo), 0, &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(device, uboMemory[fi]);

  VkDescriptorBufferInfo bufInfo = {};
  bufInfo.buffer = uboBuffer[fi];
  bufInfo.offset = 0;
  bufInfo.range = sizeof(ubo);

  VkDescriptorImageInfo fbInfo = {};
  fbInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  fbInfo.imageView = vdp1FbView;
  fbInfo.sampler = fbSampler;

  VkDescriptorImageInfo cramInfo = {};
  cramInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  cramInfo.imageView = cramView;
  cramInfo.sampler = cramSampler;

  std::array<VkWriteDescriptorSet, 3> writes = {};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = descriptorSet[fi];
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &bufInfo;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = descriptorSet[fi];
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].descriptorCount = 1;
  writes[1].pImageInfo = &fbInfo;

  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = descriptorSet[fi];
  writes[2].dstBinding = 2;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[2].descriptorCount = 1;
  writes[2].pImageInfo = &cramInfo;

  vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

  const int w = gbuffer->getWidth();
  const int h = gbuffer->getHeight();

  // Clear values match renderLayersToGBuffer: color transparent black, attr with
  // the transparent bit (bit 12) set so undrawn texels are skipped (UT-E03).
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color.float32[0] = 0.0f;
  clearValues[0].color.float32[1] = 0.0f;
  clearValues[0].color.float32[2] = 0.0f;
  clearValues[0].color.float32[3] = 0.0f;
  clearValues[1].color.uint32[0] = (1u << 12);

  VkRect2D renderArea{};
  renderArea.offset = {0, 0};
  renderArea.extent.width = (uint32_t)w;
  renderArea.extent.height = (uint32_t)h;

  VkRenderPassBeginInfo rpBegin{};
  rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpBegin.renderPass = gbPass;
  rpBegin.framebuffer = gbuffer->getFramebuffer((uint32_t)vdp2cc::kSprite);
  rpBegin.renderArea = renderArea;
  rpBegin.clearValueCount = (uint32_t)clearValues.size();
  rpBegin.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport vp{};
  vp.x = 0.0f;
  vp.y = 0.0f;
  vp.width = (float)w;
  vp.height = (float)h;
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;
  VkRect2D sc{};
  sc.offset = {0, 0};
  sc.extent.width = (uint32_t)w;
  sc.extent.height = (uint32_t)h;

  vkCmdSetViewport(commandBuffer, 0, 1, &vp);
  vkCmdSetScissor(commandBuffer, 0, 1, &sc);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                          0, 1, &descriptorSet[fi], 0, nullptr);
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffer);
}

void Vdp2SpriteDecoder::release() {
  VkDevice device = vulkan->getDevice();
  if (device == VK_NULL_HANDLE) {
    return;
  }

  invalidatePipeline();

  if (vertModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device, vertModule, nullptr);
    vertModule = VK_NULL_HANDLE;
  }
  if (fragModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device, fragModule, nullptr);
    fragModule = VK_NULL_HANDLE;
  }
  if (pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
  }
  if (descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    descriptorPool = VK_NULL_HANDLE;
  }
  if (descriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    descriptorSetLayout = VK_NULL_HANDLE;
  }
  if (fbSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, fbSampler, nullptr);
    fbSampler = VK_NULL_HANDLE;
  }
  for (int i = 0; i < kFrames; i++) {
    if (uboBuffer[i] != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, uboBuffer[i], nullptr);
      uboBuffer[i] = VK_NULL_HANDLE;
    }
    if (uboMemory[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, uboMemory[i], nullptr);
      uboMemory[i] = VK_NULL_HANDLE;
    }
  }
  setupDone = false;
}
