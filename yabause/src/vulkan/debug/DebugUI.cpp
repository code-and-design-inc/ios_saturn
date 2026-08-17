// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// DebugUI implementation, targeting ImGui v1.91.5-docking:
//   - ImGui_ImplVulkan_Init(ImGui_ImplVulkan_InitInfo*)  (RenderPass is
//     a member of InitInfo)
//   - ImGui_ImplVulkan_CreateFontsTexture()              (no-arg form;
//     the backend handles the upload internally)
//
// The pre-present hook registered on Window (Task 2) calls our
// recordImGuiDraw(), which uses the "keep" render pass (LOAD_OP_LOAD)
// so the emulator's framebuffer is preserved underneath the ImGui draw.
//
// NOTE: keep this file ASCII-only -- MSVC cp932 cannot parse UTF-8
// multi-byte sequences in source files (causes C4819 + parser errors).
#include "DebugUI.h"

#include "../Renderer.h"
#include "../Window.h"
#include "../VIDVulkan.h"
#include "../Vdp1ComputeRasterizer.h"
#include "../Vdp1Renderer.h"

#include "DebugSnapshot.h"
#include "Vdp1JsonlExporter.h"
#include "Vdp2DebugUI.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <vector>

// Platform-specific helpers for getExeDir() / openInExplorer().
// On Windows, define WIN32_LEAN_AND_MEAN and NOMINMAX BEFORE windows.h
// to avoid pulling in winsock plus the min/max macros (which collide
// with std::min/std::max used elsewhere in this translation unit's
// transitive headers). shlobj.h needs windows.h first.
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shlobj.h>
#else
#  include <unistd.h>
#endif

extern "C" {
#include "vdp1.h"
#include "vdp2.h"
}

namespace {
// Short label for a CMDCTRL value, used in the Command List table.
// Mirrors the Vdp1JsonlExporter's command type table but kept local so
// this UI doesn't pull in the exporter's longer string forms.
const char* cmdTypeShortName(uint16_t cmdctrl) {
    if (cmdctrl & 0x8000) return "DrawEnd";
    switch (cmdctrl & 0x000F) {
    case 0:  return "NormalSpr";
    case 1:  return "ScaledSpr";
    case 2: case 3: return "DistSpr";
    case 4:  return "Polygon";
    case 5: case 7: return "Polyline";
    case 6:  return "Line";
    case 8:  return "UserClip";
    case 9:  return "SysClip";
    case 10: return "LocalCoord";
    default: return "Bad";
    }
}

// Compact draw-status hint for the Command List Status column.
// Matches the dispatch-side filtering: only Polygon and Distorted Sprite
// currently produce compute draws; clip/local-coord commands apply state;
// everything else is either a draw-end terminator, skipped via the skip
// bit, or unsupported by the compute path.
const char* statusShort(uint16_t cmdctrl, uint32_t computeType) {
    if (cmdctrl & 0x8000) return "draw_end";
    if (cmdctrl & 0x4000) return "skipped_skip_bit";
    if (computeType == VDP1C_TYPE_POLYGON || computeType == VDP1C_TYPE_DISTORTED_SPRITE)
        return "drawn";
    int t = cmdctrl & 0x000F;
    if (t == 8 || t == 9 || t == 10) return "applied_state";
    return "skipped_unsupported";
}
}  // namespace

// Static helpers for the JSONL Export panel. Resolve the directory of the
// running executable so we can write outputs next to it, and shell out to
// the OS-native file explorer when the user clicks "Open Folder".
std::string DebugUI::getExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::filesystem::path p(buf);
    return p.parent_path().string();
#else
    char buf[1024];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = '\0';
    std::filesystem::path p(buf);
    return p.parent_path().string();
#endif
}

void DebugUI::openInExplorer(const std::string& path) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#else
    std::string cmd = "xdg-open \"" + path + "\" &";
    system(cmd.c_str());
#endif
}

DebugUI::DebugUI() = default;

bool DebugUI::isPaused() const {
  if (_paused) return true;
  if (_vdp2 && _vdp2->isPaused()) return true;
  return false;
}

DebugUI::~DebugUI() {
  shutdown();
}

bool DebugUI::init(Renderer* renderer, Window* window, VIDVulkan* vid) {
  if (renderer == nullptr || window == nullptr) {
    std::cerr << "[DebugUI] init: renderer/window is null" << std::endl;
    return false;
  }

  _renderer = renderer;
  _window   = window;
  _vid      = vid;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  _imguiContextCreated = true;

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  // Persist ImGui window positions/sizes/docking next to the exe so the
  // debug UI layout survives across runs. Owned by a static string with
  // app-lifetime so io.IniFilename's pointer stays valid.
  {
    static std::string s_imguiIniPath;
    s_imguiIniPath = (std::filesystem::path(getExeDir()) / "imgui.ini").string();
    io.IniFilename = s_imguiIniPath.c_str();

    // Seed a sensible default layout if the user has no saved file yet.
    // ImGui's auto-load on first NewFrame() will leave our memory load
    // intact when the on-disk file is absent. Captured at design time
    // with all 5 panels docked (Step Control + Command List/Detail on
    // the left, Offscreen Preview center, JSONL Export bottom-right).
    if (!std::filesystem::exists(s_imguiIniPath)) {
      static const char* kDefaultImguiIni = R"INI([Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][VDP1DebugDockHost]
Pos=0,0
Size=1809,1075
Collapsed=0

[Window][Step Control]
Pos=0,905
Size=385,170
Collapsed=0
DockId=0x00000004,0

[Window][Command List]
Pos=0,0
Size=385,903
Collapsed=0
DockId=0x00000003,0

[Window][Command Detail]
Pos=0,0
Size=385,903
Collapsed=0
DockId=0x00000003,1

[Window][Offscreen Preview]
Pos=387,0
Size=1422,983
Collapsed=0
DockId=0x00000005,0

[Window][JSONL Export]
Pos=387,985
Size=1422,90
Collapsed=0
DockId=0x00000006,0

[Docking][Data]
DockSpace     ID=0x1EB7EB1F Window=0x752684B0 Pos=0,0 Size=1809,1075 Split=X
  DockNode    ID=0x00000001 Parent=0x1EB7EB1F SizeRef=385,1075 Split=Y Selected=0x8E5915FE
    DockNode  ID=0x00000003 Parent=0x00000001 SizeRef=385,903 Selected=0x2D914E31
    DockNode  ID=0x00000004 Parent=0x00000001 SizeRef=385,170 Selected=0x8E5915FE
  DockNode    ID=0x00000002 Parent=0x1EB7EB1F SizeRef=1422,1075 Split=Y Selected=0x399D986A
    DockNode  ID=0x00000005 Parent=0x00000002 SizeRef=1422,983 CentralNode=1 Selected=0x399D986A
    DockNode  ID=0x00000006 Parent=0x00000002 SizeRef=1422,90 Selected=0x4A5D1046
)INI";
      ImGui::LoadIniSettingsFromMemory(kDefaultImguiIni, 0);  // 0 = strlen
    }
  }

  ImGui::StyleColorsDark();

  // ImGui descriptor pool. FREE_DESCRIPTOR_SET_BIT is required.
  {
    VkDescriptorPoolSize ps[] = {
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
    };
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pi.maxSets = 1000;
    pi.poolSizeCount = static_cast<uint32_t>(sizeof(ps) / sizeof(ps[0]));
    pi.pPoolSizes = ps;
    if (vkCreateDescriptorPool(_renderer->GetVulkanDevice(), &pi, nullptr,
                               &_imguiDescPool) != VK_SUCCESS) {
      std::cerr << "[DebugUI] failed to create ImGui descriptor pool" << std::endl;
      cleanupPartial();
      return false;
    }
  }

  // GLFW backend
  if (!ImGui_ImplGlfw_InitForVulkan(_window->getWindowHandle(), true)) {
    std::cerr << "[DebugUI] ImGui_ImplGlfw_InitForVulkan failed" << std::endl;
    cleanupPartial();
    return false;
  }
  _glfwBackendInited = true;

  // Vulkan backend
  ImGui_ImplVulkan_InitInfo init = {};
  init.Instance       = _renderer->GetVulkanInstance();
  init.PhysicalDevice = _renderer->GetVulkanPhysicalDevice();
  init.Device         = _renderer->GetVulkanDevice();
  init.QueueFamily    = _renderer->GetVulkanGraphicsQueueFamilyIndex();
  init.Queue          = _renderer->GetVulkanQueue();
  init.DescriptorPool = _imguiDescPool;
  init.RenderPass     = _window->GetVulkanKeepRenderPass();
  init.MinImageCount  = _window->getSwapchainImageCount();
  init.ImageCount     = _window->getSwapchainImageCount();
  init.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

  if (!ImGui_ImplVulkan_Init(&init)) {
    std::cerr << "[DebugUI] ImGui_ImplVulkan_Init failed" << std::endl;
    cleanupPartial();
    return false;
  }
  _vulkanBackendInited = true;

  // Font texture upload. ImGui 1.91.x exposes only the no-arg form;
  // the backend handles command buffer / queue submit internally.
  if (!ImGui_ImplVulkan_CreateFontsTexture()) {
    std::cerr << "[DebugUI] ImGui_ImplVulkan_CreateFontsTexture failed" << std::endl;
    cleanupPartial();
    return false;
  }

  // Register pre-present hook on Window.
  _window->setPrePresentHook(
    [this](VkCommandBuffer cb, uint32_t idx, VkFramebuffer fb) {
      this->recordImGuiDraw(cb, idx, fb);
    });

  // Register pre-beginFrame hook on the compute rasterizer so we can
  // snapshot cpuCmds + state right before they get cleared at the start
  // of the next frame. vdp1Compute may not exist yet at init time
  // (it is created lazily on the first rebuildFrameBuffer pass after
  // polygon mode is set), so we also retry from buildFrame().
  if (_vid != nullptr && _vid->getVdp1Compute() != nullptr) {
    _vid->getVdp1Compute()->setPreBeginFrameHook(
      [this]() { this->onPreBeginFrame(); });
    _hookedComputeInstance = _vid->getVdp1Compute();
  }

  // Vdp1Renderer pre-beginFrame hook (GPU_TESSERATION / PERSPECTIVE_CORRECTION
  // path). The renderer's drawStart() fires this at the same logical point
  // (start of next frame, before any per-frame state reset) so the pause
  // semantics match across modes.
  if (_vid != nullptr && _vid->getVdp1Renderer() != nullptr) {
    _vid->getVdp1Renderer()->setPreBeginFrameHook(
      [this]() { this->onPreBeginFrame(); });
    _hookedRendererInstance = _vid->getVdp1Renderer();
  }

  // Offscreen preview sampler (Task 14). Nearest filter with clamp; used
  // by ImGui::Image to sample the VDP1 compute offscreen image. Nearest
  // is required so the zoom-in feature in buildOffscreenPreviewPanel()
  // shows individual pixels crisply rather than smudged by interpolation.
  {
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_NEAREST;
    si.minFilter = VK_FILTER_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(_renderer->GetVulkanDevice(), &si, nullptr, &_previewSampler);
  }

  // B20: create the VDP2 debugger (paired with F10). init() is allowed to
  // fail; if it does we still bring up DebugUI without the VDP2 panel.
  _vdp2 = std::make_unique<Vdp2DebugUI>();
  if (!_vdp2->init(_renderer, _window, _vid)) {
    _vdp2.reset();
  }

  _available = true;
  _visible   = false;
  return true;
}

void DebugUI::cleanupPartial() {
  // Unwind in reverse of init() order. Each step is guarded by its
  // progress flag so partial states (e.g. context created but Vulkan
  // backend never initialized) are handled correctly.
  if (_renderer != nullptr && _renderer->GetVulkanDevice() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(_renderer->GetVulkanDevice());
    // FB readback cleanup (Task 11).
    if (_renderer != nullptr) {
      _fbReadback.release(_renderer->GetVulkanDevice());
    }
    _fbReadbackValid = false;
    // Offscreen preview cleanup (Task 14). The descriptor set was
    // allocated from the imgui internal pool by AddTexture; release it
    // before ImGui_ImplVulkan_Shutdown so the backend's own descriptor
    // accounting stays balanced. The sampler we own and destroy here.
    if (_previewTex != VK_NULL_HANDLE && _vulkanBackendInited) {
      ImGui_ImplVulkan_RemoveTexture(_previewTex);
      _previewTex = VK_NULL_HANDLE;
    }
    if (_previewSampler != VK_NULL_HANDLE) {
      vkDestroySampler(_renderer->GetVulkanDevice(), _previewSampler, nullptr);
      _previewSampler = VK_NULL_HANDLE;
    }
    _previewBoundView = VK_NULL_HANDLE;
    // Decoded texture preview cleanup (Task 9).
    if (_decodeImguiTex != VK_NULL_HANDLE && _vulkanBackendInited) {
      ImGui_ImplVulkan_RemoveTexture(_decodeImguiTex);
      _decodeImguiTex = VK_NULL_HANDLE;
    }
    if (_decodeImageView != VK_NULL_HANDLE) {
      vkDestroyImageView(_renderer->GetVulkanDevice(), _decodeImageView, nullptr);
      _decodeImageView = VK_NULL_HANDLE;
    }
    if (_decodeImage != VK_NULL_HANDLE) {
      vkDestroyImage(_renderer->GetVulkanDevice(), _decodeImage, nullptr);
      _decodeImage = VK_NULL_HANDLE;
    }
    if (_decodeImageMem != VK_NULL_HANDLE) {
      vkFreeMemory(_renderer->GetVulkanDevice(), _decodeImageMem, nullptr);
      _decodeImageMem = VK_NULL_HANDLE;
    }
    if (_vulkanBackendInited) {
      ImGui_ImplVulkan_Shutdown();
      _vulkanBackendInited = false;
    }
    if (_glfwBackendInited) {
      ImGui_ImplGlfw_Shutdown();
      _glfwBackendInited = false;
    }
    if (_imguiDescPool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(_renderer->GetVulkanDevice(), _imguiDescPool, nullptr);
      _imguiDescPool = VK_NULL_HANDLE;
    }
  }
  if (_imguiContextCreated) {
    ImGui::DestroyContext();
    _imguiContextCreated = false;
  }
}

