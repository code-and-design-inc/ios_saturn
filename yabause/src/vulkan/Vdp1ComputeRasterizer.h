// Copyright 2026 devMiyax
#pragma once

#include "VulkanScene.h"
#include "Vdp1ComputeCommands.h"
#include "Vdp1ComputeMath.h"
#include <algorithm>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <vulkan/vulkan.h>

class VIDVulkan;

class Vdp1ComputeRasterizer {
public:
    Vdp1ComputeRasterizer(VIDVulkan* vulkan, int fbWidth, int fbHeight);
    ~Vdp1ComputeRasterizer();

    Vdp1ComputeRasterizer(const Vdp1ComputeRasterizer&) = delete;
    Vdp1ComputeRasterizer& operator=(const Vdp1ComputeRasterizer&) = delete;

    void beginFrame();
    void appendCommand(const Vdp1Cmd& cmd);
    // currentLayout: caller's tracked layout for targetImage at the moment
    // dispatch is invoked. The dispatcher emits an image-memory barrier
    // transitioning from currentLayout -> GENERAL for compute writes, then back
    // to SHADER_READ_ONLY_OPTIMAL at the end. Required because erase()
    // transitions the offscreen image to COLOR_ATTACHMENT_OPTIMAL between
    // compute frames; using a hard-coded oldLayout here causes Adreno to
    // discard compute writes (Windows desktop drivers are more lenient and
    // hide this bug).
    // Optional callback invoked between internal sub-stages so callers can
    // record GPU timestamps (e.g. via VulkanGpuProfiler::markStage). Names
    // are short string literals such as "bin" / "forward".
    using StageMarkerFn = std::function<void(const char*)>;

    void dispatch(VkCommandBuffer commandBuffer, VkImage targetImage, VkImageView targetView,
                  VkImageLayout currentLayout, int frameIndex,
                  const StageMarkerFn& markStage = StageMarkerFn{});
    // Step-execution variant: dispatch only commands [0, lastCmdIndex] (inclusive).
    // Pass UINT32_MAX to dispatch all (default dispatch() behavior).
    // cmdsOverride lets callers swap the command source (for debug step UI);
    // when non-null its contents are uploaded to the SSBO instead of cpuCmds.
    // frameIndex selects the per-frame descriptor set (drawframe = 0/1).
    void dispatchUpTo(VkCommandBuffer commandBuffer, VkImage targetImage, VkImageView targetView,
                      VkImageLayout currentLayout,
                      uint32_t lastCmdIndex,
                      int frameIndex,
                      const std::vector<Vdp1Cmd>* cmdsOverride = nullptr,
                      const StageMarkerFn& markStage = StageMarkerFn{});
    void resize(int fbWidth, int fbHeight);
    void uploadVramCram(const void* vram, const void* cram);

    int getCommandCount() const { return (int)cpuCmds.size(); }

    // F17 -- Read the tile-overflow counter that the previous frame's Bin
    // shader atomic-incremented (each increment = one cmd dropped because
    // its tile already had MAX_CMDS_PER_TILE entries). Resets to 0 in the
    // SSBO so the next dispatch starts clean. Caller should drain the
    // previous frame's compute fence before invoking, otherwise the read
    // races with GPU writes. Returns 0 when the SSBO is not yet mapped.
    uint32_t readAndResetTileOverflow();

    // Per-frame spike diagnostic. Logs immediately on spike conditions
    // (tile overflow, cmd count >= 800, mesh+gouraud combo with cmds >= 400)
    // and emits a rolling 60-frame window summary so silent frames do not
    // flood logcat. Cheap (few static counters + occasional printf) -- safe
    // to call once per compute frame. tileOverflow is the value just read
    // from readAndResetTileOverflow().
    void logFrameDiagnostic(uint32_t tileOverflow);

