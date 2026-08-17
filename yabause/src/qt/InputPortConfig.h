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
#ifndef INPUTPORTCONFIG_H
#define INPUTPORTCONFIG_H

/*! \file InputPortConfig.h
    \brief What device drives a Saturn port, and what that implies.

    A Saturn port is driven by exactly one physical device. Everything that
    follows from that rule - which device a port gets by default, what happens
    to the bindings when the device changes, and how taking a device away from
    another port works - lives here rather than in the configuration dialog,
    so it can be exercised without a window.

    Nothing in here touches a widget or the peripheral core: it reads and
    writes QSettings and asks a DeviceSource what is plugged in.
*/

#include <QList>
#include <QMap>
#include <QString>

#include "../core.h"
#include "../peripheral.h"

class QSettings;

namespace InputPortConfig
{

//////////////////////////////////////////////////////////////////////////////
// Settings keys and the two reserved device identifiers.

// Value stored when the port is driven by input the host toolkit delivers
// directly - the keyboard, and the mouse for the Saturn mouse and light gun.
extern const QString KeyboardDeviceId;
// Value stored for a port with no physical device assigned. A port ends up
// here when another port takes the device it was using.
extern const QString NoDeviceId;

// Settings key formats. Literals rather than QString globals so that the
// dialogs can build on the same strings without depending on the order in
// which globals of different translation units are constructed.
#define INPUTPORT_KEY_TYPE          "Input/Port/%1/Id/%2/Type"
#define INPUTPORT_KEY_DEVICE        "Input/Port/%1/Id/%2/Device"
#define INPUTPORT_KEY_DEVICE_NAME   "Input/Port/%1/Id/%2/DeviceName"
#define INPUTPORT_KEY_BINDING       "Input/Port/%1/Id/%2/Controller/%3/Key/%4"
#define INPUTPORT_GROUP_IDS         "Input/Port/%1/Id"
#define INPUTPORT_GROUP_BINDINGS    "Input/Port/%1/Id/%2/Controller/%3/Key"

// The chosen device for a port, and the device's display name as it was when
// the port was configured. The identifier is a device path and is never shown,
// so the name is what identifies a device that is currently unplugged.
QString deviceKey( uint port, uint pad );
QString deviceNameKey( uint port, uint pad );
QString typeKey( uint port, uint pad );
QString bindingKey( uint port, uint pad, uint perType, u8 padKey );

//////////////////////////////////////////////////////////////////////////////

//! A physical input device as the peripheral core reports it.
struct Device
{
	QString id;    //!< Stable identifier: a device path, or a GUID if there is none.
	QString name;  //!< What the device calls itself. Not unique.
	bool known;    //!< The peripheral core has a game controller mapping for it.

	Device() : known( false ) {}
	Device( const QString& deviceId, const QString& deviceName, bool isKnown )
		: id( deviceId ), name( deviceName ), known( isKnown ) {}
};

/*! Where the list of plugged-in devices and their built-in mappings come from.

    This is an interface for one reason: it is the only part of the port
    configuration that needs a real device attached, so the tests replace it
    with a list they control.
*/
class DeviceSource
{
public:
	virtual ~DeviceSource();
	//! Connected devices, in the order the peripheral core reports them.
	virtual QList<Device> devices() const = 0;
	//! The device's built-in pad mapping, keyed by PERPAD_*. Empty if it has none.
	virtual QMap<u8, u32> defaultPadMapping( const QString& deviceId ) const = 0;
};

//! The real thing, backed by the SDL peripheral core.
class SdlDeviceSource : public DeviceSource
{
public:
	virtual QList<Device> devices() const;
	virtual QMap<u8, u32> defaultPadMapping( const QString& deviceId ) const;
};

//////////////////////////////////////////////////////////////////////////////
// The device list offered for a port.

enum ChoiceKind
{
	ChoiceNone,           //!< No device: the port is unassigned.
	ChoiceHostInput,      //!< Keyboard, and the mouse where the peripheral uses one.
	ChoiceGamepad,        //!< Connected, and the core knows its layout.
	ChoiceUnknownGamepad, //!< Connected, but every button has to be assigned by hand.
	ChoiceDisconnected    //!< The device this port is configured for, not plugged in.
};

struct Choice
{
	QString id;
	//! Display name, made unique among the connected devices. Untranslated: the
	//! caller adds whatever wording belongs to the kind.
	QString name;
	ChoiceKind kind;

	Choice() : kind( ChoiceNone ) {}
	Choice( const QString& deviceId, const QString& deviceName, ChoiceKind choiceKind )
		: id( deviceId ), name( deviceName ), kind( choiceKind ) {}
};

/*! The devices this port may be pointed at, in display order.

    Always starts with "none" and the host input entry. A device the port is
    configured for but that is not plugged in is kept at the end, so that its
    bindings are not silently retargeted at whatever happens to be connected
    now and so plugging it back in restores the port.
*/
QList<Choice> choicesForPort( const DeviceSource& source, uint perType,
                              const QString& currentId, const QString& storedName );

//! True where the host input entry covers the mouse as well as the keyboard.
bool hostInputIsPointer( uint perType );

//////////////////////////////////////////////////////////////////////////////
// Policy.

//! True when this port is driven by input the host toolkit delivers directly.
bool bindsHostInput( const QString& deviceId );
//! True for a real device, as opposed to the keyboard or nothing at all.
bool isPhysicalDevice( const QString& deviceId );

/*! The device a port gets the first time it is configured: the order-th
    gamepad the core recognises, or the keyboard when there is none. */
QString defaultDeviceForPort( const DeviceSource& source, uint port, QString* name );

/*! The mapping a port starts with for a given device. A recognised gamepad
    brings its own; the keyboard gets the layout below; anything else starts
    unassigned, because there is nothing sensible to guess. */
QMap<u8, u32> defaultMapping( const DeviceSource& source, const QString& deviceId, uint perType );

//! The keyboard layout, on its own. Kept separate so it can be checked directly.
QMap<u8, u32> keyboardPadMapping();

//! The device a port is configured for, defaulting to the keyboard.
QString configuredDevice( QSettings* settings, uint port, uint pad );

//! Forget every binding stored for a port.
void clearBindings( QSettings* settings, uint port, uint pad, uint perType );

/*! A physical device drives at most one Saturn port, so taking it for one port
    has to release it everywhere else. The keyboard is exempt: two players on
    one keyboard with different keys is a real configuration. */
void releaseDeviceFromOtherPorts( QSettings* settings, uint port, uint pad, const QString& deviceId );

/*! Point a port at a device.

    Bindings recorded against the previous device would keep firing, so they
    are replaced with this device's defaults. Returns false and changes
    nothing when the port is already using this device.
*/
bool selectDevice( QSettings* settings, const DeviceSource& source,
                   uint port, uint pad, uint perType,
                   const QString& deviceId, const QString& deviceName );

/*! Seed a port that has never been configured. Writes the peripheral type,
    the device and its default mapping, and returns the mapping so the caller
    can hand it to the peripheral core. */
QMap<u8, u32> seedPort( QSettings* settings, const DeviceSource& source, uint port, uint pad );

} // namespace InputPortConfig

#endif // INPUTPORTCONFIG_H
