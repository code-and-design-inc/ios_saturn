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

/*! \file test_input_port_config.cpp
    \brief Unit tests for the Saturn port / physical device policy.

    These cover the behaviour described in
    docs/superpowers/specs/2026-08-08-input-device-selection-design.md:
    the default device and mapping a port gets (section 8.1), the device list shown
    for a port (section 8.2, section 8.8), what changing the device does to the bindings
    (section 8.3), and handing a device over from one port to another (section 8.6).

    No widgets and no SDL: the device list comes from a stub, and the settings
    live in a temporary ini file. QtCore only, so this needs no display.
*/

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <stdio.h>

#include "qt/InputPortConfig.h"

using namespace InputPortConfig;

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

/* Stands in for the SDL device list. The mapping it hands out is deliberately
   not the keyboard one, so a test can tell which of the two a port ended up
   with. */
class StubDeviceSource : public DeviceSource
{
public:
	QList<Device> list;

	void add( const QString& id, const QString& name, bool known = true )
	{
		list << Device( id, name, known );
	}

	virtual QList<Device> devices() const { return list; }

	virtual QMap<u8, u32> defaultPadMapping( const QString& deviceId ) const
	{
		QMap<u8, u32> keys;
		for ( int i = 0; i < list.count(); i++ )
		{
			if ( list[i].id != deviceId || !list[i].known )
				continue;
			// A made-up but device-distinct code space, shaped like the real
			// one: the device index lives in the upper bits.
			const u32 base = 0x400000 | ( (u32)i << 18 );
			for ( u8 name = PERPAD_UP; name <= PERPAD_Z; name++ )
				keys[name] = base | name;
			return keys;
		}
		return keys;
	}
};

//! A settings object over a file of its own, so tests cannot leak into each other.
class TestSettings
{
public:
	TestSettings() : mSettings( 0 )
	{
		mSettings = new QSettings( mDir.path() + "/test.ini", QSettings::IniFormat );
	}
	~TestSettings() { delete mSettings; }

	QSettings* operator->() const { return mSettings; }
	operator QSettings*() const { return mSettings; }

private:
	QTemporaryDir mDir;
	QSettings* mSettings;
};

static bool hasBinding( QSettings* settings, uint port, uint pad, uint perType, u8 key )
{
	return settings->contains( bindingKey( port, pad, perType, key ) );
}

static u32 binding( QSettings* settings, uint port, uint pad, uint perType, u8 key )
{
	return settings->value( bindingKey( port, pad, perType, key ) ).toUInt();
}

//////////////////////////////////////////////////////////////////////////////
// section 8.1 - the device and mapping a port starts with.

/* With no gamepad attached a port falls back to the keyboard, and the keyboard
   layout is the one the user asked for: face buttons on the home row. */
static void test_default_without_a_gamepad()
{
	StubDeviceSource source;
	QString name;

	const QString id = defaultDeviceForPort( source, 1, &name );
	CHECK( id == KeyboardDeviceId, "port 1 did not fall back to the keyboard" );

	const QMap<u8, u32> keys = defaultMapping( source, id, PERPAD );
	CHECK( keys.value( PERPAD_A ) == Qt::Key_Z, "A is not Z" );
	CHECK( keys.value( PERPAD_B ) == Qt::Key_X, "B is not X" );
	CHECK( keys.value( PERPAD_C ) == Qt::Key_C, "C is not C" );
	CHECK( keys.value( PERPAD_X ) == Qt::Key_A, "X is not A" );
	CHECK( keys.value( PERPAD_Y ) == Qt::Key_S, "Y is not S" );
	CHECK( keys.value( PERPAD_Z ) == Qt::Key_D, "Z is not D" );
	CHECK( keys.value( PERPAD_LEFT_TRIGGER ) == Qt::Key_Q, "L is not Q" );
	CHECK( keys.value( PERPAD_RIGHT_TRIGGER ) == Qt::Key_E, "R is not E" );
	CHECK( keys.value( PERPAD_START ) == Qt::Key_Return, "Start is not Return" );
	CHECK( keys.value( PERPAD_UP ) == Qt::Key_Up &&
	       keys.value( PERPAD_DOWN ) == Qt::Key_Down &&
	       keys.value( PERPAD_LEFT ) == Qt::Key_Left &&
	       keys.value( PERPAD_RIGHT ) == Qt::Key_Right,
	       "the directions are not the cursor keys" );
	CHECK( keys.count() == 13, "the keyboard layout does not cover the whole pad" );
}

