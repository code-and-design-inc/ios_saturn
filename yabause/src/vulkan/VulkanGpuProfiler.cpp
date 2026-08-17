/*
        Copyright 2026 devMiyax(smiyaxdev@gmail.com)

This file is part of YabaSanshiro.
*/
#include "VulkanGpuProfiler.h"

#include "VulkanTools.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

#if defined(__ANDROID__)
#include <unistd.h>
#endif

VulkanGpuProfiler::VulkanGpuProfiler() = default;

VulkanGpuProfiler::~VulkanGpuProfiler() {
  deInit();
}

void VulkanGpuProfiler::init(VkDevice inDevice,
                             VkPhysicalDevice,
                             const VkPhysicalDeviceProperties& properties,
                             bool graphicsQueueSupportsTimestamp,
                             bool pipelineStatisticsSupported) {
  deInit();

  device = inDevice;
  timestampPeriodNs = properties.limits.timestampPeriod;
  if (device == VK_NULL_HANDLE || !graphicsQueueSupportsTimestamp ||
      timestampPeriodNs <= 0.0f || !runtimeDebugPackageEnabled()) {
    return;
  }

  VkQueryPoolCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  qci.queryType = VK_QUERY_TYPE_TIMESTAMP;
  qci.queryCount = MaxSamples * QueriesPerSample;
  if (vkCreateQueryPool(device, &qci, nullptr, &timestampPool) != VK_SUCCESS) {
    timestampPool = VK_NULL_HANDLE;
    return;
  }

  // Pipeline statistics query is suspected of slowing down compute dispatch
  // on Adreno (each query may force per-dispatch serialization). Disable on
  // Android to remove that confounder while we measure VDP1 compute perf.
  // Re-enable explicitly via VKPROF_FORCE_PIPELINE_STATS for opt-in tests.
#if defined(__ANDROID__) && !defined(VKPROF_FORCE_PIPELINE_STATS)
  (void)pipelineStatisticsSupported;
#else
  if (pipelineStatisticsSupported) {
    VkQueryPoolCreateInfo sqci{};
    sqci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    sqci.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    sqci.queryCount = MaxSamples;
    sqci.pipelineStatistics =
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
    if (vkCreateQueryPool(device, &sqci, nullptr, &pipelineStatsPool) == VK_SUCCESS) {
      pipelineStatsEnabled = true;
    }
  }
#endif

  enabled = true;
  LOGI("[VKPROF] enabled: timestampPeriod=%.3f ns samples=%u pipelineStats=%d\n",
       timestampPeriodNs, MaxSamples, pipelineStatsEnabled ? 1 : 0);
}

void VulkanGpuProfiler::deInit() {
  if (device != VK_NULL_HANDLE && pipelineStatsPool != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device, pipelineStatsPool, nullptr);
  }
  if (device != VK_NULL_HANDLE && timestampPool != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device, timestampPool, nullptr);
  }
  pipelineStatsPool = VK_NULL_HANDLE;
  timestampPool = VK_NULL_HANDLE;
  device = VK_NULL_HANDLE;
  enabled = false;
  pipelineStatsEnabled = false;
  nextSample = 0;
  completedSamples = 0;
  for (auto& sample : samples) {
    sample = {};
  }
}

uint32_t VulkanGpuProfiler::beginSample(VkCommandBuffer commandBuffer, Zone zone, uint64_t frameNo) {
  if (!enabled || commandBuffer == VK_NULL_HANDLE || zone == Zone::Count) {
    return InvalidSample;
  }

  const uint32_t sampleId = allocateSample();
  if (sampleId == InvalidSample) {
    return InvalidSample;
  }

  Sample& sample = samples[sampleId];
  sample = {};
  sample.inUse = true;
  sample.zone = zone;
  sample.frameNo = frameNo;
  sample.hasPipelineStats = pipelineStatsEnabled && ((frameNo % 60u) == 0u);

  const uint32_t firstQuery = sampleId * QueriesPerSample;
  vkCmdResetQueryPool(commandBuffer, timestampPool, firstQuery, QueriesPerSample);
  vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, firstQuery);
  if (sample.hasPipelineStats) {
    vkCmdResetQueryPool(commandBuffer, pipelineStatsPool, sampleId, 1);
    vkCmdBeginQuery(commandBuffer, pipelineStatsPool, sampleId, 0);
  }
  return sampleId;
}

