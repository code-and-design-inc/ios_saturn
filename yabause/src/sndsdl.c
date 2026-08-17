/*  Copyright 2005-2006 Theo Berkau

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

/*! \file sndsdl.c
    \brief SDL sound interface.
*/

#ifdef HAVE_LIBSDL

#include <stdlib.h>

#if defined(__APPLE__) || defined(GEKKO)
 #ifdef HAVE_LIBSDL2
  #include <SDL2/SDL.h>
 #else
  #include <SDL/SDL.h>
 #endif
#else
 #include "SDL.h"
#endif
#include "error.h"
#include "scsp.h"
#include "sndsdl.h"
#include "debug.h"

static int SNDSDLInit(void);
static void SNDSDLDeInit(void);
static int SNDSDLReset(void);
static int SNDSDLChangeVideoFormat(int vertfreq);
static void PushFrame(s32 l, s32 r);
static void SNDSDLUpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 num_samples);
static u32 SNDSDLGetAudioSpace(void);
static void SNDSDLMuteAudio(void);
static void SNDSDLUnMuteAudio(void);
static void SNDSDLSetVolume(int volume);
#ifdef USE_SCSPMIDI
int SNDSDLMidiChangePorts(int inport, int outport);
u8 SNDSDLMidiIn(int *isdata);
int SNDSDLMidiOut(u8 data);
#endif

SoundInterface_struct SNDSDL = {
SNDCORE_SDL,
"SDL Sound Interface",
SNDSDLInit,
SNDSDLDeInit,
SNDSDLReset,
SNDSDLChangeVideoFormat,
SNDSDLUpdateAudio,
SNDSDLGetAudioSpace,
SNDSDLMuteAudio,
SNDSDLUnMuteAudio,
SNDSDLSetVolume,
#ifdef USE_SCSPMIDI
SNDSDLMidiChangePorts,
SNDSDLMidiIn,
SNDSDLMidiOut
#endif
};

#define NUMSOUNDBLOCKS  4

// Frames the device asks for in a single callback. The callback drains this
// much in one go, so it is the floor on how much the ring has to hold at every
// callback, and the cushion below has to cover it. The old code asked for
// (freq/60)*2 rounded up to a power of two, i.e. 2048 frames - nearly three
// video frames of production drained at once, and more than one video frame
// (735 at 44100/60) can refill. Keep it under one video frame instead.
#define SOUNDBLOCKSAMPLES 512

// Silence primed into the ring before playback starts, in stereo frames.
// This is also the fill level the resampler below steers towards, so it is
// both the startup cushion and the operating point: deep enough that a hitch
// on the producer side does not reach the read pointer, shallow enough that
// the added latency stays unnoticeable.
#define SOUNDPREFILLSAMPLES ((44100 / 60) * 3)

// Adaptive resampling.
//
// The emulator generates 44100/60 samples per emulated video frame, i.e. its
// audio rate is whatever the video frame rate happens to be, while the device
// consumes at its own crystal's 44100Hz. Those two are never exactly equal -
// the frame limiter cannot hit 60.000 to six digits, and real Saturn NTSC is
// 59.826Hz anyway, so the very premise 44100/735 == 60 is wrong. Any residual
// difference has to go somewhere: without correction the surplus was thrown
// away upstream in ~150 sample lumps several times a second (heard as
// clicking) or, in deficit, showed up as gaps.
//
// So resample instead, by a factor trimmed continuously to hold the ring at
// SOUNDPREFILLSAMPLES. The correction needed is well under 1%; the clamp keeps
// it there, far below the few percent at which a listener starts to hear pitch
// move. Nothing is discarded and the producer never builds a backlog.
#define SOUNDRATIOMIN   0.99
#define SOUNDRATIOMAX   1.01
#define SOUNDRATIOGAIN  0.10  // ratio trim per unit of normalized fill error
#define SOUNDRATIOSLEW  0.05  // how fast the live ratio follows its target

static u16 *stereodata16;
static u32 soundoffset;    // write position in bytes
static u32 soundpos;       // read position in bytes
static u32 soundbuffered;  // bytes written but not played yet
static u32 soundbufsize;
static SDL_AudioSpec audiofmt;
static u8 soundvolume;
static int muted = 0;

static double soundratio;  // output frames produced per input frame
static double soundfrac;   // fractional read position inside the input block
static s32 soundlastl;     // last input frame, to interpolate across blocks
static s32 soundlastr;

//////////////////////////////////////////////////////////////////////////////

