// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp2DebugUI implementation. See Vdp2DebugUI.h for design notes.
//
// Phases delivered so far:
//   P4   F10 toggle, snapshot capture via setPreVdp2DrawScreensHook
//   P5   Step Control panel + replay via setVdp2StepLimit / Vdp2DrawEnd
//   P5.5 VDP1 framebuffer composite participates as per-priority steps
//   P6   Layer List + Layer Settings panels with decoded fields
//
// NOTE: ASCII-only file.
#include "Vdp2DebugUI.h"

#include "Vdp2Snapshot.h"

#include "../Renderer.h"
#include "../Window.h"
#include "../VIDVulkan.h"
#include "../Vdp2ColorCalcState.h"

#include "imgui.h"

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>

extern "C" {
#include "vdp2.h"
#include "vidshared.h"
}

// VDP2 RAM / CRAM sizes (constants from vdp2.cpp T1/T2 init).
namespace {
constexpr size_t kVdp2RamSize = 0x80000;   // 512KB
constexpr size_t kVdp2CramSize = 0x1000;   // 4KB

// Solo sentinel for the VDP1 framebuffer pseudo-layer. Shared with the
// renderer so the isolate filter and the fb gate agree.
constexpr int kIsolateVdp1Fb = VIDVulkan::kVdp2IsolateVdp1Fb;
}  // namespace

Vdp2DebugUI::Vdp2DebugUI()  = default;
Vdp2DebugUI::~Vdp2DebugUI() { shutdown(); }

bool Vdp2DebugUI::init(Renderer* renderer, Window* window, VIDVulkan* vid) {
    if (renderer == nullptr || window == nullptr || vid == nullptr) {
        std::cerr << "[Vdp2DebugUI] init: null renderer/window/vid"
                  << std::endl;
        return false;
    }
    _renderer  = renderer;
    _window    = window;
    _vid       = vid;
    _available = true;
    return true;
}

void Vdp2DebugUI::shutdown() {
    if (!_available) return;
    if (_vid) _vid->setPreVdp2DrawScreensHook(nullptr);
    if (_vid && _paused) {
        _vid->setVdp2StepLimit(-1);
    }
    _snapshot.reset();
    _renderer  = nullptr;
    _window    = nullptr;
    _vid       = nullptr;
    _available = false;
    _visible   = false;
    _paused    = false;
}

void Vdp2DebugUI::toggleVisible() {
    _visible = !_visible;
}

void Vdp2DebugUI::requestTogglePause() {
    if (!_available || _vid == nullptr) return;

    if (_paused) {
        // Resume: drop snapshot + clear step limit so live emulation runs
        // unrestricted again. Reset _stepN to -1 so the next pause's
        // first replay tick runs uncapped (to discover the true total).
        _snapshot.reset();
        _vid->setVdp2StepLimit(-1);
        _vid->setVdp2IsolateLayer(-1);
        _vid->setPreVdp2DrawScreensHook(nullptr);
        _paused             = false;
        _pendingPause       = false;
        _stepN              = -1;
        _lastDispatchedStep = -2;
        _totalDrawCount     = 0;
        _isolateLayer       = -1;
    } else {
        // Arm the pre-hook so the next Vdp2DrawScreens captures a snapshot.
        _pendingPause = true;
        _vid->setPreVdp2DrawScreensHook([this]() { this->onPreVdp2DrawScreens(); });
    }
}

void Vdp2DebugUI::stepForward() {
    if (!_paused || _totalDrawCount <= 0) return;
    if (_stepN < _totalDrawCount) ++_stepN;
}

void Vdp2DebugUI::stepBackward() {
    if (!_paused) return;
    if (_stepN > 0) --_stepN;
}

void Vdp2DebugUI::onPreVdp2DrawScreens() {
    if (!_available || _vid == nullptr) return;
    if (!_pendingPause) return;

    // Capture raw state. Vdp2Ram / Vdp2ColorRam are extern C globals from
    // vdp2.h. Vdp2Regs is also extern; the "fix" copy is on VIDVulkan
    // (fixVdp2Regs is interlace-adjusted by the rendering path).
    //
    // lineNBG0 / lineNBG1 live on VIDVulkan as size-512 arrays; they're
    // populated lazily by drawNBG0/drawNBG1 in the *previous* frame, so
    // capturing them here gives the user a coherent snapshot of "state
    // about to be rendered this frame".
    //
    // Rotation parameters paraA / paraB are also on VIDVulkan as private
    // members. For Phase 4 we leave them empty (Phase 7 RBG thumbnail work
    // will surface accessors). The decoded LayerInfo for RBG0/RBG1 still
    // works from regs only.

    // Default-construct empty rotation params for now (matches behavior
    // before Phase 7 wires up paraA/paraB accessors on VIDVulkan).
    vdp2rotationparameter_struct paraEmpty{};

    Vdp2Snapshot s = Vdp2Snapshot::takeRaw(
        *Vdp2Regs, *Vdp2Regs,
        Vdp2Ram,      kVdp2RamSize,
        Vdp2ColorRam, kVdp2CramSize,
        /*lineNBG0*/ nullptr,
        /*lineNBG1*/ nullptr,
        paraEmpty, paraEmpty,
        _vid->vdp2width, _vid->vdp2height,
        ++_frameId);
    s.decodeLayers();

    _snapshot     = std::make_unique<Vdp2Snapshot>(std::move(s));
    _paused       = true;
    _pendingPause = false;
    // Leave _stepN = -1 (the header default / requestTogglePause reset)
    // so the first renderPausedFrame tick runs uncapped, discovers the
    // true total draw count via getVdp2LastDrawCount(), and anchors
    // _stepN at total-1. Setting it to 0 here would clamp the first
    // replay to a single draw and lock _totalDrawCount at 1 forever.
}

void Vdp2DebugUI::buildFrame() {
    if (!_available) return;
    if (!_visible && !_paused) return;

    buildStepControlPanel();
    buildLayerListPanel();
    buildLayerSettingsPanel();
    buildColorCalcPanel();
    buildPixelProbePanel();

    // Pixel probe: detect a left-click on the game frame (not on any ImGui
    // window). WantCaptureMouse is true when the cursor is over a panel.
    if (_snapshot != nullptr && _vid != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.MouseClicked[0] && !io.WantCaptureMouse) {
            // Map window logical pixels -> device pixels.
            float devX = io.MousePos.x * io.DisplayFramebufferScale.x;
            float devY = io.MousePos.y * io.DisplayFramebufferScale.y;

            // Guard: skip if the rendered region has zero extent (can happen
            // during the first tick before resize() was called).
            if (_vid->renderWidth > 0 && _vid->renderHeight > 0) {
                float u = (devX - (float)_vid->originx) / (float)_vid->renderWidth;
                float v = (devY - (float)_vid->originy) / (float)_vid->renderHeight;
                if (_probeYFlip) v = 1.0f - v;

                if (u >= 0.0f && u < 1.0f && v >= 0.0f && v < 1.0f) {
                    int sx = (int)(u * (float)_vid->vdp2width);
                    int sy = (int)(v * (float)_vid->vdp2height);
                    if (sx >= 0 && sx < _vid->vdp2width &&
                        sy >= 0 && sy < _vid->vdp2height) {
                        _probeReport = probeVdp2Pixel(
                            sx, sy,
                            _snapshot->regs,
                            _snapshot->vdp2Ram.data(),
                            _snapshot->cram.data());
                        _hasProbe = true;
                    }
                }
            }
        }
    }
}

