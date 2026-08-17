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

/*! \file test_peripheral.c
    \brief Unit tests for the peripheral mapping layer.

    These cover the part of the input configuration that is pure data: which
    Saturn button a given input code drives. That is where the "one Saturn port
    is driven by one physical device" rule actually lives - the exclusivity
    between a keyboard and a gamepad is a property of the code spaces not
    overlapping, not of any check in the UI. The dialogs are hard to exercise
    automatically, this is not.

    No SDL and no Qt: only peripheral.c is linked.
*/

#include <stdio.h>
#include <string.h>

#include "peripheral.h"
#include "persdlcodes.h"

/* peripheral.c references these two from the rest of the emulator. The tests
   never reach the code paths that use them (PERDummy is not selected and
   PerInit() is not called), so empty definitions keep the link self-contained
   without pulling the emulator in. */
int YabauseExec(void) { return 0; }
PerInterface_struct * PERCoreList[] = { NULL };

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, ...) \
	do { \
		tests_run++; \
		if (!(cond)) { \
			tests_failed++; \
			printf("FAIL %s:%d: ", __FILE__, __LINE__); \
			printf(__VA_ARGS__); \
			printf("\n"); \
		} \
	} while (0)

/* Input codes as the peripheral cores produce them. Kept here rather than
   pulled from the cores so that a change to either side shows up as a failing
   test instead of two definitions silently drifting apart. */
#define KEY_QT_Z          0x5A       /* Qt::Key_Z */
#define KEY_QT_UP         0x01000013 /* Qt::Key_Up */
#define KEY_QT_UNKNOWN    0x01FFFFFF /* Qt::Key_unknown, the top of the range */
#define KEY_MOUSE_BUTTON  (1u << 31)
#define KEY_MOUSE_MOVE    (1u << 30)
#define KEY_SDL_GC_BUTTON 0x400000   /* SDL_GC_BUTTON_VALUE */
#define KEY_SDL_GC_AXIS   0x430000   /* SDL_GC_AXIS_ANALOG_VALUE */
#define KEY_SDL_RAW_AXIS  0x110000   /* SDL_MAX_AXIS_VALUE */
#define KEY_SDL_RAW_HAT   0x200000   /* SDL_HAT_VALUE */

/* Android, iOS, the Switch port and retro_arena have no key codes to map: they
   are handed a Saturn button directly and encode it as (player << 24) | name,
   which the same core then binds and feeds back in. Player 1's D-pad up is
   therefore the code 0 - real input that a pad must react to. */
#define KEY_EMBEDDED_PAD(player, name) (((u32)(player) << 24) | (u32)(name))

//////////////////////////////////////////////////////////////////////////////

/* A fresh peripheral must start with nothing bound. PerUpdateConfig() grows
   perkeyconfig with realloc(), which leaves the new entries indeterminate: a
   garbage value there means an unrelated key or axis fires a Saturn button. */
static void test_new_peripheral_starts_unbound(void)
{
	PortData_struct port;
	PerPad_struct * pad;
	u32 key;

	PerPortReset();
	memset(&port, 0, sizeof(port));
	pad = PerPadAdd(&port);
	CHECK(pad != NULL, "PerPadAdd returned NULL");
	if (!pad) return;

	/* Nothing is bound, so no input at all may reach the pad. Sweep a value
	   from every code space a peripheral core can produce. */
	*pad->padbits = 0xFF;
	*(pad->padbits + 1) = 0xFF;

	for (key = 0; key < 8; key++)
	{
		static const u32 probes[] = {
			KEY_QT_Z, KEY_QT_UP, KEY_MOUSE_BUTTON, KEY_MOUSE_MOVE,
			KEY_SDL_GC_BUTTON, KEY_SDL_GC_AXIS, KEY_SDL_RAW_AXIS, KEY_SDL_RAW_HAT
		};
		PerKeyDown(probes[key]);
	}

	CHECK(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF,
	      "an unbound pad reacted to input: padbits %02X %02X",
	      *pad->padbits, *(pad->padbits + 1));
}

//////////////////////////////////////////////////////////////////////////////

/* The bound code drives its button and nothing else does. This is the whole of
   the runtime exclusivity: bind a port to a gamepad and keyboard codes simply
   do not match. */