// Rewinds the ring to the primed state. The buffer is kept zeroed, so the
// primed region plays as silence. Callers must hold the audio lock, or be sure
// the device is not running yet.
static void ResetStream(void)
{
   soundpos = 0;
   soundoffset = SOUNDPREFILLSAMPLES * sizeof(s16) * 2;
   soundbuffered = soundoffset;

   soundratio = 1.0;
   soundfrac = 0.0;
   soundlastl = 0;
   soundlastr = 0;
}

//////////////////////////////////////////////////////////////////////////////

// Called on the SDL audio thread, which holds the audio lock for the duration,
// so soundoffset/soundbuffered are stable here.
static void MixAudio(UNUSED void *userdata, Uint8 *stream, int len) {
	u32 want = (u32)len;
	u32 done = 0;

	if (stereodata16 == NULL || soundbufsize == 0)
	{
		SDL_memset(stream, audiofmt.silence, len);
		return;
	}

	// Never read past the write pointer. Reading whatever happens to sit ahead
	// of it replays samples from the previous lap of the ring, which is heard
	// as a periodic buzz, and it lets the read pointer overtake the write
	// pointer so every later free-space calculation is wrong as well.
	while (done < want && soundbuffered > 0)
	{
		u32 chunk = want - done;

		if (chunk > soundbuffered)
			chunk = soundbuffered;
		if (chunk > soundbufsize - soundpos)
			chunk = soundbufsize - soundpos;

		if (muted)
			SDL_memset(stream + done, audiofmt.silence, chunk);
		else
			SDL_memcpy(stream + done, (Uint8 *)stereodata16 + soundpos, chunk);

		soundpos += chunk;
		if (soundpos >= soundbufsize)
			soundpos = 0;
		soundbuffered -= chunk;
		done += chunk;
	}

	// Underrun: silence is a far smaller artifact than stale samples, and it
	// keeps the pointers consistent so the stream recovers on the next refill.
	if (done < want)
		SDL_memset(stream + done, audiofmt.silence, want - done);
}

//////////////////////////////////////////////////////////////////////////////