void Vdp2DebugUI::renderPausedFrame() {
    if (!_available || !_paused || _vid == nullptr) return;

    // During pause the SH-2 emulator is not running, so the live VDP2
    // globals (Vdp2Ram / Vdp2ColorRam / *Vdp2Regs / fixVdp2Regs) are
    // frozen at the snapshot-capture moment. We can re-run the normal
    // rendering path directly with the step limit applied -- no need to
    // install/restore separate snapshot copies.
    //
    // Each renderPausedFrame call presents one frame:
    //   1. Set step limit to _stepN (or -1 on the first tick to discover
    //      the total draw count).
    //   2. Vdp2DrawScreens() -- rebuilds layers[] from live (= snapshot)
    //      VRAM/CRAM/regs. Our pre-hook returns early because _pendingPause
    //      is false during replay.
    //   3. Vdp2DrawEnd() -- composites + presents. Includes the ImGui
    //      pre-present hook so the panel overlay updates atop the new
    //      composite.
    //   4. Restore step limit to -1 so a subsequent unpause runs normally.

    int requestedLimit;
    if (_stepN < 0) {
        // First tick after pause: render everything to discover the total
        // draw count.
        requestedLimit = -1;
    } else {
        requestedLimit = _stepN;
    }

    _vid->setVdp2StepLimit(requestedLimit);
    _vid->setVdp2IsolateLayer(_isolateLayer);

    // Vdp2DrawStart() resets the texture/vertex managers (tm->reset(),
    // vm->reset()), which is required so the second Vdp2DrawScreens can
    // regenerate vertices. But it ALSO calls Vdp2RestoreRegs(0, Vdp2Lines)
    // and memcpy's that into _baseVdp2Regs, which can resurface a stale
    // per-line register slice and corrupt the snapshot state (observed
    // 2026-05-23: PRINA flipped from 0x0306 to 0x0301 between snapshot and
    // replay, swapping NBG0 priority 6 -> 1). Workaround: snapshot
    // _baseVdp2Regs before Vdp2DrawStart and restore it after, so
    // drawNBG/RBG sees the same priorities the user captured.
    Vdp2 baseBackup = _vid->getBaseVdp2Regs();
    _vid->Vdp2DrawStart();
    // Restore via Vdp2Regs as a side channel: writing through *Vdp2Regs
    // doesn't help because fixVdp2Regs already points at _baseVdp2Regs.
    // VIDVulkan owns _baseVdp2Regs; expose a write path via the same
    // const_cast indirection it already uses internally.
    const_cast<Vdp2&>(_vid->getBaseVdp2Regs()) = baseBackup;

    _vid->Vdp2DrawScreens();
    _vid->Vdp2DrawEnd();
    _vid->setVdp2StepLimit(-1);
    _vid->setVdp2IsolateLayer(-1);

    // Only adopt the draw counter as the canonical total when the render
    // was UNCAPPED (limit < 0). When limited, counter == limit and would
    // otherwise clamp _totalDrawCount permanently each time the user
    // steps back -- locking total at 0 after the first <<.
    if (requestedLimit < 0) {
        _totalDrawCount = _vid->getVdp2LastDrawCount();
        if (_stepN < 0 && _totalDrawCount > 0) {
            // Anchor _stepN at the full composite on first tick so the
            // user sees the unpaused look and can step backwards from
            // there. Step 0 = back color only, Step total = everything.
            _stepN = _totalDrawCount;
        }
    }
    _lastDispatchedStep = _stepN;
}

namespace {
const char* planeSizeStr(int p) {
    switch (p) {
    case 0:  return "1x1";
    case 1:  return "2x1";
    case 2:  return "2x2";
    default: return "?";
    }
}
const char* colorModeStr(int m) {
    switch (m) {
    case 0:  return "4bpp pal";
    case 1:  return "8bpp pal";
    case 2:  return "16bpp 5-5-5";
    case 3:  return "16bpp CRAM";
    case 4:  return "32bpp RGB";
    default: return "?";
    }
}
}  // namespace