void DebugUI::shutdown() {
  // Even if init() never reached the _available=true point, partial
  // state (context, descriptor pool, GLFW/Vulkan backends) may have
  // been created and must be torn down. cleanupPartial() handles
  // both cases via the progress flags.
  if (_vid != nullptr && _vid->getVdp1Compute() != nullptr) {
    _vid->getVdp1Compute()->setPreBeginFrameHook(nullptr);
  }
  if (_vid != nullptr && _vid->getVdp1Renderer() != nullptr) {
    _vid->getVdp1Renderer()->setPreBeginFrameHook(nullptr);
  }
  _hookedComputeInstance  = nullptr;
  _hookedRendererInstance = nullptr;
  delete _snapshot;
  _snapshot = nullptr;

  // B20: tear down VDP2 debugger before cleanupPartial() destroys the
  // ImGui context. Vdp2DebugUI also unhooks the VIDVulkan pre-hook.
  if (_vdp2) {
    _vdp2->shutdown();
    _vdp2.reset();
  }

  if (_window != nullptr) {
    _window->setPrePresentHook(nullptr);
  }
  cleanupPartial();
  _available = false;

  // Null out borrowed pointers so a second shutdown() call (e.g. from
  // ~DebugUI() at static-destructor time, after main() already invoked
  // shutdown() and then YabauseDeInit() freed VIDVulkan/Window/Renderer)
  // becomes a safe no-op instead of dereferencing dangling memory.
  _vid      = nullptr;
  _window   = nullptr;
  _renderer = nullptr;
}

void DebugUI::toggleVisible() {
  if (!_available) return;
  _visible = !_visible;
}

// Hotkey map (B20 revision):
//   F9          : toggle VDP1 debugger (200ms debounce)
//   F10         : toggle VDP2 debugger (200ms debounce)
//   PageDown    : step +1 (active debugger only; was F10 pre-B20)
//   PageUp      : step -1 (active debugger only; was F11 pre-B20)
//   Ctrl+E      : VDP1 JSONL export (Export button mirror)
//   Ctrl+L      : VDP1 JSONL open-folder
//   Esc         : resume pause (either debugger)
//
// F9 and F10 are mutually exclusive: pressing F10 while VDP1 is active
// resumes VDP1 first, and vice versa.
void DebugUI::onKeyDown(int glfwKey, bool ctrl) {
  if (!_available) return;

  double now = glfwGetTime();
  switch (glfwKey) {
    case GLFW_KEY_F9: {
      if (now - _lastF9TimeSec < 0.2) return;
      _lastF9TimeSec = now;
      // Close VDP2 first if it is active.
      if (_vdp2 && (_vdp2->isPaused() || _vdp2->isVisible())) {
        if (_vdp2->isPaused()) _vdp2->requestTogglePause();
        if (_vdp2->isVisible()) _vdp2->toggleVisible();
      }
      toggleVisible();
      requestTogglePause();
      break;
    }
    case GLFW_KEY_F10: {
      if (!_vdp2) break;
      if (now - _lastF10TimeSec < 0.2) return;
      _lastF10TimeSec = now;
      // Close VDP1 first if it is active.
      if (_paused || _visible) {
        if (_paused) requestTogglePause();
        if (_visible) toggleVisible();
      }
      _vdp2->toggleVisible();
      _vdp2->requestTogglePause();
      break;
    }
    case GLFW_KEY_F11: {
      // issue #22: toggle the new per-pixel VDP2 compositor at runtime so the
      // old and new composite paths can be compared back-to-back by eye.
      if (now - _lastF11TimeSec < 0.2) return;
      _lastF11TimeSec = now;
      if (_vid) {
        bool on = !_vid->getVdp2NewComposite();
        _vid->setVdp2NewComposite(on);
        printf("VDP2 new composite: %s\n", on ? "ON" : "OFF");
      }
      break;
    }
    case GLFW_KEY_PAGE_DOWN:
      // VDP1 active: step+1 in raw cmd list.
      if (_paused && _snapshot) {
        int total = (int)_snapshot->rawCmds.size();
        if (_stepN < total - 1) ++_stepN;
      }
      // VDP2 active: step+1 in pipeline draw count.
      if (_vdp2 && _vdp2->isPaused()) {
        _vdp2->stepForward();
      }
      break;
    case GLFW_KEY_PAGE_UP:
      if (_paused && _stepN > 0) --_stepN;
      if (_vdp2 && _vdp2->isPaused()) {
        _vdp2->stepBackward();
      }
      break;
    case GLFW_KEY_E:
      if (ctrl && _paused && _snapshot) {
        std::filesystem::path exportDir =
            std::filesystem::path(getExeDir()) / "debug" / "vdp1";
        std::filesystem::create_directories(exportDir);
        std::filesystem::path latestPath = exportDir / "latest.jsonl";
        std::string err;
        bool ok = Vdp1JsonlExporter::exportSnapshot(
            *_snapshot, latestPath.string(), &err);
        _lastExportPath = ok ? latestPath.string() : "";
        _lastExportError = ok ? "" : err;
      }
      break;
    case GLFW_KEY_L:
      if (ctrl) {
        std::filesystem::path exportDir =
            std::filesystem::path(getExeDir()) / "debug" / "vdp1";
        std::filesystem::create_directories(exportDir);
        openInExplorer(exportDir.string());
      }
      break;
    case GLFW_KEY_ESCAPE:
      if (_paused) requestTogglePause();
      if (_vdp2 && _vdp2->isPaused()) {
        _vdp2->requestTogglePause();
        if (_vdp2->isVisible()) _vdp2->toggleVisible();
      }
      break;
    default:
      break;
  }
}

void DebugUI::buildFrame() {
  if (!_available) return;

  // (Re-)install the pre-beginFrame hook whenever the compute rasterizer
  // instance changes. VIDVulkan recreates Vdp1ComputeRasterizer on resolution
  // / polygon mode changes, so we cannot install once-and-forget.
  Vdp1ComputeRasterizer* curCompute =
      (_vid != nullptr) ? _vid->getVdp1Compute() : nullptr;
  if (curCompute != nullptr && curCompute != _hookedComputeInstance) {
    curCompute->setPreBeginFrameHook([this]() { this->onPreBeginFrame(); });
    _hookedComputeInstance = curCompute;
    std::cout << "[DebugUI] pre-beginFrame hook installed (compute=" << (void*)curCompute
              << ")" << std::endl;
  } else if (curCompute == nullptr && _hookedComputeInstance != nullptr) {
    // compute was disabled; clear our cached pointer so we reinstall on revival
    _hookedComputeInstance = nullptr;
  }

  // Same logic for the graphics-pipeline renderer. Vdp1Renderer is normally
  // long-lived but VIDVulkan::Resize() can recreate it; re-installing on
  // pointer change keeps the hook live across resolution changes too.
  Vdp1Renderer* curRenderer =
      (_vid != nullptr) ? _vid->getVdp1Renderer() : nullptr;
  if (curRenderer != nullptr && curRenderer != _hookedRendererInstance) {
    curRenderer->setPreBeginFrameHook([this]() { this->onPreBeginFrame(); });
    _hookedRendererInstance = curRenderer;
    std::cout << "[DebugUI] pre-beginFrame hook installed (renderer=" << (void*)curRenderer
              << ")" << std::endl;
  } else if (curRenderer == nullptr && _hookedRendererInstance != nullptr) {
    _hookedRendererInstance = nullptr;
  }

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // Docking space hosted on the main viewport. Only present while the
  // overlay is visible or the emulator is paused; otherwise we leave
  // the live framebuffer untouched.
  ImGuiID dockspace_id = ImGui::GetID("VDP1DebugDockspace");
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (_visible || _paused) {
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("VDP1DebugDockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
  }

  if (_visible || _paused) {
    buildStepControlPanel();
    buildCommandListPanel();
    buildCommandDetailPanel();
    buildJsonlExportPanel();
    buildOffscreenPreviewPanel();
    buildMemoryViewerPanel();
  }

  // B20: VDP2 debugger has its own visible / paused state and panels.
  if (_vdp2) {
    _vdp2->buildFrame();
  }

  ImGui::Render();
}

void DebugUI::buildStepControlPanel() {
  ImGui::Begin("Step Control");
  if (_paused && _snapshot) {
    const int total = (int)_snapshot->rawCmds.size();
    ImGui::Text("Frame: %llu  CmdCount: %d  Mode: %s",
                (unsigned long long)_snapshot->frameId, total,
                _snapshotIsCompute ? "COMPUTE" : "GRAPHICS");
    if (!_snapshotIsCompute) {
      ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f),
                         "Step replay via Vdp1Renderer (skip-walk path).");
    }

    if (total == 0) {
      ImGui::Text("No commands in this frame.");
    } else {
      int sliderMax = total - 1;
      ImGui::SliderInt("N", &_stepN, 0, sliderMax);
      if (_stepN < 0) _stepN = 0;
      if (_stepN > sliderMax) _stepN = sliderMax;

      if (ImGui::Button("|<"))     _stepN = 0;
      ImGui::SameLine();
      if (ImGui::Button("<")) {
        if (_stepN > 0) --_stepN;
      }
      ImGui::SameLine();
      if (ImGui::Button(">")) {
        if (_stepN < sliderMax) ++_stepN;
      }
      ImGui::SameLine();
      if (ImGui::Button(">|"))     _stepN = sliderMax;
      ImGui::SameLine();
      ImGui::Text("(%d / %d)", _stepN, sliderMax);

      // ImGui v1.91 InputScalar (used by InputInt) asserts on EnterReturnsTrue.
      // InputInt commits on Enter / step buttons / focus loss anyway, so we
      // detect commit by comparing the post-call value to _stepN.
      int gotoVal = _stepN;
      ImGui::InputInt("Goto", &gotoVal, 1, 10);
      if (gotoVal != _stepN) {
        _stepN = (gotoVal < 0) ? 0 : (gotoVal > sliderMax ? sliderMax : gotoVal);
      }
    }
  } else {
    ImGui::Text("Not paused. Press F9 to pause.");
  }

  ImGui::Separator();
  if (_paused) {
    if (ImGui::Button("Resume (F9)")) {
      requestTogglePause();
    }
  } else {
    if (ImGui::Button("Pause (F9)")) {
      requestTogglePause();
    }
  }

  // Forward mapping toggle. Saturn-faithful rasterizer (per-texel push for
  // sprite/polygon, per-pixel pull for polyline/line) -- Phase 1C makes it
  // the default. The toggle is kept as an escape hatch: turning it OFF
  // routes everything through the tile-binning + tile_shade fallback path,
  // useful for regression checking during the migration.
  ImGui::Separator();
  Vdp1ComputeRasterizer* compute =
      (_vid != nullptr) ? _vid->getVdp1Compute() : nullptr;
  if (compute != nullptr) {
    bool fwd = compute->getUseForwardMapping();
    if (ImGui::Checkbox("Forward Mapping (default)", &fwd)) {
      compute->setUseForwardMapping(fwd);
    }
    ImGui::TextUnformatted("Saturn-faithful: texture -> screen push.");
    ImGui::TextUnformatted("Off = tile_shade fallback (legacy path).");

    // Tile-binning forward toggle (perf opt 2026-05-08). Requires
    // useForwardMapping; consolidates per-batch dispatch into a single
    // (numTilesX, numTilesY, 1) dispatch using the existing Bin pass.
    bool tileFwd = compute->getUseTileForward();
    ImGui::BeginDisabled(!fwd);
    if (ImGui::Checkbox("Tile-binning Forward", &tileFwd)) {
      compute->setUseTileForward(tileFwd);
    }
    ImGui::EndDisabled();
    ImGui::TextUnformatted("On = 2 dispatches/frame total (bin + shade).");
    ImGui::TextUnformatted("Off = per-batch forward (bbox-union dispatch).");
  }

  // Frame debugger: vkCmdDispatch / vkCmdPipelineBarrier counters from the
  // most recent dispatchUpTo() call. Forward path adds a "Batches" line with
  // batch count + cmds-per-batch min/avg/max so the bbox-union batching
  // optimization is observable. Tile-shade path is fixed at 2 dispatches
  // (bin + shade) regardless of cmd count.
  if (compute != nullptr) {
    const auto& s = compute->getLastDispatchStats();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.85f, 0.95f, 0.7f, 1.0f), "Frame Submit Stats");
    const char* pathLabel = "(none)";
    if (s.usedTileForwardPath) {
      pathLabel = "tile-binning forward";
    } else if (s.usedForwardPath && s.usedTilePath) {
      pathLabel = "forward + tile_shade";
    } else if (s.usedForwardPath) {
      pathLabel = "forward (per-batch)";
    } else if (s.usedTilePath) {
      pathLabel = "tile_shade";
    }
    ImGui::Text("Path: %s", pathLabel);
    ImGui::Text("vkCmdDispatch:        %u", s.dispatchCount);
    ImGui::Text("vkCmdPipelineBarrier: %u", s.pipelineBarrierCount);
    if (s.usedForwardPath && !s.usedTileForwardPath) {
      // Per-batch forward path: show batch breakdown.
      const float avg = (s.batchCount > 0u)
          ? static_cast<float>(s.totalCmdsDispatched) / static_cast<float>(s.batchCount)
          : 0.0f;
      ImGui::Text("Batches: %u  cmds=%u  min=%u  max=%u  avg=%.2f",
                  s.batchCount, s.totalCmdsDispatched,
                  s.minBatchSize, s.maxBatchSize, avg);
      if (s.batchCount > 0u) {
        const float ratio = static_cast<float>(s.totalCmdsDispatched)
                            / static_cast<float>(s.batchCount);
        ImGui::Text("  (= %.1fx fewer dispatches than per-cmd)", ratio);
      }
    } else if (s.usedTileForwardPath) {
      // Tile-binning forward: 2 dispatches total regardless of cmd count.
      ImGui::Text("Cmds binned: %u  (2 dispatches: bin + shade)",
                  s.totalCmdsDispatched);
    }
    // pmod-aware imageLoad skip ratio. High "skip" => more bandwidth saved
    // by the cc==0/2 fast path. Replace-heavy scenes (most Saturn games)
    // typically show 90%+ skip; Shadow / Half-trans use cc==1/3 which still
    // require imageLoad for the dst-dependent blend math.
    const uint32_t pmodTotal = s.cmdsSkipImageLoad + s.cmdsNeedImageLoad;
    if (pmodTotal > 0u) {
      const float skipPct = 100.0f * static_cast<float>(s.cmdsSkipImageLoad)
                          / static_cast<float>(pmodTotal);
      ImGui::Text("Pmod: skip imageLoad %u (%.0f%%) / need %u",
                  s.cmdsSkipImageLoad, skipPct, s.cmdsNeedImageLoad);
    }
  }

  // Invalidate Detail panel JSON cache when _stepN changes while the
  // detail is following step N (i.e. no explicit selection). DebugUI is
  // a singleton in this app, so a function-local static is safe.
  static int prevStepN = -1;
  if (prevStepN != _stepN) {
    if (_selectedCmdIndex < 0) _detailJsonCacheIndex = -2;
    prevStepN = _stepN;
  }
  ImGui::End();
}