    void setSystemClip(int xc, int yc) { state.systemClip = glm::ivec2(xc, yc); }
    void setUserClip(int xa, int ya, int xc, int yc) { state.userClip = glm::ivec4(xa, ya, xc, yc); }
    void setLocalCoord(int x, int y) { state.localCoord = glm::ivec2(x, y); }
    // Per-axis HD upscale factor (= vdp1wratio * fbScaleX / vdp1hratio *
    // fbScaleY). scaleMax is derived as max(scaleX, scaleY) for band /
    // threshold use (cell coverage / supercover) where one number is
    // sufficient; scaleX / scaleY are used for axis-aligned BR
    // extension where the X and Y deltas must each be exactly 1 Saturn
    // screen cell on its own axis (otherwise non-uniform aspect ratios
    // would over- or under-extend per side).
    void setScale(float scaleX, float scaleY) {
        state.scaleX   = scaleX;
        state.scaleY   = scaleY;
        // Parens around std::max work around the windows.h `max` macro.
        state.scaleMax = (std::max)(scaleX, scaleY);
    }
    // VDP2 SPCTL & 0x3F -- packs sprite type (bits 0..3) plus SPCTL bit 5
    // (sprite-RGB-enable). Forwarded to the shade shader through
    // ShadePushConstants. The shader masks bits 0..3 for sprite type
    // (4bpp LUT pixel-info extraction) and bit 5 for the 16bpp RGB
    // direct/palette gate (Vdp1Renderer.cpp:4140 graphics rule). Updated
    // each frame by Vdp1Renderer::renderFrame() before any compute hook.
    void setSpriteType(uint32_t t) { state.spriteType = t & 0x3Fu; }
    const vdp1c::Vdp1State& getState() const { return state; }

    // Phase 1A toggle: per-texel forward mapping shader (Saturn-faithful).
    // When enabled, sprite cmds (Normal/Scaled/Distorted) dispatch to the
    // forward shader instead of tile_shade. Polygon / Polyline / Line keep
    // using the existing tile-binning path until later phases. Off by
    // default -- see docs/superpowers/specs/2026-05-08-vdp1-compute-
    // forward-mapping-design.md for the migration plan.
    void setUseForwardMapping(bool b) { useForwardMapping = b; }
    bool getUseForwardMapping() const { return useForwardMapping; }

    // Tile-binning forward toggle (perf opt 2026-05-08). When ON together
    // with useForwardMapping, the dispatcher reuses the existing Bin pass
    // and runs a single per-tile forward shader (1 vkCmdDispatch =
    // numTilesX * numTilesY workgroups). LIFO order is enforced by an
    // intra-workgroup memoryBarrierImage() + barrier() between cmds; tiles
    // are pixel-disjoint so cross-workgroup synchronization is not
    // required. Disabling falls back to the per-batch forward dispatch.
    void setUseTileForward(bool b) { useTileForward = b; }
    bool getUseTileForward() const { return useTileForward; }

    // Hook fired immediately before beginFrame() clears cpuCmds.
    // Used as a snapshot capture point by the debug UI.
    void setPreBeginFrameHook(std::function<void()> h) { preBeginFrameHook = std::move(h); }

    // Read-only access to the host-side command buffer for snapshot / debug UI.
    const std::vector<Vdp1Cmd>& getCpuCmds() const { return cpuCmds; }

    // Frame debugger stats: how many vkCmdDispatch / vkCmdPipelineBarrier
    // calls were recorded by the most recent dispatchUpTo() invocation,
    // plus forward-path batch breakdown (batch_count / cmds_per_batch
    // min/max). Reset at the start of each dispatchUpTo() so paused step
    // replays show that step's numbers, not last frame's.
    struct DispatchStats {
        uint32_t dispatchCount        = 0;
        uint32_t pipelineBarrierCount = 0;
        uint32_t batchCount           = 0;
        uint32_t totalCmdsDispatched  = 0;
        uint32_t minBatchSize         = 0;
        uint32_t maxBatchSize         = 0;
        // pmod-aware counters: how many dispatched cmds skip imageLoad in the
        // forward shader (cc==0 Replace / cc==2 Half-luminance) versus those
        // that need it (cc==1 Shadow / cc==3 Half-transparent). High skip
        // ratio => more memory bandwidth saved by the pmod-aware optimization.
        uint32_t cmdsSkipImageLoad    = 0;
        uint32_t cmdsNeedImageLoad    = 0;
        // Path identification. usedForwardPath covers BOTH per-batch
        // forward and tile-binning forward; usedTileForwardPath is set
        // only when the tile-binning variant was used. usedTilePath is
        // the legacy tile_shade fallback.
        bool     usedForwardPath      = false;
        bool     usedTileForwardPath  = false;
        bool     usedTilePath         = false;
    };
    const DispatchStats& getLastDispatchStats() const { return lastDispatchStats; }

