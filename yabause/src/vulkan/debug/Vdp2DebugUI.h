// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp2DebugUI: VDP2 frame debugger UI for the Vulkan port.
//
// Lifecycle:
//   - Owned by DebugUI as std::unique_ptr.
//   - DebugUI calls init() once and shutdown() once.
//   - F10 (handled by DebugUI::onKeyDown) toggles pause+visible by calling
//     requestTogglePause() and toggleVisible() on this instance.
//   - F9 (VDP1) and F10 (VDP2) are mutually exclusive: pressing one while
//     the other is active forces the other to resume first.
//
// Snapshot model (Pause + raw state freeze):
//   - When _pendingPause is set, the next Vdp2DrawScreens entry triggers
//     onPreVdp2DrawScreens(), which captures a Vdp2Snapshot (deep copy of
//     VRAM/CRAM/Vdp2Regs + lineNBG0/1 + paraA/B) and transitions to paused.
//   - In paused mode, the host emulator's YabauseExec is skipped. The
//     DebugUI calls renderPausedFrame() each ImGui tick to keep the
//     overlay live and (Phase 5+) drive step replay.
//
// MVP Phase 4 scope:
//   - F10 toggle + empty Step Control panel
//   - Snapshot capture wired via VIDVulkan::setPreVdp2DrawScreensHook
//   - Step state placeholder (no replay yet)
//
// NOTE: ASCII-only file. MSVC default code page on Windows JP builds is
// cp932; non-ASCII chars cause C4819 + parser failures.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

// Vdp2PixelProbe.h is included here so that Vdp2PixelReport is a complete
// type when stored as a value member (avoids unique_ptr indirection and
// keeps the struct trivially copyable). The header is ASCII-only and pulls
// in vdp2.h via extern "C", which is the same guard already used by
// Vdp2Snapshot.h -- no ODR or include-order issues on MSVC.
#include "Vdp2PixelProbe.h"

class Renderer;
class Window;
class VIDVulkan;

struct Vdp2Snapshot;

class Vdp2DebugUI {
public:
    Vdp2DebugUI();
    ~Vdp2DebugUI();

    Vdp2DebugUI(const Vdp2DebugUI&)            = delete;
    Vdp2DebugUI& operator=(const Vdp2DebugUI&) = delete;

    bool init(Renderer* renderer, Window* window, VIDVulkan* vid);
    void shutdown();

    bool isVisible() const { return _visible; }
    bool isPaused()  const { return _paused; }

    // Called by DebugUI::onKeyDown for F10.
    void toggleVisible();
    void requestTogglePause();

    // Step navigation. Bound to PageDown / PageUp via DebugUI when this
    // debugger is the active pause owner.
    void stepForward();
    void stepBackward();

    // Hook installed on VIDVulkan via setPreVdp2DrawScreensHook(). Runs at
    // the top of Vdp2DrawScreens() before layers[] is cleared.
    void onPreVdp2DrawScreens();

    // Called once per ImGui tick (from DebugUI::buildFrame) while this UI
    // is visible or paused.
    void buildFrame();

    // Pause-loop placeholder. Phase 5+ will use this to drive cumulative
    // step replay via VIDVulkan::setVdp2StepLimit + Vdp2DrawEnd.
    void renderPausedFrame();

private:
    bool       _available    = false;
    bool       _visible      = false;
    bool       _paused       = false;
    bool       _pendingPause = false;

    Renderer*  _renderer = nullptr;
    Window*    _window   = nullptr;
    VIDVulkan* _vid      = nullptr;

    // -1 = uninitialized; on the first replay we render with no limit
    // and then clamp _stepN to (totalDraws - 1). After that, _stepN moves
    // in [0, totalDraws-1].
    int                            _stepN              = -1;
    int                            _lastDispatchedStep = -2;  // sentinel
    int                            _totalDrawCount     = 0;
    uint64_t                       _frameId            = 0;
    std::unique_ptr<Vdp2Snapshot>  _snapshot;

    // Panels.
    void buildStepControlPanel();
    void buildLayerListPanel();
    void buildLayerSettingsPanel();
    void buildColorCalcPanel();

    // Selected layer index in Layer List panel (0=NBG0..5=RBG1). Reflected
    // in Layer Settings panel. Defaults to -1 (none selected; settings
    // panel shows a placeholder).
    int _selectedLayer = -1;

    // Phase 7a: Isolate layer index. -1 = render all layers (default).
    // 0..4 = render only the layer whose VdpPipeline::id matches
    // (NBG0=0, NBG1=1, NBG2=2, NBG3=3, RBG0=4). RBG1 (index 5) cannot
    // be isolated separately because drawNBG0() reuses id=0 for RBG1
    // mode (BGON bit 5).
    //
    // When this changes, _stepN is reset to -1 so the next
    // renderPausedFrame tick re-discovers the total draw count (which
    // shrinks under isolation).
    int _isolateLayer = -1;

    // Pixel probe state (click on the rendered game frame while paused).
    // _probeReport is valid only when _hasProbe == true.
    bool            _hasProbe   = false;
    bool            _probeYFlip = false;
    Vdp2PixelReport _probeReport{};

    void buildPixelProbePanel();

    // Build the per-pixel probe section as a markdown string and append it
    // to `out`. Shared by "Copy Pixel Report" and "Copy Bug Report".
    void appendPixelProbeMarkdown(std::string& out);

    // issue #22 debug: build the "G-buffer population" section (which VDP2
    // layers renderLayersToGBuffer() actually drew into the new-composite
    // G-buffer this frame, and the skip reason for any that did not). Appended
    // to the bug report so a missing layer is attributed to a concrete cause.
    void appendGBufferTraceMarkdown(std::string& out);
};
