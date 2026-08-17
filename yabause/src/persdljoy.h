/*  Copyright 2006 Guillaume Duhamel

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

#ifndef PERSDLJOY_H
#define PERSDLJOY_H

#include "peripheral.h"

// This is implemented in C. Declare it as such rather than relying on the
// caller to include this header from inside an extern "C" block, which is
// what QtYabause.h happens to do and what every other C++ caller was getting
// its linkage from.
#if defined (__cplusplus)
extern "C" {
#endif

/** @addtogroup peripheral
 * @{ */
#define PERCORE_SDLJOY 3

extern PerInterface_struct PERSDLJoy;

// This core's default Saturn pad mapping for a gamepad SDL recognises. Fills
// keys[PERPAD_*] with the code for each Saturn button and returns 1; returns 0
// and touches nothing when there is no recognised gamepad, so the caller can
// fall back to a keyboard mapping.
//
// order selects which gamepad: 0 for the first recognised one, 1 for the
// second. That is deliberately not the SDL device index - a wheel, pedal set or
// shifter also occupies an SDL device slot, so device 0 is often not a gamepad.
int PERSDLJoyGetDefaultPadMapping(int order, u32 * keys, int keyCount);

// Enumeration of the input devices this core has open, for the per-port device
// selector. A Saturn port is driven by exactly one physical device, so the UI
// needs to name them; SDL device indices are not stable across replugging, so
// a device is identified by its GUID string everywhere it is persisted.
// Re-scan for devices that were plugged in or unplugged and rebuild the open
// device table. Returns a counter that changes whenever the set of devices
// changed, so a caller can refresh its own view cheaply by comparing it with
// the value it saw last. Bindings are stored against a device GUID, so a device
// that comes back finds its way to the same Saturn port again.
int PERSDLJoyRefreshDevices(void);

int PERSDLJoyGetDeviceCount(void);
int PERSDLJoyGetDeviceInfo(int index, char * guid, int guidSize,
                          char * name, int nameSize, int * isGameController);
// -1 when no open device has this GUID (unplugged, or a different machine).
int PERSDLJoyGetDeviceIndexForId(const char * deviceId);
// Restrict Scan() to one device while a port is being configured, so pressing a
// button on another pad or on a racing wheel cannot be bound by accident.
// -1 accepts any device.
#define PERSDL_SCAN_ANY_DEVICE (-1)
#define PERSDL_SCAN_NO_DEVICE (-2)
void PERSDLJoySetScanDeviceIndex(int index);
// Default mapping for a specific device rather than the order-th gamepad.
int PERSDLJoyGetDefaultPadMappingForIndex(int index, u32 * keys, int keyCount);
// Rewrite the device field of a stored binding so it points at deviceIndex.
// Codes embed the device they were recorded against; replugging changes SDL's
// order, so a binding has to be retargeted at load time or it goes dead.
u32 PERSDLJoyRetargetKey(u32 key, int deviceIndex);
// The code the same axis reports when it travels back the other way.
//
// Scan() compares an axis against a baseline it re-takes as soon as it has
// reported a move, so a trigger reports one direction when it is pulled and
// the opposite one when it is released - two different codes for one physical
// action. A caller that has just bound the first usually wants to swallow the
// second. Returns 0 for buttons, hats and anything else with no opposite.
u32 PERSDLJoyOppositeAxisCode(u32 key);
/** @} */

#if defined (__cplusplus)
}
#endif

#endif