    // VRAM bytes captured at uploadVramCram() time -- i.e. the same VRAM state
    // that produced cpuCmds during the just-completed render. The debug UI
    // must use this instead of live Vdp1Ram, because the game thread may have
    // overwritten Vdp1Ram (CMDLINK in particular) between the rendering pass
    // and the next frame's beginFrame hook, breaking parseRawCmds traversal
    // alignment with cpuCmds.
    const std::vector<uint8_t>& getCachedVram() const { return cachedVram; }
    const std::vector<uint8_t>& getCachedCram() const { return cachedCram; }

    // Offscreen image dimensions (physical pixel coords). Vdp1Cmd vertex
    // and bbox values are stored in this space, so debug overlays must
    // map from these dims, NOT Saturn 704x512 logical coords.
    int getFbWidth()  const { return fbWidth; }
    int getFbHeight() const { return fbHeight; }

private:
    VIDVulkan* vulkan;
    int fbWidth;
    int fbHeight;

    std::vector<Vdp1Cmd> cpuCmds;
    // Production fast-path index of POLYGON / DISTORTED_SPRITE commands within
    // cpuCmds. Built incrementally in appendCommand() so dispatchUpTo() avoids
    // an O(N) scan of the whole command list every frame. Debug paths
    // (lastCmdIndex != UINT32_MAX, cmdsOverride != nullptr) still use
    // collectDispatchIndices() since those use cases require a custom range.
    std::vector<uint32_t> dispatchableIndices;
    vdp1c::Vdp1State state;

    // Host-side VRAM/CRAM copies populated by uploadVramCram(). See
    // getCachedVram() for the reason these exist.
    std::vector<uint8_t> cachedVram;
    std::vector<uint8_t> cachedCram;

    // Optional hook fired right before cpuCmds is cleared at frame start.
    std::function<void()> preBeginFrameHook;

    bool resourcesReady = false;
    // First dispatch sees the image in UNDEFINED layout; subsequent dispatches
    // transition from SHADER_READ_ONLY restored at the end of the previous one.
    bool firstDispatch  = true;

    // Frame debugger counters. Updated by dispatchUpTo(), exposed via
    // getLastDispatchStats() to the debug UI.
    DispatchStats lastDispatchStats;

    // SSBO resources.
    VkBuffer       cmdSSBO          = VK_NULL_HANDLE;
    VkDeviceMemory cmdSSBOMem       = VK_NULL_HANDLE;
    void*          cmdSSBOMapped    = nullptr;
    VkDeviceSize   cmdSSBOSize      = 0;

    // VRAM (512KB) / CRAM (4KB) - HOST_VISIBLE+COHERENT, persistently mapped.
    static constexpr VkDeviceSize VDP1_VRAM_SIZE = 512 * 1024;
    static constexpr VkDeviceSize VDP2_CRAM_SIZE = 4 * 1024;

    VkBuffer       vramSSBO         = VK_NULL_HANDLE;
    VkDeviceMemory vramSSBOMem      = VK_NULL_HANDLE;
    void*          vramSSBOMapped   = nullptr;

    VkBuffer       cramSSBO         = VK_NULL_HANDLE;
    VkDeviceMemory cramSSBOMem      = VK_NULL_HANDLE;
    void*          cramSSBOMapped   = nullptr;

    // F17 -- Tile overflow counter SSBO. The Bin shader atomicAdds into this
    // when a tile's atomicAdd-derived slot exceeds MAX_CMDS_PER_TILE so the
    // CPU can detect dropped commands without a full TileCount read-back.
    // Host-visible + coherent so the next frame can read the previous
    // frame's value after fence drain (F14) and reset to 0 before resubmit.
    VkBuffer       overflowSSBO     = VK_NULL_HANDLE;
    VkDeviceMemory overflowSSBOMem  = VK_NULL_HANDLE;
    void*          overflowSSBOMapped = nullptr;
    static constexpr VkDeviceSize OVERFLOW_SSBO_SIZE = 16;  // 4B counter + 12B std430 pad