static int SNDSDLInit(void)
{
#if defined (_MSC_VER) && SDL_VERSION_ATLEAST(2,0,0)
   SDL_SetMainReady();
#endif
   SDL_InitSubSystem(SDL_INIT_AUDIO);
//   if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0);
//      return -1;

   audiofmt.freq = 44100;
   audiofmt.format = AUDIO_S16SYS;
   audiofmt.channels = 2;
   audiofmt.samples = SOUNDBLOCKSAMPLES; // already a power of two, as SDL wants
   audiofmt.callback = MixAudio;
   audiofmt.userdata = NULL;

   // Size the ring for the slowest video format (50Hz) and keep it that size
   // for good. ChangeVideoFormat then never has to free a buffer the audio
   // thread is reading from, and soundoffset can never be left pointing past
   // the end of a freshly shrunk buffer.
   soundbufsize = (audiofmt.freq / 50) * NUMSOUNDBLOCKS * 2 * 2;

   soundvolume = SDL_MIX_MAXVOLUME;

   if (SDL_OpenAudio(&audiofmt, NULL) != 0)
   {
      YabSetError(YAB_ERR_SDL, (void *)SDL_GetError());
      return -1;
   }

   if ((stereodata16 = (u16 *)malloc(soundbufsize)) == NULL)
      return -1;

   memset(stereodata16, 0, soundbufsize);

   ResetStream();

   SDL_PauseAudio(0);

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void SNDSDLDeInit(void)
{
   // Closes the device first, so the callback is no longer running by the time
   // the buffer it reads from goes away.
   SDL_CloseAudio();

   if (stereodata16)
   {
      free(stereodata16);
      stereodata16 = NULL;
   }
}

//////////////////////////////////////////////////////////////////////////////

static int SNDSDLReset(void)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int SNDSDLChangeVideoFormat(UNUSED int vertfreq)
{
   // The ring is already sized for the slowest format, so there is nothing to
   // reallocate. It used to free() the buffer here while the audio thread was
   // reading from it, and on 50Hz->60Hz the buffer shrank without resetting
   // soundoffset, which then made (soundbufsize - soundoffset) wrap around in
   // SNDSDLUpdateAudio() and write past the allocation.
   SDL_LockAudio();

   if (stereodata16)
      memset(stereodata16, 0, soundbufsize);

   ResetStream();

   SDL_UnlockAudio();

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

// Writes one output frame, already scaled and clipped, at the write pointer.
// The audio lock must be held.
static void PushFrame(s32 l, s32 r)
{
   s16 *dst;

   // Scale through a wider type and leave the source alone: the input is the
   // SCSP's own output ring, which other consumers read as well, so the old
   // in-place scaling both destroyed their data and could overflow s32.
   l = (s32)(((s64)l * soundvolume) / SDL_MIX_MAXVOLUME);
   r = (s32)(((s64)r * soundvolume) / SDL_MIX_MAXVOLUME);

   if (l > 0x7FFF) l = 0x7FFF;
   else if (l < -0x8000) l = -0x8000;
   if (r > 0x7FFF) r = 0x7FFF;
   else if (r < -0x8000) r = -0x8000;

   dst = (s16 *)(((u8 *)stereodata16) + soundoffset);
   dst[0] = (s16)l;   // Left Channel
   dst[1] = (s16)r;   // Right Channel

   soundoffset += sizeof(s16) * 2;
   if (soundoffset >= soundbufsize)
      soundoffset = 0;
   soundbuffered += sizeof(s16) * 2;
}

static void SNDSDLUpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 num_samples)
{
   const s32 *srcl = (const s32 *)leftchanbuffer;
   const s32 *srcr = (const s32 *)rightchanbuffer;
   const u32 framebytes = sizeof(s16) * 2;
   double step, target, err, want;

   SDL_LockAudio();

   if (stereodata16 == NULL || soundbufsize == 0 || num_samples == 0)
   {
      SDL_UnlockAudio();
      return;
   }

   // soundfrac walks the input block; every time it lands somewhere we emit one
   // output frame interpolated between the two input frames it falls between.
   // soundlastl/r carry the block's last frame so the interpolation is
   // continuous across calls instead of restarting at every block boundary.
   step = 1.0 / soundratio;

   while (soundfrac < (double)num_samples)
   {
      s32 i = (s32)soundfrac;            // >= 0, so this truncates like floor
      double f = soundfrac - (double)i;
      s32 al = (i == 0) ? soundlastl : srcl[i - 1];
      s32 ar = (i == 0) ? soundlastr : srcr[i - 1];

      // The caller sized this block from SNDSDLGetAudioSpace(), so this only
      // trips if the device stopped consuming entirely.
      if (soundbuffered + framebytes > soundbufsize)
         break;

      PushFrame((s32)(al + (srcl[i] - al) * f),
                (s32)(ar + (srcr[i] - ar) * f));

      soundfrac += step;
   }

   soundfrac -= (double)num_samples;
   if (soundfrac < 0.0)
      soundfrac = 0.0;                   // only after the overflow break above

   soundlastl = srcl[num_samples - 1];
   soundlastr = srcr[num_samples - 1];

   // Steer the ring towards the primed fill level. Proportional only: a small
   // standing error is fine here, it just means the ring settles a few ms off
   // the target, whereas an integral term would keep winding the ratio up
   // against a rate difference that never goes away.
   target = (double)(SOUNDPREFILLSAMPLES * framebytes) / (double)soundbufsize;
   err = (double)soundbuffered / (double)soundbufsize - target;

   want = 1.0 - SOUNDRATIOGAIN * err;
   if (want > SOUNDRATIOMAX) want = SOUNDRATIOMAX;
   else if (want < SOUNDRATIOMIN) want = SOUNDRATIOMIN;
   soundratio += (want - soundratio) * SOUNDRATIOSLEW;

   SDL_UnlockAudio();
}

//////////////////////////////////////////////////////////////////////////////

static u32 SNDSDLGetAudioSpace(void)
{
   u32 freeframes;
   double ratio, in;

   // Derived from an explicit fill count rather than from the two pointers:
   // soundoffset == soundpos is both "empty" and "full", and the old code
   // always read it as "full", so a ring the callback had just drained looked
   // like it had no room at all.
   SDL_LockAudio();
   freeframes = (soundbufsize - soundbuffered) / (sizeof(s16) * 2);
   ratio = soundratio;
   SDL_UnlockAudio();

   // Reported in input frames. One input frame becomes `ratio` output frames,
   // so while the resampler is compressing we can take in more than we have
   // room for. One frame is held back to cover soundfrac's carry.
   in = (double)freeframes / ratio;
   if (in < 2.0)
      return 0;

   return (u32)in - 1;
}

//////////////////////////////////////////////////////////////////////////////

static void SNDSDLMuteAudio(void)
{
   muted = 1;
}

//////////////////////////////////////////////////////////////////////////////

static void SNDSDLUnMuteAudio(void)
{
   muted = 0;
}

//////////////////////////////////////////////////////////////////////////////

static void SNDSDLSetVolume(int volume)
{
   soundvolume = ( (double)SDL_MIX_MAXVOLUME /(double)100 ) *volume;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef USE_SCSPMIDI
int SNDSDLMidiChangePorts(int inport, int outport)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

u8 SNDSDLMidiIn(int *isdata)
{
	*isdata = 0;
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

int SNDSDLMidiOut(u8 data)
{
	return 1;
}

//////////////////////////////////////////////////////////////////////////////
#endif
#endif
