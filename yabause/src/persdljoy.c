/*  Copyright 2005 Guillaume Duhamel
	Copyright 2005-2006 Theo Berkau
	Copyright 2008 Filipe Azevedo

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

/*! \file persdljoy.c
    \brief SDL joystick peripheral interface.
*/

#ifdef HAVE_LIBSDL
#if defined(__APPLE__) || defined(GEKKO)
 #ifdef HAVE_LIBSDL2
  #include <SDL2/SDL.h>
 #else
  #include <SDL/SDL.h>
 #endif
#else
 #include "SDL.h"
#endif

#include "debug.h"
#include "persdljoy.h"

#if 0
#include <windows.h>
//#include <cstdio>

#define LOG(fmt, ...) { \
    char buffer[256]; \
    snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
    OutputDebugStringA(buffer); \
}
#endif

// The code layout, the device field and the rule for moving a binding to
// another device. Kept out of this file so that part can be tested without
// SDL or a device attached.
#include "persdlcodes.h"

// A stick has to leave the centre by this much before it counts as pressed.
#define SDL_GC_AXIS_THRESHOLD 8000
#define SDL_BUTTON_PRESSED 1
#define SDL_BUTTON_RELEASED 0

int PERSDLJoyInit(void);
void PERSDLJoyDeInit(void);
int PERSDLJoyHandleEvents(void);

u32 PERSDLJoyScan(u32 flags);
void PERSDLJoyFlush(void);
void PERSDLKeyName(u32 key, char * name, int size);

PerInterface_struct PERSDLJoy = {
PERCORE_SDLJOY,
"SDL Joystick Interface",
PERSDLJoyInit,
PERSDLJoyDeInit,
PERSDLJoyHandleEvents,
PERSDLJoyScan,
1,
PERSDLJoyFlush,
PERSDLKeyName
};

typedef struct {
	// Non-NULL when SDL recognises this device as a game controller. The
	// raw-joystick scan is then skipped so a single physical pad cannot
	// report two different codes for the same button.
	SDL_GameController* mController;
	SDL_Joystick* mJoystick;
	s16* mScanStatus;
	Uint8* mHatStatus;
	int isWaiting;
} PERSDLJoystick;

unsigned int SDL_PERCORE_INITIALIZED = 0;
unsigned int SDL_PERCORE_JOYSTICKS_INITIALIZED = 0;
PERSDLJoystick* SDL_PERCORE_JOYSTICKS = 0;
static u32 PERSDLJoyEnumerationSignature(void);
// The enumeration this device table was built from, so a refresh can tell an
// actual hotplug from a poll that found nothing new.
static u32 SDL_PERCORE_SIGNATURE = 0;
unsigned int SDL_HAT_VALUES[] = { SDL_HAT_UP, SDL_HAT_RIGHT, SDL_HAT_LEFT, SDL_HAT_DOWN };
const unsigned int SDL_HAT_VALUES_NUM = sizeof(SDL_HAT_VALUES) / sizeof(SDL_HAT_VALUES[0]);

//////////////////////////////////////////////////////////////////////////////