    // Phase B1 Tile Binning (B-lite) -- 2-pass dispatch infrastructure.
    // Pass 1 (bin): scatter cmds into 16x16 px tiles via atomicAdd.
    // Pass 2 (shade): one workgroup per tile, sequentially apply tile cmds
    //   per pixel with Replace blend (atomic-free RMW within each thread).
    //
    // Tile dimensions are recomputed in resize() since fbWidth/fbHeight can
    // change with resolution settings. tileBuffersFbW/H track the dims that
    // the buffers were sized for so we can detect stale state.
    static constexpr int TILE_SIZE_PX        = 16;
    static constexpr int TILE_MAX_CMDS       = 64;
    static constexpr size_t TILE_COUNT_STRIDE = 16;  // std430 aligned
    int   numTilesX            = 0;
    int   numTilesY            = 0;
    int   tileBuffersFbW       = -1;
    int   tileBuffersFbH       = -1;

    VkBuffer       tileCountSSBO       = VK_NULL_HANDLE;
    VkDeviceMemory tileCountSSBOMem    = VK_NULL_HANDLE;
    VkDeviceSize   tileCountSSBOSize   = 0;

    VkBuffer       tileCmdListSSBO     = VK_NULL_HANDLE;
    VkDeviceMemory tileCmdListSSBOMem  = VK_NULL_HANDLE;
    VkDeviceSize   tileCmdListSSBOSize = 0;

    // Runtime toggle: when true, dispatchUpTo() uses the 2-pass tile-binning
    // path. The legacy scanline fallback was removed in Phase 2A; this flag
    // is retained as the entry point for the forward-mapping path (when
    // useForwardMapping is true) and to keep public API stable.
    bool useTileBinning = true;

    void createBuffers();
    void destroyBuffers();
    void uploadCommands();
    void recreateTileBuffers();
    void destroyTileBuffers();
    void resetTileCount(VkCommandBuffer cb);

    // Shared descriptor pool used by tile-binning (bin/shade) and forward
    // pipelines. Sized at createPipelines() time.
    VkDescriptorPool      descPool                               = VK_NULL_HANDLE;