void Vdp2DebugUI::buildLayerListPanel() {
    ImGui::SetNextWindowSize(ImVec2(360, 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 400),  ImGuiCond_FirstUseEver);
    if (ImGui::Begin("VDP2 Debugger -- Layer List")) {
        if (_snapshot == nullptr) {
            ImGui::TextDisabled("Awaiting first frame snapshot...");
            ImGui::End();
            return;
        }
        if (ImGui::BeginTable("layers", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Idx",      ImGuiTableColumnFlags_WidthFixed, 28);
            ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_WidthFixed, 48);
            ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Enabled",  ImGuiTableColumnFlags_WidthFixed, 56);
            ImGui::TableSetupColumn("Solo",     ImGuiTableColumnFlags_WidthFixed, 56);
            ImGui::TableHeadersRow();
            for (int i = 0; i < 6; ++i) {
                const auto& L = _snapshot->layers[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool selected = (_selectedLayer == i);
                char idxBuf[8];
                snprintf(idxBuf, sizeof(idxBuf), "%d##L%d", i, i);
                // AllowOverlap is required so the SpanAllColumns selectable
                // yields hit-testing to the Solo SmallButton submitted later
                // in this same row. Without it the selectable swallows the
                // button's click and the button can never be pressed.
                if (ImGui::Selectable(idxBuf, selected,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowOverlap)) {
                    _selectedLayer = i;
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(L.name);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", L.priority);
                ImGui::TableSetColumnIndex(3);
                if (L.enabled) ImGui::TextUnformatted("Y");
                else           ImGui::TextDisabled("n");
                ImGui::TableSetColumnIndex(4);
                // Solo button: isolate this layer's id. NBG0 and RBG1
                // share id=0 in the priority loop (drawNBG0 path), so
                // soloing index 5 is unsupported -- show "n/a".
                if (i == 5) {
                    ImGui::TextDisabled("n/a");
                } else {
                    bool soloed = (_isolateLayer == i);
                    char btn[16];
                    snprintf(btn, sizeof(btn), "%s##S%d",
                             soloed ? "*" : "o", i);
                    if (ImGui::SmallButton(btn)) {
                        _isolateLayer = soloed ? -1 : i;
                        // Total draw count shrinks under isolation; reset
                        // _stepN so the next replay tick re-discovers it.
                        _stepN          = -1;
                        _totalDrawCount = 0;
                    }
                }
            }

            // VDP1 framebuffer pseudo-layer row. Not part of VDP2 layers[];
            // soloing it isolates the VDP1 fb composite (see kIsolateVdp1Fb).
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("FB");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted("VDP1");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("-");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("-");
            ImGui::TableSetColumnIndex(4);
            {
                bool fbSolo = (_isolateLayer == kIsolateVdp1Fb);
                char fbBtn[16];
                snprintf(fbBtn, sizeof(fbBtn), "%s##Sfb",
                         fbSolo ? "*" : "o");
                if (ImGui::SmallButton(fbBtn)) {
                    _isolateLayer = fbSolo ? -1 : kIsolateVdp1Fb;
                    _stepN          = -1;
                    _totalDrawCount = 0;
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text("Isolate: ");
        ImGui::SameLine();
        if (_isolateLayer < 0) {
            ImGui::TextDisabled("(all layers)");
        } else {
            const char* nm;
            if (_isolateLayer == kIsolateVdp1Fb) {
                nm = "VDP1 FB";
            } else if (_isolateLayer >= 0 && _isolateLayer < 6) {
                nm = _snapshot->layers[_isolateLayer].name;
            } else {
                nm = "?";
            }
            ImGui::Text("%s only", nm);
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) {
                _isolateLayer   = -1;
                _stepN          = -1;
                _totalDrawCount = 0;
            }
        }
    }
    ImGui::End();
}

void Vdp2DebugUI::buildLayerSettingsPanel() {
    ImGui::SetNextWindowSize(ImVec2(380, 380), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(360, 20),   ImGuiCond_FirstUseEver);
    if (ImGui::Begin("VDP2 Debugger -- Layer Settings")) {
        if (_snapshot == nullptr) {
            ImGui::TextDisabled("Awaiting first frame snapshot...");
            ImGui::End();
            return;
        }
        if (_selectedLayer < 0 || _selectedLayer >= 6) {
            ImGui::TextDisabled("Select a layer in the Layer List panel.");
            ImGui::End();
            return;
        }
        const auto& L = _snapshot->layers[_selectedLayer];
        ImGui::Text("%s  (priority %d, %s)", L.name, L.priority,
                    L.enabled ? "enabled" : "disabled");
        ImGui::Separator();

        // Two-column key/value layout.
        auto kv = [](const char* k, const char* fmt, ...) {
            ImGui::TextDisabled("%s", k);
            ImGui::SameLine(140);
            char buf[128];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buf, sizeof(buf), fmt, args);
            va_end(args);
            ImGui::TextUnformatted(buf);
        };

        kv("Color mode:",   "%s", colorModeStr(L.colorMode));
        kv("Char size:",    "%dx%d cells (%dx%d pix)",
                            L.charSize, L.charSize,
                            L.charSize * 8, L.charSize * 8);
        kv("Bitmap mode:",  "%s", L.isBitmap ? "yes" : "no");
        kv("Plane size:",   "%s", planeSizeStr(L.planeSize));
        kv("Transparency:", "%s", L.transparent ? "enabled" : "disabled");
        kv("Color calc:",   "%s (ratio %d/32)",
                            L.colorCalcEnabled ? "enabled" : "disabled",
                            L.colorCalcRatio);
        kv("Scroll X:",     "%d", L.scrollX);
        kv("Scroll Y:",     "%d", L.scrollY);
        kv("Zoom X:",       "%.3f", L.zoomX);
        kv("Zoom Y:",       "%.3f", L.zoomY);
        kv("Mosaic:",       "%s", L.mosaicEnabled ? "enabled" : "disabled");
        if (L.colorOffsetSel < 0) {
            kv("Color offset:", "disabled");
        } else {
            kv("Color offset:", "set %c (R=%d G=%d B=%d)",
                                L.colorOffsetSel == 0 ? 'A' : 'B',
                                L.colorOffsetR, L.colorOffsetG, L.colorOffsetB);
        }
        kv("Rotation:",     "%s", L.isRotation ? "yes (RBG)" : "no");

        ImGui::Separator();
        ImGui::TextDisabled("Raw registers (selected):");
        const auto& R = _snapshot->regs;
        kv("BGON",   "0x%04X", R.BGON);
        kv("CHCTLA", "0x%04X", R.CHCTLA);
        kv("CHCTLB", "0x%04X", R.CHCTLB);
        kv("PRINA",  "0x%04X", R.PRINA);
        kv("PRINB",  "0x%04X", R.PRINB);
        kv("PRIR",   "0x%04X", R.PRIR);
        kv("CCCTL",  "0x%04X", R.CCCTL);
        kv("PLSZ",   "0x%04X", R.PLSZ);

        ImGui::Separator();
        // "Copy Registers" button: build a register+decoded markdown dump
        // and push it to the system clipboard.
        if (ImGui::Button("Copy Registers")) {
            // Shared body builder lambda (also used by buildColorCalcPanel).
            // Defined inline here; mirrors the one in buildColorCalcPanel.
            std::string md;
            char tmp[256];
            auto append = [&](const char* fmt, ...) {
                va_list ap;
                va_start(ap, fmt);
                vsnprintf(tmp, sizeof(tmp), fmt, ap);
                va_end(ap);
                md += tmp;
            };
            const bool newComp = _vid ? _vid->getVdp2NewComposite() : false;
            append("## VDP2 Register Dump (frame %llu, %dx%d, new-composite %s)\n\n",
                   (unsigned long long)_snapshot->frameId,
                   _snapshot->screenWidth, _snapshot->screenHeight,
                   newComp ? "ON" : "OFF");
            append("### Raw registers\n");
            append("TVMD=0x%04X  BGON=0x%04X  CHCTLA=0x%04X  CHCTLB=0x%04X\n",
                   R.TVMD, R.BGON, R.CHCTLA, R.CHCTLB);
            append("PRINA=0x%04X  PRINB=0x%04X  PRIR=0x%04X\n",
                   R.PRINA, R.PRINB, R.PRIR);
            append("CCCTL=0x%04X  CCRNA=0x%04X  CCRNB=0x%04X  CCRR=0x%04X\n",
                   R.CCCTL, R.CCRNA, R.CCRNB, R.CCRR);
            append("CCRSA=0x%04X  CCRSB=0x%04X  CCRSC=0x%04X  CCRSD=0x%04X  CCRLB=0x%04X\n",
                   R.CCRSA, R.CCRSB, R.CCRSC, R.CCRSD, R.CCRLB);
            append("PLSZ=0x%04X  LNCLEN=0x%04X  SFPRMD=0x%04X  SFCCMD=0x%04X\n",
                   R.PLSZ, R.LNCLEN, R.SFPRMD, R.SFCCMD);
            append("CLOFEN=0x%04X  CLOFSL=0x%04X  BKTAU=0x%04X  BKTAL=0x%04X\n",
                   R.CLOFEN, R.CLOFSL, R.BKTAU, R.BKTAL);
            // Color calc window (issue #22): control byte is WCTLD>>8.
            append("WCTLA=0x%04X  WCTLB=0x%04X  WCTLC=0x%04X  WCTLD=0x%04X\n",
                   R.WCTLA, R.WCTLB, R.WCTLC, R.WCTLD);
            append("W0 x=[%d..%d]  W1 x=[%d..%d]\n",
                   R.WPSX0, R.WPEX0, R.WPSX1, R.WPEX1);
            append("\n");

            // Decoded color calc section.
            std::array<uint16_t, 4> ccrs = {R.CCRSA, R.CCRSB, R.CCRSC, R.CCRSD};
            vdp2cc::State cc = vdp2cc::decodeVdp2ColorCalc(
                R.CCCTL, R.CCRNA, R.CCRNB, R.CCRR, ccrs, R.CCRLB);
            const bool boken = ((R.CCCTL >> 15) & 1) != 0;
            append("### Decoded color calc\n");
            append("Mode=%s  RatioSource=%s  EXCCEN=%s  BOKEN=%s  LineRatio=%d/32\n",
                   (cc.mode == vdp2cc::CalcMode::Add) ? "Additive" : "Ratio",
                   (cc.ratioSource == vdp2cc::RatioSource::Second) ? "Second" : "Top",
                   cc.extendedEnable ? "on" : "off",
                   boken ? "set" : "clear",
                   (int)cc.lineColorRatio);
            // Layer names and LayerIndex mapping (vdp2cc order: kNBG3=0..kSprite=5).
            const char* ccLayerNames[6] = {"NBG3", "NBG2", "NBG1", "NBG0", "RBG0", "Sprite"};
            for (int li = 0; li < 6; ++li) {
                const auto& lc = cc.perLayer[li];
                // Find priority from _snapshot->layers (index order: 0=NBG0,1=NBG1,2=NBG2,3=NBG3,4=RBG0,5=RBG1).
                // Map vdp2cc LayerIndex (kNBG3=0, kNBG2=1, kNBG1=2, kNBG0=3, kRBG0=4, kSprite=5)
                // to snapshot layers[] index (NBG0=0, NBG1=1, NBG2=2, NBG3=3, RBG0=4).
                int snapIdx = -1;
                switch (li) {
                case vdp2cc::kNBG0: snapIdx = 0; break;
                case vdp2cc::kNBG1: snapIdx = 1; break;
                case vdp2cc::kNBG2: snapIdx = 2; break;
                case vdp2cc::kNBG3: snapIdx = 3; break;
                case vdp2cc::kRBG0: snapIdx = 4; break;
                default: snapIdx = -1; break;
                }
                int prio = (snapIdx >= 0) ? _snapshot->layers[snapIdx].priority : 0;
                if (li < 5) {
                    append("%s: cc=%s ratio=%d/63 prio=%d\n",
                           ccLayerNames[li],
                           lc.ccEnable ? "on" : "off",
                           (int)lc.ratio, prio);
                } else {
                    // Sprite: no single ratio (table-driven), no snapshot priority row.
                    append("Sprite: cc=%s\n", lc.ccEnable ? "on" : "off");
                }
            }
            append("\n");

            // Decoded layers section.
            append("### Decoded layers (BGON/priority)\n");
            for (int i = 0; i < 6; ++i) {
                const auto& sl = _snapshot->layers[i];
                append("%s: enabled=%s prio=%d colorMode=%s transparent=%s colorCalc=%s ratio=%d mosaic=%s\n",
                       sl.name,
                       sl.enabled ? "Y" : "n",
                       sl.priority,
                       colorModeStr(sl.colorMode),
                       sl.transparent ? "yes" : "no",
                       sl.colorCalcEnabled ? "on" : "off",
                       sl.colorCalcRatio,
                       sl.mosaicEnabled ? "yes" : "no");
            }

            ImGui::SetClipboardText(md.c_str());
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// buildColorCalcPanel -- Feature A/B/C: Color Calc panel with register dump
// and bug-report clipboard buttons.
// ---------------------------------------------------------------------------
void Vdp2DebugUI::buildColorCalcPanel() {
    ImGui::SetNextWindowSize(ImVec2(380, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(740, 20),   ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("VDP2 Debugger -- Color Calc")) {
        ImGui::End();
        return;
    }

    if (_snapshot == nullptr) {
        ImGui::TextDisabled("Awaiting first frame snapshot...");
        ImGui::End();
        return;
    }

    // Two-column key/value layout (same lambda pattern as buildLayerSettingsPanel).
    auto kv = [](const char* k, const char* fmt, ...) {
        ImGui::TextDisabled("%s", k);
        ImGui::SameLine(160);
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        ImGui::TextUnformatted(buf);
    };

    const auto& R = _snapshot->regs;

    // Decode color-calc state from snapshot registers.
    std::array<uint16_t, 4> ccrs = {R.CCRSA, R.CCRSB, R.CCRSC, R.CCRSD};
    vdp2cc::State cc = vdp2cc::decodeVdp2ColorCalc(
        R.CCCTL, R.CCRNA, R.CCRNB, R.CCRR, ccrs, R.CCRLB);
    const bool boken = ((R.CCCTL >> 15) & 1) != 0;

    // Runtime path status.
    const bool newComp = _vid ? _vid->getVdp2NewComposite() : false;
    kv("New composite path:", "%s", newComp ? "ON" : "OFF (legacy)");

    ImGui::Separator();

    // Global CC mode fields.
    kv("Mode (CCMD):", "%s",
       (cc.mode == vdp2cc::CalcMode::Add) ? "Additive" : "Ratio");
    kv("Ratio source (CCRTMD):", "%s",
       (cc.ratioSource == vdp2cc::RatioSource::Second) ? "Second layer" : "Top layer");
    kv("Extended (EXCCEN):", "%s",
       cc.extendedEnable ? "enabled" : "disabled");
    kv("BOKEN (bit15):", "%s",
       boken ? "set (extended cc forced off)" : "clear");
    kv("Line color ratio:", "%d/32", (int)cc.lineColorRatio);

    // CRAM color mode (RAMCTL bits 13:12). Table 12.2 extended-cc ratio depends
    // on this: Mode 0 uses the 4:0:0 / 2:2:0 / 2:1:1 pair the compositor folds;
    // Mode 1/2 keeps 4:0:0 when the second image is palette format. issue #22.
    const int cramMode = (R.RAMCTL >> 12) & 0x3;
    static const char* cramModeStr[4] = {
        "0 (1024 col, RGB555)", "1 (2048 col, RGB555)",
        "2 (1024 col, RGB888)", "3 (prohibited)"};
    kv("CRAM mode (RAMCTL):", "%s", cramModeStr[cramMode]);

    // Per-line EXCCEN scan. CCCTL is a per-line register (Vdp2Lines[]). Report
    // whether EXCCEN toggles across scan lines (the case the per-line compositor
    // path handles) and how many lines have it set.
    {
        int onCount = 0, total = 0;
        bool anyOn = false, anyOff = false;
        for (int ln = 0; ln < 270; ++ln) {
            // Skip stale unused lines past a plausible active range cheaply by
            // stopping at the first all-zero CCCTL after some content; simplest:
            // count over the full snapshot-era table.
            const bool ex = (Vdp2Lines[ln].CCCTL & 0x400) != 0;
            ++total;
            if (ex) { ++onCount; anyOn = true; } else { anyOff = true; }
        }
        kv("EXCCEN per-line:", "%s (%d/%d lines on)",
           (anyOn && anyOff) ? "TOGGLES" : (anyOn ? "all on" : "all off"),
           onCount, total);
    }

    ImGui::Separator();
    ImGui::TextDisabled("Per-layer color calc:");

    // Per-layer rows. vdp2cc LayerIndex order: kNBG3=0, kNBG2=1, kNBG1=2,
    // kNBG0=3, kRBG0=4, kSprite=5.
    struct CcRow { const char* name; int vdp2ccIdx; };
    static const CcRow rows[6] = {
        {"NBG0 cc:", vdp2cc::kNBG0},
        {"NBG1 cc:", vdp2cc::kNBG1},
        {"NBG2 cc:", vdp2cc::kNBG2},
        {"NBG3 cc:", vdp2cc::kNBG3},
        {"RBG0 cc:", vdp2cc::kRBG0},
        {"Sprite cc:", vdp2cc::kSprite},
    };
    for (int i = 0; i < 6; ++i) {
        const auto& lc = cc.perLayer[rows[i].vdp2ccIdx];
        if (i < 5) {
            // NBG0..RBG0: show enable + ratio value.
            if (lc.ccEnable) {
                kv(rows[i].name, "ON  ratio %d/63", (int)lc.ratio);
            } else {
                kv(rows[i].name, "off");
            }
        } else {
            // Sprite: ratio is table-driven per pixel; show table[0] as representative.
            if (lc.ccEnable) {
                kv(rows[i].name, "ON  (table[0]=%d/63)", (int)lc.ratio);
            } else {
                kv(rows[i].name, "off");
            }
        }
    }

    ImGui::Separator();

    // ------------------------------------------------------------------
    // Shared register-body builder lambda (Feature B + C reuse it).
    // Appends the raw-register and decoded-sections markdown into `out`.
    // ------------------------------------------------------------------
    auto buildRegBody = [&](std::string& out) {
        char tmp[256];
        auto ap = [&](const char* fmt, ...) {
            va_list ap2;
            va_start(ap2, fmt);
            vsnprintf(tmp, sizeof(tmp), fmt, ap2);
            va_end(ap2);
            out += tmp;
        };

        ap("### Raw registers\n");
        ap("TVMD=0x%04X  BGON=0x%04X  CHCTLA=0x%04X  CHCTLB=0x%04X\n",
           R.TVMD, R.BGON, R.CHCTLA, R.CHCTLB);
        ap("PRINA=0x%04X  PRINB=0x%04X  PRIR=0x%04X\n",
           R.PRINA, R.PRINB, R.PRIR);
        ap("CCCTL=0x%04X  CCRNA=0x%04X  CCRNB=0x%04X  CCRR=0x%04X\n",
           R.CCCTL, R.CCRNA, R.CCRNB, R.CCRR);
        ap("CCRSA=0x%04X  CCRSB=0x%04X  CCRSC=0x%04X  CCRSD=0x%04X  CCRLB=0x%04X\n",
           R.CCRSA, R.CCRSB, R.CCRSC, R.CCRSD, R.CCRLB);
        ap("PLSZ=0x%04X  LNCLEN=0x%04X  SFPRMD=0x%04X  SFCCMD=0x%04X\n",
           R.PLSZ, R.LNCLEN, R.SFPRMD, R.SFCCMD);
        ap("CLOFEN=0x%04X  CLOFSL=0x%04X  BKTAU=0x%04X  BKTAL=0x%04X\n",
           R.CLOFEN, R.CLOFSL, R.BKTAU, R.BKTAL);
        // Window control + positions (issue #22 color calc window). WCTLA = NBG0/1
        // display window, WCTLB = NBG2/3, WCTLC = RBG0/sprite, WCTLD = parameter /
        // COLOR CALC window. The color calc window control is WCTLD>>8; the
        // compositor gates color calc on it. W0/W1 are the shared rectangles.
        ap("WCTLA=0x%04X  WCTLB=0x%04X  WCTLC=0x%04X  WCTLD=0x%04X\n",
           R.WCTLA, R.WCTLB, R.WCTLC, R.WCTLD);
        ap("W0 x=[%d..%d] y=[%d..%d]  W1 x=[%d..%d] y=[%d..%d]\n",
           R.WPSX0, R.WPEX0, R.WPSY0, R.WPEY0,
           R.WPSX1, R.WPEX1, R.WPSY1, R.WPEY1);
        {
            int ccwb = (R.WCTLD >> 8) & 0xFF;
            ap("ColorCalcWindow(WCTLD>>8=0x%02X): W0 en=%d area=%s  W1 en=%d "
               "area=%s  logic=%s\n",
               ccwb,
               (ccwb >> 1) & 1, ((ccwb & 1) ? "outside" : "inside"),
               (ccwb >> 3) & 1, (((ccwb >> 2) & 1) ? "outside" : "inside"),
               (((ccwb >> 7) & 1) ? "AND" : "OR"));
        }
        // Color offset A/B values (ch.13), sign-extended from 9-bit 2's
        // complement. Negative values darken the top image; this is the actual
        // darkening source for scenes whose top layer has CLOFEN set.
        auto sx9 = [](uint16_t v) -> int {
            int x = v & 0x1FF;
            if (x & 0x100) x |= ~0x1FF;  // sign-extend bit 8
            return x;
        };
        ap("ColorOffA RGB=(%d,%d,%d)  ColorOffB RGB=(%d,%d,%d)\n",
           sx9(R.COAR), sx9(R.COAG), sx9(R.COAB),
           sx9(R.COBR), sx9(R.COBG), sx9(R.COBB));
        ap("\n");

        ap("### Decoded color calc\n");
        ap("Mode=%s  RatioSource=%s  EXCCEN=%s  BOKEN=%s  LineRatio=%d/32\n",
           (cc.mode == vdp2cc::CalcMode::Add) ? "Additive" : "Ratio",
           (cc.ratioSource == vdp2cc::RatioSource::Second) ? "Second" : "Top",
           cc.extendedEnable ? "on" : "off",
           boken ? "set" : "clear",
           (int)cc.lineColorRatio);

        // vdp2cc LayerIndex -> snapshot layers[] index mapping.
        // snapshot layers[]: 0=NBG0, 1=NBG1, 2=NBG2, 3=NBG3, 4=RBG0, 5=RBG1.
        struct MapEntry { int ccIdx; int snapIdx; const char* name; };
        static const MapEntry layerMap[6] = {
            {vdp2cc::kNBG3,   3, "NBG3"},
            {vdp2cc::kNBG2,   2, "NBG2"},
            {vdp2cc::kNBG1,   1, "NBG1"},
            {vdp2cc::kNBG0,   0, "NBG0"},
            {vdp2cc::kRBG0,   4, "RBG0"},
            {vdp2cc::kSprite, -1, "Sprite"},
        };
        for (int i = 0; i < 6; ++i) {
            const auto& lc2 = cc.perLayer[layerMap[i].ccIdx];
            int snapIdx2 = layerMap[i].snapIdx;
            int prio2 = (snapIdx2 >= 0) ? _snapshot->layers[snapIdx2].priority : 0;
            if (i < 5) {
                ap("%s: cc=%s ratio=%d/63 prio=%d\n",
                   layerMap[i].name,
                   lc2.ccEnable ? "on" : "off",
                   (int)lc2.ratio, prio2);
            } else {
                ap("Sprite: cc=%s\n", lc2.ccEnable ? "on" : "off");
            }
        }
        ap("\n");

        ap("### Decoded layers (BGON/priority)\n");
        for (int i = 0; i < 6; ++i) {
            const auto& sl = _snapshot->layers[i];
            ap("%s: enabled=%s prio=%d colorMode=%s transparent=%s colorCalc=%s ratio=%d mosaic=%s\n",
               sl.name,
               sl.enabled ? "Y" : "n",
               sl.priority,
               colorModeStr(sl.colorMode),
               sl.transparent ? "yes" : "no",
               sl.colorCalcEnabled ? "on" : "off",
               sl.colorCalcRatio,
               sl.mosaicEnabled ? "yes" : "no");
        }
    };

    // issue #22 debug: G-buffer slice viewer. Forces the compositor to output a
    // single slice's raw color (green where the slice texel is transparent), so
    // a "drawn but empty" slice is told apart from a populated one. Slice index
    // is the vdp2cc LayerIndex (kNBG3=0..kSprite=5), NOT the enBG id.
    if (_vid != nullptr) {
        ImGui::Separator();
        ImGui::TextUnformatted("G-buffer slice view (compositor debug):");
        const int curSlice = _vid->getVdp2DebugViewSlice();
        struct SliceBtn { const char* label; int slice; };
        static const SliceBtn kSliceBtns[] = {
            {"Off", -1}, {"NBG3", 0}, {"NBG2", 1}, {"NBG1", 2},
            {"NBG0", 3}, {"RBG0", 4}, {"Sprite", 5},
        };
        for (int b = 0; b < (int)(sizeof(kSliceBtns) / sizeof(kSliceBtns[0])); ++b) {
            if (b != 0) ImGui::SameLine();
            const bool active = (curSlice == kSliceBtns[b].slice);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button,
                                              ImVec4(0.20f, 0.55f, 0.20f, 1.0f));
            if (ImGui::Button(kSliceBtns[b].label)) {
                _vid->setVdp2DebugViewSlice(kSliceBtns[b].slice);
            }
            if (active) ImGui::PopStyleColor();
        }
        ImGui::TextDisabled("green = transparent/empty texel in that slice");
        ImGui::Separator();
    }

    // Feature B: "Copy Registers" button.
    if (ImGui::Button("Copy Registers")) {
        std::string md;
        char tmp2[256];
        auto ap2 = [&](const char* fmt, ...) {
            va_list v;
            va_start(v, fmt);
            vsnprintf(tmp2, sizeof(tmp2), fmt, v);
            va_end(v);
            md += tmp2;
        };
        ap2("## VDP2 Register Dump (frame %llu, %dx%d, new-composite %s)\n\n",
            (unsigned long long)_snapshot->frameId,
            _snapshot->screenWidth, _snapshot->screenHeight,
            newComp ? "ON" : "OFF");
        buildRegBody(md);
        ImGui::SetClipboardText(md.c_str());
    }

    ImGui::SameLine();

    // Feature C: "Copy Bug Report" button.
    if (ImGui::Button("Copy Bug Report")) {
        // Build the isolate layer name string.
        const char* isolateName = "all";
        char isolateBuf[32];
        if (_isolateLayer == -1) {
            isolateName = "all";
        } else if (_isolateLayer == kIsolateVdp1Fb) {
            isolateName = "VDP1 FB";
        } else if (_isolateLayer >= 0 && _isolateLayer < 6) {
            isolateName = _snapshot->layers[_isolateLayer].name;
        } else {
            snprintf(isolateBuf, sizeof(isolateBuf), "?(%d)", _isolateLayer);
            isolateName = isolateBuf;
        }

        std::string md;
        char tmp3[256];
        auto ap3 = [&](const char* fmt, ...) {
            va_list v;
            va_start(v, fmt);
            vsnprintf(tmp3, sizeof(tmp3), fmt, v);
            va_end(v);
            md += tmp3;
        };

        ap3("## VDP2 Bug Report\n\n");
        ap3("Renderer path: new-composite %s\n",
            newComp ? "ON" : "OFF (legacy)");
        ap3("Screen: %dx%d\n",
            _snapshot->screenWidth, _snapshot->screenHeight);
        ap3("Debugger: step %d/%d, isolate %s\n\n",
            _stepN, _totalDrawCount, isolateName);

        ap3("## VDP2 Register Dump (frame %llu, %dx%d, new-composite %s)\n\n",
            (unsigned long long)_snapshot->frameId,
            _snapshot->screenWidth, _snapshot->screenHeight,
            newComp ? "ON" : "OFF");
        buildRegBody(md);

        // issue #22 debug: which layers the new composite path actually drew
        // into the G-buffer this frame (only meaningful when new-composite ON).
        if (newComp) {
            appendGBufferTraceMarkdown(md);
        }

        // Append pixel probe section if a pixel has been probed.
        if (_hasProbe) {
            appendPixelProbeMarkdown(md);
        }

        ImGui::SetClipboardText(md.c_str());
    }

    ImGui::End();
}

void Vdp2DebugUI::buildStepControlPanel() {
    // Force a visible initial location so the panel is reachable even
    // when the VDP1 docking host is not set up (VDP1 not active).
    ImGui::SetNextWindowSize(ImVec2(360, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 20),    ImGuiCond_FirstUseEver);
    if (ImGui::Begin("VDP2 Debugger -- Step Control")) {
        if (_snapshot == nullptr) {
            ImGui::TextDisabled("Awaiting first frame snapshot...");
        } else {
            ImGui::Text("Frame: %llu",
                        static_cast<unsigned long long>(_snapshot->frameId));
            ImGui::Text("Screen: %dx%d",
                        _snapshot->screenWidth, _snapshot->screenHeight);
            ImGui::Separator();
            const int total = _totalDrawCount;
            // 0-based step. Step 0 = back color only, Step total = full
            // composite. Display verbatim ("Step 0 / 12").
            ImGui::Text("Step: %d / %d", _stepN, total);
            ImGui::TextDisabled("(Step 0 = back color only; Step N = back color + first N draws)");

            const bool atStart = (_stepN <= 0);
            const bool atEnd   = (_stepN >= total);

            ImGui::BeginDisabled(atStart);
            if (ImGui::Button("<<")) _stepN = 0;
            ImGui::SameLine();
            if (ImGui::Button("<"))  _stepN = (_stepN > 0) ? (_stepN - 1) : 0;
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(atEnd || total <= 0);
            if (ImGui::Button(">"))  _stepN = (_stepN < total) ? (_stepN + 1) : total;
            ImGui::SameLine();
            if (ImGui::Button(">>")) _stepN = total;
            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::Text("Enabled layers (from BGON):");
            for (int i = 0; i < 6; ++i) {
                if (_snapshot->layers[i].enabled) {
                    ImGui::BulletText("%s (prio=%d)",
                                      _snapshot->layers[i].name,
                                      _snapshot->layers[i].priority);
                }
            }
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// appendPixelProbeMarkdown
//
// Builds a markdown block describing the current pixel probe result and
// appends it to `out`. Called from both "Copy Pixel Report" and "Copy Bug
// Report" so the text is not duplicated.
// ---------------------------------------------------------------------------
void Vdp2DebugUI::appendPixelProbeMarkdown(std::string& out) {
    if (!_hasProbe || _snapshot == nullptr) return;

    const Vdp2PixelReport& rpt = _probeReport;

    char tmp[256];
    auto ap = [&](const char* fmt, ...) {
        va_list v;
        va_start(v, fmt);
        vsnprintf(tmp, sizeof(tmp), fmt, v);
        va_end(v);
        out += tmp;
    };

    ap("\n### Pixel probe (%d, %d)\n", rpt.x, rpt.y);
    ap("Back color: #%06X\n\n", rpt.backColor & 0xFFFFFF);
    ap("| Mark | Layer  | En | Opaque | Prio | Color   | CC       |\n");
    ap("|------|--------|----|--------|------|---------|----------|\n");

    // Layer index order in Vdp2PixelReport: [0]=NBG0..[3]=NBG3,[4]=RBG0,[5]=Sprite
    for (int i = 0; i < 6; ++i) {
        const Vdp2LayerProbe& lp = rpt.layers[i];

        const char* mark = "";
        if (i == rpt.topIdx)    mark = "*TOP*";
        else if (i == rpt.secondIdx) mark = "2nd";

        const char* enStr = lp.enabled ? "Y" : "n";

        // Buffer must hold "? (" + note[48] + ")" + NUL. snprintf maps to
        // sprintf_s here (core.h), which ABORTS on overflow instead of
        // truncating, so this must be large enough for the longest note.
        char opaqueStr[64];
        if (!lp.supported) {
            snprintf(opaqueStr, sizeof(opaqueStr), "? (%s)", lp.note);
        } else {
            snprintf(opaqueStr, sizeof(opaqueStr), "%s", lp.opaque ? "Y" : "n");
        }

        char colorStr[16];
        if (lp.supported && lp.opaque) {
            snprintf(colorStr, sizeof(colorStr), "#%06X", lp.color & 0xFFFFFF);
        } else {
            snprintf(colorStr, sizeof(colorStr), "-");
        }

        char ccStr[24];
        if (lp.ccEnable) {
            snprintf(ccStr, sizeof(ccStr), "on r%d", lp.ccRatio);
        } else {
            snprintf(ccStr, sizeof(ccStr), "off");
        }

        ap("| %-5s| %-6s | %-2s | %-6s | %-4d | %-7s | %-8s |\n",
           mark, lp.name, enStr, opaqueStr, lp.priority, colorStr, ccStr);
    }
    ap("\n");

    // Result string.
    if (rpt.topIdx < 0) {
        ap("Result: back color only (#%06X)\n", rpt.backColor & 0xFFFFFF);
    } else {
        const Vdp2LayerProbe& top = rpt.layers[rpt.topIdx];
        if (top.ccEnable && rpt.secondIdx >= 0) {
            const Vdp2LayerProbe& sec = rpt.layers[rpt.secondIdx];
            // Determine blend description from snapshot registers.
            const Vdp2& R2 = _snapshot->regs;
            std::array<uint16_t, 4> ccrs2 = {R2.CCRSA, R2.CCRSB, R2.CCRSC, R2.CCRSD};
            vdp2cc::State cc2 = vdp2cc::decodeVdp2ColorCalc(
                R2.CCCTL, R2.CCRNA, R2.CCRNB, R2.CCRR, ccrs2, R2.CCRLB);
            const bool isAdd = (cc2.mode == vdp2cc::CalcMode::Add);
            const bool fromTop = (cc2.ratioSource == vdp2cc::RatioSource::Top);

            if (isAdd) {
                ap("Result: %s color-calc Additive with %s\n",
                   top.name, sec.name);
            } else {
                // Ratio blend: source of the ratio register is Top or Second.
                int ratio = fromTop ? top.ccRatio : sec.ccRatio;
                const char* src = fromTop ? "Top" : "Second";
                ap("Result: %s color-calc Ratio(%d/63 from %s) with %s\n",
                   top.name, ratio, src, sec.name);
            }
        } else {
            ap("Result: opaque %s (no color calc)\n", top.name);
        }
    }

    // Per-line color calc texel dump for the probed scan line. This reads the
    // ACTUAL perline[id] buffer (Vdp2GeneratePerLineColorCalcuration output) so
    // the real darkening source is visible instead of guessed: alpha = per-line
    // ratio (0xFF = no per-line cc this line), RGB = 128-centered signed color
    // offset (0x80 = neutral). A layer not listed has no per-line buffer.
    if (_vid != nullptr) {
        bool any = false;
        for (int id = 0; id < 5; ++id) {  // NBG0..RBG0
            uint32_t texel = _vid->getPerLineTexel(id, rpt.y);
            if (texel == 0) continue;  // no per-line buffer for this layer
            if (!any) {
                ap("\n### Per-line color-calc texel @ row %d "
                   "(alpha=ratio, RGB=offset, 0x80=neutral)\n", rpt.y);
                ap("| Layer | raw        | alpha | offR | offG | offB |\n");
                ap("|-------|------------|-------|------|------|------|\n");
                any = true;
            }
            const int a = (int)((texel >> 24) & 0xFF);
            const int r = (int)(texel & 0xFF);
            const int g = (int)((texel >> 8) & 0xFF);
            const int b = (int)((texel >> 16) & 0xFF);
            const char* nm[5] = {"NBG0", "NBG1", "NBG2", "NBG3", "RBG0"};
            ap("| %-5s | 0x%08X | 0x%02X  | %4d | %4d | %4d |\n",
               nm[id], texel, a, r, g, b);
        }
        if (any) {
            ap("Note: alpha 0xFF = line not per-line color-calc'd; offset "
               "0x80,0x80,0x80 = neutral (no per-line color offset).\n");
        }
    }

    // Per-line register values @ the probed scan line. The frame register dump
    // above is the FRAME-START (line 0) snapshot, but CCCTL / CCRNA / CCRNB /
    // LNCLEN are per-line registers (vdp2.cpp:916 copies Vdp2Regs into
    // Vdp2Lines[line] every scanline). The compositor's per-line color-calc path
    // must use Vdp2Lines[line >> line_shift], so dump those actual values here to
    // see the real EXCCEN / cc-enable / ratio in effect at this pixel. issue #22.
    if (_vid != nullptr) {
        const int line_shift = (_vid->vdp2height > 256) ? 1 : 0;
        const int srcLine = rpt.y >> line_shift;
        if (srcLine >= 0 && srcLine < 270) {
            const Vdp2& L = Vdp2Lines[srcLine];
            const bool exL  = (L.CCCTL & 0x400) != 0 && (L.CCCTL & 0x8000) == 0;
            const bool lcL  = exL && L.LNCLEN != 0 && (L.CCCTL & 0x20) != 0;
            ap("\n### Per-line registers @ row %d (Vdp2Lines[%d], real values)\n",
               rpt.y, srcLine);
            ap("CCCTL=0x%04X  CCRNA=0x%04X  CCRNB=0x%04X  CCRR=0x%04X  "
               "LNCLEN=0x%04X\n",
               L.CCCTL, L.CCRNA, L.CCRNB, L.CCRR, L.LNCLEN);
            ap("Decoded: EXCCEN=%s  N0CCEN=%s  N1CCEN=%s  LCCEN=%s  "
               "CCMD=%s  CCRTMD=%s  LNCL-inserted=%s\n",
               exL ? "on" : "off",
               (L.CCCTL & 0x01) ? "on" : "off",
               (L.CCCTL & 0x02) ? "on" : "off",
               (L.CCCTL & 0x20) ? "on" : "off",
               (L.CCCTL & 0x100) ? "add" : "ratio",
               (L.CCCTL & 0x200) ? "second" : "top",
               lcL ? "yes" : "no");
            ap("Note: compare with the frame-start dump above -- differences are "
               "the per-line modulation the compositor must honor.\n");
        }
    }
}

// ---------------------------------------------------------------------------
// appendGBufferTraceMarkdown
//
// Reports, for the new-composite path, which background layers were actually
// drawn into the G-buffer this frame and the skip reason for any that were not.
// A layer that the pixel probe shows as opaque/top but that appears here as a
// skip (or absent) is the direct cause of "the layer is missing on screen".
// ---------------------------------------------------------------------------
void Vdp2DebugUI::appendGBufferTraceMarkdown(std::string& out) {
    if (_vid == nullptr) return;

    char tmp[256];
    auto ap = [&](const char* fmt, ...) {
        va_list v;
        va_start(v, fmt);
        vsnprintf(tmp, sizeof(tmp), fmt, v);
        va_end(v);
        out += tmp;
    };

    // enBG id -> name (NBG0=0..RBG0=4, SPRITE=5).
    auto idName = [](int id) -> const char* {
        switch (id) {
            case 0: return "NBG0";
            case 1: return "NBG1";
            case 2: return "NBG2";
            case 3: return "NBG3";
            case 4: return "RBG0";
            case 5: return "Sprite";
            default: return "?";
        }
    };

    ap("\n### G-buffer population (new-composite layer draws)\n");
    const int n = _vid->gbufferTraceCount;
    if (n <= 0) {
        ap("(no trace -- new composite path did not run this frame)\n");
        return;
    }
    ap("| Layer | Prio | LineTex | Result                          |\n");
    ap("|-------|------|---------|---------------------------------|\n");
    for (int i = 0; i < n && i < 16; ++i) {
        const VIDVulkan::GBufferLayerTrace& t = _vid->gbufferTrace[i];

        // Compose a human-readable result / skip reason. snprintf maps to
        // sprintf_s here (core.h) and ABORTS on overflow, so keep the buffer
        // comfortably larger than the longest message.
        char res[64];
        if (t.drawn && t.usedPerLineCompanion) {
            snprintf(res, sizeof(res), "drawn (per-line offset)");
        } else if (t.drawn) {
            snprintf(res, sizeof(res), "drawn (normal)");
        } else if (t.skipNoVertex) {
            snprintf(res, sizeof(res), "SKIP: no vertex (vertexSize<=0)");
        } else if (t.skipMosaic) {
            snprintf(res, sizeof(res), "SKIP: mosaic (deferred)");
        } else if (t.skipPerLine) {
            snprintf(res, sizeof(res), "SKIP: per-line direct (deferred)");
        } else if (t.skipNoCompanion) {
            snprintf(res, sizeof(res), "SKIP: no companion pipeline");
        } else {
            snprintf(res, sizeof(res), "SKIP: unknown");
        }

        ap("| %-5s | %-4d | %-7s | %-31s |\n", idName(t.id), t.priority,
           t.hasLineTexture ? "yes" : "no", res);
    }
    ap("\nNote: layers not listed never reached a G-buffer slice (disabled, or "
       "id did not map). 'drawn' means the layer wrote its slice; a probe-opaque "
       "layer that is SKIP/absent here is why it is missing on screen.\n");
}

// ---------------------------------------------------------------------------
// buildPixelProbePanel
// ---------------------------------------------------------------------------
void Vdp2DebugUI::buildPixelProbePanel() {
    ImGui::SetNextWindowSize(ImVec2(380, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(740, 460),  ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("VDP2 Debugger -- Pixel Probe")) {
        ImGui::End();
        return;
    }

    if (_snapshot == nullptr) {
        ImGui::TextDisabled("Awaiting first frame snapshot...");
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Flip Y (verify)", &_probeYFlip);
    ImGui::TextDisabled("Click the game frame (outside panels) to probe a pixel.");
    ImGui::Separator();

    if (!_hasProbe) {
        ImGui::TextDisabled("No pixel probed yet.");
        ImGui::End();
        return;
    }

    const Vdp2PixelReport& rpt = _probeReport;

    // Coordinates and back color.
    ImGui::Text("Pixel: (%d, %d)", rpt.x, rpt.y);
    ImGui::Text("Back color: #%06X", rpt.backColor & 0xFFFFFF);

    ImGui::Separator();

    // Per-layer table. 6 columns: Mark, Name, En, Opaque, Prio, Color, CC.
    if (ImGui::BeginTable("probe_layers", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Mark",   ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn("Name",   ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn("En",     ImGuiTableColumnFlags_WidthFixed, 18);
        ImGui::TableSetupColumn("Opaque", ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn("Prio",   ImGuiTableColumnFlags_WidthFixed, 28);
        ImGui::TableSetupColumn("Color",  ImGuiTableColumnFlags_WidthFixed, 56);
        ImGui::TableSetupColumn("CC",     ImGuiTableColumnFlags_WidthFixed, 52);
        ImGui::TableHeadersRow();

        for (int i = 0; i < 6; ++i) {
            const Vdp2LayerProbe& lp = rpt.layers[i];
            ImGui::TableNextRow();

            // Mark column.
            ImGui::TableSetColumnIndex(0);
            if (i == rpt.topIdx)         ImGui::TextUnformatted("*TOP*");
            else if (i == rpt.secondIdx) ImGui::TextUnformatted("2nd");
            else                         ImGui::TextDisabled("-");

            // Name.
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(lp.name);

            // Enabled.
            ImGui::TableSetColumnIndex(2);
            if (lp.enabled) ImGui::TextUnformatted("Y");
            else            ImGui::TextDisabled("n");

            // Opaque / note.
            ImGui::TableSetColumnIndex(3);
            if (!lp.supported) {
                ImGui::TextDisabled("?");
                if (ImGui::IsItemHovered() && lp.note[0] != '\0') {
                    ImGui::SetTooltip("%s", lp.note);
                }
            } else {
                if (lp.opaque) ImGui::TextUnformatted("Y");
                else           ImGui::TextDisabled("n");
            }

            // Priority.
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", lp.priority);

            // Color.
            ImGui::TableSetColumnIndex(5);
            if (lp.supported && lp.opaque) {
                ImGui::Text("#%06X", lp.color & 0xFFFFFF);
            } else {
                ImGui::TextDisabled("-");
            }

            // Color calc.
            ImGui::TableSetColumnIndex(6);
            if (lp.ccEnable) {
                ImGui::Text("on r%d", lp.ccRatio);
            } else {
                ImGui::TextDisabled("off");
            }
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // Result summary.
    if (rpt.topIdx < 0) {
        ImGui::Text("Result: back color only (#%06X)", rpt.backColor & 0xFFFFFF);
    } else {
        const Vdp2LayerProbe& top = rpt.layers[rpt.topIdx];
        if (top.ccEnable && rpt.secondIdx >= 0) {
            const Vdp2LayerProbe& sec = rpt.layers[rpt.secondIdx];
            const Vdp2& R3 = _snapshot->regs;
            std::array<uint16_t, 4> ccrs3 = {R3.CCRSA, R3.CCRSB, R3.CCRSC, R3.CCRSD};
            vdp2cc::State cc3 = vdp2cc::decodeVdp2ColorCalc(
                R3.CCCTL, R3.CCRNA, R3.CCRNB, R3.CCRR, ccrs3, R3.CCRLB);
            const bool isAdd3  = (cc3.mode == vdp2cc::CalcMode::Add);
            const bool fromTop3 = (cc3.ratioSource == vdp2cc::RatioSource::Top);

            if (isAdd3) {
                ImGui::Text("Result: %s color-calc Additive with %s",
                            top.name, sec.name);
            } else {
                int ratio3 = fromTop3 ? top.ccRatio : sec.ccRatio;
                const char* src3 = fromTop3 ? "Top" : "Second";
                ImGui::Text("Result: %s color-calc Ratio(%d/63 from %s) with %s",
                            top.name, ratio3, src3, sec.name);
            }
        } else {
            ImGui::Text("Result: opaque %s (no color calc)", top.name);
        }
    }

    ImGui::Separator();

    // "Copy Pixel Report" button.
    if (ImGui::Button("Copy Pixel Report")) {
        std::string md;
        char hdr[128];
        snprintf(hdr, sizeof(hdr),
                 "## VDP2 Pixel Probe (frame %llu)\n",
                 (unsigned long long)_snapshot->frameId);
        md += hdr;
        appendPixelProbeMarkdown(md);
        ImGui::SetClipboardText(md.c_str());
    }

    ImGui::End();
}
