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
#ifndef UISETUPINPUTPLAN_H
#define UISETUPINPUTPLAN_H

/*! \file UISetupInputPlan.h
    \brief What the setup wizard should write for the input port, if anything.

    Whether the wizard touches the input settings at all depends on what the
    user did on two pages, and getting it wrong destroys a configuration the
    user already had. The decision is kept out of the wizard pages so it can be
    checked without opening a window.
*/

#include <QMap>
#include <QString>

#include "../../core.h"

struct SetupInputResult
{
	//! Write anything at all for this port. False leaves the settings alone.
	bool write;
	/*! Drop the port's stored bindings before writing, and release the device
	    from any other port. Only true when the device actually changed: the
	    new device's defaults do not necessarily cover every button (an
	    unrecognised pad has no defaults at all), so bindings recorded against
	    the previous device would otherwise survive and keep firing. */
	bool clearFirst;
	QString deviceId;
	QString deviceName;
	//! PERPAD_* -> input code. Empty when write is false.
	QMap<u8, u32> bindings;

	SetupInputResult() : write( false ), clearFirst( false ) {}
};

/*! Decide what the wizard writes for port 1.

    storedDeviceId : the Device already in the settings file. Callers pass
                     InputPortConfig::configuredDevice(), which reports the
                     keyboard when the key is absent.
    chosenDeviceId : what the device page ended up on.
    chosenDeviceName : the undecorated display name for chosenDeviceId.
    startingPoint  : the mapping the button page started from - the new
                     device's defaults after a device change, the stored
                     bindings otherwise.
    userAssigned   : only the buttons the user actually pressed an input for.
                     Skipped buttons are absent, so they keep the starting
                     point's value.
    portUnconfigured : true when the port has never been written before (no
                     Type/Device/DeviceName key exists yet). The "write
                     nothing" rule below is right for a Help-menu re-run where
                     the user passed through both pages without touching
                     anything, but wrong here: writing nothing on an
                     unconfigured port does not leave it alone, it hands the
                     decision to YabauseThread::reloadControllers(), which
                     calls InputPortConfig::seedPort() and picks the first
                     recognised gamepad -- even if the user explicitly chose
                     Keyboard on the device page. Passing true forces the
                     write so the user's choice actually lands in the ini.
*/
SetupInputResult setupInputPlan( const QString& storedDeviceId,
                                 const QString& chosenDeviceId,
                                 const QString& chosenDeviceName,
                                 const QMap<u8, u32>& startingPoint,
                                 const QMap<u8, u32>& userAssigned,
                                 bool portUnconfigured );

#endif /* UISETUPINPUTPLAN_H */
