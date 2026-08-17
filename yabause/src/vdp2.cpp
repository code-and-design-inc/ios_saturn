
/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004-2007 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
        Copyright 2019 devMiyax(smiyaxdev@gmail.com)

This file is part of YabaSanshiro.

        YabaSanshiro is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

YabaSanshiro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
along with YabaSanshiro; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file vdp2.c
    \brief VDP2 emulation functions
*/

#include <stdlib.h>
#include <atomic>
#include "vdp2.h"
#include "debug.h"
#include "peripheral.h"
#include "scu.h"
#include "sh2core.h"
#include "smpc.h"
#include "vdp1.h"
#include "yabause.h"
#include "movie.h"
#include "osdcore.h"
#include "threads.h"
#include "yui.h"
#include "frameprofile.h"
#include "vidogl.h"
#include "vidsoft.h"
#include <atomic>
#if defined(HAVE_VULKAN)
#include "vulkan/VIDVulkanCInterface.h"
#endif

u8 * Vdp2Ram;
u8 * Vdp2ColorRam;
Vdp2 * Vdp2Regs;
Vdp2Internal_struct Vdp2Internal;
Vdp2External_struct Vdp2External;

u8 Vdp2ColorRamUpdated = 0;
u8 A0_Updated = 0;
u8 A1_Updated = 0;
u8 B0_Updated = 0;
u8 B1_Updated = 0;

struct CellScrollData cell_scroll_data[270];
Vdp2 Vdp2Lines[270];


// Runtime selection of the threaded (async) VDP rendering model. The
// compile-time define only provides the default so ports without an
// explicit VdpSetAsyncRendering() call keep their historical behavior
// (Android/iOS/retro_arena/vulkan stay async, plain desktop builds stay
// sync). The Qt port picks per session: sync for the OpenGL core (its GL
// context never leaves the emulation thread), async for Vulkan. The flag
// must not change after Vdp2Init() started the VDP thread.
#if defined(YAB_ASYNC_RENDERING)
static int vdp_async_rendering = 1;
#else
static int vdp_async_rendering = 0;
#endif

void VdpSetAsyncRendering(int enable) {
  vdp_async_rendering = (enable != 0);
}

int VdpIsAsyncRendering(void) {
  return vdp_async_rendering;
}

u32 skipped_frame = 0;
u32 pre_swap_frame_buffer = 0;
static int autoframeskipenab=0;
static int throttlespeed=0;
s64 lastticks=0;
static int fps;
int vdp2_is_odd_frame = 0;
// Asyn rendering
YabEventQueue * evqueue = NULL; // Event Queue for async rendring
YabEventQueue * rcv_evqueue = NULL;
YabEventQueue * vdp1_rcv_evqueue = NULL;
YabEventQueue * vout_rcv_evqueue = NULL;
static s64 syncticks = 0;       // CPU time sync for real time.
static int vdp_proc_running = 0;
YabMutex * vrammutex = NULL;
int g_frame_count = 0;
static int framestoskip = 0;
static int framesskipped = 0;
static int skipnextframe = 0;
static int previous_skipped = 0;
static s64 curticks = 0;
static s64 diffticks = 0;
static u32 framecount = 0;
static s64 onesecondticks = 0;
static int enableFrameLimit = 1;
static int frameLimitShift = 0;

//#define LOG yprintf
#define PROFILE_RENDERING 0

YabEventQueue * command_ = NULL;

//---------------------------------------------------------------------------------------

extern "C" void * VdpProc(void *arg);      // rendering thread.
static void vdp2VBlankIN(void); // VBLANK-IN handler
static void vdp2VBlankOUT(void);// VBLANK-OUT handler
static int vdp2Vdp1FrameOps(void); // VDP1 erase/frame change/plot for one field
void VDP2genVRamCyclePattern();
int Vdp2GenerateCCode();


void Vdp1_onHblank();

void VdpLockVram() {
  YabThreadLock(vrammutex);
}

void VdpUnLockVram() {
  YabThreadUnLock(vrammutex);
}

// Capacity passed to YabThreadCreateQueue for vdp1_rcv_evqueue.
#define VDP1_RCV_QUEUE_CAPACITY 8

// Render-event completion tracking for the modelled draw-end.
//
// Dispatched is incremented on the emulation thread for every
// VDPEV_VBLANK_OUT / VDPEV_DIRECT_DRAW posted; retired is incremented by
// the render thread when it has finished processing such an event (in
// VdpProc, after the handler returned - always, whether or not the event
// plotted anything).
//
// Deliberately NOT tracked through vdp1_rcv_evqueue: that queue drops
// posts when full (ADR-0007) and is destroyed and recreated by
// Vdp2Reset(), so an event completing across a soft reset leaves an
// orphan token behind. A queue-based join then stays off-by-one forever
// and every later join returns one render too early - the draw-end fires
// while the render thread is still walking the command list, and
// Vdp1External.status races between IDLE and RUNNING from field to field
// (Street Fighter Zero 3 flickered exactly this way after a reset, but
// not after a state load). The legacy code self-healed by draining the
// whole queue at every wait; the counter pair is exact instead.
static std::atomic<int> vdp1_ev_dispatched{0};
static std::atomic<int> vdp1_ev_retired{0};

void Vdp1MarkRenderDispatched(void) {
  vdp1_ev_dispatched.fetch_add(1, std::memory_order_relaxed);
}

void Vdp1MarkRenderRetired(void) {
  vdp1_ev_retired.fetch_add(1, std::memory_order_release);
}

// Wait until the render thread has retired every dispatched render event.
// This is a data-ordering barrier only: it guarantees the render thread is
// no longer walking the VDP1 command list, so the game may be told
// (draw-end IRQ) that the list is free to rewrite, and a new render event
// can be dispatched without piling up behind an unfinished one. It is NOT
// the source of draw-end timing - that comes from the emulation-side
// estimate, see Vdp1FrameChangeLatch().
void Vdp1JoinPendingRenders(void) {
  while (vdp1_ev_retired.load(std::memory_order_acquire) <
         vdp1_ev_dispatched.load(std::memory_order_relaxed)) {
    YabThreadYield();
  }
}

// Lines left until the modelled VDP1 draw-end interrupt fires. Decremented
// once per scanline in Vdp2HBlankOUT; the interrupt is raised when it
// reaches zero. A countdown instead of a target line so that estimates
// longer than one frame (Die Hard Trilogy's boot-time framebuffer wipe
// lists take several hundred scanlines on real hardware) survive the
// per-frame wrap of yabsys.LineCount. Owned by the emulation thread.
int vdp1_drawend_lines = 0;

// ---------------------------------------------------------------------------
// YABA_VDP2_WTRACE=1: per-frame VDP2 VRAM write monitoring (stdout).
// Counts every CPU/DMA write into VDP2 VRAM between two VBlank-INs, tracks
// the touched address range and per-bank write counts, and prints one line
// per frame together with a strided hash of the whole VRAM. This separates
// "the game stopped producing new movie frames" (write count drops to zero,
// hash freezes) from "the renderer stopped consuming them" (writes and hash
// keep changing while the screen is stuck). Queue depths of the async
// rendering event queues are included to spot a render-thread backlog.
static int vdp2_wtrace_v = -1;
static u32 v2w_cnt = 0;
static u32 v2w_min = 0xFFFFFFFF;
static u32 v2w_max = 0;
static u32 v2w_bank[4] = { 0, 0, 0, 0 };

static int vdp2_wtrace_on(void) {
  if (vdp2_wtrace_v < 0)
    vdp2_wtrace_v = (getenv("YABA_VDP2_WTRACE") != NULL);
  return vdp2_wtrace_v;
}

static int gun_trace_v = -1;
static int gun_trace_on(void) {
  if (gun_trace_v < 0)
    gun_trace_v = (getenv("YABA_GUN_TRACE") != NULL);
  return gun_trace_v;
}

static void v2w_note(u32 addr, u32 size) {
  if (!vdp2_wtrace_on() && !gun_trace_on())
    return;
  v2w_cnt++;
  if (addr < v2w_min) v2w_min = addr;
  if (addr + size - 1 > v2w_max) v2w_max = addr + size - 1;
  v2w_bank[(addr >> 17) & 3]++;
}

static void gun_trace_frame(u32 vdp2_writes);