/* With a gamepad attached the port takes it, and the mapping comes from the
   device rather than from the keyboard layout. */
static void test_default_with_a_gamepad()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );
	QString name;

	const QString id = defaultDeviceForPort( source, 1, &name );
	CHECK( id == "/path/pad0", "port 1 did not take the attached gamepad" );
	CHECK( name == "Xbox Controller", "the device name was not reported" );

	const QMap<u8, u32> keys = defaultMapping( source, id, PERPAD );
	CHECK( keys.value( PERPAD_A ) != (u32)Qt::Key_Z,
	       "a gamepad port was seeded with the keyboard layout" );
	CHECK( keys.count() == 13, "the gamepad layout does not cover the whole pad" );
}

/* Each port takes a different pad, and a port with nothing left for it falls
   back to the keyboard rather than sharing. */
static void test_ports_take_different_pads()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );
	source.add( "/path/pad1", "Xbox Controller" );

	CHECK( defaultDeviceForPort( source, 1, 0 ) == "/path/pad0", "port 1 took the wrong pad" );
	CHECK( defaultDeviceForPort( source, 2, 0 ) == "/path/pad1", "port 2 took the wrong pad" );

	StubDeviceSource one;
	one.add( "/path/pad0", "Xbox Controller" );
	CHECK( defaultDeviceForPort( one, 2, 0 ) == KeyboardDeviceId,
	       "port 2 shared port 1's pad instead of falling back" );
}

/* A device the core has no mapping for cannot be seeded: it would produce a
   pad with nothing bound while looking configured. */
static void test_unknown_device_is_not_seeded()
{
	StubDeviceSource source;
	source.add( "/path/thing", "Some HID thing", false );

	CHECK( defaultDeviceForPort( source, 1, 0 ) == KeyboardDeviceId,
	       "an unrecognised device was picked as the default" );
	CHECK( defaultMapping( source, "/path/thing", PERPAD ).isEmpty(),
	       "an unrecognised device produced a mapping" );
}

/* Only the digital pad has a layout worth guessing. */
static void test_only_the_pad_has_defaults()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	CHECK( defaultMapping( source, KeyboardDeviceId, PERWHEEL ).isEmpty(),
	       "the wheel was given a default mapping" );
	CHECK( defaultMapping( source, "/path/pad0", PERMOUSE ).isEmpty(),
	       "the mouse was given a default mapping" );
	CHECK( defaultMapping( source, NoDeviceId, PERPAD ).isEmpty(),
	       "a port with no device was given a mapping" );
}

//////////////////////////////////////////////////////////////////////////////
// section 8.2, section 8.8 - the list of devices offered for a port.

/* The list always starts with "none" and the host input entry, and every
   connected device follows. */
static void test_choices_list_shape()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	const QList<Choice> choices = choicesForPort( source, PERPAD, KeyboardDeviceId, QString() );
	CHECK( choices.count() == 3, "unexpected number of entries" );
	CHECK( choices[0].id == NoDeviceId && choices[0].kind == ChoiceNone, "no 'none' entry first" );
	CHECK( choices[1].id == KeyboardDeviceId && choices[1].kind == ChoiceHostInput,
	       "no host input entry second" );
	CHECK( choices[2].id == "/path/pad0" && choices[2].kind == ChoiceGamepad,
	       "the connected pad is missing" );

	CHECK( hostInputIsPointer( PERMOUSE ) && hostInputIsPointer( PERGUN ),
	       "the mouse and gun are not treated as pointer driven" );
	CHECK( !hostInputIsPointer( PERPAD ), "the pad was treated as pointer driven" );
}

