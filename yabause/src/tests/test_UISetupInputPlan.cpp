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

/*! \file test_UISetupInputPlan.cpp
    \brief Unit tests for setupInputPlan(), the wizard's write-or-not decision.

    docs/superpowers/specs/2026-08-08-qt-first-launch-setup-wizard-design.md
    section 4.2.2 spells out four cases for what the wizard writes for the
    input port. Getting one wrong destroys a configuration the user already
    had, so this is checked without opening the wizard's window.

    The port / device rules those cases rest on come from the separate
    2026-08-08-input-device-selection-design.md.
*/

#include <QCoreApplication>
#include <QMap>
#include <QString>

#include <stdio.h>

#include "qt/InputPortConfig.h"
#include "qt/ui/UISetupInputPlan.h"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) \
	do { \
		tests_run++; \
		if (!(cond)) { \
			tests_failed++; \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       QString(msg).toLocal8Bit().constData()); \
		} \
	} while (0)

//////////////////////////////////////////////////////////////////////////////

/* No device change and nothing assigned: the wizard was reopened to change
   something else, and must leave the port's existing configuration alone.
   portUnconfigured is explicitly false here -- this is the Help re-run
   protection, and it must not regress when the port has been set up before. */
static void test_no_change_no_assignment_writes_nothing()
{
	QMap<u8, u32> startingPoint;
	startingPoint[PERPAD_A] = 0x111;
	startingPoint[PERPAD_B] = 0x222;

	const SetupInputResult result = setupInputPlan(
		"/path/pad0", "/path/pad0", "Xbox Controller",
		startingPoint, QMap<u8, u32>(), false );

	CHECK( !result.write, "no device change with nothing assigned still wrote" );
	CHECK( result.bindings.isEmpty(), "a no-op plan produced bindings" );
}

/* No device change but the user assigned some buttons: those overwrite the
   starting point, the rest is left as it was. */
static void test_no_change_with_assignment_overlays_bindings()
{
	QMap<u8, u32> startingPoint;
	startingPoint[PERPAD_UP] = 0x1;
	startingPoint[PERPAD_DOWN] = 0x2;
	startingPoint[PERPAD_A] = 0x3;
	startingPoint[PERPAD_B] = 0x4;
	startingPoint[PERPAD_C] = 0x5;

	QMap<u8, u32> userAssigned;
	userAssigned[PERPAD_A] = 0xA1;
	userAssigned[PERPAD_B] = 0xB1;
	userAssigned[PERPAD_C] = 0xC1;

	const SetupInputResult result = setupInputPlan(
		"/path/pad0", "/path/pad0", "Xbox Controller",
		startingPoint, userAssigned, false );

	CHECK( result.write, "reassigning buttons on the same device did not write" );
	CHECK( !result.clearFirst, "reassigning buttons on the same device cleared first" );

	QMap<u8, u32> expected = startingPoint;
	expected[PERPAD_A] = 0xA1;
	expected[PERPAD_B] = 0xB1;
	expected[PERPAD_C] = 0xC1;
	CHECK( result.bindings == expected,
	       "the overlay of userAssigned onto startingPoint was not as expected" );
}

/* Device changed (keyboard -> pad) and nothing was assigned: the port takes
   the new device's defaults wholesale, and the old bindings must go with it. */
static void test_device_changed_no_assignment_takes_defaults()
{
	QMap<u8, u32> padDefaults;
	padDefaults[PERPAD_A] = 0x400007;
	padDefaults[PERPAD_B] = 0x400008;

	const SetupInputResult result = setupInputPlan(
		InputPortConfig::KeyboardDeviceId, "/path/pad0", "Xbox Controller",
		padDefaults, QMap<u8, u32>(), false );

	CHECK( result.write, "a device change did not write" );
	CHECK( result.clearFirst, "a device change did not clear first" );
	CHECK( result.bindings == padDefaults,
	       "a device change with nothing assigned did not take the new defaults verbatim" );
}

/* Device changed and the user assigned some buttons: the rest still come from
   the new device's defaults. */
static void test_device_changed_with_assignment_overlays_defaults()
{
	QMap<u8, u32> padDefaults;
	padDefaults[PERPAD_UP] = 0x1;
	padDefaults[PERPAD_DOWN] = 0x2;
	padDefaults[PERPAD_A] = 0x400007;
	padDefaults[PERPAD_B] = 0x400008;

	QMap<u8, u32> userAssigned;
	userAssigned[PERPAD_A] = 0x999;
	userAssigned[PERPAD_B] = 0x888;

	const SetupInputResult result = setupInputPlan(
		InputPortConfig::KeyboardDeviceId, "/path/pad0", "Xbox Controller",
		padDefaults, userAssigned, false );

	CHECK( result.write, "a device change with assignments did not write" );
	CHECK( result.clearFirst, "a device change with assignments did not clear first" );

	QMap<u8, u32> expected = padDefaults;
	expected[PERPAD_A] = 0x999;
	expected[PERPAD_B] = 0x888;
	CHECK( result.bindings == expected,
	       "the overlay onto the new device's defaults was not as expected" );
}

