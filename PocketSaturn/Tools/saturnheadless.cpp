// Headless macOS harness for the Pocket Arcade Saturn core.
//
//   saturnheadless <bios.bin> <disc.cue|iso|chd> [frames=600] [out.ppm]
//
// Boots the disc through the same bridge the app uses, runs N frames as fast
// as possible, reports rendered-frame / audio statistics, optionally writes the
// last frame as a binary PPM, and (optionally, --state) round-trips a save
// state. Exit status is non-zero when the core never produced a non-black frame.

#include "PASaturnBridge.h"

// Register peek for --regs (debug aid; links against the static core).
extern "C" {
#include "core.h"
#include "vdp1.h"
#include "vdp2.h"
}

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static bool writePPM(const char* path, const PASaturnVideoFrame& frame)
{
    FILE* fp = fopen(path, "wb");
    if (!fp) return false;
    fprintf(fp, "P6\n%d %d\n255\n", frame.width, frame.height);
    std::vector<unsigned char> row((size_t)frame.width * 3);
    for (int y = 0; y < frame.height; ++y) {
        const uint32_t* src = frame.pixels + (size_t)y * frame.width;
        for (int x = 0; x < frame.width; ++x) {
            const unsigned char* px = (const unsigned char*)&src[x];
            row[x * 3 + 0] = px[0];
            row[x * 3 + 1] = px[1];
            row[x * 3 + 2] = px[2];
        }
        fwrite(row.data(), 1, row.size(), fp);
    }
    fclose(fp);
    return true;
}