void DebugUI::buildCommandListPanel() {
    ImGui::Begin("Command List");
    if (!_paused || !_snapshot) {
        ImGui::Text("Not paused.");
        ImGui::End();
        return;
    }

    // outer_size = (0, -FLT_MIN) makes the table consume the rest of the
    // window; without this ScrollY collapses to auto-fit and the parent
    // window clips long command lists (>~50 rows for typical NiGHTS frames).
    // ScrollFreezeTopRow keeps the column header pinned while scrolling.
    if (ImGui::BeginTable("cmds", 4,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable,
        ImVec2(0.0f, -FLT_MIN))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Idx",  ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Coords summary");
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 130);
        ImGui::TableHeadersRow();

        // ImGuiListClipper culls off-screen rows; for 300+ NiGHTS commands
        // each frame this keeps per-frame Selectable layout cost flat.
        ImGuiListClipper clipper;
        clipper.Begin((int)_snapshot->rawCmds.size());
        while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            char label[32];
            snprintf(label, sizeof(label), "%d##cmd%d", i, i);
            bool selected = (i == _selectedCmdIndex);
            if (ImGui::Selectable(label, selected,
                                  ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                _selectedCmdIndex = i;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    _stepN = i;
                }
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(cmdTypeShortName(_snapshot->rawCmds[i].CMDCTRL));

            ImGui::TableNextColumn();
            const auto& rc = _snapshot->rawCmds[i];
            // Only CMDXA/CMDYA is meaningful for every draw command. The second
            // corner depends on the command type: 4-vertex commands (distorted/
            // polygon/polyline) use C, line uses B, scaled uses C (zoom point 0)
            // or B. Normal sprite and state commands have no second corner --
            // CMDXB..CMDYD there hold stale VRAM garbage and must not be shown.
            switch (rc.CMDCTRL & 0x800Fu) {
            case 0x0002: case 0x0003: case 0x0004: case 0x0005: case 0x0007:
                ImGui::Text("(%d,%d)..(%d,%d)", rc.CMDXA, rc.CMDYA, rc.CMDXC, rc.CMDYC);
                break;
            case 0x0006:
                ImGui::Text("(%d,%d)..(%d,%d)", rc.CMDXA, rc.CMDYA, rc.CMDXB, rc.CMDYB);
                break;
            case 0x0001:
                if (((rc.CMDCTRL & 0x0F00u) >> 8) == 0)
                    ImGui::Text("(%d,%d)..(%d,%d)", rc.CMDXA, rc.CMDYA, rc.CMDXC, rc.CMDYC);
                else
                    ImGui::Text("(%d,%d) +%dx%d", rc.CMDXA, rc.CMDYA, (int16_t)rc.CMDXB, (int16_t)rc.CMDYB);
                break;
            default:
                ImGui::Text("(%d,%d)", rc.CMDXA, rc.CMDYA);
                break;
            }

            ImGui::TableNextColumn();
            // cmds is NOT parallel to rawCmds: it only holds the compute-drawn
            // entries. Translate the raw index through rawToCmdIndex (built in
            // DebugSnapshot::take) before dereferencing cmds -- indexing
            // cmds[i] by the raw index makes every command past the first
            // skipped/state entry read the wrong slot, and raw indices beyond
            // cmds.size() fall to cmdType 0 (NOOP) and render as a bogus
            // "skipped_unsupported". Mirrors Vdp1JsonlExporter::singleCommandLine.
            const int cmdIdx = (i < (int)_snapshot->rawToCmdIndex.size())
                                   ? _snapshot->rawToCmdIndex[i]
                                   : -1;
            uint32_t ct = (cmdIdx >= 0 && cmdIdx < (int)_snapshot->cmds.size())
                              ? _snapshot->cmds[cmdIdx].cmdType
                              : 0u;
            ImGui::TextUnformatted(statusShort(rc.CMDCTRL, ct));
        }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void DebugUI::buildCommandDetailPanel() {
    ImGui::Begin("Command Detail");
    if (!_paused || !_snapshot) {
        ImGui::Text("Not paused.");
        ImGui::End();
        return;
    }
    int target = (_selectedCmdIndex >= 0) ? _selectedCmdIndex : _stepN;

    // Regenerate the formatted JSON whenever the target index changes.
    // The exporter emits a single-line JSON; we insert newlines and
    // indentation here for readability inside the InputTextMultiline.
    if (target != _detailJsonCacheIndex) {
        _detailJsonCacheIndex = target;
        std::string raw = Vdp1JsonlExporter::singleCommandLine(*_snapshot, target);
        std::string out;
        out.reserve(raw.size() + 64);
        int depth = 0;
        for (size_t i = 0; i < raw.size(); ++i) {
            char ch = raw[i];
            if (ch == '{' || ch == '[') {
                out += ch;
                if (i + 1 < raw.size() && raw[i + 1] != '}' && raw[i + 1] != ']') {
                    ++depth;
                    out += '\n';
                    out.append(depth * 2, ' ');
                }
            } else if (ch == '}' || ch == ']') {
                if (i > 0 && raw[i - 1] != '{' && raw[i - 1] != '[') {
                    --depth;
                    out += '\n';
                    out.append(depth * 2, ' ');
                }
                out += ch;
            } else if (ch == ',' && i + 1 < raw.size() && raw[i + 1] == ' ') {
                out += ',';
                out += '\n';
                out.append(depth * 2, ' ');
                ++i;  // skip the space
            } else {
                out += ch;
            }
        }
        _detailJsonCache = out;
    }

    ImGui::Text("Index: %d / %d", target, (int)_snapshot->rawCmds.size() - 1);
    if (ImGui::Button("Copy to clipboard")) {
        ImGui::SetClipboardText(
            Vdp1JsonlExporter::singleCommandLine(*_snapshot, target).c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Sync to step N")) {
        _selectedCmdIndex = -1;       // detail follows step N
        _detailJsonCacheIndex = -2;   // force regenerate
    }
    ImGui::Separator();

    // Visual preview of the 4-vertex shape + bbox + simulated rasterizer
    // fill. Skipped silently for non-4-vertex command types.
    buildCommandShapePreview(target);

    // Read-only multiline view. ImGui's InputTextMultiline takes a
    // writable buffer pointer even for ReadOnly; this const_cast is the
    // documented idiom -- the read-only path does not write through it.
    char* buf = (char*)_detailJsonCache.c_str();
    ImGui::InputTextMultiline("##detail", buf, _detailJsonCache.size() + 1,
        ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
    ImGui::End();
}

void DebugUI::buildCommandShapePreview(int target) {
    if (!_snapshot) return;
    if (target < 0 || target >= (int)_snapshot->rawCmds.size()) return;
    const auto& rc = _snapshot->rawCmds[target];
    const uint16_t cmdType = rc.CMDCTRL & 0x000Fu;
    // 0x02=Distorted Sprite, 0x04=Polygon, 0x05=Polyline -> all use 4
    // vertices in Saturn standard order (A,B,C,D = TL,TR,BR,BL).
    const bool fourVertex = (cmdType == 0x02 || cmdType == 0x04 || cmdType == 0x05);
    if (!fourVertex) return;

    if (!ImGui::CollapsingHeader("Shape Preview",
                                 ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const int xs[4] = { (int16_t)rc.CMDXA, (int16_t)rc.CMDXB,
                        (int16_t)rc.CMDXC, (int16_t)rc.CMDXD };
    const int ys[4] = { (int16_t)rc.CMDYA, (int16_t)rc.CMDYB,
                        (int16_t)rc.CMDYC, (int16_t)rc.CMDYD };
    int xmin = xs[0], xmax = xs[0], ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < 4; ++i) {
        xmin = (std::min)(xmin, xs[i]); xmax = (std::max)(xmax, xs[i]);
        ymin = (std::min)(ymin, ys[i]); ymax = (std::max)(ymax, ys[i]);
    }
    // 1-pixel pad around the bbox so the bbox border sits on a clean
    // grid line and individual vertices on the edge stay visible.
    const int padXMin = xmin - 1;
    const int padYMin = ymin - 1;
    const int padXMax = xmax + 1;
    const int padYMax = ymax + 1;
    const int gridW = padXMax - padXMin + 1;
    const int gridH = padYMax - padYMin + 1;

    // Safety cap: a polygon spanning thousands of pixels would blow up
    // the per-cell allocation and the canvas size. Show a textual hint
    // instead for those.
    if (gridW * gridH > 200000) {
        ImGui::TextDisabled("Shape too large for preview (%dx%d Saturn px).",
                            gridW, gridH);
        return;
    }

    // Auto-fit cell size into ~320 ImGui px, capped so a tiny polygon
    // doesn't blow up to gigantic cells with no detail benefit.
    const float kCanvasMax = 320.0f;
    const float kCellMax   = 28.0f;
    const float fitCell = (std::min)(kCanvasMax / float(gridW),
                                     kCanvasMax / float(gridH));
    const float cellSize = (std::max)(1.0f, (std::min)(fitCell, kCellMax));

    ImGui::Text("Vertices: A=(%d,%d) B=(%d,%d) C=(%d,%d) D=(%d,%d)",
                xs[0], ys[0], xs[1], ys[1], xs[2], ys[2], xs[3], ys[3]);
    ImGui::Text("BBox: (%d,%d)..(%d,%d)  size=%dx%d  cell=%.1fpx",
                xmin, ymin, xmax, ymax,
                xmax - xmin + 1, ymax - ymin + 1, cellSize);
    // FLAG_THIN indicator (only meaningful for compute snapshots). Color
    // matches the bbox border: red = normal (strict-inside), green = THIN
    // (host applyState routed this polygon through supercover coverage).
    if (_snapshotIsCompute && (size_t)target < _snapshot->rawToCmdIndex.size()) {
        const int cmdIdx = _snapshot->rawToCmdIndex[target];
        if (cmdIdx >= 0 && (size_t)cmdIdx < _snapshot->cmds.size()) {
            const bool isThin = (_snapshot->cmds[cmdIdx].flags & VDP1C_FLAG_THIN) != 0;
            ImGui::TextColored(
                isThin ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                       : ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                "FLAG_THIN: %s", isThin ? "YES (supercover)" : "no (strict-inside)");
        }
    }

    // ----- Bresenham helpers (mirror vdp1_compute_scanline.comp) ------------
    auto bresStepCount = [](int x1, int y1, int x2, int y2) {
        return (std::max)(std::abs(x2 - x1), std::abs(y2 - y1)) + 1;
    };
    auto bresStepAt = [](int x1, int y1, int x2, int y2, int k,
                         int& outX, int& outY) {
        int dx = x2 - x1, dy = y2 - y1;
        int ax = (dx >= 0) ? 1 : -1;
        int ay = (dy >= 0) ? 1 : -1;
        int adx = std::abs(dx), ady = std::abs(dy);
        if (adx >= ady) {
            outX = x1 + k * ax;
            int yAdv = (adx == 0) ? 0 : (k * ady) / adx;
            outY = y1 + yAdv * ay;
        } else {
            outY = y1 + k * ay;
            int xAdv = (ady == 0) ? 0 : (k * adx) / ady;
            outX = x1 + xAdv * ax;
        }
    };

    std::vector<uint8_t> filled(gridW * gridH, 0);
    auto markFilled = [&](int x, int y) {
        if (x < padXMin || x > padXMax || y < padYMin || y > padYMax) return;
        filled[(y - padYMin) * gridW + (x - padXMin)] = 1;
    };
    // Greedy Bresenham scanline fill with supercover thickening (mirrors
    // drawScanline in vdp1_compute_scanline.comp). When the line is sloped
    // (minor axis advances), each major step also marks the adjacent cell
    // on the minor-axis direction so near-axis lines fill both rows/columns
    // they geometrically traverse.
    auto drawScanlineSim = [&](int lx, int ly, int rx, int ry) {
        int dx = rx - lx, dy = ry - ly;
        int ax = (dx >= 0) ? 1 : -1;
        int ay = (dy >= 0) ? 1 : -1;
        int adx = std::abs(dx), ady = std::abs(dy);
        int x = lx, y = ly, a = 0;
        if (adx >= ady) {
            for (int n = 0; n < adx; ++n) {
                markFilled(x, y);
                if (ady > 0) markFilled(x, y + ay);  // supercover
                a += ady;
                if (a >= adx) {
                    a -= adx;
                    y += ay;
                    if (ax == ay) markFilled(x + ax, y - ay);
                    else          markFilled(x, y);
                }
                x += ax;
            }
        } else {
            for (int n = 0; n < ady; ++n) {
                markFilled(x, y);
                if (adx > 0) markFilled(x + ax, y);  // supercover
                a += adx;
                if (a >= ady) {
                    a -= ady;
                    x += ax;
                    if (ay == ax) markFilled(x, y);
                    else          markFilled(x - ax, y + ay);
                }
                y += ay;
            }
        }
        markFilled(rx, ry);
    };

    int totalLeft  = bresStepCount(xs[0], ys[0], xs[3], ys[3]);  // v0 -> v3
    int totalRight = bresStepCount(xs[1], ys[1], xs[2], ys[2]);  // v1 -> v2
    int total = (std::max)(totalLeft, totalRight);
    // Polyline doesn't fill, so skip the simulation for cmdType == 0x05.
    const bool simulateFill = (cmdType == 0x02 || cmdType == 0x04);
    if (simulateFill && total > 0) {
        for (int scanY = 0; scanY < total; ++scanY) {
            int leftIdx  = (scanY * totalLeft)  / total;
            int rightIdx = (scanY * totalRight) / total;
            leftIdx  = std::clamp(leftIdx,  0, totalLeft  - 1);
            rightIdx = std::clamp(rightIdx, 0, totalRight - 1);
            int lx, ly, rx, ry;
            bresStepAt(xs[0], ys[0], xs[3], ys[3], leftIdx,  lx, ly);
            bresStepAt(xs[1], ys[1], xs[2], ys[2], rightIdx, rx, ry);
            drawScanlineSim(lx, ly, rx, ry);
        }
    }

    // ----- Render canvas with ImDrawList ------------------------------------
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize(float(gridW) * cellSize, float(gridH) * cellSize);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    auto cellTopLeft = [&](int x, int y) -> ImVec2 {
        return ImVec2(canvasOrigin.x + (x - padXMin) * cellSize,
                      canvasOrigin.y + (y - padYMin) * cellSize);
    };
    auto vertexCenter = [&](int v) -> ImVec2 {
        // Center the vertex marker on the integer pixel cell.
        return ImVec2(canvasOrigin.x + (xs[v] - padXMin + 0.5f) * cellSize,
                      canvasOrigin.y + (ys[v] - padYMin + 0.5f) * cellSize);
    };

    // Background.
    dl->AddRectFilled(canvasOrigin,
                      ImVec2(canvasOrigin.x + canvasSize.x,
                             canvasOrigin.y + canvasSize.y),
                      IM_COL32(28, 30, 38, 255));

    // Filled cells (translucent green so polygon edges remain readable on top).
    if (simulateFill) {
        for (int gy = 0; gy < gridH; ++gy) {
            for (int gx = 0; gx < gridW; ++gx) {
                if (!filled[gy * gridW + gx]) continue;
                ImVec2 a = cellTopLeft(padXMin + gx, padYMin + gy);
                ImVec2 b(a.x + cellSize, a.y + cellSize);
                dl->AddRectFilled(a, b, IM_COL32(80, 200, 110, 170));
            }
        }
    }

    // Per-pixel grid lines (only when cells are large enough to discern).
    if (cellSize >= 4.0f) {
        const ImU32 gcol = IM_COL32(70, 75, 95, 200);
        for (int gx = 0; gx <= gridW; ++gx) {
            ImVec2 p1 = cellTopLeft(padXMin + gx, padYMin);
            dl->AddLine(p1, ImVec2(p1.x, p1.y + canvasSize.y), gcol, 1.0f);
        }
        for (int gy = 0; gy <= gridH; ++gy) {
            ImVec2 p1 = cellTopLeft(padXMin, padYMin + gy);
            dl->AddLine(p1, ImVec2(p1.x + canvasSize.x, p1.y), gcol, 1.0f);
        }
    }

    // bbox border. Default red; switches to green when host applyState
    // classified the polygon as FLAG_THIN (only available in compute mode
    // snapshots; graphics fallback always uses red).
    {
        ImU32 bboxCol = IM_COL32(220, 80, 80, 220);  // red (normal)
        if (_snapshotIsCompute && (size_t)target < _snapshot->rawToCmdIndex.size()) {
            const int cmdIdx = _snapshot->rawToCmdIndex[target];
            if (cmdIdx >= 0 && (size_t)cmdIdx < _snapshot->cmds.size() &&
                (_snapshot->cmds[cmdIdx].flags & VDP1C_FLAG_THIN)) {
                bboxCol = IM_COL32(80, 220, 80, 220);  // green (THIN)
            }
        }
        ImVec2 a = cellTopLeft(xmin, ymin);
        ImVec2 b = cellTopLeft(xmax + 1, ymax + 1);
        dl->AddRect(a, b, bboxCol, 0.0f, 0, 2.0f);
    }

    // Polygon edges (cyan): v0 -> v1 -> v2 -> v3 -> v0.
    // Polyline (cmdType 5) skips the closing edge.
    {
        const ImU32 ecol = IM_COL32(120, 200, 255, 230);
        for (int v = 0; v < 4; ++v) {
            int next = (v + 1) & 3;
            if (cmdType == 0x05 && v == 3) continue;  // open path
            dl->AddLine(vertexCenter(v), vertexCenter(next), ecol, 1.5f);
        }
    }

    // Vertex markers + labels (A/B/C/D).
    static const char* kLabels[4] = { "A", "B", "C", "D" };
    static const ImU32 kColors[4] = {
        IM_COL32(255, 110, 110, 255),  // A
        IM_COL32(255, 220, 110, 255),  // B
        IM_COL32(120, 255, 120, 255),  // C
        IM_COL32(160, 200, 255, 255),  // D
    };
    for (int v = 0; v < 4; ++v) {
        ImVec2 c = vertexCenter(v);
        dl->AddCircleFilled(c, 4.5f, kColors[v]);
        dl->AddCircle      (c, 4.5f, IM_COL32(0, 0, 0, 220), 0, 1.0f);
        dl->AddText        (ImVec2(c.x + 6.0f, c.y - 8.0f), kColors[v], kLabels[v]);
    }

    // Reserve canvas space in ImGui's layout so subsequent widgets stack
    // below it instead of overlapping.
    ImGui::Dummy(canvasSize);

    // Stats line (under the canvas).
    int filledCount = 0;
    for (uint8_t v : filled) filledCount += (v != 0);
    ImGui::Text("Scanlines: %d  Filled px: %d / bbox %d  (raw vertices, no scale/localCoord)",
                total, filledCount,
                (xmax - xmin + 1) * (ymax - ymin + 1));
    ImGui::Separator();
}

void DebugUI::buildJsonlExportPanel() {
    ImGui::Begin("JSONL Export");

    std::filesystem::path exportDir = std::filesystem::path(getExeDir()) / "debug" / "vdp1";
    std::filesystem::path latestPath = exportDir / "latest.jsonl";

    ImGui::Text("Path: %s", latestPath.string().c_str());
    ImGui::Checkbox("Also write timestamped copy", &_alsoTimestampedCopy);

    bool canExport = _paused && _snapshot;
    if (!canExport) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Export Snapshot")) {
        std::string err;
        bool ok = Vdp1JsonlExporter::exportSnapshot(*_snapshot, latestPath.string(), &err);

        if (ok && _alsoTimestampedCopy) {
            // YYYYMMDD_HHMMSS_frame<N>.jsonl
            time_t s = (time_t)(_snapshot->timestampMs / 1000);
            struct tm tm_local;
#ifdef _WIN32
            localtime_s(&tm_local, &s);
#else
            localtime_r(&s, &tm_local);
#endif
            char ts[64];
            snprintf(ts, sizeof(ts), "%04d%02d%02d_%02d%02d%02d_frame%llu.jsonl",
                     tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
                     tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec,
                     (unsigned long long)_snapshot->frameId);
            std::filesystem::path tsPath = exportDir / ts;
            std::string err2;
            ok = ok && Vdp1JsonlExporter::exportSnapshot(*_snapshot, tsPath.string(), &err2);
            if (!ok) err = err2;
        }

        _lastExportPath = ok ? latestPath.string() : "";
        _lastExportError = ok ? "" : err;
    }
    if (!canExport) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Folder")) {
        std::filesystem::create_directories(exportDir);
        openInExplorer(exportDir.string());
    }

    if (!_lastExportPath.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                           "Last export OK: %s", _lastExportPath.c_str());
    }
    if (!_lastExportError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "Last export failed: %s", _lastExportError.c_str());
    }

    ImGui::End();
}