void VulkanGpuProfiler::endSample(VkCommandBuffer commandBuffer, uint32_t sampleId) {
  if (!enabled || commandBuffer == VK_NULL_HANDLE || sampleId >= MaxSamples || !samples[sampleId].inUse) {
    return;
  }
  if (samples[sampleId].hasPipelineStats) {
    vkCmdEndQuery(commandBuffer, pipelineStatsPool, sampleId);
  }
  // End timestamp goes immediately after the last marked stage (slot
  // 1+stageCount). Unused stage slots stay reset / unavailable, so we
  // never read them in collect(). This avoids VK_NOT_READY for samples
  // that emitted fewer than MaxStagesPerSample stages.
  const Sample& sample = samples[sampleId];
  const uint32_t endOffset = 1u + sample.stageCount;
  vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      timestampPool, sampleId * QueriesPerSample + endOffset);
}

void VulkanGpuProfiler::markStage(VkCommandBuffer commandBuffer,
                                  uint32_t sampleId,
                                  const char* stageName) {
  if (!enabled || commandBuffer == VK_NULL_HANDLE
      || sampleId >= MaxSamples || !samples[sampleId].inUse) {
    return;
  }
  Sample& sample = samples[sampleId];
  if (sample.stageCount >= MaxStagesPerSample) {
    return;
  }
  const uint32_t stageIdx  = sample.stageCount;
  const uint32_t queryOff  = sampleId * QueriesPerSample + 1u + stageIdx;
  vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      timestampPool, queryOff);
  sample.stageNames[stageIdx] = stageName;
  sample.stageCount = stageIdx + 1u;
}

void VulkanGpuProfiler::setVdp1Stats(uint32_t sampleId,
                                     uint32_t commandCount,
                                     const Vdp1ComputeRasterizer::DispatchStats& stats,
                                     uint32_t tileOverflow) {
  if (sampleId >= MaxSamples || !samples[sampleId].inUse) {
    return;
  }
  samples[sampleId].commandCount = commandCount;
  samples[sampleId].vdp1Stats = stats;
  samples[sampleId].tileOverflow = tileOverflow;
}