int PERSDLJoyInit(void) {
	int i, j;

	// does not need init if already done
	if ( SDL_PERCORE_INITIALIZED )
	{
		return 0;
	}

#if defined (_MSC_VER) && SDL_VERSION_ATLEAST(2,0,0)
   SDL_SetMainReady();
#endif

	// This is the only peripheral core now, and the keyboard does not go
	// through it at all (the UI feeds key events straight into PerKeyDown),
	// so a machine without a usable joystick subsystem must still boot. Failing
	// here would make PerInit() fail, which fails the whole YabauseInit().
	if ( SDL_InitSubSystem( SDL_INIT_GAMECONTROLLER ) == -1 )
	{
		LOG( "SDL joystick subsystem unavailable (%s) - continuing without gamepads\n", SDL_GetError() );
		SDL_PERCORE_JOYSTICKS_INITIALIZED = 0;
		SDL_PERCORE_JOYSTICKS = NULL;
		SDL_PERCORE_INITIALIZED = 1;
		return 0;
	}

	// ignore joysticks event in sdl event loop
	SDL_JoystickEventState( SDL_IGNORE );

	// open joysticks
	SDL_PERCORE_JOYSTICKS_INITIALIZED = SDL_NumJoysticks();
	// A key code can only name four devices (see PERSDL_DEVICE_MASK). Opening
	// more would hand out indices that do not survive being encoded, and the
	// binding would silently end up on another pad, so stop here instead. A
	// wheel, pedal set and shifter each take a slot, so this is reachable.
	if ( SDL_PERCORE_JOYSTICKS_INITIALIZED > PERSDL_MAX_DEVICES )
	{
		LOG( "%d input devices attached, only the first %d can be used\n",
		     SDL_NumJoysticks(), PERSDL_MAX_DEVICES );
		SDL_PERCORE_JOYSTICKS_INITIALIZED = PERSDL_MAX_DEVICES;
	}
	// malloc(0) may hand back NULL or a pointer that must not be written to,
	// and with no gamepad attached that is the normal case now.
	SDL_PERCORE_JOYSTICKS = SDL_PERCORE_JOYSTICKS_INITIALIZED > 0
		? malloc(sizeof(PERSDLJoystick) * SDL_PERCORE_JOYSTICKS_INITIALIZED)
		: NULL;
	if ( SDL_PERCORE_JOYSTICKS == NULL )
	{
		SDL_PERCORE_JOYSTICKS_INITIALIZED = 0;
		SDL_PERCORE_INITIALIZED = 1;
		return 0;
	}
	SDL_PERCORE_JOYSTICKS->isWaiting = 0;
	for ( i = 0; i < SDL_PERCORE_JOYSTICKS_INITIALIZED; i++ )
	{
		// Prefer the game controller API: it maps this pad onto the standard
		// layout from SDL's device database, so no per-device configuration is
		// needed. Unrecognised devices keep the raw joystick path.
		SDL_GameController* gc = SDL_IsGameController( i ) ? SDL_GameControllerOpen( i ) : NULL;
		SDL_Joystick* joy = gc ? SDL_GameControllerGetJoystick( gc ) : SDL_JoystickOpen( i );

		SDL_JoystickUpdate();

		SDL_PERCORE_JOYSTICKS[ i ].mController = gc;
		SDL_PERCORE_JOYSTICKS[ i ].mJoystick = joy;
		// For a game controller the axes are indexed by SDL_CONTROLLER_AXIS_*,
		// which can exceed the raw axis count, so size for whichever is larger.
		{
			int axisCount = joy ? SDL_JoystickNumAxes( joy ) : 0;
			if ( gc && axisCount < SDL_CONTROLLER_AXIS_MAX ) axisCount = SDL_CONTROLLER_AXIS_MAX;
			SDL_PERCORE_JOYSTICKS[ i ].mScanStatus = axisCount > 0 ? malloc(sizeof(s16) * axisCount) : 0;
		}
		SDL_PERCORE_JOYSTICKS[ i ].mHatStatus = joy ? malloc(sizeof(Uint8) * SDL_JoystickNumHats( joy )) : 0;
		
		if (joy)
		{

			LOG("Opened Joystick %d\n", i);
			LOG("Name: %s\n", SDL_JoystickNameForIndex(i));
			LOG("Number of Axes: %d\n", SDL_JoystickNumAxes(joy));
			LOG("Number of Buttons: %d\n", SDL_JoystickNumButtons(joy));
			LOG("Number of Balls: %d\n", SDL_JoystickNumBalls(joy));

			for (int x = 0; x < 3; x++) {
				for (j = 0; j < SDL_JoystickNumAxes(joy); j++)
				{
					SDL_PERCORE_JOYSTICKS[i].mScanStatus[j] = SDL_JoystickGetAxis(joy, j);
					LOG("Joy %d AID %d pre %d \n", i, j, SDL_PERCORE_JOYSTICKS[i].mScanStatus[j]);
				}
			}

			for ( j = 0; j < SDL_JoystickNumHats( joy ); j++ )
			{
				SDL_PERCORE_JOYSTICKS[ i ].mHatStatus[ j ] = SDL_JoystickGetHat( joy, j );
			}
		}
	}
	
	// success
	SDL_PERCORE_INITIALIZED = 1;
	// Record what this table was built from, so the next refresh can tell
	// whether anything actually changed.
	SDL_PERCORE_SIGNATURE = PERSDLJoyEnumerationSignature();
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void PERSDLJoyDeInit(void) {
	// close joysticks
	if ( SDL_PERCORE_INITIALIZED == 1 )
	{
		int i;
		for ( i = 0; i < SDL_PERCORE_JOYSTICKS_INITIALIZED; i++ )
		{
#if SDL_VERSION_ATLEAST(2,0,0)
         if ( SDL_PERCORE_JOYSTICKS[ i ].mJoystick )
#else
			if ( SDL_JoystickOpened( i ) )
#endif
         {
            // SDL_GameControllerClose() closes the joystick it wraps; calling
            // SDL_JoystickClose() on it as well would be a double close.
            if ( SDL_PERCORE_JOYSTICKS[ i ].mController )
               SDL_GameControllerClose( SDL_PERCORE_JOYSTICKS[ i ].mController );
            else
               SDL_JoystickClose( SDL_PERCORE_JOYSTICKS[ i ].mJoystick );

            free( SDL_PERCORE_JOYSTICKS[ i ].mScanStatus );
            free( SDL_PERCORE_JOYSTICKS[ i ].mHatStatus );
         }
		}
		free( SDL_PERCORE_JOYSTICKS );
	}
	
	SDL_PERCORE_JOYSTICKS_INITIALIZED = 0;
	SDL_PERCORE_INITIALIZED = 0;
	
	// close sdl joysticks
	SDL_QuitSubSystem( SDL_INIT_GAMECONTROLLER );
}

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

// Built-in Saturn pad mapping for a recognised game controller. SDL normalises
// the device to the standard layout, so one fixed table works for every pad in
// its database - which is the whole point of using the game controller API.
//
// The Saturn face buttons are two rows of three:
//     X Y Z   <-  X  Y  LB
//     A B C   <-  A  B  RB
// L and R are the analog triggers and the d-pad is the d-pad. The left stick is
// not mapped: a Saturn digital pad has one input per direction, and one Saturn
// button takes one physical input.
// The device Scan() is currently restricted to. PERSDL_SCAN_ANY_DEVICE accepts
// every device; PERSDL_SCAN_NO_DEVICE accepts none, which is what a port driven
// by the keyboard wants.
static int SDL_PERCORE_SCAN_DEVICE = PERSDL_SCAN_ANY_DEVICE;

// Bumped every time the set of open devices changes, so callers can notice a
// hotplug without comparing device lists themselves.
static int SDL_PERCORE_GENERATION = 0;

// What SDL currently reports, boiled down to one number. The device count on
// its own is not enough: unplugging one pad and plugging a different one into
// the same slot leaves the count unchanged, and the table would keep polling
// the closed handle while reporting the new device's identity to the UI.
static u32 PERSDLJoyEnumerationSignature(void)
{
	const int count = SDL_NumJoysticks();
	u32 signature = (u32)count * 2654435761u;
	int i;

	for ( i = 0; i < count; i++ )
	{
		char text[64];
		const char * id = SDL_JoystickPathForIndex( i );
		int j;

		if ( id == NULL || id[0] == 0 )
		{
			SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID( i );
			SDL_JoystickGetGUIDString( guid, text, sizeof(text) );
			id = text;
		}

		for ( j = 0; id[j] != 0; j++ )
			signature = signature * 31u + (unsigned char)id[j];
	}

	return signature;
}

int PERSDLJoyRefreshDevices(void)
{
	u32 signature;

	if ( PERSDLJoyInit() != 0 )
		return SDL_PERCORE_GENERATION;

	// SDL learns about arrivals and removals while it updates the joysticks.
	SDL_JoystickUpdate();
	SDL_GameControllerUpdate();

	signature = PERSDLJoyEnumerationSignature();
	if ( signature == SDL_PERCORE_SIGNATURE )
		return SDL_PERCORE_GENERATION;
	SDL_PERCORE_SIGNATURE = signature;

	// The table is indexed by SDL device index, and those shift when a device
	// appears or disappears, so rebuild it rather than patching it up. Bindings
	// are resolved through the stored GUID afterwards, which is what puts a
	// reconnected pad back on the port it came from.
	PERSDLJoyDeInit();
	if ( PERSDLJoyInit() != 0 )
		return ++SDL_PERCORE_GENERATION;

	return ++SDL_PERCORE_GENERATION;
}

//////////////////////////////////////////////////////////////////////////////

int PERSDLJoyGetDeviceCount(void)
{
	if ( PERSDLJoyInit() != 0 )
		return 0;
	return SDL_PERCORE_JOYSTICKS_INITIALIZED;
}

//////////////////////////////////////////////////////////////////////////////

int PERSDLJoyGetDeviceInfo(int index, char * deviceId, int deviceIdSize,
                          char * name, int nameSize, int * isGameController)
{
	SDL_JoystickGUID id;
	const char * n;

	if ( PERSDLJoyInit() != 0 || SDL_PERCORE_JOYSTICKS == NULL )
		return 0;
	if ( index < 0 || index >= SDL_PERCORE_JOYSTICKS_INITIALIZED )
		return 0;
	if ( SDL_PERCORE_JOYSTICKS[ index ].mJoystick == NULL )
		return 0;

	if ( deviceId != NULL && deviceIdSize > 0 )
	{
		// Prefer the device path: the GUID is derived from vendor/product/version,
		// so two pads of the same model share it and could not be told apart, while
		// the path carries the port the device sits on. Fall back to the GUID on
		// platforms that report no path.
		//
		// Ask the open handle, not SDL_JoystickPathForIndex(): the index-based
		// call reads SDL's current enumeration, which can already describe a
		// different device than the one this slot holds. Reporting that identity
		// for a slot that still polls the old handle is how a port ends up
		// configured for a pad it never reads.
		const char * path = SDL_JoystickPath( SDL_PERCORE_JOYSTICKS[ index ].mJoystick );
		if ( path != NULL && path[0] != 0 )
		{
			strncpy( deviceId, path, deviceIdSize - 1 );
			deviceId[ deviceIdSize - 1 ] = 0;
		}
		else
		{
			id = SDL_JoystickGetGUID( SDL_PERCORE_JOYSTICKS[ index ].mJoystick );
			SDL_JoystickGetGUIDString( id, deviceId, deviceIdSize );
		}
	}
	if ( name != NULL && nameSize > 0 )
	{
		n = SDL_PERCORE_JOYSTICKS[ index ].mController
			? SDL_GameControllerName( SDL_PERCORE_JOYSTICKS[ index ].mController )
			: SDL_JoystickName( SDL_PERCORE_JOYSTICKS[ index ].mJoystick );
		if ( n == NULL ) n = "Unknown device";
		strncpy( name, n, nameSize - 1 );
		name[ nameSize - 1 ] = '\0';
	}
	if ( isGameController != NULL )
		*isGameController = SDL_PERCORE_JOYSTICKS[ index ].mController != NULL;

	return 1;
}

//////////////////////////////////////////////////////////////////////////////

int PERSDLJoyGetDeviceIndexForId(const char * deviceId)
{
	char current[512];
	int i;

	if ( deviceId == NULL || deviceId[0] == '\0' )
		return -1;
	if ( PERSDLJoyInit() != 0 || SDL_PERCORE_JOYSTICKS == NULL )
		return -1;

	for ( i = 0; i < SDL_PERCORE_JOYSTICKS_INITIALIZED; i++ )
	{
		if ( !PERSDLJoyGetDeviceInfo( i, current, sizeof(current), NULL, 0, NULL ) )
			continue;
		if ( SDL_strcasecmp( current, deviceId ) == 0 )
			return i;
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////////

void PERSDLJoySetScanDeviceIndex(int index)
{
	SDL_PERCORE_SCAN_DEVICE = index;
}

//////////////////////////////////////////////////////////////////////////////

u32 PERSDLJoyRetargetKey(u32 key, int deviceIndex)
{
	return PERSDLRetargetCode( key, deviceIndex );
}

//////////////////////////////////////////////////////////////////////////////

u32 PERSDLJoyOppositeAxisCode(u32 key)
{
	return PERSDLOppositeAxisCode( key );
}

//////////////////////////////////////////////////////////////////////////////

int PERSDLJoyGetDefaultPadMappingForIndex(int padIndex, u32 * keys, int keyCount);

int PERSDLJoyGetDefaultPadMapping(int order, u32 * keys, int keyCount)
{
	int seen = 0;
	int i;

	if ( order < 0 )
		return 0;
	if ( PERSDLJoyInit() != 0 || SDL_PERCORE_JOYSTICKS == NULL )
		return 0;

	// Pick the order-th device SDL recognises as a game controller. Racing
	// wheels, pedal sets and shifters occupy SDL device slots too, so the device
	// index and the gamepad number are usually not the same.
	for ( i = 0; i < SDL_PERCORE_JOYSTICKS_INITIALIZED; i++ )
	{
		if ( SDL_PERCORE_JOYSTICKS[ i ].mController == NULL )
			continue;
		if ( seen == order )
			return PERSDLJoyGetDefaultPadMappingForIndex( i, keys, keyCount );
		seen++;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

int PERSDLJoyGetDefaultPadMappingForIndex(int padIndex, u32 * keys, int keyCount)
{
	if ( keys == NULL || keyCount <= PERPAD_Z )
		return 0;
	if ( PERSDLJoyInit() != 0 || SDL_PERCORE_JOYSTICKS == NULL )
		return 0;
	if ( padIndex < 0 || padIndex >= SDL_PERCORE_JOYSTICKS_INITIALIZED )
		return 0;
	if ( SDL_PERCORE_JOYSTICKS[ padIndex ].mController == NULL )
		return 0;

#define PERSDL_GC_BTN(b) ( SDL_GC_BUTTON_VALUE | (padIndex << 18) | (b) )
#define PERSDL_GC_AXIS_POS(a) ( SDL_GC_AXIS_POS_VALUE | (padIndex << 18) | (a) )

	keys[ PERPAD_UP ]    = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_DPAD_UP );
	keys[ PERPAD_DOWN ]  = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_DPAD_DOWN );
	keys[ PERPAD_LEFT ]  = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_DPAD_LEFT );
	keys[ PERPAD_RIGHT ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_DPAD_RIGHT );

	keys[ PERPAD_A ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_A );
	keys[ PERPAD_B ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_B );
	keys[ PERPAD_C ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_RIGHTSHOULDER );
	keys[ PERPAD_X ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_X );
	keys[ PERPAD_Y ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_Y );
	keys[ PERPAD_Z ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_LEFTSHOULDER );

	keys[ PERPAD_LEFT_TRIGGER ]  = PERSDL_GC_AXIS_POS( SDL_CONTROLLER_AXIS_TRIGGERLEFT );
	keys[ PERPAD_RIGHT_TRIGGER ] = PERSDL_GC_AXIS_POS( SDL_CONTROLLER_AXIS_TRIGGERRIGHT );

	keys[ PERPAD_START ] = PERSDL_GC_BTN( SDL_CONTROLLER_BUTTON_START );

#undef PERSDL_GC_BTN
#undef PERSDL_GC_AXIS_POS

	return 1;
}

//////////////////////////////////////////////////////////////////////////////

// Report the state of one game controller. Buttons and axes are addressed by
// the normalised SDL_CONTROLLER_* indices, so the codes mean the same thing on
// every recognised pad.
static void PERSDLGameControllerHandleEvents( int joyId, SDL_GameController* gc )
{
	int i;

	for ( i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ )
	{
		if ( SDL_GameControllerGetButton( gc, (SDL_GameControllerButton)i ) )
			PerKeyDown( SDL_GC_BUTTON_VALUE | (joyId << 18) | i );
		else
			PerKeyUp( SDL_GC_BUTTON_VALUE | (joyId << 18) | i );
	}

	for ( i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++ )
	{
		Sint16 cur = SDL_GameControllerGetAxis( gc, (SDL_GameControllerAxis)i );

		PerAxisValue( SDL_GC_AXIS_ANALOG_VALUE | (joyId << 18) | i, (u8)(((int)cur + 32768) >> 8) );

		if ( cur < -SDL_GC_AXIS_THRESHOLD )
		{
			PerKeyUp( SDL_GC_AXIS_POS_VALUE | (joyId << 18) | i );
			PerKeyDown( SDL_GC_AXIS_NEG_VALUE | (joyId << 18) | i );
		}
		else if ( cur > SDL_GC_AXIS_THRESHOLD )
		{
			PerKeyUp( SDL_GC_AXIS_NEG_VALUE | (joyId << 18) | i );
			PerKeyDown( SDL_GC_AXIS_POS_VALUE | (joyId << 18) | i );
		}
		else
		{
			PerKeyUp( SDL_GC_AXIS_NEG_VALUE | (joyId << 18) | i );
			PerKeyUp( SDL_GC_AXIS_POS_VALUE | (joyId << 18) | i );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

// Same idea for the configuration dialog: report whatever the user just did on
// this controller so it can be bound to a Saturn button.
static u32 PERSDLGameControllerScan( int joyId, SDL_GameController* gc, u32 flags )
{
	int i;

	if ( flags & PERSF_BUTTON )
	{
		for ( i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ )
		{
			if ( SDL_GameControllerGetButton( gc, (SDL_GameControllerButton)i ) )
			{
				SDL_PERCORE_JOYSTICKS->isWaiting = 0;
				return SDL_GC_BUTTON_VALUE | (joyId << 18) | i;
			}
		}
	}

	for ( i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++ )
	{
		Sint16 cur = SDL_GameControllerGetAxis( gc, (SDL_GameControllerAxis)i );
		int diff = SDL_PERCORE_JOYSTICKS[ joyId ].mScanStatus[ i ] - cur;

		if ( diff > -SDL_GC_AXIS_THRESHOLD && diff < SDL_GC_AXIS_THRESHOLD )
			continue;

		SDL_PERCORE_JOYSTICKS->isWaiting = 0;
		if ( flags & PERSF_AXIS )
			return SDL_GC_AXIS_ANALOG_VALUE | (joyId << 18) | i;
		if ( flags & PERSF_HAT )
			return ( diff < 0 ? SDL_GC_AXIS_POS_VALUE : SDL_GC_AXIS_NEG_VALUE ) | (joyId << 18) | i;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

int PERSDLJoyHandleEvents(void) {
	int joyId;
	int i;
	int j;
	SDL_Joystick* joy;
	Sint16 cur;
	Uint8 buttonState;
	Uint8 newHatState;
	Uint8 oldHatState;
	int hatValue;

  if (PlayRecorder_getStatus() == 1) {
    YabauseExec();
    return 0;
  }
	
	// update joysticks states
	SDL_JoystickUpdate();
	SDL_GameControllerUpdate();
	
	// check each joysticks
	for ( joyId = 0; joyId < SDL_PERCORE_JOYSTICKS_INITIALIZED; joyId++ )
	{
		joy = SDL_PERCORE_JOYSTICKS[ joyId ].mJoystick;
		
		if ( !joy )
		{
			continue;
		}

		if ( SDL_PERCORE_JOYSTICKS[ joyId ].mController )
		{
			PERSDLGameControllerHandleEvents( joyId, SDL_PERCORE_JOYSTICKS[ joyId ].mController );
			continue;
		}
		
		// check axis
		for ( i = 0; i < SDL_JoystickNumAxes( joy ); i++ )
		{

			cur = SDL_JoystickGetAxis( joy, i );

			PerAxisValue((joyId << 18) | SDL_MEDIUM_AXIS_VALUE | i, (u8)(((int)cur+32768) >> 8));
			
			if ( cur < -1500 )
			{
				PerKeyUp((joyId << 18) | SDL_MIN_AXIS_VALUE | i);
				PerKeyDown((joyId << 18) | SDL_MAX_AXIS_VALUE | i);
			}
			else if ( cur > 1500 )
			{
				PerKeyUp((joyId << 18) | SDL_MAX_AXIS_VALUE | i);
				PerKeyDown((joyId << 18) | SDL_MIN_AXIS_VALUE | i);
			}
			else
			{
				PerKeyUp( (joyId << 18) | SDL_MIN_AXIS_VALUE | i );
				PerKeyUp( (joyId << 18) | SDL_MAX_AXIS_VALUE | i );
			}
		}
		
		// check buttons
		for ( i = 0; i < SDL_JoystickNumButtons( joy ); i++ )
		{
			buttonState = SDL_JoystickGetButton( joy, i );
			
			if ( buttonState == SDL_BUTTON_PRESSED )
			{
				PerKeyDown( (joyId << 18) | (i +1) );
			}
			else if ( buttonState == SDL_BUTTON_RELEASED )
			{
				PerKeyUp( (joyId << 18) | (i +1) );
			}
		}

		// check hats
		for ( i = 0; i < SDL_JoystickNumHats( joy ); i++ )
		{
			newHatState = SDL_JoystickGetHat( joy, i );
			oldHatState = SDL_PERCORE_JOYSTICKS[ joyId ].mHatStatus[ i ];

			for ( j = 0 ; j < SDL_HAT_VALUES_NUM; j++ )
			{
				hatValue = SDL_HAT_VALUES[ j ];
				if ( oldHatState & hatValue && ~newHatState & hatValue )
				{
					PerKeyUp( (joyId << 18) | SDL_HAT_VALUE | (hatValue << 4) | i );
				}
			}
			for ( j = 0 ; j < SDL_HAT_VALUES_NUM; j++ )
			{
				hatValue = SDL_HAT_VALUES[ j ];
				if ( ~oldHatState & hatValue && newHatState & hatValue )
				{
					PerKeyDown( (joyId << 18) | SDL_HAT_VALUE | (hatValue << 4) | i);
				}
			}

			SDL_PERCORE_JOYSTICKS[ joyId ].mHatStatus[ i ] = newHatState;
		}
	}
	
	// execute yabause
	if ( YabauseExec() != 0 )
	{
		return -1;
	}
	
	// return success
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

u32 PERSDLJoyScan( u32 flags ) {
	// init vars
	int joyId;
	int i;
	SDL_Joystick* joy;
	Sint16 cur;
	Uint8 hatState;

	
	// With no pad attached there is no array to walk, and dereferencing it
	// for the shared isWaiting flag would crash.
	if ( SDL_PERCORE_JOYSTICKS == NULL || SDL_PERCORE_JOYSTICKS_INITIALIZED == 0 )
		return 0;

	// update joysticks states
	SDL_JoystickUpdate();
	SDL_GameControllerUpdate();

	if (SDL_PERCORE_JOYSTICKS->isWaiting == 0) {

		for (joyId = 0; joyId < SDL_PERCORE_JOYSTICKS_INITIALIZED; joyId++)
		{
			joy = SDL_PERCORE_JOYSTICKS[joyId].mJoystick;

			if (!joy)
			{
				continue;
			}

			if ( SDL_PERCORE_JOYSTICKS[joyId].mController )
			{
				for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
					SDL_PERCORE_JOYSTICKS[joyId].mScanStatus[i] =
						SDL_GameControllerGetAxis( SDL_PERCORE_JOYSTICKS[joyId].mController, (SDL_GameControllerAxis)i );
				continue;
			}

			// check axis
			for (i = 0; i < SDL_JoystickNumAxes(joy); i++)
			{
				cur = SDL_JoystickGetAxis(joy, i);
        SDL_PERCORE_JOYSTICKS[joyId].mScanStatus[i] = cur;

				LOG("Joy %d AID %d pre %d cur %d\n", joyId, i, SDL_PERCORE_JOYSTICKS[joyId].mScanStatus[i], cur);

			}
		}

		SDL_PERCORE_JOYSTICKS->isWaiting = 1;
	}
	
	// check each joysticks
	for ( joyId = 0; joyId < SDL_PERCORE_JOYSTICKS_INITIALIZED; joyId++ )
	{
		joy = SDL_PERCORE_JOYSTICKS[ joyId ].mJoystick;
		
		if ( !joy )
		{
			continue;
		}

		if ( SDL_PERCORE_SCAN_DEVICE != PERSDL_SCAN_ANY_DEVICE && joyId != SDL_PERCORE_SCAN_DEVICE )
			continue;

		if ( SDL_PERCORE_JOYSTICKS[ joyId ].mController )
		{
			u32 gcKey = PERSDLGameControllerScan( joyId, SDL_PERCORE_JOYSTICKS[ joyId ].mController, flags );
			if ( gcKey != 0 )
				return gcKey;
			continue;
		}
	
		// check axis
		for ( i = 0; i < SDL_JoystickNumAxes( joy ); i++ )
		{
			cur = SDL_JoystickGetAxis( joy, i );


			//if (i == 0 && joyId == 0) {
			//	LOG("Joy %d AID %d pre %d cur %d\n", joyId, i, SDL_PERCORE_JOYSTICKS[joyId].mScanStatus[i], cur);
			//}


			if ( cur != SDL_PERCORE_JOYSTICKS[ joyId ].mScanStatus[ i ] )
			{


        int diff = SDL_PERCORE_JOYSTICKS[joyId].mScanStatus[i] - cur;
				if (diff < -1500) {

					SDL_PERCORE_JOYSTICKS->isWaiting = 0;
					LOG("update Joy %d AID %d pre %d cur %d\n", joyId, i, SDL_PERCORE_JOYSTICKS[joyId].mScanStatus[i], cur);

					if (flags & PERSF_AXIS)
						return (joyId << 18) | SDL_MEDIUM_AXIS_VALUE | i;
					if (flags & PERSF_HAT)
						return (joyId << 18) | SDL_MIN_AXIS_VALUE | i;
				}
				else if (diff > 1500) {

					SDL_PERCORE_JOYSTICKS->isWaiting = 0;
					LOG("update Joy %d AID %d pre %d cur %d\n", joyId, i, SDL_PERCORE_JOYSTICKS[joyId].mScanStatus[i], cur);

					if (flags & PERSF_AXIS)
						return (joyId << 18) | SDL_MEDIUM_AXIS_VALUE | i;
					if (flags & PERSF_HAT)
						return (joyId << 18) | SDL_MAX_AXIS_VALUE | i;
				}
#if 0
				if ( cur < -(SDL_MEDIUM_AXIS_VALUE>>1) )
				{
					if (flags & PERSF_AXIS)
						return (joyId << 18) | SDL_MEDIUM_AXIS_VALUE | i;
					if (flags & PERSF_HAT)
						return (joyId << 18) | SDL_MIN_AXIS_VALUE | i;
				}
				else if ( cur > (SDL_MEDIUM_AXIS_VALUE>>1) )
				{
					if (flags & PERSF_AXIS)
						return (joyId << 18) | SDL_MEDIUM_AXIS_VALUE | i;
					if (flags & PERSF_HAT)
						return (joyId << 18) | SDL_MAX_AXIS_VALUE | i;
				}
#endif
			}
		}

		if (flags & PERSF_BUTTON)
		{
			// check buttons
			for ( i = 0; i < SDL_JoystickNumButtons( joy ); i++ )
			{
				if ( SDL_JoystickGetButton( joy, i ) == SDL_BUTTON_PRESSED )
				{
					SDL_PERCORE_JOYSTICKS->isWaiting = 0;
					return (joyId << 18) | (i +1);
					break;
				}
			}
		}

		if (flags & PERSF_HAT)
		{
			// check hats
			for ( i = 0; i < SDL_JoystickNumHats( joy ); i++ )
			{
				hatState = SDL_JoystickGetHat( joy, i );
				
				switch (hatState)
				{
					case SDL_HAT_UP:
					case SDL_HAT_RIGHT:
					case SDL_HAT_DOWN:
					case SDL_HAT_LEFT:
						SDL_PERCORE_JOYSTICKS->isWaiting = 0;
						return (joyId << 18) | SDL_HAT_VALUE | (hatState << 4) | i;
						break;
					default:
						break;
				}
			}
		}
	}

	return 0;
}

void PERSDLJoyFlush(void) {
}

void PERSDLKeyName(u32 key, char * name, UNUSED int size)
{
	sprintf(name, "%x", (int)key);
}

#endif