void DebugUI::recordImGuiDraw(VkCommandBuffer cb,
                              uint32_t /*swapchainImageIdx*/,
                              VkFramebuffer framebuffer) {
  if (!_available) return;

  ImDrawData* dd = ImGui::GetDrawData();
  if (dd == nullptr || dd->CmdListsCount == 0) return;

  VkExtent2D extent = _window->GetVulkanSurfaceSize();

  VkRenderPassBeginInfo rp{};
  rp.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.framebuffer              = framebuffer;
  rp.renderArea.offset        = {0, 0};
  rp.renderArea.extent.width  = extent.width;
  rp.renderArea.extent.height = extent.height;

  // Task 14: pause vs live render-pass selection.
  // Pause: emulator did not draw this frame, so use the CLEAR variant
  //        and zero the swapchain to a neutral background before the
  //        ImGui draw.
  // Live:  emulator already populated the swapchain via VIDVulkan,
  //        so use the LOAD variant to preserve it.
  VkClearValue clear[1] = {};
  if (_paused) {
    clear[0].color = {{0.05f, 0.05f, 0.05f, 1.0f}};
    rp.renderPass        = _window->GetVulkanRenderPass();
    rp.clearValueCount   = 1;
    rp.pClearValues      = clear;
  } else {
    rp.renderPass        = _window->GetVulkanKeepRenderPass();
    rp.clearValueCount   = 0;
    rp.pClearValues      = nullptr;
  }

  vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);
  ImGui_ImplVulkan_RenderDrawData(dd, cb);
  vkCmdEndRenderPass(cb);
}

void DebugUI::requestTogglePause() {
  if (!_available) return;
  if (_paused) {
    // Resume immediately. Drop the snapshot so the next pause grabs a
    // fresh one from the live emulator state.
    _paused = false;
    delete _snapshot;
    _snapshot = nullptr;
    std::cout << "[DebugUI] resumed" << std::endl;
  } else {
    // Defer the actual capture to the next pre-beginFrame hook so we
    // freeze a complete frame's cpuCmds/state pair.
    _pendingPause = true;
    Vdp1ComputeRasterizer* cur = _vid ? _vid->getVdp1Compute() : nullptr;
    std::cout << "[DebugUI] pause pending... (compute=" << (void*)cur
              << " hooked=" << (void*)_hookedComputeInstance
              << " match=" << (cur == _hookedComputeInstance) << ")" << std::endl;
  }
}

void DebugUI::onPreBeginFrame() {
  ++_frameId;
  if (_frameId == 1 || _frameId % 60 == 0) {
    std::cout << "[DebugUI] onPreBeginFrame fired (frameId=" << _frameId
              << " pending=" << _pendingPause << ")" << std::endl;
  }
  if (!_pendingPause) return;
  std::cout << "[DebugUI] onPreBeginFrame: capturing snapshot now" << std::endl;
  _pendingPause = false;

  // The renderer hook fires before the compute hook in the same frame, so
  // _pendingPause is consumed exactly once even when both are registered.

  Vdp1ComputeRasterizer* compute = (_vid != nullptr) ? _vid->getVdp1Compute() : nullptr;
  Vdp1Renderer*          rend    = (_vid != nullptr) ? _vid->getVdp1Renderer() : nullptr;

  delete _snapshot;
  _snapshot = nullptr;

  if (compute != nullptr) {
    // COMPUTE_RASTERIZER path. Use the rasterizer's cached VRAM/CRAM,
    // cpuCmds, and state -- they reflect the exact bytes / parsed list
    // that produced the just-finished frame.
    //
    // The compute rasterizer is about to clear cpuCmds for the new frame,
    // so this is the precise window where the previous frame's parsed
    // command list is still intact.
    //
    // Vdp1Ram (512KB) and Vdp2ColorRam (4KB) come from the C core;
    // they are declared in vdp1.h / vdp2.h via the extern "C" block above
    // and must NOT be redeclared as plain C++ extern (would conflict with
    // the C linkage of the original declarations).
    // Vdp1Regs is a Vdp1*; DebugSnapshot::take() takes Vdp1 by const ref,
    // so dereference at the call site.
    // Use the compute rasterizer's actual offscreen image dimensions.
    // Vdp1Cmd vertex / bbox values are stored in this physical pixel space
    // (with HD scale applied by Vdp1Renderer), so 704x512 is wrong on any
    // non-native resolution and bbox overlays will misalign.
    const int fbW = compute->getFbWidth();
    const int fbH = compute->getFbHeight();

    // Use the compute rasterizer's cached VRAM/CRAM, not the live Vdp1Ram /
    // Vdp2ColorRam. The game thread can overwrite VRAM (especially CMDLINK)
    // between the just-completed render pass and this hook, which would make
    // parseRawCmds traverse a different command chain than the one cpuCmds was
    // built from. The cache was snapshotted at uploadVramCram() time, i.e. the
    // exact moment cpuCmds was finalized, so the two stay aligned.
    const auto& cv = compute->getCachedVram();
    const auto& cc = compute->getCachedCram();
    const uint8_t* vramPtr  = !cv.empty() ? cv.data() : Vdp1Ram;
    const size_t   vramSize = !cv.empty() ? cv.size() : (512 * 1024);
    const uint8_t* cramPtr  = !cc.empty() ? cc.data() : Vdp2ColorRam;
    const size_t   cramSize = !cc.empty() ? cc.size() : (4 * 1024);
    _snapshot = new DebugSnapshot(DebugSnapshot::take(
        compute->getCpuCmds(),
        compute->getState(),
        vramPtr,        vramSize,
        cramPtr,        cramSize,
        *Vdp1Regs,
        fbW, fbH,
        _frameId));
    _snapshotIsCompute = true;

    // Step starts at the last command (whole frame visible).
    _stepN = static_cast<int>(_snapshot->cmds.size()) - 1;
    if (_stepN < 0) _stepN = 0;
  } else {
    // GPU_TESSERATION / PERSPECTIVE_CORRECTION path. No compute rasterizer
    // means no cpuCmds / state / cached VRAM available -- we capture from
    // live emulator memory. The hook fires at the start of the next frame
    // so the previous frame's VDP1 command chain in VRAM is still the one
    // the graphics pipeline rendered (the SH-2 hasn't necessarily begun
    // overwriting it yet for this frame).
    //
    // Compute-side fields stay empty: cmds = {}, state = default. The user
    // sees rawCmds (parsed straight from VRAM) in the Command List / Detail
    // panels.
    //
    // fbWidth/Height = the *Saturn fb pixel space* in which the graphics
    // pipeline emits vertices, NOT the Vulkan offscreen image dimensions.
    // The graphics path renders by scaling Saturn coords by vdp1wratio
    // and then projecting through glm::ortho(0, vdp2width, vdp2height, 0)
    // -- so content fills the full offscreen image, but the relevant
    // coordinate system for bbox math is [0, vdp2width] x [0, vdp2height].
    // Using offscreen.width here would put a saturnX=320 bbox at only
    // 17% of the displayed preview while the actual rendered content fills
    // the whole rectangle.
    int fbW = (_vid != nullptr) ? _vid->vdp2width  : 704;
    int fbH = (_vid != nullptr) ? _vid->vdp2height : 512;
    if (fbW <= 0) fbW = 704;
    if (fbH <= 0) fbH = 512;
    static const std::vector<Vdp1Cmd> kEmptyCmds;
    static const vdp1c::Vdp1State     kDefaultState{};
    _snapshot = new DebugSnapshot(DebugSnapshot::take(
        kEmptyCmds,
        kDefaultState,
        Vdp1Ram,        512 * 1024,
        Vdp2ColorRam,   4   * 1024,
        *Vdp1Regs,
        fbW, fbH,
        _frameId));
    _snapshotIsCompute = false;

    // No compute cmds, so step starts at the last raw command for browsing.
    _stepN = static_cast<int>(_snapshot->rawCmds.size()) - 1;
    if (_stepN < 0) _stepN = 0;
  }

  // Force the first replay dispatch on the new snapshot. -1 wouldn't match
  // any valid _stepN so the next renderPausedFrame() always re-renders.
  _lastDispatchedStepN = -1;

  _paused = true;
  std::cout << "[DebugUI] paused at frame " << _frameId
            << " with "       << _snapshot->rawCmds.size()
            << " commands ("  << (_snapshotIsCompute ? "compute" : "graphics")
            << " mode)"       << std::endl;
  refreshFbReadback();
}

