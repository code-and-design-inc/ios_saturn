// Pocket Arcade Sega Saturn bridge.
//
// This file is the whole "port" of Yaba Sanshiro to Pocket Arcade: it supplies
// the core-list tables and Yui* callbacks the emulator expects from its
// front-end, a lock-free ring-buffer sound core, and the C ABI in
// PASaturnBridge.h. The upstream tree is compiled unmodified.
//
// Copyright (C) 2026 Code and Design Inc.
// Derived in part from Yaba Sanshiro / Yabause (GPL-2.0-or-later); this file is
// distributed under the same terms. See ../COPYING and THIRD-PARTY-NOTICES.md.

#include "PASaturnBridge.h"

extern "C" {
#include "config.h"
#include "yabause.h"
#include "core.h"
#include "memory.h"
#include "scsp.h"
#include "vidsoft.h"
#include "vidogl.h"
#include "peripheral.h"
#include "m68kcore.h"
#include "sh2core.h"
#include "sh2int.h"
#include "cdbase.h"
#include "cs2.h"
#include "vdp1.h"
#include "vdp2.h"
#include "osdcore.h"
#include "ygl.h"
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Front-end tables the core links against
// ---------------------------------------------------------------------------

#define PASATURN_SNDCORE_RING 20

static int SNDRingInit(void);
static void SNDRingDeInit(void);
static int SNDRingReset(void);
static int SNDRingChangeVideoFormat(int vertfreq);
static void SNDRingUpdateAudio(u32* leftchanbuffer, u32* rightchanbuffer, u32 num_samples);
static u32 SNDRingGetAudioSpace(void);
static void SNDRingMuteAudio(void);
static void SNDRingUnMuteAudio(void);
static void SNDRingSetVolume(int volume);

static SoundInterface_struct SNDRing = {
    PASATURN_SNDCORE_RING,
    "Pocket Arcade ring buffer",
    SNDRingInit,
    SNDRingDeInit,
    SNDRingReset,
    SNDRingChangeVideoFormat,
    SNDRingUpdateAudio,
    SNDRingGetAudioSpace,
    SNDRingMuteAudio,
    SNDRingUnMuteAudio,
    SNDRingSetVolume,
#ifdef USE_SCSPMIDI
    NULL, NULL, NULL,
#endif
};

extern "C" {

SH2Interface_struct* SH2CoreList[] = {
    &SH2Interpreter,
    &SH2DebugInterpreter,
    NULL,
};

PerInterface_struct* PERCoreList[] = {
    &PERDummy,
    NULL,
};

CDInterface* CDCoreList[] = {
    &DummyCD,
    &ISOCD,
    NULL,
};

SoundInterface_struct* SNDCoreList[] = {
    &SNDDummy,
    &SNDRing,
    NULL,
};

VideoInterface_struct* VIDCoreList[] = {
    &VIDDummy,
    &VIDSoft,
    NULL,
};

M68K_struct* M68KCoreList[] = {
    &M68KDummy,
    &M68KMusashi,
    NULL,
};

// The OpenGL video core is not compiled in; vdp2.cpp still references its
// draw entry points by name when restoring after a skipped frame, but only
// takes that branch when the active core is VIDCORE_OGL.
void VIDOGLVdp2DrawStart(void) {}
void VIDOGLVdp2DrawEnd(void) {}
void VIDOGLVdp2DrawScreens(void) {}

// Declared in ygl.h and implemented by vidogl.c upstream; the software renderer
// calls it for line-colour and VDP1 palette look-ups. Same behaviour as the
// upstream implementation.
u32 Vdp2ColorRamGetColor(u32 colorindex, int alpha)
{
    switch (Vdp2Internal.ColorMode) {
    case 0:
    case 1: {
        colorindex <<= 1;
        u32 tmp = T2ReadWord(Vdp2ColorRam, colorindex & 0xFFF);
        return SAT2YAB1(alpha, tmp);
    }
    case 2: {
        colorindex <<= 2;
        colorindex &= 0xFFF;
        u32 tmp1 = T2ReadWord(Vdp2ColorRam, colorindex);
        u32 tmp2 = T2ReadWord(Vdp2ColorRam, colorindex + 2);
        return SAT2YAB2(alpha, tmp1, tmp2);
    }
    default:
        break;
    }
    return 0;
}

char* getLastShaderError(void) { return NULL; }

// RetroAchievements progress is not part of this port; the state format keeps
// a zero-length RA block.
size_t YabauseRA_GetProgressSize(void) { return 0; }
int YabauseRA_SerializeProgress(uint8_t*, size_t) { return 0; }
int YabauseRA_DeserializeProgress(const uint8_t*, size_t) { return 0; }

// PlayRecorder hooks (test-replay feature of the Qt front-end).
int YabauseThread_IsUseBios(void) { return 1; }
const char* YabauseThread_getBackupPath(void) { return ""; }
void YabauseThread_setUseBios(int) {}
void YabauseThread_setBackupPath(const char*) {}
void YabauseThread_resetPlaymode(void) {}
void YabauseThread_coldBoot(void) {}

int YuiRevokeOGLOnThisThread(void) { return 0; }
int YuiUseOGLOnThisThread(void) { return 0; }

int yprintf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

} // extern "C"

// ---------------------------------------------------------------------------
// Session state
// ---------------------------------------------------------------------------

namespace {

constexpr int32_t kAudioSampleRate = 44100;
constexpr uint32_t kAudioRingFrames = 1u << 15; // 32768 stereo frames (~0.74 s)
constexpr uint32_t kAudioRingMask = kAudioRingFrames - 1;

struct AudioRing {
    int16_t samples[kAudioRingFrames * 2];
    std::atomic<uint32_t> write{0};
    std::atomic<uint32_t> read{0};
    std::atomic<uint64_t> written{0};
    std::atomic<uint64_t> overruns{0};

    uint32_t available() const {
        return write.load(std::memory_order_acquire) - read.load(std::memory_order_acquire);
    }
    uint32_t space() const { return kAudioRingFrames - available(); }
    void reset() {
        write.store(0, std::memory_order_relaxed);
        read.store(0, std::memory_order_relaxed);
    }
};

// Wall-clock instrumentation. The renderer buckets wrap the VIDSoft entry
// points through the VideoInterface_struct function table (vdp1.cpp/vdp2.cpp
// call them via VIDCore), so the upstream renderer stays untouched.
struct PerfCounters {
    uint64_t frameNanosTotal = 0;
    uint64_t frameNanosMax = 0;
    uint64_t vdp1Nanos = 0;
    uint64_t vdp2Nanos = 0;
    uint64_t presentNanos = 0;
    static constexpr int kRecent = 256;
    uint64_t recent[kRecent] = {};
    int recentIndex = 0;
    int recentCount = 0;

    void recordFrame(uint64_t ns) {
        frameNanosTotal += ns;
        if (ns > frameNanosMax) frameNanosMax = ns;
        recent[recentIndex] = ns;
        recentIndex = (recentIndex + 1) % kRecent;
        if (recentCount < kRecent) recentCount++;
    }
    uint64_t recentP95() const {
        if (recentCount == 0) return 0;
        uint64_t sorted[kRecent];
        std::copy(recent, recent + recentCount, sorted);
        std::sort(sorted, sorted + recentCount);
        return sorted[(recentCount * 95) / 100 >= recentCount ? recentCount - 1 : (recentCount * 95) / 100];
    }
};

inline uint64_t nowNanos()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Session {
    std::string biosPath;
    std::string discPath;
    std::string backupPath;
    std::vector<uint32_t> frame;
    int32_t frameWidth = 0;
    int32_t frameHeight = 0;
    uint64_t frameSequence = 0;
    bool frameDirtyThisRun = false;
    uint64_t emulatedFrames = 0;
    uint64_t renderedFrames = 0;
    bool muted = false;
    PerfCounters perf;
};

Session* g_session = nullptr;
AudioRing g_audio;
std::string g_lastError;
std::mutex g_errorMutex;
char g_lastErrorBuffer[1024];

void setLastError(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_errorMutex);
    g_lastError = message;
}

const char* lastErrorCString()
{
    std::lock_guard<std::mutex> lock(g_errorMutex);
    strncpy(g_lastErrorBuffer, g_lastError.c_str(), sizeof(g_lastErrorBuffer) - 1);
    g_lastErrorBuffer[sizeof(g_lastErrorBuffer) - 1] = 0;
    return g_lastErrorBuffer;
}

// Timing wrappers around the software renderer's entry points.
void (*g_origVdp1DrawStart)(void) = nullptr;
void (*g_origVdp2DrawScreens)(void) = nullptr;
void (*g_origVdp2DrawEnd)(void) = nullptr;
static void dumpVdp1CommandList(const char* when);
void (*g_origSync)(void) = nullptr;
void loggedSync(void) { dumpVdp1CommandList("vblank-in"); if (g_origSync) g_origSync(); }
void (*g_origVdp1EraseWrite)(void) = nullptr;
void (*g_origVdp1FrameChange)(void) = nullptr;
uint64_t g_vdp1DrawStarts = 0, g_vdp1EraseWrites = 0, g_vdp1FrameChanges = 0;

void countedVdp1EraseWrite(void) { g_vdp1EraseWrites++; g_origVdp1EraseWrite(); }
void countedVdp1FrameChange(void) { g_vdp1FrameChanges++; g_origVdp1FrameChange(); }

// Debug aid (PASATURN_DUMP_VDP1=<first frame>): print the VDP1 command list
// walked from address 0 at each plot trigger, one line per command.
static void dumpVdp1CommandList(const char* when)
{
    static long firstFrame = -2;
    if (firstFrame == -2) {
        const char* env = getenv("PASATURN_DUMP_VDP1");
        firstFrame = env ? atol(env) : -1;
    }
    if (firstFrame < 0 || !g_session || (long)g_session->emulatedFrames < firstFrame) return;
    u32 addr = 0;
    u32 returnAddr = 0xFFFFFFFF;
    printf("[vdp1 list @frame %llu %s]\n", (unsigned long long)g_session->emulatedFrames, when);
    for (int n = 0; n < 4096; ++n) {
        u16 cmd = T1ReadWord(Vdp1Ram, addr);
        u16 link = T1ReadWord(Vdp1Ram, addr + 2);
        u16 pmod = T1ReadWord(Vdp1Ram, addr + 4);
        u16 colr = T1ReadWord(Vdp1Ram, addr + 6);
        s16 xa = (s16)T1ReadWord(Vdp1Ram, addr + 0x0C), ya = (s16)T1ReadWord(Vdp1Ram, addr + 0x0E);
        s16 xc = (s16)T1ReadWord(Vdp1Ram, addr + 0x14), yc = (s16)T1ReadWord(Vdp1Ram, addr + 0x16);
        u16 srca = T1ReadWord(Vdp1Ram, addr + 8), size = T1ReadWord(Vdp1Ram, addr + 0xA);
        printf("  %05X cmd=%04X%s pmod=%04X colr=%04X srca=%04X size=%04X A=(%d,%d) C=(%d,%d)\n", addr, cmd,
               (cmd & 0x4000) ? " SKIP" : "", pmod, colr, srca, size, xa, ya, xc, yc);
        if (cmd & 0x8000) break;
        switch ((cmd & 0x3000) >> 12) {
        case 0: addr += 0x20; break;
        case 1: addr = link * 8; break;
        case 2: if (returnAddr == 0xFFFFFFFF) { returnAddr = addr + 0x20; addr = link * 8; } else addr += 0x20; break;
        case 3: if (returnAddr != 0xFFFFFFFF) { addr = returnAddr; returnAddr = 0xFFFFFFFF; } else addr += 0x20; break;
        }
        if (addr > 0x7FFFF) break;
    }
}

void timedVdp1DrawStart(void)
{
    g_vdp1DrawStarts++;
    if (Vdp1External.status == VDP1_STATUS_IDLE || Vdp1Regs->addr == 0) dumpVdp1CommandList("draw-start");
    const uint64_t t0 = nowNanos();
    g_origVdp1DrawStart();
    if (g_session) g_session->perf.vdp1Nanos += nowNanos() - t0;
}

void timedVdp2DrawScreens(void)
{
    const uint64_t t0 = nowNanos();
    g_origVdp2DrawScreens();
    if (g_session) g_session->perf.vdp2Nanos += nowNanos() - t0;
}

void timedVdp2DrawEnd(void)
{
    const uint64_t t0 = nowNanos();
    g_origVdp2DrawEnd();
    if (g_session) g_session->perf.presentNanos += nowNanos() - t0;
}

void installRendererTimers()
{
    if (!g_origVdp1DrawStart) {
        g_origVdp1DrawStart = VIDSoft.Vdp1DrawStart;
        g_origVdp2DrawScreens = VIDSoft.Vdp2DrawScreens;
        g_origVdp2DrawEnd = VIDSoft.Vdp2DrawEnd;
        g_origVdp1EraseWrite = VIDSoft.Vdp1EraseWrite;
        g_origVdp1FrameChange = VIDSoft.Vdp1FrameChange;
        g_origSync = VIDSoft.Sync;
    }
    VIDSoft.Sync = loggedSync;
    VIDSoft.Vdp1EraseWrite = countedVdp1EraseWrite;
    VIDSoft.Vdp1FrameChange = countedVdp1FrameChange;
    VIDSoft.Vdp1DrawStart = timedVdp1DrawStart;
    VIDSoft.Vdp2DrawScreens = timedVdp2DrawScreens;
    VIDSoft.Vdp2DrawEnd = timedVdp2DrawEnd;
}

void attachPads()
{
    PerPortReset();
    for (int player = 0; player < 2; ++player) {
        void* pad = PerPadAdd(player == 0 ? &PORTDATA1 : &PORTDATA2);
        const u8 names[] = {
            PERPAD_UP, PERPAD_RIGHT, PERPAD_DOWN, PERPAD_LEFT,
            PERPAD_RIGHT_TRIGGER, PERPAD_LEFT_TRIGGER, PERPAD_START,
            PERPAD_A, PERPAD_B, PERPAD_C, PERPAD_X, PERPAD_Y, PERPAD_Z,
        };
        for (u8 name : names) {
            PerSetKey(((u32)player << 24) | name, name, pad);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Yui callbacks
// ---------------------------------------------------------------------------

extern "C" {

void YuiErrorMsg(const char* string)
{
    if (string) {
        setLastError(string);
        fprintf(stderr, "[SaturnCore] %s\n", string);
    }
}

// Called by the software renderer once per rendered frame, on the emulation
// thread, after TitanRender wrote dispbuffer.
void YuiSwapBuffers(void)
{
    Session* s = g_session;
    if (!s || !dispbuffer) {
        return;
    }
    int width = 0, height = 0;
    VIDSoft.GetGlSize(&width, &height);
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        return;
    }
    const size_t count = (size_t)width * (size_t)height;
    if (s->frame.size() != count) {
        s->frame.resize(count);
    }
    const uint32_t* src = (const uint32_t*)dispbuffer;
    uint32_t* dst = s->frame.data();
    for (size_t i = 0; i < count; ++i) {
        dst[i] = src[i] | 0xFF000000u; // force opaque; layout is R,G,B,A in memory
    }
    s->frameWidth = width;
    s->frameHeight = height;
    s->frameSequence++;
    s->renderedFrames++;
    s->frameDirtyThisRun = true;
}

} // extern "C"

// ---------------------------------------------------------------------------
// Sound core
// ---------------------------------------------------------------------------

static int SNDRingInit(void)
{
    g_audio.reset();
    return 0;
}

static void SNDRingDeInit(void) {}

static int SNDRingReset(void)
{
    g_audio.reset();
    return 0;
}

static int SNDRingChangeVideoFormat(int) { return 0; }

static void SNDRingUpdateAudio(u32* leftchanbuffer, u32* rightchanbuffer, u32 num_samples)
{
    const s32* left = (const s32*)leftchanbuffer;
    const s32* right = (const s32*)rightchanbuffer;
    uint32_t space = g_audio.space();
    uint32_t toWrite = num_samples;
    if (toWrite > space) {
        g_audio.overruns.fetch_add(toWrite - space, std::memory_order_relaxed);
        toWrite = space;
    }
    uint32_t w = g_audio.write.load(std::memory_order_relaxed);
    const bool muted = g_session && g_session->muted;
    for (uint32_t i = 0; i < toWrite; ++i) {
        s32 l = left[i], r = right[i];
        if (l > 0x7FFF) l = 0x7FFF; else if (l < -0x8000) l = -0x8000;
        if (r > 0x7FFF) r = 0x7FFF; else if (r < -0x8000) r = -0x8000;
        const uint32_t idx = (w & kAudioRingMask) * 2;
        g_audio.samples[idx] = muted ? 0 : (int16_t)l;
        g_audio.samples[idx + 1] = muted ? 0 : (int16_t)r;
        ++w;
    }
    g_audio.write.store(w, std::memory_order_release);
    g_audio.written.fetch_add(toWrite, std::memory_order_relaxed);
}

static u32 SNDRingGetAudioSpace(void)
{
    return g_audio.space();
}

static void SNDRingMuteAudio(void) {}
static void SNDRingUnMuteAudio(void) {}
static void SNDRingSetVolume(int) {}

// ---------------------------------------------------------------------------
// C ABI
// ---------------------------------------------------------------------------

extern "C" {

const char* PASaturnBridgeVersion(void) { return "1"; }
const char* PASaturnCoreVersion(void) { return YAB_VERSION; }
bool PASaturnIsAvailable(void) { return true; }
bool PASaturnUsesInterpreter(void) { return true; }
const char* PASaturnLastError(void) { return lastErrorCString(); }

PASaturnSessionRef PASaturnSessionCreate(const char* bios_path, const char* disc_path, const char* backup_path)
{
    if (g_session) {
        setLastError("A Saturn session already exists");
        return NULL;
    }
    if (!bios_path || !*bios_path) {
        setLastError("A Saturn BIOS image is required");
        return NULL;
    }
    if (!disc_path || !*disc_path) {
        setLastError("A disc image is required");
        return NULL;
    }
    if (!backup_path || !*backup_path) {
        setLastError("A backup RAM path is required");
        return NULL;
    }

    Session* s = new Session();
    s->biosPath = bios_path;
    s->discPath = disc_path;
    s->backupPath = backup_path;
    g_session = s;
    setLastError("");

    yabauseinit_struct yinit;
    memset(&yinit, 0, sizeof(yinit));
    yinit.m68kcoretype = M68KCORE_MUSASHI;
    yinit.percoretype = PERCORE_DUMMY;
    yinit.sh2coretype = SH2CORE_INTERPRETER;
    yinit.vidcoretype = VIDCORE_SOFT;
    yinit.sndcoretype = PASATURN_SNDCORE_RING;
    yinit.cdcoretype = CDCORE_ISO;
    yinit.carttype = CART_NONE;
    yinit.regionid = 0; // auto-detect from the disc
    yinit.biospath = s->biosPath.c_str();
    yinit.cdpath = s->discPath.c_str();
    yinit.buppath = s->backupPath.c_str();
    yinit.mpegpath = NULL;
    yinit.cartpath = NULL;
    yinit.videoformattype = VIDEOFORMATTYPE_NTSC;
    yinit.frameskip = 0;
    yinit.framelimit = 1;      // the caller paces frames
    yinit.clocksync = 0;
    yinit.basetime = 0;
    yinit.usethreads = 0;
    yinit.numthreads = 0;
    yinit.osdcoretype = OSDCORE_DUMMY;
    yinit.skip_load = 0;
    yinit.video_filter_type = 0;
    yinit.polygon_generation_mode = 0;
    yinit.play_ssf = 0;
    yinit.use_new_scsp = 0;
    yinit.resolution_mode = 0;
    yinit.rbg_resolution_mode = 0;
    yinit.rbg_use_compute_shader = 0;
    yinit.extend_backup = 1;
    yinit.rotate_screen = 0;
    yinit.scsp_sync_count_per_frame = 4;
    yinit.scsp_main_mode = 0;
    yinit.sync_shift = 0;
    yinit.playRecordPath = NULL;
    yinit.use_cpu_affinity = 0;
    yinit.use_sh2_cache = 1;

    int result = YabauseInit(&yinit);
    if (result != 0) {
        std::string err = g_lastError.empty() ? "YabauseInit failed" : g_lastError;
        YabauseDeInit();
        g_session = nullptr;
        delete s;
        setLastError(err);
        return NULL;
    }

    attachPads();
    installRendererTimers();
    ScspUnMuteAudio(SCSP_MUTE_SYSTEM);
    ScspSetVolume(100);
    // The app draws its own overlays; keep the core's software OSD out of the
    // framebuffer ("STATE LOADED" etc.).
    OSDChangeCore(OSDCORE_DUMMY);
    SetOSDToggle(0);
    return s;
}

void PASaturnSessionDestroy(PASaturnSessionRef session)
{
    Session* s = (Session*)session;
    if (!s || s != g_session) {
        return;
    }
    YabFlushBackups();
    YabauseDeInit();
    g_session = nullptr;
    delete s;
}

bool PASaturnSessionRunFrame(PASaturnSessionRef session)
{
    Session* s = (Session*)session;
    if (!s || s != g_session) {
        return false;
    }
    s->frameDirtyThisRun = false;
    const uint64_t t0 = nowNanos();
    YabauseExec();
    s->perf.recordFrame(nowNanos() - t0);
    s->emulatedFrames++;
    return s->frameDirtyThisRun;
}

void PASaturnSessionReset(PASaturnSessionRef session)
{
    Session* s = (Session*)session;
    if (!s || s != g_session) {
        return;
    }
    YabauseReset();
    attachPads();
}

bool PASaturnSessionGetVideo(PASaturnSessionRef session, PASaturnVideoFrame* out_frame)
{
    Session* s = (Session*)session;
    if (!s || s != g_session || !out_frame || s->frame.empty()) {
        return false;
    }
    out_frame->pixels = s->frame.data();
    out_frame->width = s->frameWidth;
    out_frame->height = s->frameHeight;
    out_frame->sequence = s->frameSequence;
    return true;
}

int32_t PASaturnSessionAudioSampleRate(PASaturnSessionRef)
{
    return kAudioSampleRate;
}

int32_t PASaturnSessionReadAudio(PASaturnSessionRef, int16_t* out, int32_t max_frames)
{
    if (!out || max_frames <= 0) {
        return 0;
    }
    uint32_t available = g_audio.available();
    uint32_t n = (uint32_t)max_frames < available ? (uint32_t)max_frames : available;
    uint32_t r = g_audio.read.load(std::memory_order_relaxed);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t idx = (r & kAudioRingMask) * 2;
        out[i * 2] = g_audio.samples[idx];
        out[i * 2 + 1] = g_audio.samples[idx + 1];
        ++r;
    }
    g_audio.read.store(r, std::memory_order_release);
    return (int32_t)n;
}

int32_t PASaturnSessionAudioAvailable(PASaturnSessionRef)
{
    return (int32_t)g_audio.available();
}

void PASaturnSessionSetButton(PASaturnSessionRef session, int32_t player, PASaturnButton button, bool pressed)
{
    Session* s = (Session*)session;
    if (!s || s != g_session || player < 0 || player > 1) {
        return;
    }
    if ((int)button < PASaturnButtonUp || (int)button > PASaturnButtonZ) {
        return;
    }
    const u32 key = ((u32)player << 24) | (u32)button;
    if (pressed) {
        PerKeyDown(key);
    } else {
        PerKeyUp(key);
    }
}

bool PASaturnSessionSaveState(PASaturnSessionRef session, const char* path)
{
    Session* s = (Session*)session;
    if (!s || s != g_session || !path || !*path) {
        return false;
    }
    if (YabSaveState(path) != 0) {
        setLastError("Save state failed");
        return false;
    }
    return true;
}

bool PASaturnSessionLoadState(PASaturnSessionRef session, const char* path)
{
    Session* s = (Session*)session;
    if (!s || s != g_session || !path || !*path) {
        return false;
    }
    if (YabLoadState(path) != 0) {
        setLastError("Load state failed");
        return false;
    }
    attachPads();
    return true;
}

void PASaturnSessionFlushPersistentSaves(PASaturnSessionRef session)
{
    Session* s = (Session*)session;
    if (!s || s != g_session) {
        return;
    }
    YabFlushBackups();
}

void PASaturnSessionGetStats(PASaturnSessionRef session, PASaturnStats* out_stats)
{
    Session* s = (Session*)session;
    if (!out_stats) {
        return;
    }
    memset(out_stats, 0, sizeof(*out_stats));
    if (!s || s != g_session) {
        return;
    }
    out_stats->emulatedFrames = s->emulatedFrames;
    out_stats->renderedFrames = s->renderedFrames;
    out_stats->audioFramesWritten = g_audio.written.load(std::memory_order_relaxed);
    out_stats->audioOverruns = g_audio.overruns.load(std::memory_order_relaxed);
    out_stats->frameNanosTotal = s->perf.frameNanosTotal;
    out_stats->frameNanosMax = s->perf.frameNanosMax;
    out_stats->vdp1Nanos = s->perf.vdp1Nanos;
    out_stats->vdp2Nanos = s->perf.vdp2Nanos;
    out_stats->presentNanos = s->perf.presentNanos;
    out_stats->recentFrameNanosP95 = s->perf.recentP95();
    out_stats->vdp1DrawStarts = g_vdp1DrawStarts;
    out_stats->vdp1EraseWrites = g_vdp1EraseWrites;
    out_stats->vdp1FrameChanges = g_vdp1FrameChanges;
}

} // extern "C"
