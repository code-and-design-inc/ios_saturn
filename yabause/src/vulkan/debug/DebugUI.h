// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// DebugUI: initializes ImGui (GLFW + Vulkan backend) and draws a debug
// overlay on top of the emulator's framebuffer.
//
// Task 3 stage: F9 toggles ImGui demo window. Pause / VDP1 inspector come
// in later tasks.
//
// NOTE: keep this file ASCII-only. MSVC default code page on Windows JP
// builds is cp932; non-ASCII chars in source files cause C4819 warnings
// and, with multi-byte sequences, parser failures.
#pragma once

#include <vulkan/vulkan.h>

#include "DebugSnapshot.h"
#include "FbReadback.h"
#include "MemoryDecode.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Vdp2DebugUI;

class Renderer;
class Window;
class VIDVulkan;

class DebugUI {
public:
  DebugUI();
  ~DebugUI();

  DebugUI(const DebugUI&) = delete;
  DebugUI& operator=(const DebugUI&) = delete;

  // Initialize. Call once after yabauseinit() and before the main loop.
  // Requires Window's pre-present hook plumbing from Task 2.
  bool init(Renderer* renderer, Window* window, VIDVulkan* vid);

  // Idempotent shutdown.
  void shutdown();

  // Bound to F9.
  void toggleVisible();
  bool isVisible() const { return _visible; }

  // Task 15 hotkey dispatcher. Called from yui.cpp's GLFW key callback on
  // GLFW_PRESS. Handles F9 (debounced 200ms) / F10 / F11 / Ctrl+E /
  // Ctrl+L / Esc. Unrecognized keys are silently ignored, so unrelated
  // key handling in the host can stay alongside this call.
  void onKeyDown(int glfwKey, bool ctrl);

  // Call once per frame, before YabauseExec(). Drives ImGui's NewFrame /
  // user widget construction / Render(). Actual GPU recording happens in
  // recordImGuiDraw() via the pre-present hook.
  void buildFrame();

  // Pause loop / snapshot. Returns true if either VDP1 or VDP2 debugger
  // has requested a pause; the host main loop uses this to gate
  // YabauseExec.
  bool isPaused() const;

  // F9: request a pause toggle. If currently running, sets _pendingPause so
  // the next pre-beginFrame hook captures a DebugSnapshot. If currently
  // paused, immediately resumes (and frees the snapshot).
  void requestTogglePause();

  // Called from Vdp1ComputeRasterizer::beginFrame() via the pre-begin hook.
  // When _pendingPause is set, takes a DebugSnapshot here (cpuCmds still
  // hold the previous frame's parsed commands) and transitions to paused.
  void onPreBeginFrame();

  // Pause-loop placeholder. dispatchUpTo() driving comes in Task 14.
  void renderPausedFrame();

private:
  bool _available = false;
  bool _visible   = false;

  // Pause state.
  bool     _paused       = false;
  bool     _pendingPause = false;
  // Pointer of the Vdp1ComputeRasterizer instance the pre-beginFrame hook
  // is currently registered on. The compute rasterizer is destroyed and
  // recreated whenever VIDVulkan triggers rebuildFrameBuffer (resolution
  // change, polygon mode change, etc.), so a single bool flag is insufficient.
  // buildFrame() re-installs the hook whenever this pointer differs from
  // the current vdp1Compute instance.
  class Vdp1ComputeRasterizer* _hookedComputeInstance = nullptr;
  // Same idea for the graphics path. Vdp1Renderer is created once at
  // VIDVulkan construction and survives polygon-mode changes, but the hook
  // re-installation logic is shared with the compute case for symmetry.
  class Vdp1Renderer* _hookedRendererInstance = nullptr;
  // True while the active snapshot was captured in COMPUTE_RASTERIZER mode.
  // Step replay (renderPausedFrame / dispatchUpTo) requires the compute
  // rasterizer; in GPU_TESSERATION snapshots we leave the live offscreen
  // image intact instead.
  bool     _snapshotIsCompute = false;
  int      _stepN        = 0;
  uint64_t _frameId      = 0;

  // Owned. Allocated in onPreBeginFrame(), freed in requestTogglePause()
  // (resume) and shutdown().
  DebugSnapshot* _snapshot = nullptr;

  // Partial-init progress flags. Used by cleanupPartial() to unwind
  // resources when init() fails midway, and by shutdown() for the
  // full-success path (both share the same teardown logic).
  bool _imguiContextCreated = false;
  bool _glfwBackendInited   = false;
  bool _vulkanBackendInited = false;