void VulkanGpuProfiler::collect(uint32_t sampleId) {
  if (!enabled || sampleId >= MaxSamples || !samples[sampleId].inUse) {
    return;
  }

  Sample sample = samples[sampleId];
  samples[sampleId] = {};

  // Only fetch the queries we actually wrote: begin (0), stage marks
  // (1..stageCount), end (1+stageCount). Reading the unused trailing
  // slots returns VK_NOT_READY because they were reset but never written,
  // which would silently drop the entire log line.
  uint64_t values[QueriesPerSample] = {};
  const uint32_t writtenCount = 2u + sample.stageCount;
  VkResult result = vkGetQueryPoolResults(device,
                                          timestampPool,
                                          sampleId * QueriesPerSample,
                                          writtenCount,
                                          sizeof(uint64_t) * writtenCount,
                                          values,
                                          sizeof(uint64_t),
                                          VK_QUERY_RESULT_64_BIT);
  if (result != VK_SUCCESS) {
    return;
  }

  const uint64_t beginTick = values[0];
  const uint64_t endTick   = values[1u + sample.stageCount];
  const uint64_t deltaTicks = endTick >= beginTick ? endTick - beginTick : 0;
  const double gpuMs = static_cast<double>(deltaTicks) * timestampPeriodNs / 1000000.0;

  double stageMs[MaxStagesPerSample + 1] = {};
  uint32_t stageCount = sample.stageCount;
  if (stageCount > 0) {
    uint64_t prev = beginTick;
    for (uint32_t i = 0; i < stageCount; ++i) {
      const uint64_t t = values[1u + i];
      stageMs[i] = (t >= prev)
                     ? static_cast<double>(t - prev) * timestampPeriodNs / 1000000.0
                     : 0.0;
      prev = t;
    }
    stageMs[stageCount] = (endTick >= prev)
                            ? static_cast<double>(endTick - prev) * timestampPeriodNs / 1000000.0
                            : 0.0;
  }

  PipelineStats pipelineStats = {};
  if (sample.hasPipelineStats) {
    uint64_t statsValues[4] = {};
    const VkResult statsResult = vkGetQueryPoolResults(device,
                                                       pipelineStatsPool,
                                                       sampleId,
                                                       1,
                                                       sizeof(statsValues),
                                                       statsValues,
                                                       sizeof(uint64_t),
                                                       VK_QUERY_RESULT_64_BIT);
    if (statsResult == VK_SUCCESS) {
      pipelineStats.inputAssemblyVertices = statsValues[0];
      pipelineStats.vertexShaderInvocations = statsValues[1];
      pipelineStats.fragmentShaderInvocations = statsValues[2];
      pipelineStats.computeShaderInvocations = statsValues[3];
    }
  }
  if (sample.zone == Zone::Vdp1Compute) {
    lastVdp1PipelineStats = pipelineStats;
  }
  else if (sample.zone == Zone::Vdp2Draw || sample.zone == Zone::ExternalDraw) {
    lastVdp2PipelineStats = pipelineStats;
  }
  logSummaryIfNeeded(sample.zone, sample.frameNo, gpuMs, sample, stageMs);
}

void VulkanGpuProfiler::recordFenceWait(Zone zone, double waitMs) {
  if (!enabled) {
    return;
  }
  if (zone == Zone::Vdp1Compute) {
    lastVdp1FenceWaitMs = waitMs;
  }
  else if (zone == Zone::Vdp2Draw || zone == Zone::ExternalDraw) {
    lastVdp2FenceWaitMs = waitMs;
  }
}

bool VulkanGpuProfiler::runtimeDebugPackageEnabled() const {
#if defined(__ANDROID__)
  char cmdline[256] = {};
  FILE* fp = std::fopen("/proc/self/cmdline", "rb");
  if (!fp) {
    return false;
  }
  const size_t n = std::fread(cmdline, 1, sizeof(cmdline) - 1, fp);
  std::fclose(fp);
  if (n == 0) {
    return false;
  }
  return std::strstr(cmdline, ".debug") != nullptr;
#else
  return false;
#endif
}

uint32_t VulkanGpuProfiler::allocateSample() {
  for (uint32_t i = 0; i < MaxSamples; ++i) {
    const uint32_t candidate = (nextSample + i) % MaxSamples;
    if (!samples[candidate].inUse) {
      nextSample = (candidate + 1) % MaxSamples;
      return candidate;
    }
  }
  return InvalidSample;
}

