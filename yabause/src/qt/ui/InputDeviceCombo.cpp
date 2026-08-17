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
#include "InputDeviceCombo.h"
#include "../QtYabause.h"

#include <QComboBox>

namespace InputDeviceCombo
{

void fill( QComboBox* combo, uint perType,
           const QList<InputPortConfig::Choice>& choices,
           const QString& currentId )
{
	if ( !combo )
		return;

	// Refilling changes the current index; that must not look like the user
	// picking a device, which would wipe this port's bindings.
	const bool blocked = combo->blockSignals( true );
	combo->clear();

	foreach ( const InputPortConfig::Choice& choice, choices )
	{
		QString label;
		switch ( choice.kind )
		{
			case InputPortConfig::ChoiceNone:
				label = QtYabause::translate( "None" );
				break;
			case InputPortConfig::ChoiceHostInput:
				// For a Saturn mouse or light gun the host mouse is what drives the
				// peripheral, so name the entry after that rather than the keyboard.
				label = InputPortConfig::hostInputIsPointer( perType )
					? QtYabause::translate( "Keyboard / Mouse" )
					: QtYabause::translate( "Keyboard" );
				break;
			case InputPortConfig::ChoiceGamepad:
				label = choice.name;
				break;
			case InputPortConfig::ChoiceUnknownGamepad:
				// Flag the ones the core has no mapping for: those need every button
				// assigning by hand.
				label = QString( "%1 (%2)" )
					.arg( choice.name, QtYabause::translate( "not a known gamepad" ) );
				break;
			case InputPortConfig::ChoiceDisconnected:
				label = QString( "%1 (%2)" )
					.arg( choice.name.isEmpty() ? QtYabause::translate( "Unknown device" ) : choice.name,
					      QtYabause::translate( "not connected" ) );
				break;
		}
		combo->addItem( label, choice.id );
		combo->setItemData( combo->count() - 1,
			choice.name.isEmpty() ? label : choice.name, DeviceNameRole );
	}

	const int index = combo->findData( currentId );
	combo->setCurrentIndex( index < 0 ? 0 : index );
	combo->blockSignals( blocked );
}

QString selectedDeviceId( const QComboBox* combo )
{
	if ( !combo || combo->currentIndex() < 0 )
		return QString();
	return combo->itemData( combo->currentIndex() ).toString();
}

QString selectedDeviceName( const QComboBox* combo )
{
	if ( !combo || combo->currentIndex() < 0 )
		return QString();
	return combo->itemData( combo->currentIndex(), DeviceNameRole ).toString();
}

} // namespace InputDeviceCombo
