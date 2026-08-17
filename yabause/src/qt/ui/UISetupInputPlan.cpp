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
#include "UISetupInputPlan.h"
#include "../InputPortConfig.h"

SetupInputResult setupInputPlan( const QString& storedDeviceId,
                                 const QString& chosenDeviceId,
                                 const QString& chosenDeviceName,
                                 const QMap<u8, u32>& startingPoint,
                                 const QMap<u8, u32>& userAssigned,
                                 bool portUnconfigured )
{
	SetupInputResult result;
	result.deviceId = chosenDeviceId;
	result.deviceName = chosenDeviceName;

	const bool deviceChanged = ( chosenDeviceId != storedDeviceId );

	// Passing straight through both input pages must not rewrite a
	// configuration the user already had. That is the normal case when the
	// wizard is reopened from the Help menu to change something else. An
	// unconfigured port is the exception: there is no existing configuration
	// to protect, and leaving it unwritten just hands the decision to
	// seedPort() instead of honouring what the user picked on the device page.
	if ( !deviceChanged && userAssigned.isEmpty() && !portUnconfigured )
		return result;

	result.write = true;
	// Still gated on deviceChanged alone: an unconfigured port has nothing to
	// clear, and releaseDeviceFromOtherPorts() must not run for a port the
	// user did not actually change.
	result.clearFirst = deviceChanged;

	// A port with no device answers to nothing. The caller may still be holding
	// the mapping from a device the user picked and then changed their mind
	// about, so do not let it through.
	if ( chosenDeviceId == InputPortConfig::NoDeviceId )
		return result;

	// Skipped buttons keep the starting point, so a user who skips everything
	// still ends up with a playable pad rather than an inert one.
	result.bindings = startingPoint;
	for ( QMap<u8, u32>::const_iterator it = userAssigned.constBegin();
	      it != userAssigned.constEnd(); ++it )
		result.bindings.insert( it.key(), it.value() );

	return result;
}
