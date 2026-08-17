/*
        Copyright 2026 devMiyax(smiyaxdev@gmail.com)

This file is part of YabaSanshiro.
*/
#pragma once

#include "Platform.h"
#include "Vdp1ComputeRasterizer.h"

#include <array>
#include <cstdint>

class VulkanGpuProfiler {
public:
  enum class Zone : uint32_t {
    Vdp1Compute = 0,
    Vdp2Draw,
    ExternalDraw,
    Count,
  };

  static constexpr uint32_t InvalidSample = 0xffffffffu;

  VulkanGpuProfiler();
  ~VulkanGpuProfiler();

  void init(VkDevice device,
            VkPhysicalDevice physicalDevice,
            const VkPhysicalDeviceProperties& properties,
            bool graphicsQueueSupportsTimestamp,
            bool pipelineStatisticsSupported);
  void deInit();

  bool isEnabled() const { return enabled; }

  uint32_t beginSample(VkCommandBuffer commandBuffer, Zone zone, uint64_t frameNo);
  void endSample(VkCommandBuffer commandBuffer, uint32_t sampleId);
  // Insert an intermediate timestamp so the zone can be split into named
  // stages (e.g. "bin", "forward"). Up to MaxStagesPerSample stages can be
  // recorded between beginSample and endSample. Stage names are stored as
  // raw const-char* pointers; pass string literals only.
  void markStage(VkCommandBuffer commandBuffer, uint32_t sampleId, const char* stageName);
  void setVdp1Stats(uint32_t sampleId,
                    uint32_t commandCount,
                    const Vdp1ComputeRasterizer::DispatchStats& stats,
                    uint32_t tileOverflow);
  void collect(uint32_t sampleId);
  void recordFenceWait(Zone zone, double waitMs);

private:
  static constexpr uint32_t MaxStagesPerSample = 6;
  struct Sample {
    bool inUse = false;
    Zone zone = Zone::Count;
    uint64_t frameNo = 0;
    uint32_t commandCount = 0;
    uint32_t tileOverflow = 0;
    bool hasPipelineStats = false;
    uint32_t stageCount = 0;
    const char* stageNames[MaxStagesPerSample] = {};
    Vdp1ComputeRasterizer::DispatchStats vdp1Stats = {};
  };

  struct PipelineStats {
    uint64_t inputAssemblyVertices = 0;
    uint64_t vertexShaderInvocations = 0;
    uint64_t fragmentShaderInvocations = 0;
    uint64_t computeShaderInvocations = 0;
  };

  static constexpr uint32_t MaxSamples = 128;
  // Layout: slot 0 = begin, slots 1..MaxStagesPerSample = optional stage marks,
  // end timestamp lands at slot 1+stageCount (dynamic). Trailing reserved
  // slots stay reset/unavailable and must NOT be read back; collect()
  // limits its vkGetQueryPoolResults() range to 2+stageCount entries.
  static constexpr uint32_t QueriesPerSample = MaxStagesPerSample + 2;

  bool runtimeDebugPackageEnabled() const;
  uint32_t allocateSample();
  void logSummaryIfNeeded(Zone zone, uint64_t frameNo, double gpuMs,
                          const Sample& sample,
                          const double* stageMs);
  const char* zoneName(Zone zone) const;

  VkDevice device = VK_NULL_HANDLE;
  VkQueryPool timestampPool = VK_NULL_HANDLE;
  VkQueryPool pipelineStatsPool = VK_NULL_HANDLE;
  float timestampPeriodNs = 0.0f;
  bool enabled = false;
  bool pipelineStatsEnabled = false;
  uint32_t nextSample = 0;
  uint64_t completedSamples = 0;

  double lastVdp1ComputeMs = 0.0;
  double lastVdp2DrawMs = 0.0;
  double lastVdp1FenceWaitMs = 0.0;
  double lastVdp2FenceWaitMs = 0.0;
  uint32_t lastVdp1CommandCount = 0;
  uint32_t lastVdp1DispatchCount = 0;
  uint32_t lastVdp1BarrierCount = 0;
  uint32_t lastVdp1BatchCount = 0;
  uint32_t lastVdp1TileOverflow = 0;
  PipelineStats lastVdp1PipelineStats = {};
  PipelineStats lastVdp2PipelineStats = {};
  bool lastVdp1TileForward = false;
  bool lastVdp1Forward = false;

  std::array<Sample, MaxSamples> samples;
};