static void v2w_frame(void) {
  u64 h;
  const u64 *p;
  u32 i;
  if (gun_trace_on()) {
    gun_trace_frame(v2w_cnt);
    if (!vdp2_wtrace_on()) {
      v2w_cnt = 0;
      v2w_min = 0xFFFFFFFF;
      v2w_max = 0;
      v2w_bank[0] = v2w_bank[1] = v2w_bank[2] = v2w_bank[3] = 0;
      return;
    }
  }
  if (!vdp2_wtrace_on())
    return;
  h = 1469598103934665603ULL;
  p = (const u64 *)Vdp2Ram;
  // Sample every 64 bytes of the 512KB VRAM: cheap per frame and still
  // guaranteed to catch a movie-frame update (a 256KB bitmap rewrite).
  for (i = 0; i < 0x80000 / 8; i += 8) {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
  printf("[V2W f%u] n=%u range=%05X-%05X banks=%u/%u/%u/%u flags=%d%d%d%d "
         "q=%d/%d/%d/%d hash=%08X%08X\n",
         (unsigned)yabsys.frame_count, v2w_cnt,
         (v2w_cnt ? v2w_min : 0), (v2w_cnt ? v2w_max : 0),
         v2w_bank[0], v2w_bank[1], v2w_bank[2], v2w_bank[3],
         A0_Updated, A1_Updated, B0_Updated, B1_Updated,
         (evqueue ? YaGetQueueSize(evqueue) : -1),
         (rcv_evqueue ? YaGetQueueSize(rcv_evqueue) : -1),
         (vdp1_rcv_evqueue ? YaGetQueueSize(vdp1_rcv_evqueue) : -1),
         (vout_rcv_evqueue ? YaGetQueueSize(vout_rcv_evqueue) : -1),
         (u32)(h >> 32), (u32)h);
  v2w_cnt = 0;
  v2w_min = 0xFFFFFFFF;
  v2w_max = 0;
  v2w_bank[0] = v2w_bank[1] = v2w_bank[2] = v2w_bank[3] = 0;
}

// YABA_GUN_TRACE=<file>: per-VBlank trace of Gungriffon's movie pacing
// counters plus this frame's VDP2 VRAM write count. Mirrors the trace added
// to mednafen's Emulate() so the two emulators' movie cadence can be
// compared exactly (see docs/B20/gungriffon_fmv_framedrop_investigation.md).
// HLE-boot addresses: 0x0603649C = decoded video frame counter, 0x06032250 =
// audio-driven master clock.
static void gun_trace_frame(u32 vdp2_writes) {
  static FILE *gt_fp = (FILE *)(intptr_t)-1;
  if (gt_fp == (FILE *)(intptr_t)-1) {
    const char *p = getenv("YABA_GUN_TRACE");
    gt_fp = p ? fopen(p, "w") : NULL;
  }
  if (!gt_fp)
    return;
  // Columns 10/11 (YABA_SH2_PROF only): master instructions retired and
  // cycles charged during this VBlank interval. Together with the per-PC
  // histogram dumped below they show whether the decode gap vs mednafen is
  // extra work (more instructions) or a more expensive cost model.
  {
    static u64 prev_i = 0, prev_c = 0;
    u64 icnt = 0, ccnt = 0;
    if (Sh2ProfEnabled()) {
      icnt = Sh2ProfICount(0);
      ccnt = Sh2ProfCycles(0);
    }
    fprintf(gt_fp, "%u %d %d %u %08X %d %d %d %d %llu %llu\n",
            (unsigned)yabsys.frame_count,
            (int)T2ReadLong(HighWram, 0x3649C),
            (int)T2ReadLong(HighWram, 0x32250), vdp2_writes,
            MSH2 ? (unsigned)MSH2->regs.PC : 0,
            (int)T2ReadLong(HighWram, 0x364A0),
            (int)T2ReadLong(HighWram, 0x36494),
            (int)T2ReadLong(HighWram, 0x36450),
            (int)T2ReadLong(HighWram, 0x36454),
            (unsigned long long)(icnt - prev_i),
            (unsigned long long)(ccnt - prev_c));
    prev_i = icnt;
    prev_c = ccnt;
  }
  if (Sh2ProfEnabled() && !(yabsys.frame_count % 120)) {
    char tagbuf[64];
    snprintf(tagbuf, sizeof(tagbuf), "frame=%u vf=%d",
             (unsigned)yabsys.frame_count,
             (int)T2ReadLong(HighWram, 0x3649C));
    Sh2ProfDump(getenv("YABA_SH2_PROF"), tagbuf);
  }
  if (!(yabsys.frame_count & 0x3F))
    fflush(gt_fp);
}


//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp2RamReadByte(u32 addr) {
   addr &= 0x7FFFF;
   return T1ReadByte(Vdp2Ram, addr);
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp2RamReadWord(u32 addr) {
   addr &= 0x7FFFF;
   return T1ReadWord(Vdp2Ram, addr);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp2RamReadLong(u32 addr) {
   addr &= 0x7FFFF;
   return T1ReadLong(Vdp2Ram, addr);
}

//////////////////////////////////////////////////////////////////////////////
//#define VRAM_WRITE_CHECK 1
#if VRAM_WRITE_CHECK
int prelinev = 0;
#endif
void FASTCALL Vdp2RamWriteByte(u32 addr, u8 val) {
   addr &= 0x7FFFF;
#if VRAM_WRITE_CHECK
   if (yabsys.LineCount != prelinev) {
     LOG("VRAM: write byte @%d, cycle_a=%d cycle_b=%d A0=%04X%04X A1=%04X%04X B0=%04X%04X B1=%04X%04X ",
       yabsys.LineCount,
       Vdp2External.cpu_cycle_a, Vdp2External.cpu_cycle_b,
       Vdp2Regs->CYCA0L, Vdp2Regs->CYCA0U, Vdp2Regs->CYCA0L, Vdp2Regs->CYCA0U, Vdp2Regs->CYCB0L, Vdp2Regs->CYCB0U, Vdp2Regs->CYCB1L, Vdp2Regs->CYCB1U);
     prelinev = yabsys.LineCount;
   }
#endif
   if (A0_Updated == 0 && addr >= 0 && addr < 0x20000){
     A0_Updated = 1;
   }
   else if (A1_Updated == 0 &&  addr >= 0x20000 && addr < 0x40000){
     A1_Updated = 1;
   }
   else if (B0_Updated == 0 && addr >= 0x40000 && addr < 0x60000){
     B0_Updated = 1;
   }
   else if (B1_Updated == 0 && addr >= 0x60000 && addr < 0x80000){
     B1_Updated = 1;
   }

   v2w_note(addr, 1);
   T1WriteByte(Vdp2Ram, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2RamWriteWord(u32 addr, u16 val) {
   addr &= 0x7FFFF;
#if VRAM_WRITE_CHECK
   if (yabsys.LineCount != prelinev) {
     LOG("VRAM: write word @%d, cycle_a=%d cycle_b=%d A0=%04X%04X A1=%04X%04X B0=%04X%04X B1=%04X%04X ",
       yabsys.LineCount,
       Vdp2External.cpu_cycle_a, Vdp2External.cpu_cycle_b,
       Vdp2Regs->CYCA0L, Vdp2Regs->CYCA0U, Vdp2Regs->CYCA0L, Vdp2Regs->CYCA0U, Vdp2Regs->CYCB0L, Vdp2Regs->CYCB0U, Vdp2Regs->CYCB1L, Vdp2Regs->CYCB1U);
     prelinev = yabsys.LineCount;
   }
#endif
   if (A0_Updated == 0 && addr >= 0 && addr < 0x20000){
     A0_Updated = 1;
   }
   else if (A1_Updated == 0 && addr >= 0x20000 && addr < 0x40000){
     A1_Updated = 1;
   }
   else if (B0_Updated == 0 && addr >= 0x40000 && addr < 0x60000){
     B0_Updated = 1;
   }
   else if (B1_Updated == 0 && addr >= 0x60000 && addr < 0x80000){
     B1_Updated = 1;
   }

   v2w_note(addr, 2);
   T1WriteWord(Vdp2Ram, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2RamWriteLong(u32 addr, u32 val) {
  addr &= 0x7FFFF;
#if VRAM_WRITE_CHECK
  if (yabsys.LineCount != prelinev) {
    LOG("VRAM: write long @%d, cycle_a=%d cycle_b=%d A0=%04X%04X A1=%04X%04X B0=%04X%04X B1=%04X%04X ",
      yabsys.LineCount,
      Vdp2External.cpu_cycle_a, Vdp2External.cpu_cycle_b,
      Vdp2Regs->CYCA0L, Vdp2Regs->CYCA0U, Vdp2Regs->CYCA0L, Vdp2Regs->CYCA0U, Vdp2Regs->CYCB0L, Vdp2Regs->CYCB0U, Vdp2Regs->CYCB1L, Vdp2Regs->CYCB1U );
    prelinev = yabsys.LineCount;
  }
#endif
  if (A0_Updated == 0 && addr >= 0 && addr < 0x20000){
     A0_Updated = 1;
   }
   else if (A1_Updated == 0 && addr >= 0x20000 && addr < 0x40000){
     A1_Updated = 1;
   }
   else if (B0_Updated == 0 && addr >= 0x40000 && addr < 0x60000){
     B0_Updated = 1;
   }
   else if (B1_Updated == 0 && addr >= 0x60000 && addr < 0x80000){
     B1_Updated = 1;
   }

   v2w_note(addr, 4);
   T1WriteLong(Vdp2Ram, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp2ColorRamReadByte(u32 addr) {
   addr &= 0xFFF;
   return T2ReadByte(Vdp2ColorRam, addr);
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp2ColorRamReadWord(u32 addr) {
   addr &= 0xFFF;
   return T2ReadWord(Vdp2ColorRam, addr);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp2ColorRamReadLong(u32 addr) {
   addr &= 0xFFF;
   return T2ReadLong(Vdp2ColorRam, addr);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2ColorRamWriteByte(u32 addr, u8 val) {
   addr &= 0xFFF;
   //LOG("[VDP2] Update Coloram Byte %08X:%02X", addr, val);
   T2WriteByte(Vdp2ColorRam, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2ColorRamWriteWord(u32 addr, u16 val) {
   addr &= 0xFFF;
   //LOG("[VDP2] Update Coloram Word %08X:%04X", addr, val);
   if (Vdp2Internal.ColorMode == 0 ) {
     if (val != T2ReadWord(Vdp2ColorRam, addr)) {
       T2WriteWord(Vdp2ColorRam, addr, val);
       VIDCore->OnUpdateColorRamWord(addr);
     }

     if (addr < 0x800) {
       if (val != T2ReadWord(Vdp2ColorRam, addr + 0x800)) {
         T2WriteWord(Vdp2ColorRam, addr + 0x800, val);
         VIDCore->OnUpdateColorRamWord(addr + 0x800);
       }
     }
   }
   else {
     if (val != T2ReadWord(Vdp2ColorRam, addr)) {
       T2WriteWord(Vdp2ColorRam, addr, val);
       VIDCore->OnUpdateColorRamWord(addr);
     }
   }
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2ColorRamWriteLong(u32 addr, u32 val) {
   addr &= 0xFFF;
   //LOG("[VDP2] Update Coloram Long %08X:%08X", addr, val);

   if (Vdp2Internal.ColorMode == 0) {

     const u32 base_addr = addr;
     T2WriteLong(Vdp2ColorRam, base_addr, val);
     VIDCore->OnUpdateColorRamWord(base_addr + 2);
     VIDCore->OnUpdateColorRamWord(base_addr);

     if (addr < 0x800) {
       const u32 mirror_addr = base_addr + 0x800;
       T2WriteLong(Vdp2ColorRam, mirror_addr, val);
       VIDCore->OnUpdateColorRamWord(mirror_addr + 2);
       VIDCore->OnUpdateColorRamWord(mirror_addr);
     }
   }
   else {
     T2WriteLong(Vdp2ColorRam, addr, val);
     if (Vdp2Internal.ColorMode == 2) {
       VIDCore->OnUpdateColorRamWord(addr);
     }
     else {
       VIDCore->OnUpdateColorRamWord(addr + 2);
       VIDCore->OnUpdateColorRamWord(addr);
     }
   }

}

//////////////////////////////////////////////////////////////////////////////

int Vdp2Init(void) {
   if ((Vdp2Regs = (Vdp2 *) calloc(1, sizeof(Vdp2))) == NULL)
      return -1;

   if ((Vdp2Ram = T1MemoryInit(0x80000)) == NULL)
      return -1;

   if ((Vdp2ColorRam = T2MemoryInit(0x1000)) == NULL)
      return -1;

   memset(Vdp2Lines, 0, sizeof(Vdp2) * 270);

   Vdp2Reset();

   // Both rendering models poll this: the synchronous path compares it against
   // LineCount to decide when a draw is due. Leaving it at 0 (a cold boot) or at
   // the previous session's value makes that comparison fire on the first line
   // and send a draw-end before anything has been plotted.
   yabsys.wait_line_count = -1;

   if (VdpIsAsyncRendering()) {
     if (rcv_evqueue==NULL) rcv_evqueue = YabThreadCreateQueue(8);
     if (vdp1_rcv_evqueue==NULL) vdp1_rcv_evqueue = YabThreadCreateQueue(VDP1_RCV_QUEUE_CAPACITY);
     if (vout_rcv_evqueue==NULL) vout_rcv_evqueue = YabThreadCreateQueue(2);
     Vdp1JoinPendingRenders();
     vdp1_ev_dispatched.store(0, std::memory_order_relaxed);
     vdp1_ev_retired.store(0, std::memory_order_relaxed);
     vdp1_drawend_lines = 0;
   }

   vrammutex = YabThreadCreateMutex();

   command_ = YabThreadCreateQueue(1);


   memset(Vdp2ColorRam, 0xFF, 0x1000);
   for (int i = 0; i < 0x1000; i += 2) {
     VIDCore->OnUpdateColorRamWord(i);
   }

   if (VdpIsAsyncRendering()) {
     YuiRevokeOGLOnThisThread();
     evqueue = YabThreadCreateQueue(32);
     vdp_proc_running = 1;
     YabThreadStart(YAB_THREAD_VDP, "vdp", VdpProc, NULL);
   }
   return 0;

}

//////////////////////////////////////////////////////////////////////////////

void Vdp2DeInit(void) {
   if (vdp_proc_running == 1) {
   	YabAddEventQueue(evqueue,VDPEV_FINSH);
   	//vdp_proc_running = 0;
   	YabThreadWait(YAB_THREAD_VDP);
   }
   if (Vdp2Regs)
      free(Vdp2Regs);
   Vdp2Regs = NULL;

   if (Vdp2Ram)
      T1MemoryDeInit(Vdp2Ram);
   Vdp2Ram = NULL;

   if (Vdp2ColorRam)
      T2MemoryDeInit(Vdp2ColorRam);
   Vdp2ColorRam = NULL;

   YabThreadFreeMutex(vrammutex);

}

//////////////////////////////////////////////////////////////////////////////

void Vdp2Reset(void) {
   Vdp2Regs->TVMD = 0x0000;
   Vdp2Regs->EXTEN = 0x0000;
   Vdp2Regs->TVSTAT = Vdp2Regs->TVSTAT & 0x1;
   Vdp2Regs->VRSIZE = 0x0000; // fix me(version should be set)
   Vdp2Regs->RAMCTL = 0x0000;
   Vdp2Regs->BGON = 0x0000;
   Vdp2Regs->CHCTLA = 0x0000;
   Vdp2Regs->CHCTLB = 0x0000;
   Vdp2Regs->BMPNA = 0x0000;
   Vdp2Regs->MPOFN = 0x0000;
   Vdp2Regs->MPABN2 = 0x0000;
   Vdp2Regs->MPCDN2 = 0x0000;
   Vdp2Regs->SCXIN0 = 0x0000;
   Vdp2Regs->SCXDN0 = 0x0000;
   Vdp2Regs->SCYIN0 = 0x0000;
   Vdp2Regs->SCYDN0 = 0x0000;
   Vdp2Regs->ZMXN0.all = 0x00000000;
   Vdp2Regs->ZMYN0.all = 0x00000000;
   Vdp2Regs->SCXIN1 = 0x0000;
   Vdp2Regs->SCXDN1 = 0x0000;
   Vdp2Regs->SCYIN1 = 0x0000;
   Vdp2Regs->SCYDN1 = 0x0000;
   Vdp2Regs->ZMXN1.all = 0x00000000;
   Vdp2Regs->ZMYN1.all = 0x00000000;
   Vdp2Regs->SCXN2 = 0x0000;
   Vdp2Regs->SCYN2 = 0x0000;
   Vdp2Regs->SCXN3 = 0x0000;
   Vdp2Regs->SCYN3 = 0x0000;
   Vdp2Regs->ZMCTL = 0x0000;
   Vdp2Regs->SCRCTL = 0x0000;
   Vdp2Regs->VCSTA.all = 0x00000000;
   Vdp2Regs->BKTAU = 0x0000;
   Vdp2Regs->BKTAL = 0x0000;
   Vdp2Regs->RPMD = 0x0000;
   Vdp2Regs->RPRCTL = 0x0000;
   Vdp2Regs->KTCTL = 0x0000;
   Vdp2Regs->KTAOF = 0x0000;
   Vdp2Regs->OVPNRA = 0x0000;
   Vdp2Regs->OVPNRB = 0x0000;
   Vdp2Regs->WPSX0 = 0x0000;
   Vdp2Regs->WPSY0 = 0x0000;
   Vdp2Regs->WPEX0 = 0x0000;
   Vdp2Regs->WPEY0 = 0x0000;
   Vdp2Regs->WPSX1 = 0x0000;
   Vdp2Regs->WPSY1 = 0x0000;
   Vdp2Regs->WPEX1 = 0x0000;
   Vdp2Regs->WPEY1 = 0x0000;
   Vdp2Regs->WCTLA = 0x0000;
   Vdp2Regs->WCTLB = 0x0000;
   Vdp2Regs->WCTLC = 0x0000;
   Vdp2Regs->WCTLD = 0x0000;
   Vdp2Regs->SPCTL = 0x0000;
   Vdp2Regs->SDCTL = 0x0000;
   Vdp2Regs->CRAOFA = 0x0000;
   Vdp2Regs->CRAOFB = 0x0000;
   Vdp2Regs->LNCLEN = 0x0000;
   Vdp2Regs->SFPRMD = 0x0000;
   Vdp2Regs->CCCTL = 0x0000;
   Vdp2Regs->SFCCMD = 0x0000;
   Vdp2Regs->PRISA = 0x0000;
   Vdp2Regs->PRISB = 0x0000;
   Vdp2Regs->PRISC = 0x0000;
   Vdp2Regs->PRISD = 0x0000;
   Vdp2Regs->PRINA = 0x0000;
   Vdp2Regs->PRINB = 0x0000;
   Vdp2Regs->PRIR = 0x0000;
   Vdp2Regs->CCRNA = 0x0000;
   Vdp2Regs->CCRNB = 0x0000;
   Vdp2Regs->CLOFEN = 0x0000;
   Vdp2Regs->CLOFSL = 0x0000;
   Vdp2Regs->COAR = 0x0000;
   Vdp2Regs->COAG = 0x0000;
   Vdp2Regs->COAB = 0x0000;
   Vdp2Regs->COBR = 0x0000;
   Vdp2Regs->COBG = 0x0000;
   Vdp2Regs->COBB = 0x0000;

   yabsys.VBlankLineCount = 225;
   Vdp2Internal.ColorMode = 0;

   Vdp2External.disptoggle = 0xFF;
   Vdp2External.perline_alpha_a = 0;
   Vdp2External.perline_alpha_b = 0;
   Vdp2External.perline_alpha = &Vdp2External.perline_alpha_a;
   Vdp2External.perline_alpha_draw = &Vdp2External.perline_alpha_b;
   Vdp2External.cpu_cycle_a = 0;
   Vdp2External.cpu_cycle_b = 0;

   // Not inside the async branch below: the synchronous draw-end poller reads
   // this too, and a stale value makes it fire on the first line after a reset.
   yabsys.wait_line_count = -1;

   if (VdpIsAsyncRendering()) {
     if (rcv_evqueue != NULL){
       YabThreadDestoryQueue(rcv_evqueue);
       rcv_evqueue = YabThreadCreateQueue(8);
     }
     if (vdp1_rcv_evqueue != NULL){
       YabThreadDestoryQueue(vdp1_rcv_evqueue);
       vdp1_rcv_evqueue = YabThreadCreateQueue(VDP1_RCV_QUEUE_CAPACITY);
     }
     Vdp1JoinPendingRenders();
     vdp1_ev_dispatched.store(0, std::memory_order_relaxed);
     vdp1_ev_retired.store(0, std::memory_order_relaxed);
     vdp1_drawend_lines = 0;
   }

}


///////////////////////////////////////////////////////////////////////////////
extern "C" void * VdpProc( void *arg ){

  int evcode;

  if( YuiUseOGLOnThisThread() < 0 ){
    LOG("VDP2 Fail to USE GL");
    return NULL;
  }

  if( yabsys.use_cpu_affinity ){
    YabThreadSetCurrentThreadAffinityMask(YabThreadGetFastestCpuIndex());
  }

  while( vdp_proc_running ){
    evcode = YabWaitEventQueue(evqueue);
    switch(evcode){
    case VDPEV_VBLANK_IN:
      FrameProfileAdd("VIN start");
      vdp2VBlankIN();
      FrameProfileAdd("VIN end");
      break;
    case VDPEV_VBLANK_OUT:
      FrameProfileAdd("VOUT start");
      vdp2VBlankOUT();
      FrameProfileAdd("VOUT end");
      Vdp1MarkRenderRetired();
      break;
    case VDPEV_DIRECT_DRAW:
      FrameProfileAdd("DirectDraw start");
      FRAMELOG("VDP1: VDPEV_DIRECT_DRAW(T)");
      Vdp1Draw();
      VIDCore->Vdp1DrawEnd();
      Vdp1External.frame_change_plot = 0;
      FrameProfileAdd("DirectDraw end");
      Vdp1MarkRenderRetired();
      break;
    case VDPEV_MAKECURRENT:
      YuiUseOGLOnThisThread();
      YabAddEventQueue(command_,0);
      break;
    case VDPEV_REVOKE:
      YuiRevokeOGLOnThisThread();
      YabAddEventQueue(command_,0);
      break;
    case VDPEV_FINSH:
      vdp_proc_running = 0;
      break;
    }
  }
  return NULL;
}

// Decode the per-bank VRAM access pattern out of one register snapshot.
//
// VDP2genVRamCyclePattern() below keeps refreshing Vdp2External.AC_VRAM from
// the live registers at line 1, because the CPU-cycle penalty model it also
// computes has to track the current state. A renderer must not use that: it
// resolves a whole frame from the line-0 snapshot (Vdp2Lines[0]), and mixing
// in a pattern sampled one line later can disagree with the map registers of
// that same frame. Gungriffon's movie player flips the cycle pattern and
// NBG0's map registers together at every buffer swap, so the one-line skew
// left single frames whose character fetches all pointed at a bank the
// pattern said was not granted -- drawCell() emits a transparent cell for a
// denied bank, which blanked the entire screen for that frame.
void Vdp2GetAccessPattern(const Vdp2 * regs, u8 ac[4][8]) {
  int i;

  ac[0][0] = (regs->CYCA0L >> 12) & 0x0F;
  ac[0][1] = (regs->CYCA0L >> 8) & 0x0F;
  ac[0][2] = (regs->CYCA0L >> 4) & 0x0F;
  ac[0][3] = (regs->CYCA0L >> 0) & 0x0F;
  ac[0][4] = (regs->CYCA0U >> 12) & 0x0F;
  ac[0][5] = (regs->CYCA0U >> 8) & 0x0F;
  ac[0][6] = (regs->CYCA0U >> 4) & 0x0F;
  ac[0][7] = (regs->CYCA0U >> 0) & 0x0F;

  if (regs->RAMCTL & 0x100) {
    ac[1][0] = (regs->CYCA1L >> 12) & 0x0F;
    ac[1][1] = (regs->CYCA1L >> 8) & 0x0F;
    ac[1][2] = (regs->CYCA1L >> 4) & 0x0F;
    ac[1][3] = (regs->CYCA1L >> 0) & 0x0F;
    ac[1][4] = (regs->CYCA1U >> 12) & 0x0F;
    ac[1][5] = (regs->CYCA1U >> 8) & 0x0F;
    ac[1][6] = (regs->CYCA1U >> 4) & 0x0F;
    ac[1][7] = (regs->CYCA1U >> 0) & 0x0F;
  }
  else {
    for (i = 0; i < 8; i++) ac[1][i] = ac[0][i];
  }

  ac[2][0] = (regs->CYCB0L >> 12) & 0x0F;
  ac[2][1] = (regs->CYCB0L >> 8) & 0x0F;
  ac[2][2] = (regs->CYCB0L >> 4) & 0x0F;
  ac[2][3] = (regs->CYCB0L >> 0) & 0x0F;
  ac[2][4] = (regs->CYCB0U >> 12) & 0x0F;
  ac[2][5] = (regs->CYCB0U >> 8) & 0x0F;
  ac[2][6] = (regs->CYCB0U >> 4) & 0x0F;
  ac[2][7] = (regs->CYCB0U >> 0) & 0x0F;

  if (regs->RAMCTL & 0x200) {
    ac[3][0] = (regs->CYCB1L >> 12) & 0x0F;
    ac[3][1] = (regs->CYCB1L >> 8) & 0x0F;
    ac[3][2] = (regs->CYCB1L >> 4) & 0x0F;
    ac[3][3] = (regs->CYCB1L >> 0) & 0x0F;
    ac[3][4] = (regs->CYCB1U >> 12) & 0x0F;
    ac[3][5] = (regs->CYCB1U >> 8) & 0x0F;
    ac[3][6] = (regs->CYCB1U >> 4) & 0x0F;
    ac[3][7] = (regs->CYCB1U >> 0) & 0x0F;
  }
  else {
    for (i = 0; i < 8; i++) ac[3][i] = ac[2][i];
  }
}

void VDP2genVRamCyclePattern() {
  int cpu_cycle_a = 0;
  int cpu_cycle_b = 0;
  int i = 0;

  Vdp2External.AC_VRAM[0][0] = (Vdp2Regs->CYCA0L >> 12) & 0x0F;
  Vdp2External.AC_VRAM[0][1] = (Vdp2Regs->CYCA0L >> 8) & 0x0F;
  Vdp2External.AC_VRAM[0][2] = (Vdp2Regs->CYCA0L >> 4) & 0x0F;
  Vdp2External.AC_VRAM[0][3] = (Vdp2Regs->CYCA0L >> 0) & 0x0F;
  Vdp2External.AC_VRAM[0][4] = (Vdp2Regs->CYCA0U >> 12) & 0x0F;
  Vdp2External.AC_VRAM[0][5] = (Vdp2Regs->CYCA0U >> 8) & 0x0F;
  Vdp2External.AC_VRAM[0][6] = (Vdp2Regs->CYCA0U >> 4) & 0x0F;
  Vdp2External.AC_VRAM[0][7] = (Vdp2Regs->CYCA0U >> 0) & 0x0F;

  for (i = 0; i < 8; i++) {
    if (Vdp2External.AC_VRAM[0][i] >= 0x0E) {
      cpu_cycle_a++;
    }
    else if (Vdp2External.AC_VRAM[0][i] >= 4 && Vdp2External.AC_VRAM[0][i] <= 7) {
      if ((Vdp2Regs->BGON & (1 << (Vdp2External.AC_VRAM[0][i] - 4))) == 0) {
        cpu_cycle_a++;
      }
    }
  }

  if (Vdp2Regs->RAMCTL & 0x100) {
    int fcnt = 0;
    Vdp2External.AC_VRAM[1][0] = (Vdp2Regs->CYCA1L >> 12) & 0x0F;
    Vdp2External.AC_VRAM[1][1] = (Vdp2Regs->CYCA1L >> 8) & 0x0F;
    Vdp2External.AC_VRAM[1][2] = (Vdp2Regs->CYCA1L >> 4) & 0x0F;
    Vdp2External.AC_VRAM[1][3] = (Vdp2Regs->CYCA1L >> 0) & 0x0F;
    Vdp2External.AC_VRAM[1][4] = (Vdp2Regs->CYCA1U >> 12) & 0x0F;
    Vdp2External.AC_VRAM[1][5] = (Vdp2Regs->CYCA1U >> 8) & 0x0F;
    Vdp2External.AC_VRAM[1][6] = (Vdp2Regs->CYCA1U >> 4) & 0x0F;
    Vdp2External.AC_VRAM[1][7] = (Vdp2Regs->CYCA1U >> 0) & 0x0F;

    for (i = 0; i < 8; i++) {
      if (Vdp2External.AC_VRAM[0][i] == 0x0E) {
        if (Vdp2External.AC_VRAM[1][i] != 0x0E) {
          cpu_cycle_a--;
        }
        else {
          if (fcnt == 0) {
            cpu_cycle_a--;
          }
        }
      }
      if (Vdp2External.AC_VRAM[1][i] == 0x0F) {
        fcnt++;
      }
    }
    if (fcnt == 0)cpu_cycle_a = 0;
    if (cpu_cycle_a < 0)cpu_cycle_a = 0;
  }
  else {
    Vdp2External.AC_VRAM[1][0] = Vdp2External.AC_VRAM[0][0];
    Vdp2External.AC_VRAM[1][1] = Vdp2External.AC_VRAM[0][1];
    Vdp2External.AC_VRAM[1][2] = Vdp2External.AC_VRAM[0][2];
    Vdp2External.AC_VRAM[1][3] = Vdp2External.AC_VRAM[0][3];
    Vdp2External.AC_VRAM[1][4] = Vdp2External.AC_VRAM[0][4];
    Vdp2External.AC_VRAM[1][5] = Vdp2External.AC_VRAM[0][5];
    Vdp2External.AC_VRAM[1][6] = Vdp2External.AC_VRAM[0][6];
    Vdp2External.AC_VRAM[1][7] = Vdp2External.AC_VRAM[0][7];
  }

  Vdp2External.AC_VRAM[2][0] = (Vdp2Regs->CYCB0L >> 12) & 0x0F;
  Vdp2External.AC_VRAM[2][1] = (Vdp2Regs->CYCB0L >> 8) & 0x0F;
  Vdp2External.AC_VRAM[2][2] = (Vdp2Regs->CYCB0L >> 4) & 0x0F;
  Vdp2External.AC_VRAM[2][3] = (Vdp2Regs->CYCB0L >> 0) & 0x0F;
  Vdp2External.AC_VRAM[2][4] = (Vdp2Regs->CYCB0U >> 12) & 0x0F;
  Vdp2External.AC_VRAM[2][5] = (Vdp2Regs->CYCB0U >> 8) & 0x0F;
  Vdp2External.AC_VRAM[2][6] = (Vdp2Regs->CYCB0U >> 4) & 0x0F;
  Vdp2External.AC_VRAM[2][7] = (Vdp2Regs->CYCB0U >> 0) & 0x0F;

  for (i = 0; i < 8; i++) {
    if (Vdp2External.AC_VRAM[2][i] >= 0x0E) {
      cpu_cycle_b++;
    }
    else if (Vdp2External.AC_VRAM[2][i] >= 4 && Vdp2External.AC_VRAM[2][i] <= 7) {
      if ((Vdp2Regs->BGON & (1 << (Vdp2External.AC_VRAM[2][i] - 4))) == 0) {
        cpu_cycle_b++;
      }
    }
  }


  if (Vdp2Regs->RAMCTL & 0x200) {
    int fcnt = 0;
    Vdp2External.AC_VRAM[3][0] = (Vdp2Regs->CYCB1L >> 12) & 0x0F;
    Vdp2External.AC_VRAM[3][1] = (Vdp2Regs->CYCB1L >> 8) & 0x0F;
    Vdp2External.AC_VRAM[3][2] = (Vdp2Regs->CYCB1L >> 4) & 0x0F;
    Vdp2External.AC_VRAM[3][3] = (Vdp2Regs->CYCB1L >> 0) & 0x0F;
    Vdp2External.AC_VRAM[3][4] = (Vdp2Regs->CYCB1U >> 12) & 0x0F;
    Vdp2External.AC_VRAM[3][5] = (Vdp2Regs->CYCB1U >> 8) & 0x0F;
    Vdp2External.AC_VRAM[3][6] = (Vdp2Regs->CYCB1U >> 4) & 0x0F;
    Vdp2External.AC_VRAM[3][7] = (Vdp2Regs->CYCB1U >> 0) & 0x0F;

    for (i = 0; i < 8; i++) {
      if (Vdp2External.AC_VRAM[2][i] == 0x0E) {
        if (Vdp2External.AC_VRAM[3][i] != 0x0E) {
          cpu_cycle_b--;
        }
        else {
          if (fcnt == 0) {
            cpu_cycle_b--;
          }
        }
      }
      if (Vdp2External.AC_VRAM[3][i] == 0x0F) {
        fcnt++;
      }
    }
    if (fcnt == 0)cpu_cycle_b = 0;
    if (cpu_cycle_b < 0)cpu_cycle_b = 0;
  }
  else {
    Vdp2External.AC_VRAM[3][0] = Vdp2External.AC_VRAM[2][0];
    Vdp2External.AC_VRAM[3][1] = Vdp2External.AC_VRAM[2][1];
    Vdp2External.AC_VRAM[3][2] = Vdp2External.AC_VRAM[2][2];
    Vdp2External.AC_VRAM[3][3] = Vdp2External.AC_VRAM[2][3];
    Vdp2External.AC_VRAM[3][4] = Vdp2External.AC_VRAM[2][4];
    Vdp2External.AC_VRAM[3][5] = Vdp2External.AC_VRAM[2][5];
    Vdp2External.AC_VRAM[3][6] = Vdp2External.AC_VRAM[2][6];
    Vdp2External.AC_VRAM[3][7] = Vdp2External.AC_VRAM[2][7];
  }

  if (cpu_cycle_a == 0) {
    Vdp2External.cpu_cycle_a = 100;
  }
  else if (Vdp2External.cpu_cycle_a == 1) {
    Vdp2External.cpu_cycle_a = 100;
  }
  else {
    Vdp2External.cpu_cycle_a = 80;
  }

  if (cpu_cycle_b == 0) {
    Vdp2External.cpu_cycle_b = 100;
  }
  else if (Vdp2External.cpu_cycle_a == 1) {
    Vdp2External.cpu_cycle_b = 100;
  }
  else {
    Vdp2External.cpu_cycle_b = 80;
  }
}

// 0 .. 60Hz, 1 .. no limit, 2 .. 2x(120Hz)
void VDP2SetFrameLimit(int mode) {
  switch (mode) {
  case 0:
    enableFrameLimit = 1;
    frameLimitShift = 0; // 60Hz
    framecount = 0;
    onesecondticks = 0;
    lastticks = YabauseGetTicks();
    break;
  case 1:
    enableFrameLimit = 0;
    frameLimitShift = 0;
    break;
  case 2:
    enableFrameLimit = 1;
    frameLimitShift = 1; // 120Hz
    framecount = 0;
    onesecondticks = 0;
    lastticks = YabauseGetTicks();
    break;
  default:
    enableFrameLimit = 1;
    frameLimitShift = 0;
    framecount = 0;
    onesecondticks = 0;
    lastticks = YabauseGetTicks();
    break;
  }
  VideoSetSetting(VDP_SETTING_FRAMELIMIT_MODE, mode);
}

void frameSkipAndLimit() {
  if (FrameAdvanceVariable == 0 && enableFrameLimit )
  {
    const u32 fps = (yabsys.IsPal ? 50 : 60) << frameLimitShift ;
    framecount++;
    curticks = YabauseGetTicks();
    if (framecount > fps)
    {
      onesecondticks -= yabsys.tickfreq;
      if (onesecondticks > (s64)( (yabsys.OneFrameTime>>frameLimitShift)  * 4)) {
        onesecondticks = 0;
      }
      framecount = 1;
      // Start the new second from now, not from one frame time ago. Backdating
      // it made the first frame of every second reach its deadline without
      // waiting at all, because diffticks then came out as a whole OneFrameTime
      // against a target of OneFrameTime-1000.
      lastticks = curticks;
    }

    // Signed: onesecondticks is s64 and legitimately goes negative right after
    // the one second boundary (tickfreq is taken off a value that already had
    // 1000 subtracted). Comparing it against a u64 target turned that -1000
    // into a huge unsigned value, so the frame right after every boundary
    // looked like it was already past its deadline and skipped the wait.
    s64 targetTime = ( (s64)(yabsys.OneFrameTime>>frameLimitShift) * (s64)framecount);
    if (framecount == fps) {
      targetTime = (s64)yabsys.tickfreq; // 1sec
    }

    diffticks = curticks - lastticks;

    if ( autoframeskipenab && (onesecondticks + diffticks) > targetTime )
    {
      LOG("Frame skip target:%lu current:%lu", targetTime, (onesecondticks + diffticks));
      // Skip the next frame
      skipnextframe = 1;

      // How many frames should we skip?
      framestoskip = 1;

    }

    // just wait for next vsync
    targetTime -= 1000;
    if ( (onesecondticks + diffticks) < targetTime )
    {

      s64 sleeptime = (targetTime - (onesecondticks + diffticks));
      s64 xcurticks = YabauseGetTicks();
      if (sleeptime-1000 > 0) {
        YabNanosleep(sleeptime-1000);
        // YABA_VDP2_WTRACE: report a sleep that overshot its request by more
        // than 4x + 5ms - the render thread sitting here starves the emu
        // thread waiting on rcv_evqueue and freezes the whole app.
        if (vdp2_wtrace_on()) {
          s64 slept = YabauseGetTicks() - xcurticks;
          if (slept > (sleeptime - 1000) * 4 + 50000)
            printf("[V2W SLEEP] req=%lld got=%lld fc=%u ost=%lld last=%lld\n",
                   (long long)(sleeptime - 1000), (long long)slept,
                   framecount, (long long)onesecondticks,
                   (long long)lastticks);
        }
      }

      for (;;)
      {
        curticks = YabauseGetTicks();
        diffticks = curticks - lastticks;
        if ((onesecondticks + diffticks) >= targetTime)
          break;
      }
      //u64 realstime = YabauseGetTicks() - xcurticks;
      //yprintf("req time %d,real time %d diff = %d", (u32)sleeptime, (u32)realstime, realstime-sleeptime);
    }

    // Accumulate the time that really elapsed. Overshoot past targetTime (the
    // sleep is only accurate to the OS timer granularity) is carried forward,
    // so the next frame waits correspondingly less and the second as a whole
    // still lands on tickfreq - that is what holds the rate at 60Hz. Snapping
    // onesecondticks to targetTime here instead threw the overshoot away, and
    // it then piled up in real time at roughly 125us per frame (~0.75% slow);
    // the once-per-second unthrottled frame above happened to cancel it out.
    onesecondticks += diffticks;
    lastticks = curticks;
  }

}


//////////////////////////////////////////////////////////////////////////////
void vdp2VBlankIN(void) {
   /* this should be done after a frame change or a plot trigger */
   //Vdp1Regs->COPR = 0;
   //printf("COPR = 0 at %d\n", __LINE__);
   /* I'm not 100% sure about this, but it seems that when using manual change
   we should swap framebuffers in the "next field" and thus, clear the CEF...
   now we're lying a little here as we're not swapping the framebuffers. */
   //if (Vdp1External.manualchange) Vdp1Regs->EDSR >>= 1;

   // YABA_VDP2_WTRACE: time each phase; a phase blocking for 250ms+ on the
   // render thread stalls the emu thread's rcv_evqueue wait (whole-app
   // freeze), so report which phase it was.
   {
     s64 t0 = YabauseGetTicks();
     VIDCore->Vdp2DrawEnd();
     s64 t1 = YabauseGetTicks();
     frameSkipAndLimit();
     s64 t2 = YabauseGetTicks();
     VIDCore->Sync();
     s64 t3 = YabauseGetTicks();
     if (vdp2_wtrace_on() && (t3 - t0) > 2500000)
       printf("[V2W VIN-SLOW] drawend=%lld limit=%lld sync=%lld ticks\n",
              (long long)(t1 - t0), (long long)(t2 - t1),
              (long long)(t3 - t2));
   }
   Vdp2Regs->TVSTAT |= 0x0008;

   ScuSendVBlankIN();

   //if (yabsys.IsSSH2Running)
   //   SH2SendInterrupt(SSH2, 0x43, 0x6);
   FrameProfileAdd("VIN flag");
   FRAMELOG("**** VIN(T) *****\n");
   YabAddEventQueue(rcv_evqueue, 0);


}

//////////////////////////////////////////////////////////////////////////////

// Whether the latch decided this field swaps the frame buffer. Written by
// the emulation thread in the latch, read by the render thread's frame
// skip heuristics (a plain flag read; a one-field skew is harmless there).
static volatile u32 vdp1_latched_swap = 0;

// Async only. Set when the VDP1 frame change of the upcoming field was
// already latched at the VBlank-OUT boundary (instant-list case below);
// the regular hand-off latch then skips its own latch for that field.
static int vdp1_latched_this_frame = 0;

// Latch the VDP1 frame change: consume manual change / one cycle mode,
// derive the plot trigger, shift EDSR on a swap and schedule the modelled
// draw-end as a scanline countdown (vdp1_drawend_lines, fired from
// Vdp2HBlankOUT decoupled from the render thread rendez-vous).
//
// Phase matters, and three variants are known wrong:
// - Firing the draw-end from the render rendez-vous a whole VBlank after
//   the hand-off pushed every draw-end a frame late; a game that arms its
//   next frame from the draw-end IRQ then missed every other swap (Sonic
//   Jam (Europe) ran at half its swap rate).
// - Latching at VBlank-IN cleared CEF a whole VBlank early. Astal samples
//   EDSR during VBlank and relies on CEF surviving into the next field.
// - Latching at the VBlank-OUT boundary before the VBlank-OUT interrupt
//   cleared CEF before the game's VBlank-OUT handler could see it. Astal
//   reads EDSR right after VBlank-OUT and only pumps its next manual
//   frame change when it sees CEF still set, so it froze fading to the
//   title screen.
//
// Therefore the regular latch runs at the render hand-off (the line-1
// block in Vdp2HBlankOUT), which is after the game's VBlank-OUT handler
// ran - matching where the culprit-free legacy code latched. The single
// exception is a list the VDP1 finishes instantly (estimate 0): its
// draw-end must be pending together with the VBlank-OUT interrupt itself,
// before mainline code resumes, so Vdp2VBlankOUT pre-latches exactly that
// case at the boundary (see the est == 0 branch below and the caller).
//
// Runs on the emulation thread with the render thread joined (idle), so
// the Vdp1External flags are safe to touch.
static void Vdp1FrameChangeLatch(void) {
  // Manual change armed by an FBCR write before this point.
  if (Vdp1External.manualchange == 1) {
    Vdp1External.swap_frame_buffer = 1;
    Vdp1External.manualchange = 0;
  }

  // One cycle mode changes the frame buffer every field.
  if ((Vdp1Regs->FBCR & 0x03) == 0x00 ||
    (Vdp1Regs->FBCR & 0x03) == 0x01) {  // 0x01 is treated as one cyscle mode in Sonic R.
    Vdp1External.swap_frame_buffer = 1;
  }

  // Plot trigger mode = Draw when frame is changed
  if (Vdp1Regs->PTMR == 2) {
    Vdp1External.frame_change_plot = 1;
    FRAMELOG("frame_change_plot 1");
  }
  else {
    Vdp1External.frame_change_plot = 0;
    FRAMELOG("frame_change_plot 0");
  }


  if (Vdp1External.swap_frame_buffer == 1) {
    Vdp1Regs->EDSR >>= 1;
    if (Vdp1External.frame_change_plot == 1) {
      int est = Vdp1EstimateDrawLines(Vdp1Ram, Vdp1Regs);
      if (est == 0) {
        // A list the VDP1 finishes instantly (first command is END, or a
        // trivial draw): real hardware completes it within the frame
        // switch itself, so the draw-end must be pending together with
        // the VBlank-OUT interrupt and handled before mainline code
        // resumes. Any delay (the culprit build fired it a line later,
        // an earlier attempt floored it to 45 lines) drops the IRQ into
        // the master/slave restart handshake Die Hard Trilogy runs right
        // after VBlank-OUT and deadlocks the boot (GitLab #136).
        Vdp1Regs->EDSR |= 2;
        ScuSendDrawEnd();
        FRAMELOG("Vdp1Draw end immediate (empty list) EDSR=%02X", Vdp1Regs->EDSR);
      }
      else {
        vdp1_drawend_lines = est;
        FRAMELOG("SET Vdp1 end in %d lines", vdp1_drawend_lines);
      }
    }
  }

  vdp1_latched_swap = Vdp1External.swap_frame_buffer;
}

//////////////////////////////////////////////////////////////////////////////
void Vdp2VBlankIN(void) {
  FRAMELOG("***** VIN *****");
  v2w_frame();

  if (VdpIsAsyncRendering()) {
    FrameProfileAdd("VIN event");
    YabAddEventQueue(evqueue,VDPEV_VBLANK_IN);

    // sync with the render thread's VBlank-IN handling
    YabWaitEventQueue(rcv_evqueue);
    FrameProfileAdd("VIN sync");
  }
  else {
    FrameProfileAdd("VIN start");
    /* this should be done after a frame change or a plot trigger */
    //Vdp1Regs->COPR = 0;

    /* I'm not 100% sure about this, but it seems that when using manual change
    we should swap framebuffers in the "next field" and thus, clear the CEF...
    now we're lying a little here as we're not swapping the framebuffers. */
    //if (Vdp1External.manualchange) Vdp1Regs->EDSR >>= 1;

    VIDCore->Vdp2DrawEnd();
    frameSkipAndLimit();
    VIDCore->Sync();
    Vdp2Regs->TVSTAT |= 0x0008;

    ScuSendVBlankIN();

    FrameProfileAdd("VIN end");
  }
}

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

// Assert the TVSTAT HBLANK status bit ahead of the HBlankIN interrupt point.
// On real hardware horizontal blanking lasts about a quarter of a line, while
// the HBLANK-IN interrupt is raised once at its start. If the status bit were
// only raised together with the interrupt it would stay set for a single
// deciline (~17 SH2 cycles), and the CPU spends that whole window entering the
// HBLANK interrupt handler. A game that busy-polls TVSTAT for an HBLANK edge
// would then never observe the bit and would hang forever. Raising the status
// bit a few decilines early gives such polling loops a realistic window while
// leaving the interrupt timing and the per-line register latch untouched.
void Vdp2HBlankStatusIN(void) {
  if (yabsys.LineCount < yabsys.VBlankLineCount) {
    Vdp2Regs->TVSTAT |= 0x0004;
  }
}

void Vdp2HBlankIN(void) {

  if (yabsys.LineCount < yabsys.VBlankLineCount) {
    Vdp2Regs->TVSTAT |= 0x0004;
    ScuSendHBlankIN();
    //if (yabsys.IsSSH2Running)
    //  SH2SendInterrupt(SSH2, 0x42, 0x2);
  }
}

using std::atomic;
extern atomic<int> vdp1_clock;


void Vdp2HBlankOUT(void) {
  int i;
  if (yabsys.LineCount < yabsys.VBlankLineCount)
  {
    ScuRemoveHBlankIN();

    Vdp2Regs->TVSTAT &= ~0x0004;
    u32 cell_scroll_table_start_addr = (Vdp2Regs->VCSTA.all & 0x7FFFE) << 1;
    memcpy(Vdp2Lines + yabsys.LineCount, Vdp2Regs, sizeof(Vdp2));
    for (i = 0; i < 88; i++)
    {
      cell_scroll_data[yabsys.LineCount].data[i] = Vdp2RamReadLong(cell_scroll_table_start_addr + i * 4);
    }


    if ((Vdp2Lines[0].BGON & 0x01) != (Vdp2Lines[yabsys.LineCount].BGON & 0x01)){
      *Vdp2External.perline_alpha |= 0x1;
    }
    else if ((Vdp2Lines[0].CCRNA & 0x00FF) != (Vdp2Lines[yabsys.LineCount].CCRNA & 0x00FF)){
      *Vdp2External.perline_alpha |= 0x1;
    }

    if ((Vdp2Lines[0].BGON & 0x02) != (Vdp2Lines[yabsys.LineCount].BGON & 0x02)){
      *Vdp2External.perline_alpha |= 0x2;
    }
    else if ((Vdp2Lines[0].CCRNA & 0xFF00) != (Vdp2Lines[yabsys.LineCount].CCRNA & 0xFF00)){
      *Vdp2External.perline_alpha |= 0x2;
    }

    if ((Vdp2Lines[0].BGON & 0x04) != (Vdp2Lines[yabsys.LineCount].BGON & 0x04)){
      *Vdp2External.perline_alpha |= 0x4;
    }
    else if ((Vdp2Lines[0].CCRNB & 0xFF00) != (Vdp2Lines[yabsys.LineCount].CCRNB & 0xFF00)){
      *Vdp2External.perline_alpha |= 0x4;
    }

    if ((Vdp2Lines[0].BGON & 0x08) != (Vdp2Lines[yabsys.LineCount].BGON & 0x08)){
      *Vdp2External.perline_alpha |= 0x8;
    }
    else if ((Vdp2Lines[0].CCRNB & 0x00FF) != (Vdp2Lines[yabsys.LineCount].CCRNB & 0x00FF)){
      *Vdp2External.perline_alpha |= 0x8;
    }

    if ((Vdp2Lines[0].BGON & 0x10) != (Vdp2Lines[yabsys.LineCount].BGON & 0x10)){
      *Vdp2External.perline_alpha |= 0x10;
    }
    else if (Vdp2Lines[0].CCRR != Vdp2Lines[yabsys.LineCount].CCRR){
      *Vdp2External.perline_alpha |= 0x10;
    }

    if (Vdp2Lines[0].COBR != Vdp2Lines[yabsys.LineCount].COBR){

      *Vdp2External.perline_alpha |= Vdp2Lines[yabsys.LineCount].CLOFEN;
    }
    if (Vdp2Lines[0].COAR != Vdp2Lines[yabsys.LineCount].COAR){

      *Vdp2External.perline_alpha |= Vdp2Lines[yabsys.LineCount].CLOFEN;
    }

    if (Vdp2Lines[0].CLOFSL != Vdp2Lines[yabsys.LineCount].CLOFSL) {

      *Vdp2External.perline_alpha |= Vdp2Lines[yabsys.LineCount].CLOFEN;
    }

    if (Vdp2Lines[0].PRISA != Vdp2Lines[yabsys.LineCount].PRISA) {

      *Vdp2External.perline_alpha |= 0x40;
    }

    if ( Vdp2Lines[0].SCYN2 != Vdp2Lines[yabsys.LineCount].SCYN2  ||  Vdp2Lines[0].SCXN2 != Vdp2Lines[yabsys.LineCount].SCXN2 ) {

      *Vdp2External.perline_alpha |= 0x100;
    }

    if ( Vdp2Lines[0].SCYN3 != Vdp2Lines[yabsys.LineCount].SCYN3  ||  Vdp2Lines[0].SCXN3 != Vdp2Lines[yabsys.LineCount].SCXN3 ) {

      *Vdp2External.perline_alpha |= 0x80;
    }


    if (Vdp2Lines[0].PRINA != Vdp2Lines[yabsys.LineCount].PRINA) {
      //printf("Perline priority");
    }
  }

  if (yabsys.LineCount == 1) {
    VDP2genVRamCyclePattern();
    Vdp2External.frame_render_flg = 0;
  }

  if (Vdp2External.frame_render_flg == 0 && vdp1_clock>0 ){ // Delay if vdp1 ram was written
    FrameProfileAdd("VOUT event");
    Vdp2External.frame_render_flg = 1;
    if (VdpIsAsyncRendering()) {
      // Join the previous render event first so at most one VDP1-consuming
      // event is ever in flight - that keeps the done-event bookkeeping of
      // Vdp1JoinPendingRenders() exact, stops the render thread from
      // falling more than one frame behind the command list, and makes the
      // latch below safe against the render thread.
      Vdp1JoinPendingRenders();
      // Latch the frame change here, after the game's VBlank-OUT handler
      // has run (see Vdp1FrameChangeLatch for why the phase matters) -
      // unless the instant-list case was already latched at the VBlank-OUT
      // boundary by Vdp2VBlankOUT.
      if (!vdp1_latched_this_frame) {
        Vdp1FrameChangeLatch();
      }
      vdp1_latched_this_frame = 0;
      FRAMELOG("YabAddEventQueue(evqueue, VDPEV_VBLANK_OUT)");
      YabAddEventQueue(evqueue, VDPEV_VBLANK_OUT);
      Vdp1MarkRenderDispatched();
      YabThreadYield();
    }
    else {
      // The sync path latches the frame change here and renders inline
      // below. The async path latches at the hardware phase instead, see
      // Vdp1FrameChangeLatch().
      // Manual Change
      if (Vdp1External.manualchange == 1) {
        Vdp1External.swap_frame_buffer = 1;
        Vdp1External.manualchange = 0;
      }

      // One Cyclemode
      if ((Vdp1Regs->FBCR & 0x03) == 0x00 ||
        (Vdp1Regs->FBCR & 0x03) == 0x01) {  // 0x01 is treated as one cyscle mode in Sonic R.
        Vdp1External.swap_frame_buffer = 1;
      }

      // Plot trigger mode = Draw when frame is changed
      if (Vdp1Regs->PTMR == 2) {
        Vdp1External.frame_change_plot = 1;
        FRAMELOG("frame_change_plot 1");
      }
      else {
        Vdp1External.frame_change_plot = 0;
        FRAMELOG("frame_change_plot 0");
      }
      vdp2VBlankOUT();
    }
  }

  if (VdpIsAsyncRendering()) {
    if (vdp1_drawend_lines > 0 && --vdp1_drawend_lines == 0) {
      // Modelled draw-end: the scheduled draw time has elapsed. The join is
      // only a data-ordering barrier - it makes sure the render thread is
      // not still walking the command list before the game is told it may
      // rewrite it. The old code blocked here on the render rendez-vous
      // unconditionally (which deadlocks when no render was dispatched) and
      // skipped the IRQ whenever Vdp1External.status was still RUNNING,
      // silently losing the draw-end and starving draw-end-chained game
      // loops.
      FRAMELOG("**WAIT START %d**", YaGetQueueSize(vdp1_rcv_evqueue));
      Vdp1JoinPendingRenders();
      FRAMELOG("**WAIT END**");
      FrameProfileAdd("DirectDraw sync");
      FRAMELOG("Vdp1Draw end at %d line EDSR=%02X", yabsys.LineCount, Vdp1Regs->EDSR);
      Vdp1Regs->EDSR |= 2;
      ScuSendDrawEnd();
    }
  }
  else {
    if (yabsys.wait_line_count != -1 && yabsys.LineCount == yabsys.wait_line_count) {
      //Vdp1Regs->COPR = Vdp1Regs->addr >> 3;
      if ( Vdp1External.status == VDP1_STATUS_IDLE) {
        ScuSendDrawEnd();
        FRAMELOG("Vdp1Draw end at %d line EDSR=%02X", yabsys.LineCount, Vdp1Regs->EDSR);
        yabsys.wait_line_count = -1;
        Vdp1Regs->EDSR |= 2;
      }
      else {
        yabsys.wait_line_count += 10;
        yabsys.wait_line_count %= yabsys.VBlankLineCount;
      }
      //VIDCore->Vdp1DrawEnd();
    }
  }
  Vdp1_onHblank();
}

//////////////////////////////////////////////////////////////////////////////

Vdp2 * Vdp2RestoreRegs(int line, Vdp2* lines) {
   return line > 270 ? NULL : lines + line;
}

//////////////////////////////////////////////////////////////////////////////
int vdp1_frame = 0;
int show_vdp1_frame = 0;
u32 show_skipped_frame = 0;
static void FPSDisplay(void)
{
  static int fpsframecount = 0;
  static u64 fpsticks;
  //yprintf("%02d/%02d FPS skip=%d vdp1=%02d", fps, yabsys.IsPal ? 50 : 60, show_skipped_frame, show_vdp1_frame);
#if 1 // FPS only
   OSDPushMessage(OSDMSG_FPS, 1, "%02d/%02d FPS skip=%d vdp1=%02d, %d", fps, yabsys.IsPal ? 50 : 60, show_skipped_frame, show_vdp1_frame, yabsys.frame_count);
   //printf("\033[%d;%dH %02d/%02d FPS skip=%d vdp1=%02d \n", 0, 0, fps, yabsys.IsPal ? 50 : 60, show_skipped_frame, show_vdp1_frame);
#else
  FILE * fp = NULL;
  FILE * gup_fp = NULL;
  char fname[128];
  char buf[64];
  int i;
  int cpu_f[8];
  int gpu_f;

  if (gup_fp == NULL){
    gup_fp = fopen("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq", "r");
  }

  if (gup_fp != NULL){
    fread(buf, 1, 64, gup_fp);
    gpu_f = atoi(buf);
    fclose(gup_fp);
  }
  else{
    gpu_f = 0;
  }

  for (i = 0; i < 8; i++){
    sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
    fp = fopen(fname, "r");
    if (fp){
      fread(buf, 1, 64, fp);
      cpu_f[i] = atoi(buf);
      fclose(fp);
    }
    else{
      cpu_f[i] = 0;
    }
  }


  OSDPushMessage(OSDMSG_FPS, 1, "%02d/%02d FPS , gpu = %d, cpu0 = %d, cpu1 = %d, cpu2 = %d, cpu3 = %d, cpu4 = %d, cpu5 = %d, cpu6 = %d, cpu7 = %d"
    , fps, yabsys.IsPal ? 50 : 60, gpu_f / 1000000,
    cpu_f[0] / 1000, cpu_f[1] / 1000, cpu_f[2] / 1000, cpu_f[3] / 1000,
    cpu_f[4] / 1000, cpu_f[5] / 1000, cpu_f[6] / 1000, cpu_f[7] / 1000);
#endif
  //OSDPushMessage(OSDMSG_DEBUG, 1, "%d %d %s %s", framecounter, lagframecounter, MovieStatus, InputDisplayString);
  fpsframecount++;
  if (YabauseGetTicks() >= fpsticks + yabsys.tickfreq)
  {
    fps = fpsframecount;
    fpsframecount = 0;
    show_vdp1_frame = vdp1_frame;
    vdp1_frame = 0;
    show_skipped_frame = skipped_frame;
    skipped_frame = 0;
    fpsticks = YabauseGetTicks();
  }
}

//////////////////////////////////////////////////////////////////////////////

void SpeedThrottleEnable(void) {
  throttlespeed = 1;
}

//////////////////////////////////////////////////////////////////////////////

void SpeedThrottleDisable(void) {
  throttlespeed = 0;
}

void dumpvram() {
  FILE * fp = fopen("vdp2vram.bin", "wb");
  fwrite(Vdp2Regs, sizeof(Vdp2), 1, fp);
  fwrite(Vdp2Ram, 0x80000, 1, fp);
  fwrite(Vdp2ColorRam, 0x1000, 1, fp);
  fwrite(&Vdp2Internal, sizeof(Vdp2Internal_struct), 1, fp);
  fwrite((void *)Vdp1Regs, sizeof(Vdp1), 1, fp);
  fwrite((void *)Vdp1Ram, 0x80000, 1, fp);
  fwrite(&Vdp1External, sizeof(Vdp1External_struct), 1, fp);
  fclose(fp);
}

void restorevram() {
  FILE * fp = fopen("vdp2vram.bin", "rb");
  fread(Vdp2Regs, sizeof(Vdp2), 1, fp);
  fread(Vdp2Ram, 0x80000, 1, fp);
  fread(Vdp2ColorRam, 0x1000, 1, fp);
  fread(&Vdp2Internal, sizeof(Vdp2Internal_struct), 1, fp);
  fread((void *)Vdp1Regs, sizeof(Vdp1), 1, fp);
  fread((void *)Vdp1Ram, 0x80000, 1, fp);
  fread(&Vdp1External, sizeof(Vdp1External_struct), 1, fp);
  fclose(fp);

  for (int i = 0; i < 0x1000; i += 2) {
    VIDCore->OnUpdateColorRamWord(i);
  }
}

int g_vdp_debug_dmp = 0;

void vdp2ReqDump() {
  g_vdp_debug_dmp = 2;
}

void vdp2ReqRestore() {
  g_vdp_debug_dmp = 1;
}


//////////////////////////////////////////////////////////////////////////////

// VDP1 frame operations for one field: VBlank erase, frame change and the
// plot kick. Runs on the render thread, called from vdp2VBlankOUT. In the
// async build the flags it consumes were latched on the emulation thread
// (Vdp1FrameChangeLatch) before the VDPEV_VBLANK_OUT dispatch. Returns
// nonzero when a draw was started.
static int vdp2Vdp1FrameOps(void) {
  int isrender = 0;

  // VBlank Erase
  if (Vdp1External.vbalnk_erase ||  // VBlank Erace (VBE1)
    ((Vdp1Regs->FBCR & 2) == 0)) {  // One cycle mode
    VIDCore->Vdp1EraseWrite();
  }

  // Frame Change
  if (Vdp1External.swap_frame_buffer == 1)
  {
    vdp1_frame++;
    if (Vdp1External.manualerase) {  // Manual Erace (FCM1 FCT0) Just before frame changing
      VIDCore->Vdp1EraseWrite();
      Vdp1External.manualerase = 0;
    }

    FRAMELOG("Vdp1FrameChange swap=%d,plot=%d*****", Vdp1External.swap_frame_buffer, Vdp1External.frame_change_plot);
    VIDCore->Vdp1FrameChange();
    Vdp1External.current_frame = !Vdp1External.current_frame;
    Vdp1External.swap_frame_buffer = 0;
    if (!VdpIsAsyncRendering()) {
      // Async shifts EDSR in Vdp1FrameChangeLatch() on the emulation
      // thread instead.
      Vdp1Regs->EDSR >>= 1;
    }

    FRAMELOG("[VDP1] Displayed framebuffer changed. EDSR=%02X", Vdp1Regs->EDSR);

    // if Plot Trigger mode == 0x02 draw start
    if (Vdp1External.frame_change_plot == 1 || Vdp1External.status == VDP1_STATUS_RUNNING ){
      FRAMELOG("[VDP1] frame_change_plot == 1 start drawing immidiatly", Vdp1Regs->EDSR);
      LOG("[VDP1] Start Drawing %d", yabsys.LineCount);
      Vdp1Regs->addr = 0;
      Vdp1Regs->COPR = 0;
      Vdp1Draw();
      LOG("[VDP1] End Drawing %d", yabsys.LineCount);
      isrender = 1;
    }
  }
  else {
    // Continue from previus frame
    if ( Vdp1External.status == VDP1_STATUS_RUNNING) {
      LOG("[VDP1] Start Drawing continue");
      Vdp1Draw();
      isrender = 1;
    }
  }
  return isrender;
}

//////////////////////////////////////////////////////////////////////////////
void vdp2VBlankOUT(void) {
  static VideoInterface_struct * saved = NULL;
  int isrender = 0;
#if PROFILE_RENDERING
  s64 starttime = YabauseGetTicks();
#endif
  VdpLockVram();
  FRAMELOG("***** VOUT(T) swap=%d,plot=%d,vdp1status=%d*****", Vdp1External.swap_frame_buffer, Vdp1External.frame_change_plot, Vdp1External.status );

#if _DEBUG
  if (g_vdp_debug_dmp == 1) {
    g_vdp_debug_dmp = 0;
    restorevram();
  }

  if (g_vdp_debug_dmp == 2) {
    g_vdp_debug_dmp = 0;
    dumpvram();
    Vdp2GenerateCCode();
  }
#endif

  // Async: the swap flag itself is consumed by vdp2Vdp1FrameOps() below,
  // so the skip heuristics read the per-field value the latch recorded.
  const u32 cur_swap = VdpIsAsyncRendering() ? vdp1_latched_swap
                                             : Vdp1External.swap_frame_buffer;
  if (pre_swap_frame_buffer == 0 && skipnextframe && cur_swap ){
    skipnextframe = 0;
    previous_skipped = 0;
    framestoskip = 1;
  }

  if (previous_skipped != 0 && skipnextframe != 0) {
    skipnextframe = 0;
    previous_skipped = 0;
    framestoskip = 1;
  }

  pre_swap_frame_buffer = cur_swap;


  if (skipnextframe && (!saved))
  {
    skipped_frame++;
    saved = VIDCore;

    previous_skipped = 1;
    VIDCore->Vdp2DrawStart = VIDDummy.Vdp2DrawStart;
    VIDCore->Vdp2DrawEnd   = VIDDummy.Vdp2DrawEnd;
    VIDCore->Vdp2DrawScreens = VIDDummy.Vdp2DrawScreens;

  }
  else if (saved && (!skipnextframe))
  {
    skipnextframe = 0;
    previous_skipped = 0;
    //VIDCore = saved;
    if( saved != NULL ){
#if defined(HAVE_VULKAN)
      if (VIDCore->id == VIDCORE_VULKAN) {

        VIDCore->Vdp2DrawStart = VIDVulkanVdp2DrawStart;
        VIDCore->Vdp2DrawEnd = VIDVulkanVdp2DrawEnd;
        VIDCore->Vdp2DrawScreens = VIDVulkanVdp2DrawScreens;
#else
      if (0) {
#endif
      }
      else if (VIDCore->id == VIDCORE_OGL) {
        VIDCore->Vdp2DrawStart = VIDOGLVdp2DrawStart;
        VIDCore->Vdp2DrawEnd = VIDOGLVdp2DrawEnd;
        VIDCore->Vdp2DrawScreens = VIDOGLVdp2DrawScreens;
      }
      else if (VIDCore->id == VIDCORE_SOFT ) {
        VIDCore->Vdp2DrawStart = VIDSoftVdp2DrawStart;
        VIDCore->Vdp2DrawEnd = VIDSoftVdp2DrawEnd;
        VIDCore->Vdp2DrawScreens = VIDSoftVdp2DrawScreens;
      }
    }
    saved = NULL;
  }

  VIDCore->Vdp2DrawStart();

  isrender = vdp2Vdp1FrameOps();

  // Async: completion is reported through Vdp1MarkRenderRetired() in
  // VdpProc after this handler returns; see the counter pair above.

  if (Vdp2Regs->TVMD & 0x8000) {
     FRAMELOG("Vdp2DrawScreens Start %d", yabsys.LineCount);
    VIDCore->Vdp2DrawScreens();
    FRAMELOG("Vdp2DrawScreens End %d", yabsys.LineCount);
  }

  if (isrender){
     FRAMELOG("Vdp1DrawEnd %d", yabsys.LineCount);
    VIDCore->Vdp1DrawEnd();
    // Sync only. Delay the draw-end interrupt by an estimate of the real
    // VDP1 draw time instead of a flat 45 lines. On real hardware a
    // frame-change plot starts at the frame swap (VBlank-IN), which is a
    // whole VBlank period before this VBlank-OUT anchored code runs, so a
    // draw that fits into VBlank has already finished by the time we get
    // here. (Async models the draw-end with vdp1_drawend_lines instead.)
    if (!VdpIsAsyncRendering()) {
      const int est = Vdp1EstimateDrawLines(Vdp1Ram, Vdp1Regs);
      int lines = est - (yabsys.MaxLineCount - yabsys.VBlankLineCount);
      if (lines <= 0 && Vdp1External.status == VDP1_STATUS_IDLE) {
        // Draw already complete. Raising the interrupt right here would
        // put it immediately after the VBlank-OUT interrupt, so mainline
        // code gets no window at all between VBlank-OUT and draw-end -
        // Jikkyou Oshaberi Parodius drives its main loop off exactly that
        // window (draw-end -> VBlank-IN -> VBlank-OUT -> mainline) and
        // never resumes. Schedule it at the phase hardware uses instead,
        // just before the next VBlank-IN, which keeps it ahead of the
        // mainline resume that Die Hard Trilogy depends on.
        yabsys.wait_line_count =
            (yabsys.VBlankLineCount + est) % yabsys.MaxLineCount;
      }
      else {
        if (lines <= 0) lines = 1;
        // A draw that spills past VBlank must not raise its draw-end
        // earlier than the legacy 45-line delay. The estimator costs
        // textured texels at 1 cycle each, but the real VDP1 pays at
        // least a texture read plus a framebuffer write per texel, so
        // large textured scenes (X-Men vs. Street Fighter's FMV sprite,
        // est=42 -> lines=4) finished 40+ lines earlier than both the
        // legacy timing and the real hardware, and the movie player
        // deadlocked waiting for its expected frame pacing. Tiny lists
        // that fit into VBlank are unaffected (branch above), which is
        // all Die Hard Trilogy and Parodius need.
        if (lines < 45) lines = 45;
        yabsys.wait_line_count += lines;
        yabsys.wait_line_count %= yabsys.VBlankLineCount;
      }
    }
  }


   FPSDisplay();
#if 1
   //if ((Vdp1Regs->FBCR & 2) && (Vdp1Regs->TVMR & 8))
   //   Vdp1External.manualerase = 1;

   if ( skipnextframe == 0)
   {
      framesskipped = 0;

      if (framestoskip > 0)
         skipnextframe = 1;
   }
   else
   {
      framestoskip--;

      if (framestoskip < 1)
         skipnextframe = 0;
      else
         skipnextframe = 1;

      framesskipped++;
   }

#endif
   VdpUnLockVram();
#if PROFILE_RENDERING
   {
       // 1 Hz aggregation. Per-frame logging floods logcat / frame.csv on
       // Adreno where each frame may emit several MB of profiler output.
       // Accumulate frames + render time over yabsys.tickfreq (= 1 second
       // in YabauseGetTicks units) and emit one summary line.
       static s64  s_windowStart = 0;
       static u32  s_frames      = 0;
       static u64  s_renderAccum = 0;
       static u32  s_renderMax   = 0;
       s64 endTicks   = YabauseGetTicks();
       u32 frameTime  = (u32)(endTicks - starttime);
       if (s_windowStart == 0) s_windowStart = endTicks;
       s_frames++;
       s_renderAccum += frameTime;
       if (frameTime > s_renderMax) s_renderMax = frameTime;
       if ((u64)(endTicks - s_windowStart) >= yabsys.tickfreq) {
           u32 avg = (u32)(s_renderAccum / s_frames);
#if defined(ANDROID)
           printf("YAB_PROFILE fps=%u frame=%d avgRender=%u maxRender=%u\n",
                  s_frames, g_frame_count, avg, s_renderMax);
#else
           static FILE * framefp = NULL;
           if (framefp == NULL) framefp = fopen("frame.csv", "w");
           if (framefp != NULL) {
               fprintf(framefp, "%d,%u,%u,%u\n",
                       g_frame_count, s_frames, avg, s_renderMax);
               fflush(framefp);
           }
#endif
           s_windowStart = endTicks;
           s_frames = 0;
           s_renderAccum = 0;
           s_renderMax = 0;
       }
   }
#endif
}

//////////////////////////////////////////////////////////////////////////////
void Vdp2VBlankOUT(void) {
  g_frame_count++;

  //if (g_frame_count == 60){
  //  YabSaveStateSlot(".\\", 1);
  //}

  //if (g_frame_count >= 1){
  //  YabLoadStateSlot(".\\", 1);
  //}

  FRAMELOG("***** VOUT %d *****", g_frame_count);
  if (Vdp2External.perline_alpha == &Vdp2External.perline_alpha_a){
    Vdp2External.perline_alpha = &Vdp2External.perline_alpha_b;
    Vdp2External.perline_alpha_draw = &Vdp2External.perline_alpha_a;
    *Vdp2External.perline_alpha = 0;
  }
  else{
    Vdp2External.perline_alpha = &Vdp2External.perline_alpha_a;
    Vdp2External.perline_alpha_draw = &Vdp2External.perline_alpha_b;
    *Vdp2External.perline_alpha = 0;
  }

  if (((Vdp1Regs->TVMR >> 3) & 0x01) == 1){  // VBlank Erace (VBE1)
    Vdp1External.vbalnk_erase = 1;
  }else{
    Vdp1External.vbalnk_erase = 0;
  }

#ifdef _VDP_PROFILE_
  FrameProfileShow();
  FrameProfileInit();
#endif

   if (((Vdp2Regs->TVMD >> 6) & 0x3) == 0){
     vdp2_is_odd_frame = 1;
   }else{ // p02_50.htm#TVSTAT_
     if (vdp2_is_odd_frame)
       vdp2_is_odd_frame = 0;
     else
       vdp2_is_odd_frame = 1;
   }

   Vdp2Regs->TVSTAT = ((Vdp2Regs->TVSTAT & ~0x0008) & ~0x0002) | (vdp2_is_odd_frame << 1);

   // Async only. Instant-list pre-latch: when the upcoming frame change
   // would start a plot that the VDP1 finishes instantly, the draw-end
   // interrupt must be pending together with the VBlank-OUT interrupt
   // sent below, before mainline code resumes (Die Hard Trilogy, GitLab
   // #136). All other fields latch at the render hand-off in
   // Vdp2HBlankOUT instead, after the game's VBlank-OUT handler ran
   // (Astal requires CEF to survive into that handler). The peek must
   // not consume any state - the real latch does that.
   if (VdpIsAsyncRendering()) {
     int would_swap = (Vdp1External.manualchange == 1) ||
                      ((Vdp1Regs->FBCR & 0x03) == 0x00) ||
                      ((Vdp1Regs->FBCR & 0x03) == 0x01);
     if (would_swap && Vdp1Regs->PTMR == 2) {
       Vdp1JoinPendingRenders();
       if (Vdp1EstimateDrawLines(Vdp1Ram, Vdp1Regs) == 0) {
         Vdp1FrameChangeLatch();
         vdp1_latched_this_frame = 1;
       }
     }
   }

   ScuSendVBlankOUT();

   if (Vdp2Regs->EXTEN & 0x200) // Should be revised for accuracy(should occur only occur on the line it happens at, etc.)
   {
      // Only Latch if EXLTEN is enabled
      if (SmpcRegs->EXLE & 0x1)
         Vdp2SendExternalLatch((PORTDATA1.data[3]<<8)|PORTDATA1.data[4], (PORTDATA1.data[5]<<8)|PORTDATA1.data[6]);
   }
}

//////////////////////////////////////////////////////////////////////////////

void Vdp2UpdateHv( int hcnt, int line ){
   Vdp2Regs->HCNT = (yabsys.Hcount*hcnt) << 2;
   Vdp2Regs->VCNT = line;
}


void Vdp2SendExternalLatch(int hcnt, int vcnt)
{
   Vdp2Regs->HCNT = hcnt << 1;
   Vdp2Regs->VCNT = vcnt;
   Vdp2Regs->TVSTAT |= 0x200;
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp2ReadByte(u32 addr) {
   LOG("VDP2 register byte read = %08X\n", addr);
   addr &= 0x1FF;
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp2ReadWord(u32 addr) {
   addr &= 0x1FF;

   switch (addr)
   {
   case 0x000:
     return Vdp2Regs->TVMD;
   case 0x002:
     if (!(Vdp2Regs->EXTEN & 0x200))
     {
       // Latch HV counter on read
       // Vdp2Regs->HCNT = ?;
       Vdp2Regs->VCNT = yabsys.LineCount;
       Vdp2Regs->TVSTAT |= 0x200;
     }

     return Vdp2Regs->EXTEN;
   case 0x004:
   {
     u16 tvstat = Vdp2Regs->TVSTAT;

     // Clear External latch and sync flags
     Vdp2Regs->TVSTAT &= 0xFCFF;

     // if TVMD's DISP bit is cleared, TVSTAT's VBLANK bit is always set
     if (Vdp2Regs->TVMD & 0x8000)
       return tvstat;
     else
       return (tvstat | 0x8);
   }
   case 0x006:
     return Vdp2Regs->VRSIZE;
   case 0x008:
     LOG("HCNT = %d VCNT = %d\n", Vdp2Regs->HCNT, Vdp2Regs->VCNT);
     return Vdp2Regs->HCNT;
   case 0x00A:
     return Vdp2Regs->VCNT;
   case 0x00C:
     return 0 ;
   case 0x00E:
     return Vdp2Regs->RAMCTL;
   case 0x010:
     return Vdp2Regs->CYCA0L ;

   case 0x012:
     return Vdp2Regs->CYCA0U ;

   case 0x014:
     return Vdp2Regs->CYCA1L ;

   case 0x016:
     return Vdp2Regs->CYCA1U ;

   case 0x018:
     return Vdp2Regs->CYCB0L ;

   case 0x01A:
     return Vdp2Regs->CYCB0U ;

   case 0x01C:
     return Vdp2Regs->CYCB1L ;

   case 0x01E:
     return Vdp2Regs->CYCB1U ;

   case 0x020:
     return Vdp2Regs->BGON ;

   case 0x022:
     return Vdp2Regs->MZCTL ;

   case 0x024:
     return Vdp2Regs->SFSEL ;

   case 0x026:
     return Vdp2Regs->SFCODE ;

   case 0x028:
     return Vdp2Regs->CHCTLA ;

   case 0x02A:
     return Vdp2Regs->CHCTLB ;

   case 0x02C:
     return Vdp2Regs->BMPNA ;

   case 0x02E:
     return Vdp2Regs->BMPNB ;

   case 0x030:
     return Vdp2Regs->PNCN0 ;

   case 0x032:
     return Vdp2Regs->PNCN1 ;

   case 0x034:
     return Vdp2Regs->PNCN2 ;

   case 0x036:
     return Vdp2Regs->PNCN3 ;

   case 0x038:
     return Vdp2Regs->PNCR ;

   case 0x03A:
     return Vdp2Regs->PLSZ ;

   case 0x03C:
     return Vdp2Regs->MPOFN ;

   case 0x03E:
     return Vdp2Regs->MPOFR ;

   case 0x040:
     return Vdp2Regs->MPABN0 ;

   case 0x042:
     return Vdp2Regs->MPCDN0 ;

   case 0x044:
     return Vdp2Regs->MPABN1 ;

   case 0x046:
     return Vdp2Regs->MPCDN1 ;

   case 0x048:
     return Vdp2Regs->MPABN2 ;

   case 0x04A:
     return Vdp2Regs->MPCDN2 ;

   case 0x04C:
     return Vdp2Regs->MPABN3 ;

   case 0x04E:
     return Vdp2Regs->MPCDN3 ;

   case 0x050:
     return Vdp2Regs->MPABRA ;

   case 0x052:
     return Vdp2Regs->MPCDRA ;

   case 0x054:
     return Vdp2Regs->MPEFRA ;

   case 0x056:
     return Vdp2Regs->MPGHRA ;

   case 0x058:
     return Vdp2Regs->MPIJRA ;

   case 0x05A:
     return Vdp2Regs->MPKLRA ;

   case 0x05C:
     return Vdp2Regs->MPMNRA ;

   case 0x05E:
     return Vdp2Regs->MPOPRA ;

   case 0x060:
     return Vdp2Regs->MPABRB ;

   case 0x062:
     return Vdp2Regs->MPCDRB ;

   case 0x064:
     return Vdp2Regs->MPEFRB ;

   case 0x066:
     return Vdp2Regs->MPGHRB ;

   case 0x068:
     return Vdp2Regs->MPIJRB ;

   case 0x06A:
     return Vdp2Regs->MPKLRB ;

   case 0x06C:
     return Vdp2Regs->MPMNRB ;

   case 0x06E:
     return Vdp2Regs->MPOPRB ;

   case 0x070:
     return Vdp2Regs->SCXIN0 ;

   case 0x072:
     return Vdp2Regs->SCXDN0 ;

   case 0x074:
     return Vdp2Regs->SCYIN0 ;

   case 0x076:
     return Vdp2Regs->SCYDN0 ;

   case 0x078:
     return Vdp2Regs->ZMXN0.part.I ;

   case 0x07A:
     return Vdp2Regs->ZMXN0.part.D ;

   case 0x07C:
     return Vdp2Regs->ZMYN0.part.I ;

   case 0x07E:
     return Vdp2Regs->ZMYN0.part.D ;

   case 0x080:
     return Vdp2Regs->SCXIN1 ;

   case 0x082:
     return Vdp2Regs->SCXDN1 ;

   case 0x084:
     return Vdp2Regs->SCYIN1 ;

   case 0x086:
     return Vdp2Regs->SCYDN1 ;

   case 0x088:
     return Vdp2Regs->ZMXN1.part.I ;

   case 0x08A:
     return Vdp2Regs->ZMXN1.part.D ;

   case 0x08C:
     return Vdp2Regs->ZMYN1.part.I ;

   case 0x08E:
     return Vdp2Regs->ZMYN1.part.D ;

   case 0x090:
     return Vdp2Regs->SCXN2 ;

   case 0x092:
     return Vdp2Regs->SCYN2 ;

   case 0x094:
     return Vdp2Regs->SCXN3 ;

   case 0x096:
     return Vdp2Regs->SCYN3 ;

   case 0x098:
     return Vdp2Regs->ZMCTL ;

   case 0x09A:
     return Vdp2Regs->SCRCTL ;

   case 0x09C:
     return Vdp2Regs->VCSTA.part.U ;

   case 0x09E:
     return Vdp2Regs->VCSTA.part.L ;

   case 0x0A0:
     return Vdp2Regs->LSTA0.part.U ;

   case 0x0A2:
     return Vdp2Regs->LSTA0.part.L ;

   case 0x0A4:
     return Vdp2Regs->LSTA1.part.U ;

   case 0x0A6:
     return Vdp2Regs->LSTA1.part.L ;

   case 0x0A8:
     return Vdp2Regs->LCTA.part.U ;

   case 0x0AA:
     return Vdp2Regs->LCTA.part.L ;

   case 0x0AC:
     return Vdp2Regs->BKTAU ;

   case 0x0AE:
     return Vdp2Regs->BKTAL ;

   case 0x0B0:
     return Vdp2Regs->RPMD ;

   case 0x0B2:
     return Vdp2Regs->RPRCTL ;

   case 0x0B4:
     return Vdp2Regs->KTCTL ;

   case 0x0B6:
     return Vdp2Regs->KTAOF ;

   case 0x0B8:
     return Vdp2Regs->OVPNRA ;

   case 0x0BA:
     return Vdp2Regs->OVPNRB ;

   case 0x0BC:
     return Vdp2Regs->RPTA.part.U ;

   case 0x0BE:
     return Vdp2Regs->RPTA.part.L ;

   case 0x0C0:
     return Vdp2Regs->WPSX0 ;

   case 0x0C2:
     return Vdp2Regs->WPSY0 ;

   case 0x0C4:
     return Vdp2Regs->WPEX0 ;

   case 0x0C6:
     return Vdp2Regs->WPEY0 ;

   case 0x0C8:
     return Vdp2Regs->WPSX1 ;

   case 0x0CA:
     return Vdp2Regs->WPSY1 ;

   case 0x0CC:
     return Vdp2Regs->WPEX1 ;

   case 0x0CE:
     return Vdp2Regs->WPEY1 ;

   case 0x0D0:
     return Vdp2Regs->WCTLA ;

   case 0x0D2:
     return Vdp2Regs->WCTLB ;

   case 0x0D4:
     return Vdp2Regs->WCTLC ;

   case 0x0D6:
     return Vdp2Regs->WCTLD ;

   case 0x0D8:
     return Vdp2Regs->LWTA0.part.U ;

   case 0x0DA:
     return Vdp2Regs->LWTA0.part.L ;

   case 0x0DC:
     return Vdp2Regs->LWTA1.part.U ;

   case 0x0DE:
     return Vdp2Regs->LWTA1.part.L ;

   case 0x0E0:
     return Vdp2Regs->SPCTL ;

   case 0x0E2:
     return Vdp2Regs->SDCTL ;

   case 0x0E4:
     return Vdp2Regs->CRAOFA ;

   case 0x0E6:
     return Vdp2Regs->CRAOFB ;

   case 0x0E8:
     return Vdp2Regs->LNCLEN ;

   case 0x0EA:
     return Vdp2Regs->SFPRMD ;

   case 0x0EC:
     return Vdp2Regs->CCCTL ;

   case 0x0EE:
     return Vdp2Regs->SFCCMD ;

   case 0x0F0:
     return Vdp2Regs->PRISA ;

   case 0x0F2:
     return Vdp2Regs->PRISB ;

   case 0x0F4:
     return Vdp2Regs->PRISC ;

   case 0x0F6:
     return Vdp2Regs->PRISD ;

   case 0x0F8:
     return Vdp2Regs->PRINA ;

   case 0x0FA:
     return Vdp2Regs->PRINB ;

   case 0x0FC:
     return Vdp2Regs->PRIR ;

   case 0x0FE:
     // Reserved
     return 0;

   case 0x100:
     return Vdp2Regs->CCRSA ;

   case 0x102:
     return Vdp2Regs->CCRSB ;

   case 0x104:
     return Vdp2Regs->CCRSC ;

   case 0x106:
     return Vdp2Regs->CCRSD ;

   case 0x108:
     return Vdp2Regs->CCRNA ;

   case 0x10A:
     return Vdp2Regs->CCRNB ;

   case 0x10C:
     return Vdp2Regs->CCRR ;

   case 0x10E:
     return Vdp2Regs->CCRLB ;

   case 0x110:
     return Vdp2Regs->CLOFEN ;

   case 0x112:
     return Vdp2Regs->CLOFSL ;

   case 0x114:
     return Vdp2Regs->COAR ;

   case 0x116:
     return Vdp2Regs->COAG ;

   case 0x118:
     return Vdp2Regs->COAB ;

   case 0x11A:
     return Vdp2Regs->COBR ;

   case 0x11C:
     return Vdp2Regs->COBG ;

   case 0x11E:
     return Vdp2Regs->COBB ;

   default:
   {
     LOG("Unhandled VDP2 word write: %08X\n", addr);
     break;
   }
   }


   return 0;
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp2ReadLong(u32 addr) {
   LOG("VDP2 register long read = %08X\n", addr);
   addr &= 0x1FF;
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2WriteByte(u32 addr, UNUSED u8 val) {
   LOG("VDP2 register byte write = %08X\n", addr);
   addr &= 0x1FF;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2WriteWord(u32 addr, u16 val) {
   addr &= 0x1FF;

   switch (addr)
   {
      case 0x000:
         Vdp2Regs->TVMD = val;
         {
           int vreso = (val >> 4) & 0x3;
           // VRESO=3 is prohibited per hardware spec, treat as 256 lines
           if (vreso == 3) vreso = 2;
           yabsys.VBlankLineCount = 225 + (vreso << 4);
         }

         switch( val&0x07){
           case 0:
             yabsys.Hcount = 320 / 9;
             break;
           case 1:
             yabsys.Hcount = 352 / 9;
             break;
           case 2:
             yabsys.Hcount = 640 / 9;
             break;
           case 3:
             yabsys.Hcount = 704 / 9;
             break;
           case 4:
             yabsys.Hcount = 320 / 9;
             break;
           case 5:
             yabsys.Hcount = 352 / 9;
             break;
           case 6:
             yabsys.Hcount = 640 / 9;
             break;
           case 7:
             yabsys.Hcount = 704 / 9;
             break;
         }

         return;
      case 0x002:
         Vdp2Regs->EXTEN = val;
         return;
      case 0x004:
         // TVSTAT is read-only
         return;
      case 0x006:
         Vdp2Regs->VRSIZE = val;
         return;
      case 0x008:
         // HCNT is read-only
         return;
      case 0x00A:
         // VCNT is read-only
         return;
      case 0x00C:
         // Reserved
         return;
      case 0x00E:
         Vdp2Regs->RAMCTL = val;
         if (Vdp2Internal.ColorMode != ((val >> 12) & 0x3) ) {
           Vdp2Internal.ColorMode = (val >> 12) & 0x3;
           for (int i = 0; i < 0x1000; i += 2) {
             VIDCore->OnUpdateColorRamWord(i);
           }
         }

         return;
      case 0x010:
         Vdp2Regs->CYCA0L = val;
         return;
      case 0x012:
         Vdp2Regs->CYCA0U = val;
         return;
      case 0x014:
         Vdp2Regs->CYCA1L = val;
         return;
      case 0x016:
         Vdp2Regs->CYCA1U = val;
         return;
      case 0x018:
         Vdp2Regs->CYCB0L = val;
         return;
      case 0x01A:
         Vdp2Regs->CYCB0U = val;
         return;
      case 0x01C:
         Vdp2Regs->CYCB1L = val;
         return;
      case 0x01E:
         Vdp2Regs->CYCB1U = val;
         return;
      case 0x020:
         Vdp2Regs->BGON = val;
         return;
      case 0x022:
         Vdp2Regs->MZCTL = val;
         return;
      case 0x024:
         Vdp2Regs->SFSEL = val;
         return;
      case 0x026:
         Vdp2Regs->SFCODE = val;
         return;
      case 0x028:
         Vdp2Regs->CHCTLA = val;
         return;
      case 0x02A:
         Vdp2Regs->CHCTLB = val;
         return;
      case 0x02C:
         Vdp2Regs->BMPNA = val;
         return;
      case 0x02E:
         Vdp2Regs->BMPNB = val;
         return;
      case 0x030:
         Vdp2Regs->PNCN0 = val;
         return;
      case 0x032:
         Vdp2Regs->PNCN1 = val;
         return;
      case 0x034:
         Vdp2Regs->PNCN2 = val;
         return;
      case 0x036:
         Vdp2Regs->PNCN3 = val;
         return;
      case 0x038:
         Vdp2Regs->PNCR = val;
         return;
      case 0x03A:
         Vdp2Regs->PLSZ = val;
         return;
      case 0x03C:
         Vdp2Regs->MPOFN = val;
         return;
      case 0x03E:
         Vdp2Regs->MPOFR = val;
         return;
      case 0x040:
         Vdp2Regs->MPABN0 = val;
         return;
      case 0x042:
         Vdp2Regs->MPCDN0 = val;
         return;
      case 0x044:
         Vdp2Regs->MPABN1 = val;
         return;
      case 0x046:
         Vdp2Regs->MPCDN1 = val;
         return;
      case 0x048:
         Vdp2Regs->MPABN2 = val;
         return;
      case 0x04A:
         Vdp2Regs->MPCDN2 = val;
         return;
      case 0x04C:
         Vdp2Regs->MPABN3 = val;
         return;
      case 0x04E:
         Vdp2Regs->MPCDN3 = val;
         return;
      case 0x050:
         Vdp2Regs->MPABRA = val;
         return;
      case 0x052:
         Vdp2Regs->MPCDRA = val;
         return;
      case 0x054:
         Vdp2Regs->MPEFRA = val;
         return;
      case 0x056:
         Vdp2Regs->MPGHRA = val;
         return;
      case 0x058:
         Vdp2Regs->MPIJRA = val;
         return;
      case 0x05A:
         Vdp2Regs->MPKLRA = val;
         return;
      case 0x05C:
         Vdp2Regs->MPMNRA = val;
         return;
      case 0x05E:
         Vdp2Regs->MPOPRA = val;
         return;
      case 0x060:
         Vdp2Regs->MPABRB = val;
         return;
      case 0x062:
         Vdp2Regs->MPCDRB = val;
         return;
      case 0x064:
         Vdp2Regs->MPEFRB = val;
         return;
      case 0x066:
         Vdp2Regs->MPGHRB = val;
         return;
      case 0x068:
         Vdp2Regs->MPIJRB = val;
         return;
      case 0x06A:
         Vdp2Regs->MPKLRB = val;
         return;
      case 0x06C:
         Vdp2Regs->MPMNRB = val;
         return;
      case 0x06E:
         Vdp2Regs->MPOPRB = val;
         return;
      case 0x070:
         Vdp2Regs->SCXIN0 = val;
         return;
      case 0x072:
         Vdp2Regs->SCXDN0 = val;
         return;
      case 0x074:
         Vdp2Regs->SCYIN0 = val;
         return;
      case 0x076:
         Vdp2Regs->SCYDN0 = val;
         return;
      case 0x078:
         Vdp2Regs->ZMXN0.part.I = val;
         return;
      case 0x07A:
         Vdp2Regs->ZMXN0.part.D = val;
         return;
      case 0x07C:
         Vdp2Regs->ZMYN0.part.I = val;
         return;
      case 0x07E:
         Vdp2Regs->ZMYN0.part.D = val;
         return;
      case 0x080:
         Vdp2Regs->SCXIN1 = val;
         return;
      case 0x082:
         Vdp2Regs->SCXDN1 = val;
         return;
      case 0x084:
         Vdp2Regs->SCYIN1 = val;
         return;
      case 0x086:
         Vdp2Regs->SCYDN1 = val;
         return;
      case 0x088:
         Vdp2Regs->ZMXN1.part.I = val;
         return;
      case 0x08A:
         Vdp2Regs->ZMXN1.part.D = val;
         return;
      case 0x08C:
         Vdp2Regs->ZMYN1.part.I = val;
         return;
      case 0x08E:
         Vdp2Regs->ZMYN1.part.D = val;
         return;
      case 0x090:
         Vdp2Regs->SCXN2 = val;
         return;
      case 0x092:
         Vdp2Regs->SCYN2 = val;
         return;
      case 0x094:
         Vdp2Regs->SCXN3 = val;
         return;
      case 0x096:
         Vdp2Regs->SCYN3 = val;
         return;
      case 0x098:
         Vdp2Regs->ZMCTL = val;
         return;
      case 0x09A:
         Vdp2Regs->SCRCTL = val;
         return;
      case 0x09C:
         Vdp2Regs->VCSTA.part.U = val;
         return;
      case 0x09E:
         Vdp2Regs->VCSTA.part.L = val;
         return;
      case 0x0A0:
         Vdp2Regs->LSTA0.part.U = val;
         return;
      case 0x0A2:
         Vdp2Regs->LSTA0.part.L = val;
         return;
      case 0x0A4:
         Vdp2Regs->LSTA1.part.U = val;
         return;
      case 0x0A6:
         Vdp2Regs->LSTA1.part.L = val;
         return;
      case 0x0A8:
         Vdp2Regs->LCTA.part.U = val;
         return;
      case 0x0AA:
         Vdp2Regs->LCTA.part.L = val;
         return;
      case 0x0AC:
         Vdp2Regs->BKTAU = val;
         return;
      case 0x0AE:
         Vdp2Regs->BKTAL = val;
         return;
      case 0x0B0:
         Vdp2Regs->RPMD = val;
         return;
      case 0x0B2:
         Vdp2Regs->RPRCTL = val;
         return;
      case 0x0B4:
         Vdp2Regs->KTCTL = val;
         return;
      case 0x0B6:
         Vdp2Regs->KTAOF = val;
         return;
      case 0x0B8:
         Vdp2Regs->OVPNRA = val;
         return;
      case 0x0BA:
         Vdp2Regs->OVPNRB = val;
         return;
      case 0x0BC:
         Vdp2Regs->RPTA.part.U = val;
         return;
      case 0x0BE:
         Vdp2Regs->RPTA.part.L = val;
         return;
      case 0x0C0:
         Vdp2Regs->WPSX0 = val;
         return;
      case 0x0C2:
         Vdp2Regs->WPSY0 = val;
         return;
      case 0x0C4:
         Vdp2Regs->WPEX0 = val;
         return;
      case 0x0C6:
         Vdp2Regs->WPEY0 = val;
         return;
      case 0x0C8:
         Vdp2Regs->WPSX1 = val;
         return;
      case 0x0CA:
         Vdp2Regs->WPSY1 = val;
         return;
      case 0x0CC:
         Vdp2Regs->WPEX1 = val;
         return;
      case 0x0CE:
         Vdp2Regs->WPEY1 = val;
         return;
      case 0x0D0:
         Vdp2Regs->WCTLA = val;
         return;
      case 0x0D2:
         Vdp2Regs->WCTLB = val;
         return;
      case 0x0D4:
         Vdp2Regs->WCTLC = val;
         return;
      case 0x0D6:
         Vdp2Regs->WCTLD = val;
         return;
      case 0x0D8:
         Vdp2Regs->LWTA0.part.U = val;
         return;
      case 0x0DA:
         Vdp2Regs->LWTA0.part.L = val;
         return;
      case 0x0DC:
         Vdp2Regs->LWTA1.part.U = val;
         return;
      case 0x0DE:
         Vdp2Regs->LWTA1.part.L = val;
         return;
      case 0x0E0:
         Vdp2Regs->SPCTL = val;
         return;
      case 0x0E2:
         Vdp2Regs->SDCTL = val;
         return;
      case 0x0E4:
         Vdp2Regs->CRAOFA = val;
         return;
      case 0x0E6:
         Vdp2Regs->CRAOFB = val;
         return;
      case 0x0E8:
         Vdp2Regs->LNCLEN = val;
         return;
      case 0x0EA:
         Vdp2Regs->SFPRMD = val;
         return;
      case 0x0EC:
         Vdp2Regs->CCCTL = val;
         return;
      case 0x0EE:
         Vdp2Regs->SFCCMD = val;
         return;
      case 0x0F0:
         Vdp2Regs->PRISA = val;
         return;
      case 0x0F2:
         Vdp2Regs->PRISB = val;
         return;
      case 0x0F4:
         Vdp2Regs->PRISC = val;
         return;
      case 0x0F6:
         Vdp2Regs->PRISD = val;
         return;
      case 0x0F8:
         Vdp2Regs->PRINA = val;
         return;
      case 0x0FA:
         Vdp2Regs->PRINB = val;
         return;
      case 0x0FC:
         Vdp2Regs->PRIR = val;
         return;
      case 0x0FE:
         // Reserved
         return;
      case 0x100:
         Vdp2Regs->CCRSA = val;
         return;
      case 0x102:
         Vdp2Regs->CCRSB = val;
         return;
      case 0x104:
         Vdp2Regs->CCRSC = val;
         return;
      case 0x106:
         Vdp2Regs->CCRSD = val;
         return;
      case 0x108:
         Vdp2Regs->CCRNA = val;
         return;
      case 0x10A:
         Vdp2Regs->CCRNB = val;
         return;
      case 0x10C:
         Vdp2Regs->CCRR = val;
         return;
      case 0x10E:
         Vdp2Regs->CCRLB = val;
         return;
      case 0x110:
         Vdp2Regs->CLOFEN = val;
         return;
      case 0x112:
         Vdp2Regs->CLOFSL = val;
         return;
      case 0x114:
         Vdp2Regs->COAR = val;
         return;
      case 0x116:
         Vdp2Regs->COAG = val;
         return;
      case 0x118:
         Vdp2Regs->COAB = val;
         return;
      case 0x11A:
         Vdp2Regs->COBR = val;
         return;
      case 0x11C:
         Vdp2Regs->COBG = val;
         return;
      case 0x11E:
         Vdp2Regs->COBB = val;
         return;
      default:
      {
         LOG("Unhandled VDP2 word write: %08X\n", addr);
         break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2WriteLong(u32 addr, u32 val) {

   Vdp2WriteWord(addr,val>>16);
   Vdp2WriteWord(addr+2,val&0xFFFF);
   return;
}

//////////////////////////////////////////////////////////////////////////////

int Vdp2SaveState(FILE *fp)
{
   int offset;
   IOCheck_struct check = { 0, 0 };

   offset = StateWriteHeader(fp, "VDP2", 1);

   // Write registers
   ywrite(&check, (void *)Vdp2Regs, sizeof(Vdp2), 1, fp);

   // Write VDP2 ram
   ywrite(&check, (void *)Vdp2Ram, 0x80000, 1, fp);

   // Write CRAM
   ywrite(&check, (void *)Vdp2ColorRam, 0x1000, 1, fp);

   // Write internal variables
   ywrite(&check, (void *)&Vdp2Internal, sizeof(Vdp2Internal_struct), 1, fp);

   return StateFinishHeader(fp, offset);
}

//////////////////////////////////////////////////////////////////////////////

int Vdp2LoadState(FILE *fp, UNUSED int version, int size)
{
   IOCheck_struct check = { 0, 0 };

   // Read registers
   yread(&check, (void *)Vdp2Regs, sizeof(Vdp2), 1, fp);

   // Read VDP2 ram
   yread(&check, (void *)Vdp2Ram, 0x80000, 1, fp);

   // Read CRAM
   yread(&check, (void *)Vdp2ColorRam, 0x1000, 1, fp);

   // Read internal variables
   yread(&check, (void *)&Vdp2Internal, sizeof(Vdp2Internal_struct), 1, fp);

   //if(VIDCore) VIDCore->Resize(0,0,-1,-1,0,0);

   for (int i = 0; i < 0x1000; i += 2) {
     VIDCore->OnUpdateColorRamWord(i);
   }

   return size;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG0(void)
{
   Vdp2External.disptoggle ^= 0x1;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG1(void)
{
   Vdp2External.disptoggle ^= 0x2;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG2(void)
{
   Vdp2External.disptoggle ^= 0x4;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG3(void)
{
   Vdp2External.disptoggle ^= 0x8;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleRBG0(void)
{
   Vdp2External.disptoggle ^= 0x10;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleFullScreen(void)
{
   if (VIDCore->IsFullscreen())
   {
      VIDCore->Resize(0,0,320, 224, 0, 0);
   }
   else
   {
      VIDCore->Resize(0,0,640, 480, 1, 0);
   }
}

//////////////////////////////////////////////////////////////////////////////

void EnableAutoFrameSkip(void)
{
   autoframeskipenab = 1;
   lastticks = YabauseGetTicks();
}

//////////////////////////////////////////////////////////////////////////////

void DisableAutoFrameSkip(void)
{
   autoframeskipenab = 0;
}



void VdpResume( void ){
  if (VdpIsAsyncRendering() && vdp_proc_running) {
    YabAddEventQueue(evqueue,VDPEV_MAKECURRENT);
    YabWaitEventQueue(command_);
  }
}

void VdpRevoke( void ){
  if (VdpIsAsyncRendering() && vdp_proc_running) {
    YabAddEventQueue(evqueue,VDPEV_REVOKE);
    YabWaitEventQueue(command_);
  }
}



//////////////////////////////////////////////////////////////////////////////
// This dump code can be used by the real SEGA saturn link this code.
/*
#include	"sgl.h"

extern short vreg[];
extern char vram[];
extern short cram[];

volatile Uint8* vrm_dst = (Uint8*)0x25E00000;
volatile Uint16* const crm_dst = (Uint8*)0x25F00000;
volatile Uint16* const vreg_dst = (Uint16*)0x260ffcc0; //0x25F80000;
volatile Uint16* const frame_dst = (Uint16*)0x25C00000;

#define N0ON 0x01
#define N1ON 0x02
#define N2ON 0x04
#define N3ON 0x08
#define R0ON 0x10
#define R1ON 0x20

void ss_main(void)
{
	int i;

	slInitSystem(TV_320x224, NULL, 1);
	slTVOn();
	for (i = 0; i < (0xFFF>>1) ; i++ ) {
		crm_dst[i] = cram[i];
	}

	for (i = 0; i < 0x7FFFF; i += 1) {
		vrm_dst[i] = vram[i];
	}

	while(1){

    for (i = 7; i < (0x11E>>1); i++) {
			vreg_dst[i] = vreg[i];
		}

    slSynch();
	}
}

*/
int Vdp2GenerateCCode() {

  FILE * regfp = fopen("vreg.c", "w");
  fprintf(regfp, "short vreg[] = { \n");
  for (int i = 0; i < 0x11E; i += 2) {
    u16 data = Vdp2ReadWord(i);
    fprintf(regfp, "0x%04X", data);
    if (i != 0 && (i % 16) == 0) {
      fprintf(regfp, ",\n");
    }
    else {
      fprintf(regfp, ",");
    }
  }
  fprintf(regfp, "};\n");
  fclose(regfp);


  FILE * ramfp = fopen("vram.c","w");
  fprintf(ramfp, "char vram[] = { \n");
  for (int i = 0; i < 0x7FFFF; i++) {
    u8 data = Vdp2RamReadByte(i);
    fprintf(ramfp, "0x%02X", data);
    if ( i != 0 && (i % 16) == 0) {
      fprintf(ramfp, ",\n");
    }
    else {
      fprintf(ramfp, ",");
    }
  }
  fprintf(ramfp, "};\n");
  fclose(ramfp);

  FILE * cramfp = fopen("cram.c", "w");
  fprintf(cramfp, "short cram[] = { \n");
  for (int i = 0; i < 0xFFF; i += 2) {
    u16 data = Vdp2ColorRamReadWord(i);
    fprintf(cramfp, "0x%04X", data);
    if (i != 0 && (i % 16) == 0) {
      fprintf(cramfp, ",\n");
    }
    else {
      fprintf(cramfp, ",");
    }
  }
  fprintf(cramfp, "};\n");
  fclose(cramfp);

  return 0;
}
