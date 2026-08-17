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
#ifndef INPUTDEVICECOMBO_H
#define INPUTDEVICECOMBO_H

/*! \file InputDeviceCombo.h
    \brief Turning InputPortConfig choices into combo box entries.

    Two places let the user pick which physical device drives a Saturn port:
    the pad configuration dialog and the first-launch setup wizard. The wording
    has to match between them - a device that reads "Xbox Controller" in one and
    "Xbox Controller (not a known gamepad)" in the other looks like two devices.
    The label building lives here so there is one copy of it.

    This does not belong in InputPortConfig: that namespace deliberately keeps
    translation and widgets out so its policy can be tested with QtCore alone.
*/

#include <QList>
#include <QString>
#include <Qt>

#include "../InputPortConfig.h"

class QComboBox;

namespace InputDeviceCombo
{

//! Item data role carrying the device name without any "(...)" decoration.
//! This, not the label, is what gets stored as DeviceName - a decorated string
//! would accumulate its suffix every time the settings are written back.
const int DeviceNameRole = Qt::UserRole + 1;

/*! Fill combo with these choices and select currentId.

    Signals are blocked while refilling: a rebuild must not look like the user
    picking a device, which would wipe the port's bindings (input device design
    5.6). Falls back to the first entry when currentId is not in the list.
*/
void fill( QComboBox* combo, uint perType,
           const QList<InputPortConfig::Choice>& choices,
           const QString& currentId );

//! The device identifier behind the current entry, or an empty string.
QString selectedDeviceId( const QComboBox* combo );

//! The undecorated name behind the current entry, for storing as DeviceName.
QString selectedDeviceName( const QComboBox* combo );

} // namespace InputDeviceCombo

#endif /* INPUTDEVICECOMBO_H */