/* Two pads of the same model are numbered so the user can tell them apart -
   both of them, because a bare name next to a numbered one reads as a
   different device. A device with a unique name is left alone. */
static void test_duplicate_names_are_numbered()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );
	source.add( "/path/pad1", "Xbox Controller" );
	source.add( "/path/pad2", "DualSense" );

	const QList<Choice> choices = choicesForPort( source, PERPAD, KeyboardDeviceId, QString() );
	CHECK( choices.count() == 5, "unexpected number of entries" );
	CHECK( choices[2].name == "Xbox Controller #1", "the first duplicate was not numbered" );
	CHECK( choices[3].name == "Xbox Controller #2", "the second duplicate was not numbered" );
	CHECK( choices[4].name == "DualSense", "a unique name was numbered" );
}

/* A device the core has no mapping for is still offered - it just has to be
   marked, because every button needs assigning by hand. */
static void test_unknown_device_is_marked()
{
	StubDeviceSource source;
	source.add( "/path/thing", "Some HID thing", false );

	const QList<Choice> choices = choicesForPort( source, PERPAD, KeyboardDeviceId, QString() );
	CHECK( choices.count() == 3, "the unrecognised device was dropped from the list" );
	CHECK( choices[2].kind == ChoiceUnknownGamepad, "the unrecognised device was not marked" );
}

/* The device a port is configured for stays in the list while it is unplugged,
   under the name it had. Dropping it would retarget the bindings at whatever
   happens to be connected instead. */
static void test_disconnected_device_stays_listed()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	const QList<Choice> choices = choicesForPort( source, PERPAD, "/path/gone", "DualSense" );
	CHECK( choices.count() == 4, "the unplugged device was dropped" );
	CHECK( choices[3].id == "/path/gone" && choices[3].kind == ChoiceDisconnected,
	       "the unplugged device is not marked as such" );
	CHECK( choices[3].name == "DualSense", "the unplugged device lost its name" );

	// Plug it back in and it is an ordinary entry again, listed once.
	source.add( "/path/gone", "DualSense" );
	const QList<Choice> back = choicesForPort( source, PERPAD, "/path/gone", "DualSense" );
	CHECK( back.count() == 4, "the device was listed twice after reconnecting" );
	CHECK( back[3].id == "/path/gone" && back[3].kind == ChoiceGamepad,
	       "the reconnected device is still marked as disconnected" );
}

//////////////////////////////////////////////////////////////////////////////
// section 8.3 - changing the device for a port.

/* Selecting a device replaces the bindings. Those recorded against the
   previous one would otherwise keep firing - that is how keyboard input
   survived selecting a gamepad. */
static void test_changing_device_replaces_bindings()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	TestSettings settings;
	seedPort( settings, source, 1, 1 );
	CHECK( configuredDevice( settings, 1, 1 ) == "/path/pad0",
	       "the port was not seeded with the pad" );

	const u32 padCode = binding( settings, 1, 1, PERPAD, PERPAD_A );
	CHECK( padCode != 0, "the seeded port has no binding for A" );

	const bool changed = selectDevice( settings, source, 1, 1, PERPAD,
	                                   KeyboardDeviceId, "Keyboard" );
	CHECK( changed, "selecting a different device reported no change" );
	CHECK( configuredDevice( settings, 1, 1 ) == KeyboardDeviceId,
	       "the device was not stored" );
	CHECK( binding( settings, 1, 1, PERPAD, PERPAD_A ) == (u32)Qt::Key_Z,
	       "the gamepad binding survived switching to the keyboard" );

	// And back again: no keyboard code may be left behind.
	selectDevice( settings, source, 1, 1, PERPAD, "/path/pad0", "Xbox Controller" );
	CHECK( binding( settings, 1, 1, PERPAD, PERPAD_A ) == padCode,
	       "the keyboard binding survived switching to the pad" );
	CHECK( settings->value( deviceNameKey( 1, 1 ) ).toString() == "Xbox Controller",
	       "the device name was not stored" );
}