  Renderer*  _renderer = nullptr;
  Window*    _window   = nullptr;
  VIDVulkan* _vid      = nullptr;

  VkDescriptorPool _imguiDescPool = VK_NULL_HANDLE;

  void recordImGuiDraw(VkCommandBuffer cb,
                       uint32_t        swapchainImageIdx,
                       VkFramebuffer   framebuffer);

  // Step Control panel (Task 10). Drawn while paused (or when the user
  // toggled the debug overlay visible) inside the docking host set up
  // by buildFrame(). Mutates _stepN; does not yet drive dispatchUpTo().
  void buildStepControlPanel();

  // Command List panel (Task 11). 4-column table (Idx/Type/Coords/Status)
  // of the current snapshot's rawCmds. Double-click on a row jumps
  // _stepN to that index. _selectedCmdIndex == -1 means "none selected,
  // fall back to _stepN" (used by the Detail panel in Task 12).
  void buildCommandListPanel();
  int  _selectedCmdIndex = -1;

  // Command Detail panel (Task 12). Shows the JSONL representation of
  // the currently selected command, or step N when nothing is selected.
  // _detailJsonCacheIndex tracks the index whose JSON is currently
  // formatted into _detailJsonCache; -2 forces regeneration.
  void        buildCommandDetailPanel();
  std::string _detailJsonCache;
  int         _detailJsonCacheIndex = -2;

  // Shape preview (rendered above the JSON dump): visualizes the raw
  // CMDXA..CMDYD vertices in Saturn pixel space, the bbox grid, and the
  // pixels the scanline rasterizer would actually fill. Used to spot
  // degenerate / twisted quad cases at a glance.
  void buildCommandShapePreview(int target);

  // JSONL Export panel (Task 13). Writes the current snapshot to
  // <exe_dir>/debug/vdp1/latest.jsonl plus an optional timestamped
  // copy. The Open Folder button launches the OS file explorer at
  // the export directory.
  void buildJsonlExportPanel();
  bool        _alsoTimestampedCopy = true;
  std::string _lastExportPath;
  std::string _lastExportError;

  // Offscreen Preview panel (Task 14). Binds the VDP1 compute
  // rasterizer's offscreen image as an ImGui texture and displays it
  // inside an ImGui window with aspect-ratio-preserving fit.
  void buildOffscreenPreviewPanel();

  // Memory Viewer panel: hex view of VDP1 VRAM / VDP2 CRAM / VDP1 offscreen
  // FB, with optional decoded texture / LUT preview and FB pixel detail.
  // See docs/superpowers/specs/2026-05-06-memory-viewer-design.md.
  void buildMemoryViewerPanel();

  // Re-run FbReadback if cache is stale (frameId or stepN changed). Called
  // from onPreBeginFrame, renderPausedFrame, and the Memory Viewer panel
  // when the user first selects FB region.
  void refreshFbReadback();

  enum class MemRegion : int {
      Vdp1Vram = 0,
      Vdp2Cram = 1,
      Vdp1Fb   = 2,
  };
  MemRegion       _memViewRegion       = MemRegion::Vdp1Vram;
  uint32_t        _memViewAddr         = 0;
  int             _memViewBytesPerRow  = 16;
  int             _memDecodeMode       = -1;   // -1 = Hex only
  uint32_t        _memDecodeWidth      = 0;
  uint32_t        _memDecodeHeight     = 0;
  uint32_t        _memDecodeLutAddr    = 0;
  // FB pixel click detail (set when user clicks a pixel in FB hex view).
  // -1 = no selection.
  int             _memFbClickX         = -1;
  int             _memFbClickY         = -1;
  // Set true when _memViewAddr changes via address input or Jump button so
  // the hex view scrolls to that row on the next frame. Cleared after the
  // scroll is applied.
  bool            _memViewScrollPending = false;

  // FB readback cache (populated by captureFbReadback() at snapshot/step
  // changes). Owned by the DebugUI instance; released on shutdown.
  FbReadback      _fbReadback;
  bool            _fbReadbackValid     = false;
  // Tracks the (frameId, stepN) the cache was filled for -- invalidate when
  // either changes.
  uint64_t        _fbReadbackFrameId   = ~0ull;
  int             _fbReadbackStepN     = -1;