/* The user picked a pad, the button page computed that pad's defaults as the
   starting point, then they went Back and chose "None" instead. Without a
   guard the stale pad defaults sitting in startingPoint would be written to a
   port that is supposed to end up with no device at all. */
static void test_choosing_no_device_drops_stale_starting_point()
{
	QMap<u8, u32> stalePadDefaults;
	stalePadDefaults[PERPAD_A] = 0x400007;
	stalePadDefaults[PERPAD_B] = 0x400008;

	const SetupInputResult result = setupInputPlan(
		InputPortConfig::KeyboardDeviceId, InputPortConfig::NoDeviceId, QString(),
		stalePadDefaults, QMap<u8, u32>(), false );

	CHECK( result.write, "switching to no device did not write" );
	CHECK( result.clearFirst, "switching to no device did not clear first" );
	CHECK( result.bindings.isEmpty(),
	       "switching to no device kept the stale starting point's bindings" );
}

/* An unrecognised pad has no built-in mapping at all, so the starting point
   the button page hands over is empty. The result must be exactly what the
   user assigned - nothing more, nothing less. */
static void test_empty_starting_point_keeps_only_assigned()
{
	QMap<u8, u32> userAssigned;
	userAssigned[PERPAD_A] = 0x10;
	userAssigned[PERPAD_B] = 0x20;

	const SetupInputResult result = setupInputPlan(
		InputPortConfig::KeyboardDeviceId, "/path/unknown", "Some HID thing",
		QMap<u8, u32>(), userAssigned, false );

	CHECK( result.write, "a device change from an empty starting point did not write" );
	CHECK( result.bindings.count() == 2,
	       "an empty starting point produced more bindings than were assigned" );
	CHECK( result.bindings == userAssigned,
	       "an empty starting point did not keep exactly what was assigned" );
}

/* The caller may not always be able to produce a name (for example, a device
   that vanished between the two wizard pages). An empty name must not be
   treated as a reason to skip writing. */
static void test_empty_device_name_still_writes()
{
	QMap<u8, u32> padDefaults;
	padDefaults[PERPAD_A] = 0x400007;

	const SetupInputResult result = setupInputPlan(
		InputPortConfig::KeyboardDeviceId, "/path/pad0", QString(),
		padDefaults, QMap<u8, u32>(), false );

	CHECK( result.write, "an empty device name suppressed writing a physical device" );
	CHECK( result.deviceName.isEmpty(), "the empty device name was not passed through" );
}

/* Re-picking the device the port is already on, with nothing assigned, must
   be indistinguishable from never having opened the input pages at all - see
   input-device-selection-design.md section 8.3, point 3-5. */
static void test_reselecting_same_device_writes_nothing()
{
	QMap<u8, u32> startingPoint;
	startingPoint[PERPAD_A] = 0x400007;
	startingPoint[PERPAD_B] = 0x400008;

	const SetupInputResult result = setupInputPlan(
		"/path/pad0", "/path/pad0", "Xbox Controller",
		startingPoint, QMap<u8, u32>(), false );

	CHECK( !result.write, "reselecting the same device with nothing assigned still wrote" );
	CHECK( result.bindings.isEmpty(), "reselecting the same device produced bindings" );
}

/* First launch: the port has never been configured, the user leaves the
   device combo on Keyboard (deviceChanged is false because configuredDevice()
   already reports the keyboard as the stored default) and skips every button.
   Writing nothing here would let YabauseThread::reloadControllers() fall
   through to InputPortConfig::seedPort(), which can pick a gamepad instead of
   the keyboard the user actually chose. portUnconfigured must force the write
   through unchanged: same bindings as the starting point, no clearing. */
static void test_unconfigured_port_writes_even_with_no_change()
{
	QMap<u8, u32> startingPoint;
	startingPoint[PERPAD_A] = 0x111;
	startingPoint[PERPAD_B] = 0x222;

	const SetupInputResult result = setupInputPlan(
		InputPortConfig::KeyboardDeviceId, InputPortConfig::KeyboardDeviceId, QString(),
		startingPoint, QMap<u8, u32>(), true );

	CHECK( result.write, "an unconfigured port with no change and nothing assigned did not write" );
	CHECK( !result.clearFirst, "an unconfigured port with no device change cleared first" );
	CHECK( result.bindings == startingPoint,
	       "an unconfigured port's write did not keep the starting point verbatim" );
}

//////////////////////////////////////////////////////////////////////////////

int main( int argc, char** argv )
{
	QCoreApplication app( argc, argv );

	printf( "setup wizard input plan tests\n" );

	test_no_change_no_assignment_writes_nothing();
	test_no_change_with_assignment_overlays_bindings();
	test_device_changed_no_assignment_takes_defaults();
	test_device_changed_with_assignment_overlays_defaults();
	test_choosing_no_device_drops_stale_starting_point();
	test_empty_starting_point_keeps_only_assigned();
	test_empty_device_name_still_writes();
	test_reselecting_same_device_writes_nothing();
	test_unconfigured_port_writes_even_with_no_change();

	printf( "%d checks, %d failed\n", tests_run, tests_failed );
	return tests_failed == 0 ? 0 : 1;
}