void DebugUI::renderPausedFrame() {
  // B20: if VDP2 debugger is paused, drive its replay path. Vdp2DebugUI
  // calls VIDVulkan::Vdp2DrawEnd which internally does BeginRender +
  // present (via YuiSwapBuffers -> VulkanScene::present), and the
  // pre-present hook draws the ImGui overlay on top, so no extra
  // present is needed here.
  if (_vdp2 && _vdp2->isPaused()) {
    _vdp2->renderPausedFrame();
    if (!_paused) return;
  }

  // Task 14: pause-mode frame. Replay commands [0, _stepN] from the
  // captured snapshot into the VDP1 offscreen image, then present so
  // the pre-present hook can overlay ImGui (incl. the Offscreen Preview
  // panel that samples the offscreen image).
  if (!_paused || _snapshot == nullptr || !_available) return;

  // Graphics-mode snapshots (GPU_TESSERATION / PERSPECTIVE_CORRECTION):
  // re-render commands [0, _stepN] into the VDP1 offscreen image via the
  // dedicated replay path. The path does its own GPU submit + waitIdle, so
  // the offscreen image is fully written by the time we present below.
  // The dispatch is gated on _stepN change so panning the window or
  // typing in ImGui doesn't trigger a re-render every UI tick.
  if (!_snapshotIsCompute) {
    if (_stepN != _lastDispatchedStepN) {
      dispatchGraphicsUpTo(_stepN);
      _lastDispatchedStepN = _stepN;
    }
    _window->BeginRender();
    _window->EndRender({});
    refreshFbReadback();
    return;
  }

  Vdp1ComputeRasterizer* compute = _vid->getVdp1Compute();
  if (compute == nullptr) return;

  // 1) Push the snapshot's VRAM/CRAM into the compute SSBOs so the
  //    rasterizer reads the exact bytes captured at pause time, not
  //    the live (and now drifting) emulator memory.
  compute->uploadVramCram(_snapshot->vram.data(), _snapshot->cram.data());

  // 2) Acquire the next swapchain image. Window::EndRender below will
  //    submit the empty primary command buffer (containing only the
  //    pre-present hook draw) and present.
  _window->BeginRender();

  // 3) Allocate a transient command buffer for the compute dispatch.
  //    Performance is not a concern in pause mode (single-threaded,
  //    user-driven), so a per-frame pool keeps lifetime management
  //    trivial.
  VkDevice dev = _renderer->GetVulkanDevice();
  VkCommandPool pool = VK_NULL_HANDLE;
  {
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = _renderer->GetVulkanGraphicsQueueFamilyIndex();
    vkCreateCommandPool(dev, &pci, nullptr, &pool);
  }

  VkCommandBufferAllocateInfo cba{};
  cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cba.commandPool = pool;
  cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cba.commandBufferCount = 1;
  VkCommandBuffer cb = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(dev, &cba, &cb);

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);

  // Translate _stepN (rawCmds index, 1:1 with what Command List shows)
  // into the highest compute cmds index whose rawCmds position is <= _stepN.
  // -1 means "no compute-drawn command at or before this step" -> dispatch
  // nothing (image stays cleared).
  int computeLast = -1;
  if ((size_t)_stepN < _snapshot->rawToCmdIndex.size()) {
    for (int i = 0; i <= _stepN && i < (int)_snapshot->rawToCmdIndex.size(); ++i) {
      int k = _snapshot->rawToCmdIndex[i];
      if (k >= 0) computeLast = k;
    }
  }

  VkImage     img  = _vid->getVdp1OffscreenImage();
  VkImageView view = _vid->getVdp1OffscreenImageView();
  if (img != VK_NULL_HANDLE && view != VK_NULL_HANDLE) {
    // Clear offscreen image to transparent before each step replay, so
    // step N shows ONLY pixels touched by commands [0..N], not whatever
    // remained from the previous live-mode frame or previous step.
    // The image is left in SHADER_READ_ONLY_OPTIMAL after the previous
    // dispatch (live or pause), so we transition through TRANSFER_DST,
    // clear, then back to SHADER_READ_ONLY (which is what dispatchUpTo
    // expects for non-first dispatches).
    {
      VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

      VkImageMemoryBarrier toClear{};
      toClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      toClear.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toClear.image = img;
      toClear.subresourceRange = range;
      toClear.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      vkCmdPipelineBarrier(cb,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          0, 0, nullptr, 0, nullptr, 1, &toClear);

      VkClearColorValue clearColor = {};
      clearColor.float32[0] = 0.0f;
      clearColor.float32[1] = 0.0f;
      clearColor.float32[2] = 0.0f;
      clearColor.float32[3] = 0.0f;
      vkCmdClearColorImage(cb, img,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          &clearColor, 1, &range);

      VkImageMemoryBarrier toShader{};
      toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toShader.image = img;
      toShader.subresourceRange = range;
      toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(cb,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          0, 0, nullptr, 0, nullptr, 1, &toShader);
    }

    // computeLast was derived above by walking rawToCmdIndex up to _stepN.
    // If -1, no compute cmd has been processed yet -> skip dispatch and
    // leave the image cleared. Otherwise dispatch [0, computeLast].
    if (computeLast >= 0) {
      // The toShader barrier above leaves img in SHADER_READ_ONLY_OPTIMAL.
      // Step replay always uses frame slot 0 -- single-shot debug dispatch,
      // not double-buffered with the live frame loop.
      compute->dispatchUpTo(cb, img, view,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            (uint32_t)computeLast, /*frameIndex=*/0, &_snapshot->cmds);
    }
  }

  vkEndCommandBuffer(cb);

  // 4) Submit the dispatch. Signal a semaphore so EndRender() waits on
  //    compute completion before presenting (which doesn't matter for
  //    presentation correctness, but does ensure the pre-present hook's
  //    ImGui sampling of the offscreen image sees finished writes).
  VkSemaphore dispatchDone = VK_NULL_HANDLE;
  {
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(dev, &sci, nullptr, &dispatchDone);
  }
  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &dispatchDone;
  vkQueueSubmit(_renderer->GetVulkanQueue(), 1, &si, VK_NULL_HANDLE);

  // 5) ImGui draw + present. recordImGuiDraw() observes _paused==true
  //    and uses the CLEAR variant render pass.
  _window->EndRender({ dispatchDone });

  // 6) Pause is single-threaded and not perf-sensitive; wait until
  //    everything submitted above has retired before destroying the
  //    transient pool / semaphore. This avoids needing to track a fence.
  vkQueueWaitIdle(_renderer->GetVulkanQueue());
  vkDestroySemaphore(dev, dispatchDone, nullptr);
  vkDestroyCommandPool(dev, pool, nullptr);

  _lastDispatchedStepN = _stepN;
  refreshFbReadback();
}