static void test_only_the_bound_code_fires(void)
{
	PortData_struct port;
	PerPad_struct * pad;

	PerPortReset();
	memset(&port, 0, sizeof(port));
	pad = PerPadAdd(&port);
	if (!pad) { CHECK(0, "PerPadAdd returned NULL"); return; }

	PerSetKey(KEY_SDL_GC_BUTTON, PERPAD_A, pad);

	*pad->padbits = 0xFF;
	*(pad->padbits + 1) = 0xFF;

	/* A keyboard code must not reach a port configured for a gamepad. */
	PerKeyDown(KEY_QT_Z);
	CHECK(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF,
	      "keyboard input reached a gamepad-configured port");

	PerKeyDown(KEY_SDL_GC_BUTTON);
	CHECK(!(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF),
	      "the bound gamepad code did not press the button");

	PerKeyUp(KEY_SDL_GC_BUTTON);
	CHECK(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF,
	      "releasing the bound code did not release the button");
}

//////////////////////////////////////////////////////////////////////////////

/* Rebinding replaces the previous code rather than adding to it. A Saturn
   button takes one physical input. */
static void test_rebinding_replaces(void)
{
	PortData_struct port;
	PerPad_struct * pad;

	PerPortReset();
	memset(&port, 0, sizeof(port));
	pad = PerPadAdd(&port);
	if (!pad) { CHECK(0, "PerPadAdd returned NULL"); return; }

	PerSetKey(KEY_QT_Z, PERPAD_A, pad);
	PerSetKey(KEY_SDL_GC_BUTTON, PERPAD_A, pad);

	*pad->padbits = 0xFF;
	*(pad->padbits + 1) = 0xFF;

	PerKeyDown(KEY_QT_Z);
	CHECK(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF,
	      "the replaced binding still fires");
}

//////////////////////////////////////////////////////////////////////////////

/* PERKEY_UNBOUND must never match real input, or an unconfigured button would
   fire on whatever produces that value. */
static void test_unbound_value_never_matches(void)
{
	PortData_struct port;
	PerPad_struct * pad;

	PerPortReset();
	memset(&port, 0, sizeof(port));
	pad = PerPadAdd(&port);
	if (!pad) { CHECK(0, "PerPadAdd returned NULL"); return; }

	PerSetKey(PERKEY_UNBOUND, PERPAD_A, pad);

	*pad->padbits = 0xFF;
	*(pad->padbits + 1) = 0xFF;

	PerKeyDown(PERKEY_UNBOUND);
	CHECK(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF,
	      "PERKEY_UNBOUND pressed a button");
}

//////////////////////////////////////////////////////////////////////////////

/* The lowest code an embedded port can produce is player 1's D-pad up, and it
   is zero. Nothing in the input path may treat a zero code as "no input", or
   that one direction goes dead on every touch and physical pad at once. */
static void test_embedded_pad_up_reaches_the_pad(void)
{
	PortData_struct port;
	PerPad_struct * pad;

	PerPortReset();
	memset(&port, 0, sizeof(port));
	pad = PerPadAdd(&port);
	if (!pad) { CHECK(0, "PerPadAdd returned NULL"); return; }

	PerSetKey(KEY_EMBEDDED_PAD(0, PERPAD_UP), PERPAD_UP, pad);

	*pad->padbits = 0xFF;
	*(pad->padbits + 1) = 0xFF;

	PerKeyDown(KEY_EMBEDDED_PAD(0, PERPAD_UP));
	CHECK(!(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF),
	      "player 1 D-pad up did not press the button");

	PerKeyUp(KEY_EMBEDDED_PAD(0, PERPAD_UP));
	CHECK(*pad->padbits == 0xFF && *(pad->padbits + 1) == 0xFF,
	      "player 1 D-pad up stayed pressed after release");
}

//////////////////////////////////////////////////////////////////////////////

/* The code spaces of the peripheral cores must not overlap. If they ever do,
   a keyboard key and a gamepad button can collide on the same value and the
   runtime exclusivity above stops holding - without anything else changing. */