/* Re-selecting the device a port already uses must not touch anything: the
   dialog refills its list on every hotplug, and that must not read as the user
   picking a device. */
static void test_reselecting_the_same_device_changes_nothing()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	TestSettings settings;
	seedPort( settings, source, 1, 1 );
	settings->setValue( bindingKey( 1, 1, PERPAD, PERPAD_A ), 0x1234u );

	const bool changed = selectDevice( settings, source, 1, 1, PERPAD,
	                                   "/path/pad0", "Xbox Controller" );
	CHECK( !changed, "reselecting the same device reported a change" );
	CHECK( binding( settings, 1, 1, PERPAD, PERPAD_A ) == 0x1234u,
	       "reselecting the same device wiped a hand-made binding" );
}

/* Switching a port to "none" leaves it with nothing bound. Seeding the new
   device's defaults happens to overwrite every key when there is a layout to
   seed, so this is the case that actually proves the old bindings are dropped
   rather than just written over. */
static void test_selecting_no_device_drops_the_bindings()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	TestSettings settings;
	seedPort( settings, source, 1, 1 );
	CHECK( hasBinding( settings, 1, 1, PERPAD, PERPAD_A ), "the port was not seeded" );

	selectDevice( settings, source, 1, 1, PERPAD, NoDeviceId, QString() );
	for ( u8 name = PERPAD_UP; name <= PERPAD_Z; name++ )
		CHECK( !hasBinding( settings, 1, 1, PERPAD, name ),
		       QString( "button %1 kept its binding after the device was removed" ).arg( name ) );
}

/* A peripheral with no built-in layout ends up with nothing bound rather than
   with the previous device's bindings. */
static void test_changing_device_on_a_peripheral_without_defaults()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	TestSettings settings;
	settings->setValue( typeKey( 1, 1 ), PERWHEEL );
	settings->setValue( bindingKey( 1, 1, PERWHEEL, 0 ), 0x5A );

	selectDevice( settings, source, 1, 1, PERWHEEL, "/path/pad0", "Xbox Controller" );
	CHECK( !hasBinding( settings, 1, 1, PERWHEEL, 0 ),
	       "a wheel kept its old binding after changing device" );
}

//////////////////////////////////////////////////////////////////////////////
// section 8.6 - handing a device over between ports.

/* Taking a device that another port is using releases it there, and leaves
   that port visibly unassigned rather than quietly on the keyboard. */
static void test_taking_a_device_releases_the_other_port()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );

	TestSettings settings;
	seedPort( settings, source, 1, 1 );
	CHECK( configuredDevice( settings, 1, 1 ) == "/path/pad0", "port 1 was not seeded" );

	settings->setValue( typeKey( 2, 1 ), PERPAD );
	selectDevice( settings, source, 2, 1, PERPAD, "/path/pad0", "Xbox Controller" );

	CHECK( configuredDevice( settings, 2, 1 ) == "/path/pad0", "port 2 did not take the pad" );
	CHECK( settings->value( deviceKey( 1, 1 ) ).toString() == NoDeviceId,
	       "port 1 still claims the pad another port took" );
	CHECK( !hasBinding( settings, 1, 1, PERPAD, PERPAD_A ),
	       "port 1 kept bindings for a device it no longer has" );
	CHECK( hasBinding( settings, 2, 1, PERPAD, PERPAD_A ),
	       "port 2 was not given the pad's mapping" );
}

/* The keyboard is exempt: two players on one keyboard with different keys is
   a real configuration. */