  // Decoded texture preview (Memory Viewer): owns its own VkImage / view /
  // descriptor (independent from _previewTex which binds the live offscreen
  // image). uploadedKey caches "{region, addr, mode, w, h, lutAddr}" so we
  // skip re-upload on no-change frames.
  VkImage         _decodeImage         = VK_NULL_HANDLE;
  VkDeviceMemory  _decodeImageMem      = VK_NULL_HANDLE;
  VkImageView     _decodeImageView     = VK_NULL_HANDLE;
  VkDescriptorSet _decodeImguiTex      = VK_NULL_HANDLE;
  uint32_t        _decodeUploadedW     = 0;
  uint32_t        _decodeUploadedH     = 0;
  uint64_t        _decodeUploadedKey   = ~0ull;
  // Helper: hash {region, addr, mode, w, h, lutAddr} into one uint64_t.
  static uint64_t makeDecodeCacheKey(int region, uint32_t addr, int mode,
                                     uint32_t w, uint32_t h, uint32_t lut);
  // Encode RGBA8 pixels into _decodeImage. Reallocates VkImage if the
  // requested size differs. Returns true on success.
  bool uploadDecodedTexture(const std::vector<uint32_t>& rgba,
                            uint32_t w, uint32_t h);

  VkSampler       _previewSampler    = VK_NULL_HANDLE;
  VkDescriptorSet _previewTex        = VK_NULL_HANDLE;  // ImGui_ImplVulkan_AddTexture result
  VkImageView     _previewBoundView  = VK_NULL_HANDLE;  // currently-bound view (cache)
  int             _lastDispatchedStepN = -1;
  // 0 = aspect-fit (panel-relative). >0 = native-pixel multiplier with
  // scrollable region; the sampler uses VK_FILTER_NEAREST so pixels
  // stay crisp at high zoom.
  float           _previewZoom       = 0.0f;
  // Last frame's zoom value. Used by buildOffscreenPreviewPanel to detect
  // a zoom change and re-anchor the scroll position so the viewport-center
  // image coordinate stays put (default behavior when content size grows
  // under a fixed scroll is to anchor at the top-left corner instead).
  float           _previewZoomPrev   = 0.0f;

  // Task 15: F9 debounce. glfwGetTime() returns seconds since GLFW init;
  // suppress repeated F9 events within 200ms so a held key doesn't
  // pause/resume rapidly.
  double _lastF9TimeSec = 0.0;

  // B20: F10 debounce (same idea as F9). Toggles the VDP2 debugger.
  double _lastF10TimeSec = 0.0;

  // issue #22: F11 debounce. Toggles VIDVulkan's new per-pixel VDP2
  // compositor (_vdp2NewComposite) for live A/B comparison.
  double _lastF11TimeSec = 0.0;

  // B20: VDP2 debugger. Mutually exclusive with the VDP1 panel: pressing
  // F9 while _vdp2 is active forces VDP2 resume, and vice versa.
  std::unique_ptr<Vdp2DebugUI> _vdp2;

  static std::string getExeDir();
  static void openInExplorer(const std::string& path);

  // Tear down whatever subset of init() succeeded. Safe to call from
  // both the failure path (init -> false) and from shutdown(); only
  // touches resources whose corresponding flag is set.
  void cleanupPartial();

  // GPU_TESSERATION-mode replay. Re-renders snapshot commands [0, stepN]
  // into the VDP1 offscreen image by:
  //   1. memcpy-saving live Vdp1Ram + struct-copy *Vdp1Regs.
  //   2. Installing snapshot.vram / snapshot.regs into those globals.
  //   3. Asking Vdp1Renderer to skip its auto Vdp1DrawCommands walk and
  //      calling drawStart() (state reset + system/user clip pseudo-polys).
  //   4. Walking the snapshot's command chain ourselves with addr / jump
  //      tracking, dispatching each cmd via VIDCore->Vdp1*Draw and stopping
  //      after stepN+1 commands (skipped commands count toward the step).
  //   5. Calling drawEnd() to submit the GPU render pass.
  //   6. Restoring Vdp1Ram + *Vdp1Regs from the backups, clearing the skip
  //      flag, vkDeviceWaitIdle so live emulation cannot resume mid-replay.
  // Safe in pause mode only (YabauseExec is not running, so the SH-2 cannot
  // race against the global swap).
  void dispatchGraphicsUpTo(int stepN);
};