void DebugUI::buildOffscreenPreviewPanel() {
  ImGui::Begin("Offscreen Preview");
  if (!_paused || !_snapshot) {
    ImGui::Text("Not paused.");
    ImGui::End();
    return;
  }

  VkImageView view = _vid ? _vid->getVdp1OffscreenImageView() : VK_NULL_HANDLE;
  if (view == VK_NULL_HANDLE) {
    ImGui::Text("Offscreen image not ready.");
    ImGui::End();
    return;
  }

  // Re-bind on view change: the offscreen image may be recreated on
  // resize, in which case the cached descriptor points at a stale view
  // and would alias-sample arbitrary memory.
  if (view != _previewBoundView) {
    if (_previewTex != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(_previewTex);
      _previewTex = VK_NULL_HANDLE;
    }
    _previewTex = ImGui_ImplVulkan_AddTexture(
        _previewSampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _previewBoundView = view;
  }

  ImGui::Text("Resolution: %dx%d", _snapshot->fbWidth, _snapshot->fbHeight);

  // Zoom toolbar. _previewZoom == 0 means aspect-fit (default); >0 is a
  // native-pixel multiplier rendered inside a scrollable child region.
  ImGui::SameLine();
  ImGui::TextUnformatted("  Zoom:");
  ImGui::SameLine();
  if (ImGui::SmallButton("Fit")) _previewZoom = 0.0f;
  ImGui::SameLine();
  if (ImGui::SmallButton("1x"))  _previewZoom = 1.0f;
  ImGui::SameLine();
  if (ImGui::SmallButton("2x"))  _previewZoom = 2.0f;
  ImGui::SameLine();
  if (ImGui::SmallButton("4x"))  _previewZoom = 4.0f;
  ImGui::SameLine();
  if (ImGui::SmallButton("8x"))  _previewZoom = 8.0f;
  ImGui::SameLine();
  if (ImGui::SmallButton("16x")) _previewZoom = 16.0f;
  ImGui::SameLine();
  ImGui::SetNextItemWidth(140.0f);
  ImGui::SliderFloat("##zoom", &_previewZoom, 0.0f, 32.0f,
                     _previewZoom <= 0.0f ? "Fit" : "%.1fx",
                     ImGuiSliderFlags_AlwaysClamp);

  // Scrollable region so zoomed views are pannable. Horizontal scroll is
  // enabled because zoomed-in image width usually exceeds the panel.
  ImGui::BeginChild("##preview_scroll", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);

  // Center-preserving zoom: when _previewZoom changes, recompute scroll so
  // the image coordinate that was at the viewport center stays at the new
  // viewport center. Without this the scroll position is left as-is and
  // the larger/smaller content effectively pivots around the top-left.
  if (_previewZoom != _previewZoomPrev) {
    if (_previewZoom > 0.0f) {
      const ImVec2 windowSize = ImGui::GetWindowSize();
      const float vw = windowSize.x;
      const float vh = windowSize.y;
      float centerImgX, centerImgY;
      if (_previewZoomPrev > 0.0f) {
        // Both old and new zooms are explicit: derive the previously
        // centered image coord from the active scroll value.
        const float oldScrollX = ImGui::GetScrollX();
        const float oldScrollY = ImGui::GetScrollY();
        centerImgX = (oldScrollX + vw * 0.5f) / _previewZoomPrev;
        centerImgY = (oldScrollY + vh * 0.5f) / _previewZoomPrev;
      } else {
        // Coming from Fit (or first activation): anchor at image center
        // so the user's first zoom-in lands on the middle of the frame.
        centerImgX = (float)_snapshot->fbWidth  * 0.5f;
        centerImgY = (float)_snapshot->fbHeight * 0.5f;
      }
      const float imgW = (float)_snapshot->fbWidth  * _previewZoom;
      const float imgH = (float)_snapshot->fbHeight * _previewZoom;
      // Parenthesize to defeat windows.h's max() macro that some
      // upstream header pulls in transitively.
      const float maxScrollX = (std::max)(0.0f, imgW - vw);
      const float maxScrollY = (std::max)(0.0f, imgH - vh);
      const float newScrollX = std::clamp(centerImgX * _previewZoom - vw * 0.5f,
                                          0.0f, maxScrollX);
      const float newScrollY = std::clamp(centerImgY * _previewZoom - vh * 0.5f,
                                          0.0f, maxScrollY);
      ImGui::SetScrollX(newScrollX);
      ImGui::SetScrollY(newScrollY);
    }
    _previewZoomPrev = _previewZoom;
  }

  ImVec2 size;
  if (_previewZoom <= 0.0f) {
    // Aspect-ratio-preserving fit to the panel's content region.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int sw = _snapshot->fbWidth;
    int sh = _snapshot->fbHeight;
    if (sh < 1) sh = 1;
    float aspect = (float)sw / (float)sh;
    if (avail.x / aspect <= avail.y) {
      size = ImVec2(avail.x, avail.x / aspect);
    } else {
      size = ImVec2(avail.y * aspect, avail.y);
    }
  } else {
    size = ImVec2((float)_snapshot->fbWidth  * _previewZoom,
                  (float)_snapshot->fbHeight * _previewZoom);
  }

  // Y-flip via UV: the VDP1 compute offscreen image stores Y growing
  // downward in Vulkan texel space, but the live VDP2 compositor flips
  // when it samples. Sampling directly from ImGui appears upside-down,
  // so we flip the V coordinate here to match the VDP2-rendered look.
  ImGui::Image((ImTextureID)_previewTex, size,
               ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

  // Cache the image rect once so the hover-tooltip and bbox overlay
  // below share a consistent reference (subsequent ImGui calls like
  // SetTooltip can shift the "current item").
  const ImVec2 imgMin = ImGui::GetItemRectMin();
  const ImVec2 imgMax = ImGui::GetItemRectMax();
  const bool   imgHovered = ImGui::IsItemHovered();

  // Pixel-coord tooltip on hover. Useful for matching the rendered
  // output against debug logs that report Saturn fb pixel coords. When
  // FB readback cache is valid, additionally show the raw RGBA bytes and
  // VDP1COLOR-decoded fields (C / colorcl / priority / shadow /
  // sprite_window / colorindex). Trigger refresh on first hover so the
  // user does not need to visit Memory Viewer first.
  if (imgHovered) {
    ImVec2 m = ImGui::GetIO().MousePos;
    float w = imgMax.x - imgMin.x;
    float h = imgMax.y - imgMin.y;
    if (w > 0.0f && h > 0.0f) {
      float u = (m.x - imgMin.x) / w;
      float v = (m.y - imgMin.y) / h;
      int px = (int)(u * _snapshot->fbWidth);
      int py = (int)(v * _snapshot->fbHeight);
      if (px >= 0 && py >= 0 &&
          px < _snapshot->fbWidth && py < _snapshot->fbHeight) {
        if (!_fbReadbackValid) refreshFbReadback();
        char buf[320];
        int n = snprintf(buf, sizeof(buf), "Pixel: %d, %d", px, py);
        if (_fbReadbackValid &&
            _fbReadback.width() > 0 && _fbReadback.height() > 0) {
          // Image is drawn with UV (0,1)-(1,0). Both modes write
          // V-flipped storage:
          //   - compute: vdp1_compute_*.comp imageStores at ivec2(x,
          //     fbH-1-y), explicit V-flip
          //   - tessellation/graphics: glm::ortho(0,W,H,0) inverts top
          //     and bottom, so combined with Vulkan's Y-down NDC the
          //     graphics pipeline lands Saturn Y=0 at storage row fbH-1
          //     (same orientation as compute, just reached via projection)
          // In both cases the UV flip on display restores Saturn
          // orientation, so a hover at display row py corresponds to
          // Saturn row py and storage row physH-1-py_phys.
          //
          // _snapshot->fbWidth/Height is the LOGICAL bbox space:
          //   - compute: physical pixel space (= offscreen image dims)
          //   - tessellation: vdp2width x vdp2height (Saturn fb space),
          //     smaller than the physical offscreen image at HD upscale
          // Readback is always at the physical offscreen dims (set in
          // refreshFbReadback), so map (px, py) from logical to physical
          // before indexing storage. Compute mode ratios are 1:1.
          const uint32_t physW = _fbReadback.width();
          const uint32_t physH = _fbReadback.height();
          const int px_phys = static_cast<int>(
              (float)px * (float)physW / (float)_snapshot->fbWidth);
          const int py_phys = static_cast<int>(
              (float)py * (float)physH / (float)_snapshot->fbHeight);
          if (px_phys >= 0 && py_phys >= 0 &&
              (uint32_t)px_phys < physW && (uint32_t)py_phys < physH) {
            const int storageY = static_cast<int>(physH) - 1 - py_phys;
            const size_t pixIdx =
                static_cast<size_t>(storageY) * physW + px_phys;
            const uint8_t* p = _fbReadback.pixels().data() + pixIdx * 4;
            const uint32_t rgba =
                static_cast<uint32_t>(p[0]) |
                (static_cast<uint32_t>(p[1]) <<  8) |
                (static_cast<uint32_t>(p[2]) << 16) |
                (static_cast<uint32_t>(p[3]) << 24);
            const auto d = memdecode::decodeVdp1Color(rgba);
            snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n),
                     "\nRGBA: 0x%02X_%02X_%02X_%02X"
                     "\nC=%u colorcl=%u priority=%u"
                     "\nshadow=%u sprite_window=%u"
                     "\ncolorindex=0x%04X (%u)",
                     p[3], p[2], p[1], p[0],
                     d.c, d.colorcl, d.priority,
                     d.shadow, d.sprite_window,
                     d.colorindex, d.colorindex);
          }
        }
        ImGui::SetTooltip("%s", buf);
      }
    }
  }

  // Bbox overlay for the currently selected (or step-N) command.
  // Same single-rect behavior in both modes:
  //   - if the target rawCmds index is a draw command, show its bbox
  //     (red = normal, green = FLAG_THIN classified by host applyState)
  //   - otherwise (state-only cmd: LocalCoord / SystemClip / UserClip /
  //     draw_end / skipped) show nothing
  // Compute mode pulls the rasterizer-recorded bbox out of cmds[].bbox.
  // Graphics mode reconstructs the bbox from rawCmds vertices, applying
  // localX/localY (tracked by walking earlier rawCmds) and the renderer's
  // vdp1wratio / vdp1hratio so the rectangle lands in fb pixel space.
  const int fbW = _snapshot->fbWidth  > 0 ? _snapshot->fbWidth  : 1;
  const int fbH = _snapshot->fbHeight > 0 ? _snapshot->fbHeight : 1;
  const float scaleX = (imgMax.x - imgMin.x) / (float)fbW;
  const float scaleY = (imgMax.y - imgMin.y) / (float)fbH;
  const int rawTarget = (_selectedCmdIndex >= 0) ? _selectedCmdIndex : _stepN;

  auto drawHighlight = [&](float x0, float y0, float x1, float y1, ImU32 color) {
    if (x1 <= x0 || y1 <= y0) return;
    ImVec2 p0(imgMin.x + x0 * scaleX, imgMin.y + y0 * scaleY);
    ImVec2 p1(imgMin.x + x1 * scaleX, imgMin.y + y1 * scaleY);
    ImGui::GetWindowDrawList()->AddRect(p0, p1, color, 0.0f, 0, 2.0f);
  };
  constexpr ImU32 kBboxColorNormal = IM_COL32(255, 50, 50, 255);   // red
  constexpr ImU32 kBboxColorThin   = IM_COL32(50, 220, 50, 255);   // green: FLAG_THIN

  if (_snapshotIsCompute) {
    int cmdIdx = -1;
    if (rawTarget >= 0 && (size_t)rawTarget < _snapshot->rawToCmdIndex.size()) {
      cmdIdx = _snapshot->rawToCmdIndex[rawTarget];
    }
    if (cmdIdx >= 0 && (size_t)cmdIdx < _snapshot->cmds.size()) {
      const auto& cmd = _snapshot->cmds[cmdIdx];
      const ImU32 color = (cmd.flags & VDP1C_FLAG_THIN) ? kBboxColorThin
                                                        : kBboxColorNormal;
      drawHighlight(cmd.bbox.x, cmd.bbox.y, cmd.bbox.z, cmd.bbox.w, color);
    }
  } else if (rawTarget >= 0 && rawTarget < (int)_snapshot->rawCmds.size()) {
    Vdp1Renderer* rend = _vid ? _vid->getVdp1Renderer() : nullptr;
    const float wratio = rend ? rend->vdp1wratio : 1.0f;
    const float hratio = rend ? rend->vdp1hratio : 1.0f;

    auto sextY = [](int16_t v) -> int16_t {
      return (v & 0x1000) ? (int16_t)((uint16_t)v | 0xE000)
                          : (int16_t)((uint16_t)v & ~0xE000);
    };
    auto sextX = [](int16_t v) -> int16_t {
      return (v & 0x1000) ? (int16_t)((uint16_t)v | 0xE000)
                          : (int16_t)((uint16_t)v & ~0xE000);
    };

    // Track localX/localY by walking earlier cmds. LocalCoordinate (cmd
    // type 10) sets the running offset for everything after it; without
    // this walk the highlight rect would land at the wrong place whenever
    // the selected cmd lives under a non-zero local origin.
    int16_t localX = 0;
    int16_t localY = 0;
    for (int i = 0; i < rawTarget; ++i) {
      const auto& rc = _snapshot->rawCmds[i];
      const uint16_t ctrl = rc.CMDCTRL;
      if (ctrl & 0x8000) break;
      if (ctrl & 0x4000) continue;
      if ((ctrl & 0x000F) == 10) {
        localX = (int16_t)rc.CMDXA;
        localY = sextY((int16_t)rc.CMDYA);
      }
    }

    const auto& rc = _snapshot->rawCmds[rawTarget];
    const uint16_t ctrl = rc.CMDCTRL;
    if (!(ctrl & 0xC000)) {  // skip draw_end / skipped
      int16_t xs[4]{}, ys[4]{};
      int n = 0;
      switch (ctrl & 0x000F) {
        case 0: {
          int16_t XA = sextX((int16_t)rc.CMDXA);
          int16_t YA = sextY((int16_t)rc.CMDYA);
          int w = ((rc.CMDSIZE >> 8) & 0x3F) * 8;
          int h =  (rc.CMDSIZE       & 0xFF);
          xs[0] = XA;                 ys[0] = YA;
          xs[1] = XA + (int16_t)w;    ys[1] = YA + (int16_t)h;
          n = 2;
          break;
        }
        case 1: {
          int16_t XA = sextX((int16_t)rc.CMDXA);
          int16_t YA = sextY((int16_t)rc.CMDYA);
          int16_t XC = sextX((int16_t)rc.CMDXC);
          int16_t YC = sextY((int16_t)rc.CMDYC);
          xs[0] = XA; ys[0] = YA;
          xs[1] = XC; ys[1] = YC;
          n = 2;
          break;
        }
        case 2: case 3:
        case 4:
        case 5: case 7: {
          xs[0] = (int16_t)rc.CMDXA; ys[0] = sextY((int16_t)rc.CMDYA);
          xs[1] = (int16_t)rc.CMDXB; ys[1] = sextY((int16_t)rc.CMDYB);
          xs[2] = (int16_t)rc.CMDXC; ys[2] = sextY((int16_t)rc.CMDYC);
          xs[3] = (int16_t)rc.CMDXD; ys[3] = sextY((int16_t)rc.CMDYD);
          n = 4;
          break;
        }
        case 6: {
          xs[0] = (int16_t)rc.CMDXA; ys[0] = sextY((int16_t)rc.CMDYA);
          xs[1] = (int16_t)rc.CMDXB; ys[1] = sextY((int16_t)rc.CMDYB);
          n = 2;
          break;
        }
        default:
          n = 0;  // state-only cmd: nothing to highlight
          break;
      }
      if (n > 0) {
        int16_t mnx = xs[0], mxx = xs[0], mny = ys[0], mxy = ys[0];
        for (int k = 1; k < n; ++k) {
          if (xs[k] < mnx) mnx = xs[k];
          if (xs[k] > mxx) mxx = xs[k];
          if (ys[k] < mny) mny = ys[k];
          if (ys[k] > mxy) mxy = ys[k];
        }
        // Graphics mode: FLAG_THIN is only set by the host compute pipeline,
        // so the graphics fallback path always renders in the normal red.
        drawHighlight((mnx + localX) * wratio,
                      (mny + localY) * hratio,
                      (mxx + localX) * wratio,
                      (mxy + localY) * hratio,
                      kBboxColorNormal);
      }
    }
  }

  ImGui::EndChild();
  ImGui::End();
}

// ---------------------------------------------------------------------------
// makeDecodeCacheKey -- hash {region,addr,mode,w,h,lutAddr} into uint64_t.
// ---------------------------------------------------------------------------
uint64_t DebugUI::makeDecodeCacheKey(int region, uint32_t addr, int mode,
                                     uint32_t w, uint32_t h, uint32_t lut) {
    uint64_t k = 0;
    k ^= static_cast<uint64_t>(region) << 56;
    k ^= static_cast<uint64_t>(addr)   << 24;
    k ^= static_cast<uint64_t>(mode & 0xFF) << 16;
    k ^= static_cast<uint64_t>(w & 0xFFFF);
    k ^= static_cast<uint64_t>(h & 0xFFFF) << 40;
    k ^= static_cast<uint64_t>(lut)        << 8;
    return k;
}

// ---------------------------------------------------------------------------
// uploadDecodedTexture -- upload RGBA8 pixels to a device-local VkImage and
// register the result with ImGui. Reallocates VkImage when size changes.
// Returns true on success; on failure cleans up any partial allocations.
// ---------------------------------------------------------------------------
namespace {

uint32_t findMemoryTypeForDecode(VkPhysicalDevice physDev, uint32_t typeBits,
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

bool DebugUI::uploadDecodedTexture(const std::vector<uint32_t>& rgba,
                                   uint32_t w, uint32_t h) {
    if (!_renderer) return false;
    VkDevice         dev     = _renderer->GetVulkanDevice();
    VkPhysicalDevice physDev = _renderer->GetVulkanPhysicalDevice();
    VkQueue          queue   = _renderer->GetVulkanQueue();
    uint32_t         qfam    = _renderer->GetVulkanGraphicsQueueFamilyIndex();
    if (dev == VK_NULL_HANDLE) return false;

    const VkDeviceSize dataBytes = VkDeviceSize(w) * h * 4u;
    if (dataBytes == 0 || rgba.size() < static_cast<size_t>(w) * h) return false;

    // -- Step 1: destroy old resources if size changed ----------------------
    if (w != _decodeUploadedW || h != _decodeUploadedH) {
        if (_decodeImguiTex != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(_decodeImguiTex);
            _decodeImguiTex = VK_NULL_HANDLE;
        }
        if (_decodeImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, _decodeImageView, nullptr);
            _decodeImageView = VK_NULL_HANDLE;
        }
        if (_decodeImage != VK_NULL_HANDLE) {
            vkDestroyImage(dev, _decodeImage, nullptr);
            _decodeImage = VK_NULL_HANDLE;
        }
        if (_decodeImageMem != VK_NULL_HANDLE) {
            vkFreeMemory(dev, _decodeImageMem, nullptr);
            _decodeImageMem = VK_NULL_HANDLE;
        }
        _decodeUploadedW = 0;
        _decodeUploadedH = 0;
    }

    // -- Step 2: create VkImage if not yet created --------------------------
    if (_decodeImage == VK_NULL_HANDLE) {
        VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.imageType   = VK_IMAGE_TYPE_2D;
        ici.format      = VK_FORMAT_R8G8B8A8_UNORM;
        ici.extent      = {w, h, 1};
        ici.mipLevels   = 1;
        ici.arrayLayers = 1;
        ici.samples     = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ici.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(dev, &ici, nullptr, &_decodeImage) != VK_SUCCESS) {
            return false;
        }

        // -- Step 3: allocate device-local memory ---------------------------
        VkMemoryRequirements imgreq{};
        vkGetImageMemoryRequirements(dev, _decodeImage, &imgreq);
        uint32_t memType = findMemoryTypeForDecode(
            physDev, imgreq.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == 0xFFFFFFFFu) {
            // Fallback to host-visible if no device-local type found.
            memType = findMemoryTypeForDecode(
                physDev, imgreq.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        }
        if (memType == 0xFFFFFFFFu) {
            vkDestroyImage(dev, _decodeImage, nullptr);
            _decodeImage = VK_NULL_HANDLE;
            return false;
        }
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize  = imgreq.size;
        mai.memoryTypeIndex = memType;
        if (vkAllocateMemory(dev, &mai, nullptr, &_decodeImageMem) != VK_SUCCESS) {
            vkDestroyImage(dev, _decodeImage, nullptr);
            _decodeImage = VK_NULL_HANDLE;
            return false;
        }

        // -- Step 4: bind image memory --------------------------------------
        if (vkBindImageMemory(dev, _decodeImage, _decodeImageMem, 0) != VK_SUCCESS) {
            vkFreeMemory(dev, _decodeImageMem, nullptr);
            _decodeImageMem = VK_NULL_HANDLE;
            vkDestroyImage(dev, _decodeImage, nullptr);
            _decodeImage = VK_NULL_HANDLE;
            return false;
        }

        // -- Step 5: create VkImageView -------------------------------------
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image    = _decodeImage;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format   = VK_FORMAT_R8G8B8A8_UNORM;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(dev, &vci, nullptr, &_decodeImageView) != VK_SUCCESS) {
            vkFreeMemory(dev, _decodeImageMem, nullptr);
            _decodeImageMem = VK_NULL_HANDLE;
            vkDestroyImage(dev, _decodeImage, nullptr);
            _decodeImage = VK_NULL_HANDLE;
            return false;
        }
    }