static void test_the_keyboard_is_shared()
{
	StubDeviceSource source;

	TestSettings settings;
	settings->setValue( typeKey( 1, 1 ), PERPAD );
	settings->setValue( typeKey( 2, 1 ), PERPAD );
	settings->setValue( deviceKey( 1, 1 ), KeyboardDeviceId );
	settings->setValue( bindingKey( 1, 1, PERPAD, PERPAD_A ), (quint32)Qt::Key_Z );

	settings->setValue( deviceKey( 2, 1 ), NoDeviceId );
	selectDevice( settings, source, 2, 1, PERPAD, KeyboardDeviceId, "Keyboard" );

	CHECK( configuredDevice( settings, 1, 1 ) == KeyboardDeviceId,
	       "port 1 lost the keyboard when port 2 took it" );
	CHECK( binding( settings, 1, 1, PERPAD, PERPAD_A ) == (u32)Qt::Key_Z,
	       "port 1's keyboard bindings were cleared" );
}

/* Releasing must not reach the port being configured, and must not touch a
   port that uses a different device. */
static void test_releasing_leaves_other_devices_alone()
{
	StubDeviceSource source;
	source.add( "/path/pad0", "Xbox Controller" );
	source.add( "/path/pad1", "DualSense" );

	TestSettings settings;
	seedPort( settings, source, 1, 1 );
	settings->setValue( typeKey( 2, 1 ), PERPAD );
	selectDevice( settings, source, 2, 1, PERPAD, "/path/pad1", "DualSense" );

	releaseDeviceFromOtherPorts( settings, 2, 1, "/path/pad1" );
	CHECK( configuredDevice( settings, 2, 1 ) == "/path/pad1",
	       "the port being configured released its own device" );
	CHECK( configuredDevice( settings, 1, 1 ) == "/path/pad0",
	       "a port using a different device was released" );
	CHECK( hasBinding( settings, 1, 1, PERPAD, PERPAD_A ),
	       "a port using a different device lost its bindings" );
}

//////////////////////////////////////////////////////////////////////////////
// Identifiers.

static void test_device_id_classification()
{
	CHECK( bindsHostInput( KeyboardDeviceId ), "the keyboard does not bind host input" );
	CHECK( !bindsHostInput( "/path/pad0" ), "a gamepad binds host input" );
	CHECK( !bindsHostInput( NoDeviceId ), "an unassigned port binds host input" );

	CHECK( isPhysicalDevice( "/path/pad0" ), "a device path is not a physical device" );
	CHECK( !isPhysicalDevice( KeyboardDeviceId ), "the keyboard counts as a physical device" );
	CHECK( !isPhysicalDevice( NoDeviceId ), "'none' counts as a physical device" );
	CHECK( !isPhysicalDevice( QString() ), "an empty id counts as a physical device" );

	TestSettings settings;
	CHECK( configuredDevice( settings, 1, 1 ) == KeyboardDeviceId,
	       "an unconfigured port does not default to the keyboard" );
}

//////////////////////////////////////////////////////////////////////////////

int main( int argc, char** argv )
{
	QCoreApplication app( argc, argv );

	printf( "input port configuration tests\n" );

	test_default_without_a_gamepad();
	test_default_with_a_gamepad();
	test_ports_take_different_pads();
	test_unknown_device_is_not_seeded();
	test_only_the_pad_has_defaults();

	test_choices_list_shape();
	test_duplicate_names_are_numbered();
	test_unknown_device_is_marked();
	test_disconnected_device_stays_listed();

	test_changing_device_replaces_bindings();
	test_reselecting_the_same_device_changes_nothing();
	test_selecting_no_device_drops_the_bindings();
	test_changing_device_on_a_peripheral_without_defaults();

	test_taking_a_device_releases_the_other_port();
	test_the_keyboard_is_shared();
	test_releasing_leaves_other_devices_alone();

	test_device_id_classification();

	printf( "%d checks, %d failed\n", tests_run, tests_failed );
	return tests_failed == 0 ? 0 : 1;
}