static void test_code_spaces_do_not_overlap(void)
{
	/* Qt key codes: printable range and the special-key range. */
	CHECK(KEY_QT_Z < KEY_SDL_RAW_AXIS,
	      "a printable Qt key code reaches into the SDL range");
	CHECK(KEY_QT_UP > KEY_SDL_GC_AXIS + 0xFFFF,
	      "the Qt special-key range starts below the SDL game controller range");
	CHECK(KEY_QT_UNKNOWN > KEY_QT_UP,
	      "the Qt special-key range is not ordered as assumed");

	/* SDL game controller codes sit above the raw joystick ones and below the
	   Qt special keys. */
	CHECK(KEY_SDL_RAW_HAT < KEY_SDL_GC_BUTTON,
	      "the game controller range overlaps the raw joystick range");
	CHECK(KEY_SDL_GC_AXIS + 0xFFFF < KEY_QT_UP,
	      "the game controller range overlaps the Qt special-key range");

	/* Mouse codes use the top two bits, which nothing else sets. */
	CHECK(KEY_MOUSE_MOVE > KEY_QT_UNKNOWN,
	      "the mouse codes collide with Qt key codes");
	CHECK(KEY_MOUSE_BUTTON > KEY_MOUSE_MOVE,
	      "the mouse code layout is not as assumed");

	CHECK(KEY_QT_Z != PERKEY_UNBOUND && KEY_SDL_GC_BUTTON != PERKEY_UNBOUND,
	      "a real input code equals PERKEY_UNBOUND");

	/* The embedded ports start their codes at zero and the mouse owns the top
	   two bits, so the unbound marker has to sit outside both. */
	CHECK(KEY_EMBEDDED_PAD(0, PERPAD_UP) != PERKEY_UNBOUND,
	      "player 1 D-pad up collides with PERKEY_UNBOUND");
	CHECK(KEY_EMBEDDED_PAD(1, PERPAD_Z) != PERKEY_UNBOUND,
	      "player 2 Z collides with PERKEY_UNBOUND");
	CHECK((KEY_MOUSE_BUTTON | 0xFFFF) != PERKEY_UNBOUND &&
	      (KEY_MOUSE_MOVE | 0xFFFF) != PERKEY_UNBOUND,
	      "a mouse code collides with PERKEY_UNBOUND");
}

//////////////////////////////////////////////////////////////////////////////

/* SDL hands out device indices in connection order, so a stored binding has to
   be moved to wherever its device landed this time. Everything this core emits
   must move, and nothing else may. */
static void test_retarget_moves_every_code_this_core_emits(void)
{
	static const u32 codes[] = {
		1,                                        /* raw button 0 */
		SDL_MEDIUM_AXIS_VALUE | 2,                /* raw analog axis 2 */
		SDL_MIN_AXIS_VALUE | 1,                   /* raw axis, negative */
		SDL_MAX_AXIS_VALUE | 1,                   /* raw axis, positive */
		SDL_HAT_VALUE | 0x10,                     /* raw hat */
		SDL_GC_BUTTON_VALUE | 3,                  /* game controller button */
		SDL_GC_AXIS_POS_VALUE | 4,
		SDL_GC_AXIS_NEG_VALUE | 4,
		SDL_GC_AXIS_ANALOG_VALUE | 5
	};
	int i;
	int device;

	for (i = 0; i < (int)(sizeof(codes) / sizeof(codes[0])); i++)
	{
		for (device = 0; device < PERSDL_MAX_DEVICES; device++)
		{
			const u32 moved = PERSDLRetargetCode(codes[i], device);

			CHECK((moved & PERSDL_DEVICE_MASK) == ((u32)device << PERSDL_DEVICE_SHIFT),
			      "code %08X did not move to device %d (got %08X)",
			      codes[i], device, moved);
			CHECK((moved & ~PERSDL_DEVICE_MASK) == codes[i],
			      "retargeting code %08X changed its payload (got %08X)",
			      codes[i], moved);
			/* Round trip: moving it back has to reproduce the original. */
			CHECK(PERSDLRetargetCode(moved, 0) == codes[i],
			      "code %08X did not survive a round trip through device %d",
			      codes[i], device);
		}
	}
}

/* Codes from the keyboard and the mouse pass through untouched: they are not
   produced by this core and have no device field to rewrite. */
static void test_retarget_leaves_host_input_alone(void)
{
	static const u32 codes[] = {
		KEY_QT_UP, KEY_QT_UNKNOWN, KEY_MOUSE_MOVE, KEY_MOUSE_BUTTON | 1
	};
	int i;

	for (i = 0; i < (int)(sizeof(codes) / sizeof(codes[0])); i++)
		CHECK(PERSDLRetargetCode(codes[i], 1) == codes[i],
		      "host input code %08X was retargeted", codes[i]);

	CHECK(PERSDLRetargetCode(PERKEY_UNBOUND, 1) == PERKEY_UNBOUND,
	      "an unbound button was given a device");
}

/* An index the device field cannot hold must not be encoded. Truncating it
   would silently point the binding at a different pad, which is worse than
   leaving it where it was. */