    // -- Step 6: staging buffer creation and memcpy -------------------------
    VkBuffer       staging    = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size        = dataBytes;
    bci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &bci, nullptr, &staging) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements breq{};
    vkGetBufferMemoryRequirements(dev, staging, &breq);
    const uint32_t stagMemType = findMemoryTypeForDecode(
        physDev, breq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (stagMemType == 0xFFFFFFFFu) {
        vkDestroyBuffer(dev, staging, nullptr);
        return false;
    }
    VkMemoryAllocateInfo bmai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    bmai.allocationSize  = breq.size;
    bmai.memoryTypeIndex = stagMemType;
    if (vkAllocateMemory(dev, &bmai, nullptr, &stagingMem) != VK_SUCCESS) {
        vkDestroyBuffer(dev, staging, nullptr);
        return false;
    }
    if (vkBindBufferMemory(dev, staging, stagingMem, 0) != VK_SUCCESS) {
        vkFreeMemory(dev, stagingMem, nullptr);
        vkDestroyBuffer(dev, staging, nullptr);
        return false;
    }
    void* mapped = nullptr;
    if (vkMapMemory(dev, stagingMem, 0, dataBytes, 0, &mapped) != VK_SUCCESS) {
        vkFreeMemory(dev, stagingMem, nullptr);
        vkDestroyBuffer(dev, staging, nullptr);
        return false;
    }
    std::memcpy(mapped, rgba.data(), static_cast<size_t>(dataBytes));
    vkUnmapMemory(dev, stagingMem);

    // -- One-time command buffer for layout transitions + copy --------------
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.queueFamilyIndex = qfam;
        if (vkCreateCommandPool(dev, &pci, nullptr, &cmdPool) != VK_SUCCESS) {
            vkFreeMemory(dev, stagingMem, nullptr);
            vkDestroyBuffer(dev, staging, nullptr);
            return false;
        }
    }
    VkCommandBuffer cb = VK_NULL_HANDLE;
    {
        VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbai.commandPool        = cmdPool;
        cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(dev, &cbai, &cb) != VK_SUCCESS) {
            vkDestroyCommandPool(dev, cmdPool, nullptr);
            vkFreeMemory(dev, stagingMem, nullptr);
            vkDestroyBuffer(dev, staging, nullptr);
            return false;
        }
    }
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cb, &bi) != VK_SUCCESS) {
        vkDestroyCommandPool(dev, cmdPool, nullptr);
        vkFreeMemory(dev, stagingMem, nullptr);
        vkDestroyBuffer(dev, staging, nullptr);
        return false;
    }

    // -- Step 7a: layout transition UNDEFINED -> TRANSFER_DST ---------------
    VkImageMemoryBarrier barr{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barr.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
    barr.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barr.srcAccessMask    = 0;
    barr.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
    barr.image            = _decodeImage;
    barr.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barr);

    // -- Step 6b: vkCmdCopyBufferToImage ------------------------------------
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset      = 0;
    copyRegion.bufferRowLength   = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageOffset       = {0, 0, 0};
    copyRegion.imageExtent       = {w, h, 1};
    vkCmdCopyBufferToImage(cb, staging, _decodeImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // -- Step 7b: layout transition TRANSFER_DST -> SHADER_READ_ONLY -------
    barr.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barr.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barr);

    vkEndCommandBuffer(cb);

    // -- Step 8: submit + wait fence ----------------------------------------
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(dev, &fci, nullptr, &fence) != VK_SUCCESS) {
        vkDestroyCommandPool(dev, cmdPool, nullptr);
        vkFreeMemory(dev, stagingMem, nullptr);
        vkDestroyBuffer(dev, staging, nullptr);
        return false;
    }
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cb;
    if (vkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS) {
        vkDestroyFence(dev, fence, nullptr);
        vkDestroyCommandPool(dev, cmdPool, nullptr);
        vkFreeMemory(dev, stagingMem, nullptr);
        vkDestroyBuffer(dev, staging, nullptr);
        return false;
    }
    vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);

    // -- Step 9: free staging -----------------------------------------------
    vkDestroyFence(dev, fence, nullptr);
    vkDestroyCommandPool(dev, cmdPool, nullptr);
    vkFreeMemory(dev, stagingMem, nullptr);
    vkDestroyBuffer(dev, staging, nullptr);

    // -- Step 10: register with ImGui ---------------------------------------
    if (_decodeImguiTex != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(_decodeImguiTex);
        _decodeImguiTex = VK_NULL_HANDLE;
    }
    _decodeImguiTex = ImGui_ImplVulkan_AddTexture(
        _previewSampler, _decodeImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // -- Step 11: update size cache -----------------------------------------
    _decodeUploadedW = w;
    _decodeUploadedH = h;
    return true;
}

void DebugUI::buildMemoryViewerPanel() {
    ImGui::Begin("Memory Viewer");
    if (!_paused || !_snapshot) {
        ImGui::Text("Not paused.");
        ImGui::End();
        return;
    }

    // Refresh FB readback cache if user just selected FB region or step changed.
    if (_memViewRegion == MemRegion::Vdp1Fb && !_fbReadbackValid) {
        refreshFbReadback();
    }

    // --- Region selector ----------------------------------------------------
    const char* kRegionNames[] = {
        "VDP1 VRAM (512KB)",
        "VDP2 CRAM (4KB)",
        "VDP1 Offscreen FB",
    };
    int regionIdx = static_cast<int>(_memViewRegion);
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::Combo("Region", &regionIdx, kRegionNames, IM_ARRAYSIZE(kRegionNames))) {
        _memViewRegion = static_cast<MemRegion>(regionIdx);
        _memViewAddr   = 0;
        _memFbClickX = _memFbClickY = -1;
        _memViewScrollPending = true;
    }

    // --- Resolve current region buffer + size ------------------------------
    const uint8_t* base = nullptr;
    size_t         size = 0;
    switch (_memViewRegion) {
    case MemRegion::Vdp1Vram:
        base = _snapshot->vram.data(); size = _snapshot->vram.size(); break;
    case MemRegion::Vdp2Cram:
        base = _snapshot->cram.data(); size = _snapshot->cram.size(); break;
    case MemRegion::Vdp1Fb:
        if (_fbReadbackValid) {
            base = _fbReadback.pixels().data();
            size = _fbReadback.pixels().size();
        }
        break;
    }
    if (base == nullptr || size == 0) {
        ImGui::TextDisabled("(region empty or FB readback not yet performed)");
        ImGui::End();
        return;
    }

    // --- Address input ------------------------------------------------------
    char addrBuf[16];
    snprintf(addrBuf, sizeof(addrBuf), "%08X", _memViewAddr);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputText("Address (hex)", addrBuf, sizeof(addrBuf),
                         ImGuiInputTextFlags_CharsHexadecimal |
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        uint32_t parsed = 0;
        sscanf(addrBuf, "%x", &parsed);
        if (parsed >= size) parsed = static_cast<uint32_t>(size - 1);
        _memViewAddr = parsed;
        _memViewScrollPending = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    int bpr = 1;  // default index = 16 bytes per row
    if (_memViewBytesPerRow == 8)       bpr = 0;
    else if (_memViewBytesPerRow == 32) bpr = 2;
    if (ImGui::Combo("##bpr", &bpr, "8\0 16\0 32\0", 3)) {
        static const int kVals[] = {8, 16, 32};
        _memViewBytesPerRow = kVals[bpr];
    }
    ImGui::SameLine();
    ImGui::TextDisabled("size = 0x%X (%zu B)", static_cast<unsigned>(size), size);

    // --- Jump shortcuts ----------------------------------------------------
    {
        const int target = (_selectedCmdIndex >= 0) ? _selectedCmdIndex : _stepN;
        const bool hasCmd = target >= 0 &&
                            target < static_cast<int>(_snapshot->rawCmds.size());
        if (!hasCmd) ImGui::BeginDisabled();
        if (ImGui::Button("Jump: cmd CMDSRCA")) {
            const auto& rc = _snapshot->rawCmds[target];
            _memViewRegion = MemRegion::Vdp1Vram;
            _memViewAddr   = static_cast<uint32_t>(rc.CMDSRCA) * 8u;
            _memDecodeMode = static_cast<int>((rc.CMDPMOD >> 3) & 0x7u);
            _memDecodeWidth  = static_cast<uint32_t>(((rc.CMDSIZE >> 8) & 0x3Fu) * 8u);
            _memDecodeHeight = static_cast<uint32_t>(rc.CMDSIZE & 0xFFu);
            _memDecodeLutAddr = (_memDecodeMode == 1)
                ? static_cast<uint32_t>(rc.CMDCOLR) * 8u : 0u;
            _memViewScrollPending = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Jump: cmd CMDCOLR (LUT)")) {
            const auto& rc = _snapshot->rawCmds[target];
            _memViewRegion = MemRegion::Vdp1Vram;
            _memViewAddr   = static_cast<uint32_t>(rc.CMDCOLR) * 8u;
            _memDecodeLutAddr = _memViewAddr;
            _memViewScrollPending = true;
        }
        if (!hasCmd) ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(target = cmd %d)",
                            (_selectedCmdIndex >= 0) ? _selectedCmdIndex : _stepN);
    }

    ImGui::Separator();

    // --- Hex view (scrollable) ---------------------------------------------
    const int rowBytes = _memViewBytesPerRow;
    const int totalRows = static_cast<int>((size + rowBytes - 1) / rowBytes);
    ImGui::BeginChild("##memhex", ImVec2(0, -200.0f), true);
    if (_memViewScrollPending) {
        // Scroll the row containing _memViewAddr to ~1/4 down from the top
        // so the target stays in context (not pinned to the top edge).
        const int   targetRow = static_cast<int>(_memViewAddr / rowBytes);
        const float rowH      = ImGui::GetTextLineHeightWithSpacing();
        const float viewH     = ImGui::GetWindowHeight();
        const float marginY   = viewH * 0.25f;
        float       scrollY   = static_cast<float>(targetRow) * rowH - marginY;
        if (scrollY < 0.0f) scrollY = 0.0f;
        ImGui::SetScrollY(scrollY);
        _memViewScrollPending = false;
    }
    ImGuiListClipper clipper;
    clipper.Begin(totalRows);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const size_t rowAddr = static_cast<size_t>(row) * rowBytes;
            ImGui::Text("%08zX  ", rowAddr);
            for (int i = 0; i < rowBytes; ++i) {
                const size_t a = rowAddr + i;
                ImGui::SameLine(0.0f, 0.0f);
                if (a < size) {
                    if (_memViewRegion == MemRegion::Vdp1Fb) {
                        // FB: each pixel = 4 bytes. Click selects pixel.
                        char lbl[24];
                        snprintf(lbl, sizeof(lbl), " %02X##b%zu", base[a], a);
                        if (ImGui::SmallButton(lbl)) {
                            const size_t pix = a / 4;
                            _memFbClickX = static_cast<int>(pix % _fbReadback.width());
                            _memFbClickY = static_cast<int>(pix / _fbReadback.width());
                        }
                    } else {
                        ImGui::Text(" %02X", base[a]);
                    }
                } else {
                    ImGui::TextDisabled("   ");
                }
            }
            // ASCII column.
            ImGui::SameLine(0.0f, 12.0f);
            for (int i = 0; i < rowBytes; ++i) {
                const size_t a = rowAddr + i;
                if (a >= size) break;
                const char c = (base[a] >= 32 && base[a] < 127)
                    ? static_cast<char>(base[a]) : '.';
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::Text("%c", c);
            }
        }
    }
    clipper.End();
    ImGui::EndChild();

    // --- Decoded preview (collapsible) ------------------------------------
    if (ImGui::CollapsingHeader("Decoded", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* kModes[] = {
            "Hex only", "4bpp Bank", "4bpp LUT",
            "8bpp 64-bank", "8bpp 128-bank", "8bpp 256-bank",
            "16bpp RGB"
        };
        int modeIdx = (_memDecodeMode < 0) ? 0 : (_memDecodeMode + 1);
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::Combo("Mode", &modeIdx, kModes, IM_ARRAYSIZE(kModes))) {
            _memDecodeMode = (modeIdx == 0) ? -1 : (modeIdx - 1);
        }
        ImGui::SameLine();
        int wInt = static_cast<int>(_memDecodeWidth);
        int hInt = static_cast<int>(_memDecodeHeight);
        ImGui::SetNextItemWidth(60.0f);
        if (ImGui::InputInt("W", &wInt, 0, 0)) {
            _memDecodeWidth  = static_cast<uint32_t>((wInt < 0) ? 0 : wInt);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        if (ImGui::InputInt("H", &hInt, 0, 0)) {
            _memDecodeHeight = static_cast<uint32_t>((hInt < 0) ? 0 : hInt);
        }

        if (_memDecodeMode >= 0 && _memDecodeWidth > 0 && _memDecodeHeight > 0) {
            std::vector<uint32_t> rgba;
            const uint8_t* vram = _snapshot->vram.data();
            const size_t   vsz  = _snapshot->vram.size();
            const uint8_t* cram = _snapshot->cram.data();
            const size_t   csz  = _snapshot->cram.size();
            const uint32_t srcA = _memViewAddr;
            const uint32_t W = _memDecodeWidth;
            const uint32_t H = _memDecodeHeight;
            switch (_memDecodeMode) {
            case 0: rgba = memdecode::decode4bppBank(vram, vsz, srcA, W, H,
                            0xFFF0u, cram, csz, true, true); break;
            case 1: rgba = memdecode::decode4bppLut (vram, vsz, srcA, W, H,
                            _memDecodeLutAddr, cram, csz, true, true); break;
            case 2: rgba = memdecode::decode8bppBank(vram, vsz, srcA, W, H,
                            0xFFC0u, 0x3Fu, cram, csz, true, true); break;
            case 3: rgba = memdecode::decode8bppBank(vram, vsz, srcA, W, H,
                            0xFF80u, 0x7Fu, cram, csz, true, true); break;
            case 4: rgba = memdecode::decode8bppBank(vram, vsz, srcA, W, H,
                            0xFF00u, 0xFFu, cram, csz, true, true); break;
            case 5: rgba = memdecode::decode16bppRGB(vram, vsz, srcA, W, H, true); break;
            default: break;
            }
            const uint64_t key = makeDecodeCacheKey(
                static_cast<int>(_memViewRegion), _memViewAddr,
                _memDecodeMode, W, H, _memDecodeLutAddr);
            if (key != _decodeUploadedKey) {
                if (uploadDecodedTexture(rgba, W, H)) {
                    _decodeUploadedKey = key;
                }
            }
            if (_decodeImguiTex != VK_NULL_HANDLE) {
                const float disp = 4.0f;
                ImGui::Image(reinterpret_cast<ImTextureID>(_decodeImguiTex),
                             ImVec2(W * disp, H * disp));
            }
            // LUT swatches (4bpp LUT mode only -- 16 entries).
            if (_memDecodeMode == 1 && _memDecodeLutAddr > 0) {
                ImGui::Separator();
                ImGui::Text("LUT @ 0x%05X (16 entries):", _memDecodeLutAddr);
                auto entries = memdecode::readLut(
                    _snapshot->vram.data(), _snapshot->vram.size(),
                    _memDecodeLutAddr, 16);
                for (size_t i = 0; i < entries.size(); ++i) {
                    const uint16_t w = entries[i];
                    ImVec4 col;
                    if (w & 0x8000u) {
                        // Direct RGB.
                        const float r = float((w      ) & 0x1Fu) / 31.0f;
                        const float g = float((w >>  5) & 0x1Fu) / 31.0f;
                        const float b = float((w >> 10) & 0x1Fu) / 31.0f;
                        col = ImVec4(r, g, b, 1.0f);
                    } else {
                        // Palette index -> CRAM lookup.
                        const size_t addr =
                            static_cast<size_t>(w & 0x7FFu) * 2;
                        uint16_t cw = 0;
                        if (addr + 1 < _snapshot->cram.size()) {
                            cw = static_cast<uint16_t>(
                                (static_cast<uint16_t>(_snapshot->cram[addr]) << 8) |
                                _snapshot->cram[addr + 1]);
                        }
                        const float r = float((cw      ) & 0x1Fu) / 31.0f;
                        const float g = float((cw >>  5) & 0x1Fu) / 31.0f;
                        const float b = float((cw >> 10) & 0x1Fu) / 31.0f;
                        col = ImVec4(r, g, b, 1.0f);
                    }
                    if ((i % 8) != 0) ImGui::SameLine();
                    char tip[64];
                    snprintf(tip, sizeof(tip),
                             "[%zu]\n0x%04X\n%s", i, w,
                             (w & 0x8000u) ? "direct" : "palette");
                    ImGui::ColorButton(tip, col,
                        ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip,
                        ImVec2(28, 28));
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
                }
            }
        } else {
            ImGui::TextDisabled("(set Mode + W + H to render)");
        }
        // FB pixel detail (active only when in FB region with a clicked pixel).
        if (_memViewRegion == MemRegion::Vdp1Fb &&
            _memFbClickX >= 0 && _memFbClickY >= 0 &&
            _fbReadbackValid) {
            ImGui::Separator();
            const uint32_t fbw = _fbReadback.width();
            const size_t pixIdx =
                static_cast<size_t>(_memFbClickY) * fbw + _memFbClickX;
            const uint8_t* p = _fbReadback.pixels().data() + pixIdx * 4;
            const uint32_t rgba =
                static_cast<uint32_t>(p[0]) |
                (static_cast<uint32_t>(p[1]) <<  8) |
                (static_cast<uint32_t>(p[2]) << 16) |
                (static_cast<uint32_t>(p[3]) << 24);
            const auto d = memdecode::decodeVdp1Color(rgba);
            ImGui::Text("FB pixel (%d, %d) = 0x%02X_%02X_%02X_%02X",
                        _memFbClickX, _memFbClickY, p[3], p[2], p[1], p[0]);
            ImGui::Text("  C=%u colorcl=%u priority=%u",
                        d.c, d.colorcl, d.priority);
            ImGui::Text("  shadow=%u sprite_window=%u",
                        d.shadow, d.sprite_window);
            ImGui::Text("  colorindex=0x%04X (%u)",
                        d.colorindex, d.colorindex);
        }
    }

    ImGui::End();
}

