#ifndef PA_SATURN_BRIDGE_H
#define PA_SATURN_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PASATURN_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

// Pocket Arcade Sega Saturn core: Yaba Sanshiro 1.20.37 (Yabause lineage,
// GPL-2.0-or-later) driven through the SH-2 interpreter and the software
// (VIDSoft) renderer. No OpenGL, no JIT, no executable memory. Everything the
// app needs goes through this C ABI so the framework can be rebuilt or
// replaced without touching Swift.
//
// The core keeps global state; only one session may exist at a time.
typedef void* PASaturnSessionRef;

typedef enum PASaturnButton {
    PASaturnButtonUp = 0,
    PASaturnButtonRight = 1,
    PASaturnButtonDown = 2,
    PASaturnButtonLeft = 3,
    PASaturnButtonRightTrigger = 4, // R shoulder
    PASaturnButtonLeftTrigger = 5,  // L shoulder
    PASaturnButtonStart = 6,
    PASaturnButtonA = 7,
    PASaturnButtonB = 8,
    PASaturnButtonC = 9,
    PASaturnButtonX = 10,
    PASaturnButtonY = 11,
    PASaturnButtonZ = 12,
} PASaturnButton;

typedef struct PASaturnVideoFrame {
    const uint32_t* pixels; // RGBA8888, tightly packed, owned by the core
    int32_t width;
    int32_t height;
    uint64_t sequence;      // increments once per rendered frame
} PASaturnVideoFrame;

typedef struct PASaturnStats {
    uint64_t emulatedFrames;  // YabauseExec calls that completed
    uint64_t renderedFrames;  // frames the software renderer produced
    uint64_t audioFramesWritten; // stereo frames the SCSP produced
    uint64_t audioOverruns;   // stereo frames dropped because the ring was full
    // Wall-clock cost, cumulative nanoseconds since the session started; the
    // caller differentiates over time. frameNanos covers PASaturnSessionRunFrame
    // as a whole; the three renderer buckets are measured around the software
    // renderer's entry points, so SH-2/SCU/SCSP time is frame - (vdp1 + vdp2 +
    // present).
    uint64_t frameNanosTotal;
    uint64_t frameNanosMax;
    uint64_t vdp1Nanos;      // VDP1 command rasterisation
    uint64_t vdp2Nanos;      // VDP2 layer drawing
    uint64_t presentNanos;   // per-pixel compositing + frame copy
    uint64_t recentFrameNanosP95; // p95 of the last 256 frames
} PASaturnStats;

/// Creates a session and boots the disc. `bios_path` must point at a 512 KiB
/// Saturn BIOS image (the core does not bundle one). `disc_path` is a .cue,
/// .iso, .bin, .chd or .ccd/.mds image. `backup_path` is the file that
/// receives the internal backup RAM image; its directory must exist.
/// Returns NULL and records the last error on failure.
PASATURN_EXPORT PASaturnSessionRef PASaturnSessionCreate(
    const char* bios_path,
    const char* disc_path,
    const char* backup_path
);
PASATURN_EXPORT void PASaturnSessionDestroy(PASaturnSessionRef session);

/// Runs one emulated frame (about 1/60 s NTSC, 1/50 s PAL of guest time). The
/// core does not sleep; the caller paces calls. Returns true when a new video
/// frame is available through PASaturnSessionGetVideo.
PASATURN_EXPORT bool PASaturnSessionRunFrame(PASaturnSessionRef session);
PASATURN_EXPORT void PASaturnSessionReset(PASaturnSessionRef session);

/// The returned pixel pointer is valid until the next PASaturnSessionRunFrame.
PASATURN_EXPORT bool PASaturnSessionGetVideo(PASaturnSessionRef session, PASaturnVideoFrame* out_frame);

/// Sample rate of the audio the core produces (44100).
PASATURN_EXPORT int32_t PASaturnSessionAudioSampleRate(PASaturnSessionRef session);
/// Drains up to `max_frames` interleaved stereo int16 frames into `out`.
/// Returns the number of frames written. Safe to call from any thread.
PASATURN_EXPORT int32_t PASaturnSessionReadAudio(PASaturnSessionRef session, int16_t* out, int32_t max_frames);
/// Interleaved stereo frames currently buffered.
PASATURN_EXPORT int32_t PASaturnSessionAudioAvailable(PASaturnSessionRef session);

/// `player` is 0 or 1. Both ports carry a standard six-button pad.
PASATURN_EXPORT void PASaturnSessionSetButton(PASaturnSessionRef session, int32_t player, PASaturnButton button, bool pressed);

/// Save states are written to / read from a file the caller owns.
PASATURN_EXPORT bool PASaturnSessionSaveState(PASaturnSessionRef session, const char* path);
PASATURN_EXPORT bool PASaturnSessionLoadState(PASaturnSessionRef session, const char* path);

/// Writes dirty backup RAM to `backup_path`.
PASATURN_EXPORT void PASaturnSessionFlushPersistentSaves(PASaturnSessionRef session);

PASATURN_EXPORT void PASaturnSessionGetStats(PASaturnSessionRef session, PASaturnStats* out_stats);

/// Pointer remains valid until the next bridge call.
PASATURN_EXPORT const char* PASaturnLastError(void);
PASATURN_EXPORT const char* PASaturnBridgeVersion(void);
/// Upstream core version string ("1.20.37").
PASATURN_EXPORT const char* PASaturnCoreVersion(void);
/// True on every slice; the simulator slice is a real build of the core.
PASATURN_EXPORT bool PASaturnIsAvailable(void);
/// True when the SH-2 core is the interpreter (always, in this build).
PASATURN_EXPORT bool PASaturnUsesInterpreter(void);

#ifdef __cplusplus
}
#endif

#endif
