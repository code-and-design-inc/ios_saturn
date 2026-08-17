/*  Copyright 2026 devMiyax

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

#ifndef PERSDLCODES_H
#define PERSDLCODES_H

/*! \file persdlcodes.h
    \brief The layout of an SDL peripheral input code.

    Kept apart from persdljoy.c because it is pure arithmetic: no SDL, no
    device, nothing to open. That makes the one part of the input path that is
    easy to get quietly wrong - which device a stored binding names - testable
    on its own.
*/

#include "core.h"
#include "peripheral.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @addtogroup peripheral
 * @{ */

/* Code layout, from the bottom up:

     bits 0-15   payload (button index, axis index, hat value)
     bits 16-17  sub-type inside a range
     bits 18-19  device the binding was recorded against
     bit  20     raw axis        (SDL_MIN_AXIS_VALUE / SDL_MAX_AXIS_VALUE)
     bit  21     raw hat         (SDL_HAT_VALUE)
     bit  22     game controller (SDL_GC_*)

   Keyboard and mouse codes are produced elsewhere and sit above all of this:
   Qt's special keys start at 0x01000000 and the mouse uses bits 30 and 31. */

/* Game controller codes. SDL normalises a recognised pad onto the standard
   layout, so these carry no device-specific button numbering. */
#define SDL_GC_BUTTON_VALUE 0x400000
#define SDL_GC_AXIS_POS_VALUE 0x410000
#define SDL_GC_AXIS_NEG_VALUE 0x420000
#define SDL_GC_AXIS_ANALOG_VALUE 0x430000

/* Raw joystick codes, for devices SDL has no mapping for. */
#define SDL_MAX_AXIS_VALUE 0x110000
#define SDL_MIN_AXIS_VALUE 0x100000
#define SDL_HAT_VALUE 0x200000
#define SDL_MEDIUM_AXIS_VALUE 0x8000

/* Bits 18-19 hold the device. Two bits is all there is: bit 20 upwards is
   taken by the type flags above and bits 0-17 by the payload, so a fifth
   device would encode as 0x100000 and be indistinguishable from a raw axis on
   device 0. The core therefore opens at most PERSDL_MAX_DEVICES devices. */
#define PERSDL_DEVICE_SHIFT 18
#define PERSDL_DEVICE_MASK (3u << PERSDL_DEVICE_SHIFT)
#define PERSDL_MAX_DEVICES 4

/* Every code this core emits is below this once the device field is masked
   off, and every code produced elsewhere is above it. */
#define PERSDL_CODE_LIMIT (SDL_GC_AXIS_ANALOG_VALUE + 0x10000)

/*! Point a stored binding at a different device.

    SDL hands out device indices in connection order, so the index a binding
    was recorded against moves whenever anything is plugged in or removed. The
    binding has to follow the device it was configured for, which is persisted
    by path rather than by index.

    Returns the code unchanged when it does not belong to this core, and when
    deviceIndex cannot be encoded - a code that named the wrong device would be
    worse than one that names none.
*/
static INLINE u32 PERSDLRetargetCode(u32 key, int deviceIndex)
{
   if (key == PERKEY_UNBOUND)
      return key;
   if (deviceIndex < 0 || deviceIndex >= PERSDL_MAX_DEVICES)
      return key;
   /* Keyboard and mouse codes are above everything this core emits. Note that
      the raw button and analog-axis codes are *below* the type flags, not
      above them, so a lower bound of SDL_MIN_AXIS_VALUE would skip exactly
      the bindings of a device SDL does not recognise. */
   if ((key & ~PERSDL_DEVICE_MASK) >= PERSDL_CODE_LIMIT)
      return key;
   return (key & ~PERSDL_DEVICE_MASK) | ((u32)deviceIndex << PERSDL_DEVICE_SHIFT);
}

/*! The code the same axis produces travelling the other way.

    Scan() compares an axis against a baseline it re-takes as soon as it has
    reported a move, and the code it returns carries the direction of travel.
    One pull of a trigger therefore reads as two codes: one when it leaves rest
    and the opposite one when it comes back. Somebody who has just bound the
    first usually wants to swallow the second rather than treat it as a fresh
    press.

    The device (bits 18-19) and the payload identify the physical axis; only
    the type flag says which way it went. Returns 0 for buttons, hats and
    anything else with no opposite.
*/
static INLINE u32 PERSDLOppositeAxisCode(u32 key)
{
   const u32 rest = key & (PERSDL_DEVICE_MASK | 0xFFFFu);
   const u32 kind = key & ~(PERSDL_DEVICE_MASK | 0xFFFFu);

   if (kind == SDL_GC_AXIS_POS_VALUE) return SDL_GC_AXIS_NEG_VALUE | rest;
   if (kind == SDL_GC_AXIS_NEG_VALUE) return SDL_GC_AXIS_POS_VALUE | rest;
   if (kind == SDL_MIN_AXIS_VALUE)    return SDL_MAX_AXIS_VALUE | rest;
   if (kind == SDL_MAX_AXIS_VALUE)    return SDL_MIN_AXIS_VALUE | rest;
   return 0;
}

/** @} */

#if defined (__cplusplus)
}
#endif

#endif