void DebugUI::refreshFbReadback() {
    if (!_paused || !_snapshot || !_vid || !_renderer) return;
    if (_fbReadbackValid &&
        _fbReadbackFrameId == _snapshot->frameId &&
        _fbReadbackStepN   == _stepN) {
        return;  // cache still valid
    }
    VkImage img = _vid->getVdp1OffscreenImage();
    if (img == VK_NULL_HANDLE) return;
    // Capture at the PHYSICAL offscreen image dimensions, not the
    // _snapshot->fbWidth/Height which is a logical Saturn-bbox space in
    // tessellation mode (vdp2width x vdp2height) and would skip the bulk
    // of the image at HD upscale. Compute mode's logical space matches
    // the physical image, so behavior is unchanged there.
    Vdp1Renderer* rend = _vid->getVdp1Renderer();
    const uint32_t W = rend
        ? static_cast<uint32_t>(rend->getFramebufferWidth())
        : static_cast<uint32_t>(_snapshot->fbWidth);
    const uint32_t H = rend
        ? static_cast<uint32_t>(rend->getFramebufferHeight())
        : static_cast<uint32_t>(_snapshot->fbHeight);
    if (_fbReadback.capture(
            _renderer->GetVulkanDevice(),
            _renderer->GetVulkanPhysicalDevice(),
            _renderer->GetVulkanQueue(),
            _renderer->GetVulkanGraphicsQueueFamilyIndex(),
            img,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            W, H)) {
        _fbReadbackValid   = true;
        _fbReadbackFrameId = _snapshot->frameId;
        _fbReadbackStepN   = _stepN;
    }
}

void DebugUI::dispatchGraphicsUpTo(int stepN) {
  if (!_paused || _snapshot == nullptr || !_available) return;
  Vdp1Renderer* rend = _vid ? _vid->getVdp1Renderer() : nullptr;
  if (rend == nullptr) return;
  // Negative stepN clamps to 0 (display nothing meaningful but still safe).
  if (stepN < 0) stepN = 0;
  const int totalRaw = static_cast<int>(_snapshot->rawCmds.size());
  if (stepN >= totalRaw) stepN = totalRaw - 1;
  if (totalRaw == 0) return;

  // Backup live globals. Vdp1Ram is a 512KB heap allocation owned by the
  // C core (T1MemoryInit); we cannot pointer-swap because external code
  // may read through the live pointer mid-call. memcpy on/off is the only
  // safe option.
  static thread_local std::vector<uint8_t> s_savedVram;
  if ((int)s_savedVram.size() != (int)_snapshot->vram.size()) {
    s_savedVram.resize(_snapshot->vram.size());
  }
  std::memcpy(s_savedVram.data(), Vdp1Ram, _snapshot->vram.size());
  Vdp1 savedRegs = *Vdp1Regs;

  // Install snapshot data into the globals the renderer reads from.
  std::memcpy(Vdp1Ram, _snapshot->vram.data(), _snapshot->vram.size());
  *Vdp1Regs = _snapshot->regs;

  // Tell Vdp1Renderer::drawStart() to skip its trailing Vdp1DrawCommands
  // auto-walk; we drive the dispatch manually below with a step limit.
  rend->setSkipAutoCommandWalk(true);

  // Clear the offscreen image first. The render pass uses LOAD_OP_LOAD,
  // so without this the previous live frame (or previous step replay)
  // remains underneath -- step 0 would not show "almost nothing", it
  // would show "the previous full frame".
  rend->clearCurrentOffscreenForReplay();

  // drawStart resets per-frame state and emits the system/user clip
  // pseudo-polygons. Without this, polygon/sprite draws would sit on top
  // of stale state from the previous live frame and the system clip would
  // not be honored.
  VIDCore->Vdp1DrawStart();

  // Bounded walker. Mirrors the C core's Vdp1DrawCommands loop in
  // src/vdp1.cpp but stops after stepN+1 iterations regardless of jump
  // mode. We dispatch via VIDCore->Vdp1*Draw so the registered Vulkan
  // entry points (which read Vdp1Regs->addr / Vdp1Ram globals internally)
  // see the snapshot we just installed. Skipped commands (CMDCTRL bit14)
  // count toward the step like the live code does.
  {
    constexpr size_t kCmdSize = 0x20;
    constexpr size_t kAddrCap = 0x7FFE0;
    uint32_t addr       = 0;
    uint32_t returnAddr = 0xffffffff;
    int      processed  = 0;
    const uint8_t* ram = _snapshot->vram.data();
    const size_t   ramSize = _snapshot->vram.size();
    while (addr + kCmdSize <= ramSize && processed <= stepN) {
      // Big-endian word read. T1ReadWord in the C core is the same byte
      // order; we replicate inline so we don't need its dependencies.
      uint16_t command = static_cast<uint16_t>(
          (uint16_t)ram[addr] << 8 | ram[addr + 1]);
      if (command & 0x8000) break;
      if ((command & 0x000C) == 0x000C) break;

      // Set the global addr so Vdp1ReadCommand inside the draw functions
      // pulls the right command from the snapshot's VRAM bytes.
      Vdp1Regs->addr = addr;

      if (!(command & 0x4000)) {
        switch (command & 0x000F) {
        case 0:  VIDCore->Vdp1NormalSpriteDraw   (Vdp1Ram, Vdp1Regs, NULL); break;
        case 1:  VIDCore->Vdp1ScaledSpriteDraw   (Vdp1Ram, Vdp1Regs, NULL); break;
        case 2:
        case 3:  VIDCore->Vdp1DistortedSpriteDraw(Vdp1Ram, Vdp1Regs, NULL); break;
        case 4:  VIDCore->Vdp1PolygonDraw        (Vdp1Ram, Vdp1Regs, NULL); break;
        case 5:
        case 7:  VIDCore->Vdp1PolylineDraw       (Vdp1Ram, Vdp1Regs, NULL); break;
        case 6:  VIDCore->Vdp1LineDraw           (Vdp1Ram, Vdp1Regs, NULL); break;
        case 8:
        case 11: VIDCore->Vdp1UserClipping       (Vdp1Ram, Vdp1Regs);       break;
        case 9:  VIDCore->Vdp1SystemClipping     (Vdp1Ram, Vdp1Regs);       break;
        case 10: VIDCore->Vdp1LocalCoordinate    (Vdp1Ram, Vdp1Regs);       break;
        default: /* bad command: stop walking */ addr = static_cast<uint32_t>(kAddrCap + 1); break;
        }
      }

      ++processed;

      // Jump mode (CMDCTRL bits 13-12). Identical to the C core's table.
      switch ((command & 0x3000) >> 12) {
      case 0:
        addr += static_cast<uint32_t>(kCmdSize);
        break;
      case 1:
        addr = static_cast<uint32_t>(
            ((uint16_t)ram[addr + 2] << 8 | ram[addr + 3])) * 8;
        if (addr == 0) {
          // BAD jump-to-0 -- abort (matches C core safeguard).
          addr = static_cast<uint32_t>(kAddrCap + 1);
        }
        break;
      case 2:
        if (returnAddr == 0xffffffff)
          returnAddr = addr + static_cast<uint32_t>(kCmdSize);
        addr = static_cast<uint32_t>(
            ((uint16_t)ram[addr + 2] << 8 | ram[addr + 3])) * 8;
        break;
      case 3:
        if (returnAddr != 0xffffffff) {
          addr = returnAddr;
          returnAddr = 0xffffffff;
        } else {
          addr += static_cast<uint32_t>(kCmdSize);
        }
        break;
      }
      if (addr > kAddrCap) break;
    }
  }

  // drawEnd submits the recorded VDP1 render pass to the GPU.
  VIDCore->Vdp1DrawEnd();

  // Wait for the GPU to finish so the offscreen image is fully written
  // before the swapchain present samples it. Pause is single-shot, so
  // device-wide idle is acceptable here.
  vkDeviceWaitIdle(_renderer->GetVulkanDevice());

  // Restore live globals before returning. Skip-flag also goes back to
  // false so the next live frame's drawStart() walks normally.
  rend->setSkipAutoCommandWalk(false);
  std::memcpy(Vdp1Ram, s_savedVram.data(), s_savedVram.size());
  *Vdp1Regs = savedRegs;
}