void VulkanGpuProfiler::logSummaryIfNeeded(Zone zone,
                                           uint64_t frameNo,
                                           double gpuMs,
                                           const Sample& sample,
                                           const double* stageMs) {
  // Defensive: profiler is off in release/pro packages, so the printf calls
  // below must never reach logcat in production builds. collect() already
  // guards on `enabled`, but enforce it here too so any future call site
  // cannot leak [VKPROF] lines into shipping APKs.
  if (!enabled) {
    return;
  }
  completedSamples++;

  bool shouldLog = (completedSamples % 60u) == 0u || gpuMs >= 12.0;

  if (zone == Zone::Vdp1Compute) {
    lastVdp1ComputeMs = gpuMs;
    lastVdp1CommandCount = sample.commandCount;
    lastVdp1DispatchCount = sample.vdp1Stats.dispatchCount;
    lastVdp1BarrierCount = sample.vdp1Stats.pipelineBarrierCount;
    lastVdp1BatchCount = sample.vdp1Stats.batchCount;
    lastVdp1TileOverflow = sample.tileOverflow;
    lastVdp1TileForward = sample.vdp1Stats.usedTileForwardPath;
    lastVdp1Forward = sample.vdp1Stats.usedForwardPath;
    shouldLog = shouldLog || sample.tileOverflow != 0u;
  }
  else if (zone == Zone::Vdp2Draw || zone == Zone::ExternalDraw) {
    lastVdp2DrawMs = gpuMs;
    shouldLog = shouldLog || (lastVdp1ComputeMs + lastVdp2DrawMs) >= 16.6;
  }

  if (!shouldLog) {
    return;
  }

  const char* mode = lastVdp1TileForward ? "tile-forward" : (lastVdp1Forward ? "per-batch-forward" : "tile-shade");
  // Use printf instead of LOGI: on Android the LOGI tag (vulkanYaba) does
  // not reliably reach logcat. yui.cpp overrides printf to __android_log
  // (tag=yabause) so this path is guaranteed visible.
  printf("[VKPROF] f=%llu zone=%s gpu=%.3fms vdp1=%.3fms vdp2=%.3fms "
       "mode=%s cmds=%u dispatch=%u barrier=%u batch=%u overflow=%u "
       "waitVdp1=%.3fms waitVdp2=%.3fms "
       "vdp1CS=%llu vdp2VS=%llu vdp2FS=%llu vdp2IA=%llu\n",
       static_cast<unsigned long long>(frameNo),
       zoneName(zone),
       gpuMs,
       lastVdp1ComputeMs,
       lastVdp2DrawMs,
       mode,
       lastVdp1CommandCount,
       lastVdp1DispatchCount,
       lastVdp1BarrierCount,
       lastVdp1BatchCount,
       lastVdp1TileOverflow,
       lastVdp1FenceWaitMs,
       lastVdp2FenceWaitMs,
       static_cast<unsigned long long>(lastVdp1PipelineStats.computeShaderInvocations),
       static_cast<unsigned long long>(lastVdp2PipelineStats.vertexShaderInvocations),
       static_cast<unsigned long long>(lastVdp2PipelineStats.fragmentShaderInvocations),
       static_cast<unsigned long long>(lastVdp2PipelineStats.inputAssemblyVertices));

  if (sample.stageCount > 0u && stageMs != nullptr) {
    char stageBuf[256] = {};
    int written = 0;
    for (uint32_t i = 0; i < sample.stageCount && written < (int)sizeof(stageBuf) - 32; ++i) {
      const char* nm = sample.stageNames[i] ? sample.stageNames[i] : "?";
      written += std::snprintf(stageBuf + written, sizeof(stageBuf) - written,
                               "%s%s=%.3fms", (i == 0 ? "" : " "), nm, stageMs[i]);
    }
    if (written < (int)sizeof(stageBuf) - 32) {
      std::snprintf(stageBuf + written, sizeof(stageBuf) - written,
                    " tail=%.3fms", stageMs[sample.stageCount]);
    }
    printf("[VKPROF-STAGES] f=%llu zone=%s gpu=%.3fms %s\n",
         static_cast<unsigned long long>(frameNo),
         zoneName(zone), gpuMs, stageBuf);
  }
}

const char* VulkanGpuProfiler::zoneName(Zone zone) const {
  switch (zone) {
  case Zone::Vdp1Compute: return "vdp1_compute";
  case Zone::Vdp2Draw: return "vdp2_draw";
  case Zone::ExternalDraw: return "external_draw";
  default: return "unknown";
  }
}