static void test_retarget_refuses_an_unencodable_device(void)
{
	const u32 code = SDL_GC_BUTTON_VALUE | 3;

	CHECK(PERSDLRetargetCode(code, PERSDL_MAX_DEVICES) == code,
	      "device %d was encoded even though the field holds %d",
	      PERSDL_MAX_DEVICES, PERSDL_MAX_DEVICES);
	CHECK(PERSDLRetargetCode(code, 99) == code, "device 99 was encoded");
	CHECK(PERSDLRetargetCode(code, -1) == code, "a negative device was encoded");

	/* The field really is only as wide as PERSDL_MAX_DEVICES claims. */
	CHECK(((u32)(PERSDL_MAX_DEVICES - 1) << PERSDL_DEVICE_SHIFT) == PERSDL_DEVICE_MASK,
	      "PERSDL_MAX_DEVICES does not match the width of the device field");
	CHECK((((u32)PERSDL_MAX_DEVICES << PERSDL_DEVICE_SHIFT) & PERSDL_DEVICE_MASK) == 0,
	      "one device past the end aliases device 0");
}

/* The device field must not collide with anything else in a code. */
static void test_device_field_does_not_collide(void)
{
	CHECK((PERSDL_DEVICE_MASK & 0xFFFF) == 0,
	      "the device field overlaps the payload");
	CHECK((PERSDL_DEVICE_MASK & SDL_MIN_AXIS_VALUE) == 0 &&
	      (PERSDL_DEVICE_MASK & SDL_HAT_VALUE) == 0 &&
	      (PERSDL_DEVICE_MASK & SDL_GC_BUTTON_VALUE) == 0,
	      "the device field overlaps a type flag");
	CHECK(((u32)(PERSDL_MAX_DEVICES - 1) << PERSDL_DEVICE_SHIFT) < SDL_MIN_AXIS_VALUE,
	      "the last device reaches into the raw axis range");
	CHECK((SDL_GC_AXIS_ANALOG_VALUE | 0xFFFF) < PERSDL_CODE_LIMIT,
	      "the code limit cuts off the game controller range");
	CHECK(PERSDL_CODE_LIMIT < KEY_QT_UP && PERSDL_CODE_LIMIT < KEY_MOUSE_MOVE,
	      "the code limit reaches into the host input ranges");
}

//////////////////////////////////////////////////////////////////////////////

/* Two ports must stay independent: the same code bound on one must not reach
   the other. */
static void test_ports_are_independent(void)
{
	PortData_struct port1, port2;
	PerPad_struct * pad1;
	PerPad_struct * pad2;

	PerPortReset();
	memset(&port1, 0, sizeof(port1));
	memset(&port2, 0, sizeof(port2));
	pad1 = PerPadAdd(&port1);
	pad2 = PerPadAdd(&port2);
	if (!pad1 || !pad2) { CHECK(0, "PerPadAdd returned NULL"); return; }

	/* Same model of pad on each port reports different device bits. */
	PerSetKey(KEY_SDL_GC_BUTTON | (0u << 18), PERPAD_A, pad1);
	PerSetKey(KEY_SDL_GC_BUTTON | (1u << 18), PERPAD_A, pad2);

	*pad1->padbits = 0xFF; *(pad1->padbits + 1) = 0xFF;
	*pad2->padbits = 0xFF; *(pad2->padbits + 1) = 0xFF;

	PerKeyDown(KEY_SDL_GC_BUTTON | (0u << 18));

	CHECK(!(*pad1->padbits == 0xFF && *(pad1->padbits + 1) == 0xFF),
	      "port 1 did not react to its own device");
	CHECK(*pad2->padbits == 0xFF && *(pad2->padbits + 1) == 0xFF,
	      "port 2 reacted to port 1's device");
}

//////////////////////////////////////////////////////////////////////////////

int main(void)
{
	printf("peripheral mapping tests\n");

	test_new_peripheral_starts_unbound();
	test_only_the_bound_code_fires();
	test_rebinding_replaces();
	test_unbound_value_never_matches();
	test_embedded_pad_up_reaches_the_pad();
	test_code_spaces_do_not_overlap();
	test_retarget_moves_every_code_this_core_emits();
	test_retarget_leaves_host_input_alone();
	test_retarget_refuses_an_unencodable_device();
	test_device_field_does_not_collide();
	test_ports_are_independent();

	PerPortReset();

	printf("%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
