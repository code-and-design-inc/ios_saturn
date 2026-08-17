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

#include "InputPortConfig.h"

#include <QSettings>
#include <QStringList>
#include <QtCore/qnamespace.h>

#ifdef HAVE_LIBSDL
#include "../persdljoy.h"
#endif

namespace InputPortConfig
{

const QString KeyboardDeviceId = "keyboard";
const QString NoDeviceId = "none";

// The Saturn has two ports. Releasing a device from the ports that are not
// being configured has to look at all of them.
static const uint PortCount = 2;

//////////////////////////////////////////////////////////////////////////////

QString deviceKey( uint port, uint pad )
{
	return QString( INPUTPORT_KEY_DEVICE ).arg( port ).arg( pad );
}

QString deviceNameKey( uint port, uint pad )
{
	return QString( INPUTPORT_KEY_DEVICE_NAME ).arg( port ).arg( pad );
}

QString typeKey( uint port, uint pad )
{
	return QString( INPUTPORT_KEY_TYPE ).arg( port ).arg( pad );
}

QString bindingKey( uint port, uint pad, uint perType, u8 padKey )
{
	return QString( INPUTPORT_KEY_BINDING ).arg( port ).arg( pad ).arg( perType ).arg( padKey );
}

//////////////////////////////////////////////////////////////////////////////

DeviceSource::~DeviceSource()
{
}

QList<Device> SdlDeviceSource::devices() const
{
	QList<Device> list;
#ifdef HAVE_LIBSDL
	const int count = PERSDLJoyGetDeviceCount();
	for ( int i = 0; i < count; i++ )
	{
		char deviceId[512] = { 0 };
		char name[256] = { 0 };
		int isGameController = 0;
		if ( !PERSDLJoyGetDeviceInfo( i, deviceId, sizeof(deviceId), name, sizeof(name), &isGameController ) )
			continue;
		list << Device( QString::fromLatin1( deviceId ), QString::fromUtf8( name ), isGameController != 0 );
	}
#endif
	return list;
}

QMap<u8, u32> SdlDeviceSource::defaultPadMapping( const QString& deviceId ) const
{
	QMap<u8, u32> keys;
#ifdef HAVE_LIBSDL
	const int index = PERSDLJoyGetDeviceIndexForId( deviceId.toLatin1().constData() );
	if ( index < 0 )
		return keys;

	// Anything the core leaves untouched has to read as unbound, so the marker
	// is what this starts from - 0 is a real code on the embedded ports.
	u32 padKeys[PERPAD_Z + 1];
	for ( u8 name = PERPAD_UP; name <= PERPAD_Z; name++ )
		padKeys[name] = PERKEY_UNBOUND;
	if ( !PERSDLJoyGetDefaultPadMappingForIndex( index, padKeys, PERPAD_Z + 1 ) )
		return keys;

	for ( u8 name = PERPAD_UP; name <= PERPAD_Z; name++ )
		if ( padKeys[name] != PERKEY_UNBOUND )
			keys[name] = padKeys[name];
#else
	Q_UNUSED( deviceId );
#endif
	return keys;
}

//////////////////////////////////////////////////////////////////////////////

bool hostInputIsPointer( uint perType )
{
	// The Saturn mouse and light gun are driven by the host mouse, not by keys,
	// so naming that entry after the keyboard alone would be wrong.
	return perType == PERMOUSE || perType == PERGUN;
}

bool bindsHostInput( const QString& deviceId )
{
	return deviceId == KeyboardDeviceId;
}

bool isPhysicalDevice( const QString& deviceId )
{
	return !deviceId.isEmpty() && deviceId != KeyboardDeviceId && deviceId != NoDeviceId;
}

//////////////////////////////////////////////////////////////////////////////

QList<Choice> choicesForPort( const DeviceSource& source, uint perType,
                              const QString& currentId, const QString& storedName )
{
	Q_UNUSED( perType );

	QList<Choice> choices;
	choices << Choice( NoDeviceId, QString(), ChoiceNone );
	choices << Choice( KeyboardDeviceId, QString(), ChoiceHostInput );

	const QList<Device> devices = source.devices();

	// Two pads of the same model report the same name, which would leave the
	// user picking blind. Number them - and number every one of the duplicates,
	// because a bare name next to a numbered one reads as a different device.
	QMap<QString, int> total;
	for ( int i = 0; i < devices.count(); i++ )
		total[ devices[i].name ] = total.value( devices[i].name, 0 ) + 1;

	QMap<QString, int> seen;
	for ( int i = 0; i < devices.count(); i++ )
	{
		QString label = devices[i].name;
		if ( total.value( label ) > 1 )
		{
			const int n = seen.value( devices[i].name, 0 ) + 1;
			seen[ devices[i].name ] = n;
			label = QString( "%1 #%2" ).arg( label ).arg( n );
		}
		choices << Choice( devices[i].id, label,
		                   devices[i].known ? ChoiceGamepad : ChoiceUnknownGamepad );
	}

	// The port is configured for something that is not plugged in. Keep it in
	// the list: dropping it would retarget the bindings at whatever happens to
	// be connected now, and plugging the device back in has to restore the port.
	if ( isPhysicalDevice( currentId ) )
	{
		bool present = false;
		for ( int i = 0; i < devices.count(); i++ )
			if ( devices[i].id == currentId )
			{
				present = true;
				break;
			}

		if ( !present )
			choices << Choice( currentId, storedName, ChoiceDisconnected );
	}

	return choices;
}

//////////////////////////////////////////////////////////////////////////////

QString defaultDeviceForPort( const DeviceSource& source, uint port, QString* name )
{
	const QList<Device> devices = source.devices();
	const int order = (int)port - 1;
	int seen = 0;

	for ( int i = 0; i < devices.count(); i++ )
	{
		// A device the core has no mapping for cannot be seeded automatically:
		// every button would have to be assigned by hand anyway.
		if ( !devices[i].known )
			continue;
		if ( seen++ == order )
		{
			if ( name )
				*name = devices[i].name;
			return devices[i].id;
		}
	}

	if ( name )
		*name = QString();
	return KeyboardDeviceId;
}

//////////////////////////////////////////////////////////////////////////////

QMap<u8, u32> keyboardPadMapping()
{
	// Assumes a Saturn pad is what the player has in mind: the face buttons sit
	// on the home row and the shoulders on the row above.
	QMap<u8, u32> keys;
	keys[PERPAD_UP]    = Qt::Key_Up;
	keys[PERPAD_RIGHT] = Qt::Key_Right;
	keys[PERPAD_DOWN]  = Qt::Key_Down;
	keys[PERPAD_LEFT]  = Qt::Key_Left;
	keys[PERPAD_RIGHT_TRIGGER] = Qt::Key_E;
	keys[PERPAD_LEFT_TRIGGER]  = Qt::Key_Q;
	keys[PERPAD_START] = Qt::Key_Return;
	keys[PERPAD_A] = Qt::Key_Z;
	keys[PERPAD_B] = Qt::Key_X;
	keys[PERPAD_C] = Qt::Key_C;
	keys[PERPAD_X] = Qt::Key_A;
	keys[PERPAD_Y] = Qt::Key_S;
	keys[PERPAD_Z] = Qt::Key_D;
	return keys;
}

QMap<u8, u32> defaultMapping( const DeviceSource& source, const QString& deviceId, uint perType )
{
	// Only the standard digital pad has a built-in layout. For the wheel,
	// sticks, gun and mouse there is nothing sensible to guess, so they start
	// unassigned.
	if ( perType != PERPAD )
		return QMap<u8, u32>();

	if ( deviceId == NoDeviceId || deviceId.isEmpty() )
		return QMap<u8, u32>();

	if ( deviceId == KeyboardDeviceId )
		return keyboardPadMapping();

	return source.defaultPadMapping( deviceId );
}

//////////////////////////////////////////////////////////////////////////////

QString configuredDevice( QSettings* settings, uint port, uint pad )
{
	return settings->value( deviceKey( port, pad ), KeyboardDeviceId ).toString();
}

void clearBindings( QSettings* settings, uint port, uint pad, uint perType )
{
	const QString group = QString( INPUTPORT_GROUP_BINDINGS ).arg( port ).arg( pad ).arg( perType );

	settings->beginGroup( group );
	const QStringList keys = settings->childKeys();
	settings->endGroup();

	foreach ( const QString& key, keys )
		settings->remove( group + "/" + key );
}

//////////////////////////////////////////////////////////////////////////////

void releaseDeviceFromOtherPorts( QSettings* settings, uint port, uint pad, const QString& deviceId )
{
	if ( !isPhysicalDevice( deviceId ) )
		return;

	for ( uint otherPort = 1; otherPort <= PortCount; otherPort++ )
	{
		settings->beginGroup( QString( INPUTPORT_GROUP_IDS ).arg( otherPort ) );
		const QStringList ids = settings->childGroups();
		settings->endGroup();

		foreach ( const QString& id, ids )
		{
			const uint otherPad = id.toUInt();
			if ( otherPort == port && otherPad == pad )
				continue;

			if ( settings->value( deviceKey( otherPort, otherPad ) ).toString() != deviceId )
				continue;

			// Leave that port visibly unassigned rather than moving it to the
			// keyboard, which would silently make it controllable again.
			settings->setValue( deviceKey( otherPort, otherPad ), NoDeviceId );
			clearBindings( settings, otherPort, otherPad,
			               settings->value( typeKey( otherPort, otherPad ) ).toUInt() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

bool selectDevice( QSettings* settings, const DeviceSource& source,
                   uint port, uint pad, uint perType,
                   const QString& deviceId, const QString& deviceName )
{
	if ( configuredDevice( settings, port, pad ) == deviceId )
		return false;

	settings->setValue( deviceKey( port, pad ), deviceId );
	settings->setValue( deviceNameKey( port, pad ), deviceName );

	// Hand the device over: it can only drive one port.
	releaseDeviceFromOtherPorts( settings, port, pad, deviceId );

	// Bindings recorded against the previous device would keep firing - that is
	// how keyboard input survived selecting a gamepad - so replace them with
	// this device's defaults.
	clearBindings( settings, port, pad, perType );

	const QMap<u8, u32> defaults = defaultMapping( source, deviceId, perType );
	for ( QMap<u8, u32>::const_iterator it = defaults.constBegin(); it != defaults.constEnd(); ++it )
		settings->setValue( bindingKey( port, pad, perType, it.key() ), (quint32)it.value() );

	return true;
}

//////////////////////////////////////////////////////////////////////////////

QMap<u8, u32> seedPort( QSettings* settings, const DeviceSource& source, uint port, uint pad )
{
	QString deviceName;
	const QString deviceId = defaultDeviceForPort( source, port, &deviceName );
	const QMap<u8, u32> defaults = defaultMapping( source, deviceId, PERPAD );

	settings->setValue( typeKey( port, pad ), PERPAD );
	settings->setValue( deviceKey( port, pad ), deviceId );
	settings->setValue( deviceNameKey( port, pad ), deviceName );

	for ( QMap<u8, u32>::const_iterator it = defaults.constBegin(); it != defaults.constEnd(); ++it )
		settings->setValue( bindingKey( port, pad, PERPAD, it.key() ), (quint32)it.value() );

	return defaults;
}

} // namespace InputPortConfig