    // Phase B1 - Tile binning Bin shader pipeline (Pass 1).
    VkDescriptorSetLayout binDescLayout    = VK_NULL_HANDLE;
    VkPipelineLayout      binPipeLayout    = VK_NULL_HANDLE;
    VkPipeline            binPipeline      = VK_NULL_HANDLE;
    // Per-frame descriptor sets (drawframe = 0/1). Adreno validation rejects
    // vkUpdateDescriptorSets on a set still bound by an in-flight command
    // buffer; cycling sets per frame avoids that even when the offscreen
    // image view alternates each frame.
    VkDescriptorSet       binDescSet[2]    = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    // Phase B1 -- Tile shade pipeline (Pass 2).
    VkDescriptorSetLayout shadeDescLayout  = VK_NULL_HANDLE;
    VkPipelineLayout      shadePipeLayout  = VK_NULL_HANDLE;
    // Shade pipeline is specialized on SPCTL (constant_id 0, mask 0x3F).
    // shadeModule is the compiled shader (compiled once, reused for every
    // variant). shadePipelineCache stores one pipeline per observed SPCTL
    // value; typical games steady-state at 1-3 entries. shadePipeline is
    // kept as the pre-built default (SPCTL=0) so cold-frame dispatch never
    // builds inline.
    VkShaderModule        shadeModule      = VK_NULL_HANDLE;
    VkPipeline            shadePipeline    = VK_NULL_HANDLE;
    std::unordered_map<uint32_t, VkPipeline> shadePipelineCache;
    // Per-frame feature flags. Set during appendCommand based on scanning
    // each cmd; cleared in beginFrame. Feed getOrBuildShadePipeline so
    // unused features compile-eliminate via specialization constants.
    bool                  frameUsesUserClip = false;
    bool                  frameUsesGouraud  = false;
    bool                  frameUsesMesh     = false;
    bool                  frameUsesMsbShadow = false;
    VkDescriptorSet       shadeDescSet[2]  = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageView           lastShadeTargetView[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    // Pipeline cache key layout:
    //   bits 0..5  = SPCTL low 6 bits (sprite type + sprite-RGB)
    //   bit  6     = USE_USERCLIP
    //   bit  7     = USE_GOURAUD
    //   bit  8     = USE_MESH
    //   bit  9     = USE_MSB_SHADOW
    VkPipeline getOrBuildShadePipeline(uint32_t key);
    // Prebuild the set of shade pipeline variants observed in representative
    // games at createPipelines() time. Avoids 100ms+ in-game stutter when a
    // new variant first appears mid-gameplay. Full 1024-variant prebuild is
    // too slow (~10s+) so we use a curated subset; new variants log via
    // printf so the list can be extended over time.
    void prebuildAllShadeVariants();

    // Phase 1A -- Forward mapping pipeline. Bindings:
    //   0 = fb image, 1 = Cmd SSBO, 2 = VRAM SSBO.
    // No tile binning input (forward dispatches per-cmd, not per-tile).
    // Phase 1C: forward is the default rasterizer for ALL VDP1 draw cmds on
    // desktop GPUs (Sprite / Polygon via per-texel forward push; Polyline /
    // Line via per-pixel pull within the same per-cmd workgroup, Phase 2C).
    //
    // Sprint 7 (2026-05-10): Android (Adreno) defaults to false to use the
    // tile_shade (inverse mapping) path instead. Per-texel forward scatter
    // is pathologically slow on Adreno: imageLoad/Store RMW has no
    // concurrent resolve, full-screen bbox cmds get redundantly rasterised
    // by every tile workgroup, and compute disables UBWC compression.
    // tile_shade is per-pixel 1-thread/pixel with register accumulation, no
    // RMW, and bounded work per workgroup (16x16 = 256 px / cmd-list walk).
    // Empirically restores the 5/4 120fps baseline. Windows desktop GPUs
    // do not exhibit these issues so keep forward mapping there (Sprint 5
    // proved tile-forward = 14ms on Win for cmds=530).
#if defined(__ANDROID__)
    bool                  useForwardMapping       = false;
#else
    bool                  useForwardMapping       = false;
#endif
    // Tile-binning forward (perf opt 2026-05-08). Pipeline shares the
    // expanded forward descriptor / pipeline layout (5 bindings: fb, Cmd,
    // VRAM, TileCount, TileCmdList). Per-cmd shader does not reference
    // TileCount/TileCmdList so the bindings are inert in that mode.
    //
    // Semantics (Sprint 5, 2026-05-10):
    //   true  -> always tile-forward (Bin pass + tile-shade dispatch).
    //            This is the default for both Windows and Android -- Windows
    //            wins because of locality on faster GPUs; Android wins on
    //            cmds-heavy scenes (per-batch overhead is O(N) in cmds).
    //   false -> per-batch-forward (DebugUI override; A/B testing or
    //            debugging only). Auto-fallback also kicks in when the
    //            tile SSBOs are not yet initialised.
    //
    // The frame-adaptive heuristic explored in Sprint 3-4 was removed:
    // bbox / cmd-count thresholds proved scene-dependent and produced
    // surprising mode switches. Performance improvements for tile-forward
    // on Adreno are pursued internally (sprite tile-clip, mediump, SSBO
    // -> texelBuffer, etc.) instead of dispatcher-level switching.
    bool                  useTileForward          = true;
    VkPipeline            tileForwardPipeline     = VK_NULL_HANDLE;
    VkDescriptorSetLayout forwardDescLayout       = VK_NULL_HANDLE;
    VkPipelineLayout      forwardPipeLayout       = VK_NULL_HANDLE;
    VkPipeline            forwardPipeline         = VK_NULL_HANDLE;
    VkDescriptorSet       forwardDescSet[2]       = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageView           lastForwardTargetView[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    void createDescriptorLayouts();
    void createPipelines();
    void destroyPipelines();
    void updateTileDescriptorSets(VkImageView targetView, int frameIndex);
    void updateForwardDescriptorSets(VkImageView targetView, int frameIndex);

    VkShaderModule compileComputeShader(const std::string& source, const std::string& name);
};