static double nonBlackRatio(const PASaturnVideoFrame& frame)
{
    size_t count = (size_t)frame.width * frame.height;
    if (count == 0) return 0.0;
    size_t nonBlack = 0;
    for (size_t i = 0; i < count; ++i) {
        if ((frame.pixels[i] & 0x00FFFFFFu) != 0) ++nonBlack;
    }
    return (double)nonBlack / (double)count;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <bios.bin> <disc> [frames] [out.ppm] [--state] [--press-start]\n", argv[0]);
        return 2;
    }
    const char* bios = argv[1];
    const char* disc = argv[2];
    int frames = argc > 3 && argv[3][0] != '-' ? atoi(argv[3]) : 600;
    const char* out = argc > 4 && argv[4][0] != '-' ? argv[4] : nullptr;
    bool testState = false, pressStart = false;
    int dumpEvery = 0;
    bool dumpRegs = false;
    for (int i = 3; i < argc; ++i) {
        if (!strcmp(argv[i], "--state")) testState = true;
        if (!strcmp(argv[i], "--press-start")) pressStart = true;
        if (!strncmp(argv[i], "--dump-every=", 13)) dumpEvery = atoi(argv[i] + 13);
        if (!strcmp(argv[i], "--regs")) dumpRegs = true;
    }

    std::string backup = std::string(out ? out : "saturnheadless") + ".bup";
    PASaturnSessionRef session = PASaturnSessionCreate(bios, disc, backup.c_str());
    if (!session) {
        fprintf(stderr, "session create failed: %s\n", PASaturnLastError());
        return 1;
    }
    printf("core %s bridge %s interpreter=%d\n", PASaturnCoreVersion(), PASaturnBridgeVersion(), PASaturnUsesInterpreter());

    auto t0 = std::chrono::steady_clock::now();
    int firstNonBlack = -1;
    double bestRatio = 0.0;
    std::vector<int16_t> audio(4096 * 2);
    uint64_t audioDrained = 0, audioNonZero = 0;
    PASaturnVideoFrame frame{};
    for (int i = 0; i < frames; ++i) {
        if (pressStart) {
            // Tap START every 2 seconds from frame 300 to move past title screens,
            // then stop so an in-game START does not pause the fight.
            bool down = i >= 300 && i < 3900 && (i % 120) < 6;
            PASaturnSessionSetButton(session, 0, PASaturnButtonStart, down);
        }
        bool rendered = PASaturnSessionRunFrame(session);
        int32_t n;
        while ((n = PASaturnSessionReadAudio(session, audio.data(), 4096)) > 0) {
            audioDrained += n;
            for (int32_t k = 0; k < n * 2; ++k) if (audio[k] != 0) ++audioNonZero;
        }
        if (rendered && PASaturnSessionGetVideo(session, &frame)) {
            double r = nonBlackRatio(frame);
            if (r > bestRatio) bestRatio = r;
            if (r > 0.01 && firstNonBlack < 0) {
                firstNonBlack = i;
                printf("first non-black frame at %d (%dx%d, %.1f%% lit)\n", i, frame.width, frame.height, r * 100.0);
            }
            if (dumpRegs && dumpEvery > 0 && i % dumpEvery == 0) {
                PASaturnStats st{};
                PASaturnSessionGetStats(session, &st);
                printf("f%05d VDP1 TVMR=%04X FBCR=%04X PTMR=%04X EDSR=%04X EWLR=%04X EWRR=%04X EWDR=%04X draws=%llu erases=%llu changes=%llu | VDP2 TVMD=%04X BGON=%04X PRISA=%04X SPCTL=%04X\n",
                       i, Vdp1Regs->TVMR, Vdp1Regs->FBCR, Vdp1Regs->PTMR, Vdp1Regs->EDSR, Vdp1Regs->EWLR, Vdp1Regs->EWRR, Vdp1Regs->EWDR,
                       (unsigned long long)st.vdp1DrawStarts, (unsigned long long)st.vdp1EraseWrites, (unsigned long long)st.vdp1FrameChanges,
                       Vdp2Regs->TVMD, Vdp2Regs->BGON, Vdp2Regs->PRISA, Vdp2Regs->SPCTL);
                printf("       VDP2 PRISB=%04X PRINA=%04X CCCTL=%04X CCRSA=%04X SFPRMD=%04X SDCTL=%04X WCTLA=%04X WCTLC=%04X CLOFEN=%04X SFSEL=%04X\n",
                       Vdp2Regs->PRISB, Vdp2Regs->PRINA, Vdp2Regs->CCCTL, Vdp2Regs->CCRSA, Vdp2Regs->SFPRMD, Vdp2Regs->SDCTL,
                       Vdp2Regs->WCTLA, Vdp2Regs->WCTLC, Vdp2Regs->CLOFEN, Vdp2Regs->SFSEL);
                {
                    // Per-line snapshots for the top of the frame: which
                    // registers change mid-frame (raster effects) and where.
                    printf("       lines:");
                    for (int ln = 0; ln < 60; ++ln) {
                        const Vdp2* L = &Vdp2Lines[ln];
                        const Vdp2* P = &Vdp2Lines[ln ? ln - 1 : 0];
                        if (ln == 0 || L->BGON != P->BGON || L->PRINA != P->PRINA || L->SCXIN0 != P->SCXIN0 || L->SCYIN0 != P->SCYIN0 ||
                            L->CHCTLA != P->CHCTLA || L->MPABN0 != P->MPABN0 || L->PNCN0 != P->PNCN0 || L->CCCTL != P->CCCTL || L->CCRNA != P->CCRNA ||
                            L->WCTLA != P->WCTLA || L->PRISA != P->PRISA || L->SPCTL != P->SPCTL || L->TVMD != P->TVMD || L->CLOFEN != P->CLOFEN)
                            printf(" [%d BGON=%04X PRINA=%04X SCX0=%04X SCY0=%04X CHCTLA=%04X MPABN0=%04X CCCTL=%04X CCRNA=%04X WCTLA=%04X PRISA=%04X SPCTL=%04X TVMD=%04X]",
                                   ln, L->BGON, L->PRINA, L->SCXIN0, L->SCYIN0, L->CHCTLA, L->MPABN0, L->CCCTL, L->CCRNA, L->WCTLA, L->PRISA, L->SPCTL, L->TVMD);
                    }
                    printf("\n");
                }
                printf("       VDP2 MPOFN=%04X MPABN0=%04X MPCDN0=%04X MPABN1=%04X MPCDN1=%04X PNCN0=%04X PNCN1=%04X CHCTLA=%04X PLSZ=%04X SCXIN0=%04X SCYIN0=%04X SCXIN1=%04X SCYIN1=%04X RAMCTL=%04X\n",
                       Vdp2Regs->MPOFN, Vdp2Regs->MPABN0, Vdp2Regs->MPCDN0, Vdp2Regs->MPABN1, Vdp2Regs->MPCDN1, Vdp2Regs->PNCN0, Vdp2Regs->PNCN1,
                       Vdp2Regs->CHCTLA, Vdp2Regs->PLSZ, Vdp2Regs->SCXIN0, Vdp2Regs->SCYIN0, Vdp2Regs->SCXIN1, Vdp2Regs->SCYIN1, Vdp2Regs->RAMCTL);
            }
            if (out && dumpEvery > 0 && i % dumpEvery == 0) {
                char path[1024];
                snprintf(path, sizeof(path), "%s.%05d.ppm", out, i);
                writePPM(path, frame);
            }
        }
        if (testState && i == frames / 2) {
            std::string statePath = std::string(out ? out : "saturnheadless") + ".yss";
            bool ok = PASaturnSessionSaveState(session, statePath.c_str());
            printf("save state: %s (%s)\n", ok ? "ok" : "FAILED", PASaturnLastError());
            ok = PASaturnSessionLoadState(session, statePath.c_str());
            printf("load state: %s (%s)\n", ok ? "ok" : "FAILED", PASaturnLastError());
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();

    PASaturnStats stats{};
    PASaturnSessionGetStats(session, &stats);
    printf("ran %d frames in %.2fs (%.1f fps host), rendered=%llu audio frames=%llu drained=%llu nonzero samples=%llu overruns=%llu\n",
           frames, secs, frames / secs,
           (unsigned long long)stats.renderedFrames,
           (unsigned long long)stats.audioFramesWritten,
           (unsigned long long)audioDrained,
           (unsigned long long)audioNonZero,
           (unsigned long long)stats.audioOverruns);
    printf("best non-black ratio %.1f%%\n", bestRatio * 100.0);
    if (stats.emulatedFrames > 0) {
        const double f = (double)stats.emulatedFrames;
        printf("per-frame host cost: avg %.2f ms (p95 %.2f, max %.2f) = vdp1 %.2f + vdp2 %.2f + present %.2f + cpu/scsp %.2f ms\n",
               stats.frameNanosTotal / f / 1e6, stats.recentFrameNanosP95 / 1e6, stats.frameNanosMax / 1e6,
               stats.vdp1Nanos / f / 1e6, stats.vdp2Nanos / f / 1e6, stats.presentNanos / f / 1e6,
               (stats.frameNanosTotal - stats.vdp1Nanos - stats.vdp2Nanos - stats.presentNanos) / f / 1e6);
    }

    if (out && PASaturnSessionGetVideo(session, &frame)) {
        if (writePPM(out, frame)) printf("wrote %s (%dx%d)\n", out, frame.width, frame.height);
    }

    PASaturnSessionFlushPersistentSaves(session);
    PASaturnSessionDestroy(session);
    return firstNonBlack >= 0 ? 0 : 3;
}
