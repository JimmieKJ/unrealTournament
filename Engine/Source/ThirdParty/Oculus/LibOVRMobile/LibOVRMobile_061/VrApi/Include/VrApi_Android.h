/************************************************************************************

Filename    :   VrApi_Android.h
Content     :   Android specific API.
Created     :   February 20, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Android_h
#define OVR_VrApi_Android_h

#include "VrApi_Config.h"
#include "VrApi_Types.h"

/*

Android/Mobile Specific API
===========================

These functions can be called any time from any thread in between ovr_Initialize()
and ovr_Shutdown().

*/

#if defined( __cplusplus )
extern "C" {
#endif

//-----------------------------------------------------------------
// System Properties
//-----------------------------------------------------------------

OVR_VRAPI_EXPORT int				ovr_GetSystemProperty( const ovrJava * java, const ovrSystemProperty prop );

//-----------------------------------------------------------------
// Device Status
//
// Note that there is currently no way to tell what the initial dock/mount state
// is when vrapi_Initialize() is first called. Only when the state changes after
// vrapi_Initialize() has been called we can tell what the real state is. To work
// around this problem, we assume the device is docked and the headset is mounted
// when we enter VR mode, and no dock/mount state changes have been recorded up
// to that point. Under normal circumstances this is usually correct. This is
// incorrect when in developer mode, but in that case you want the device to
// appear as docked and the headset as mounted anyway.
//-----------------------------------------------------------------

// Can be used to detect when the device is docked/undocked.
OVR_VRAPI_EXPORT bool				ovr_DeviceIsDocked();

// Can be used to detect when the headset is mounted/unmounted.
OVR_VRAPI_EXPORT bool				ovr_HeadsetIsMounted();

// Can be used to detect whether or not headphones are plugged in.
OVR_VRAPI_EXPORT bool				ovr_HeadPhonesArePluggedIn();

// This can be used to pop up low battery and temperature warnings.
OVR_VRAPI_EXPORT ovrBatteryState	ovr_GetBatteryState();
OVR_VRAPI_EXPORT int				ovr_GetBatteryLevel();
OVR_VRAPI_EXPORT int				ovr_GetBatteryTemperature();

// Returns the current WIFI state and signal level.
OVR_VRAPI_EXPORT ovrWifiState		ovr_GetWifiState();
OVR_VRAPI_EXPORT int				ovr_GetWifiSignalLevel();

// Returns the current cellular state and signal level.
OVR_VRAPI_EXPORT ovrCellularState	ovr_GetCellularState();
OVR_VRAPI_EXPORT int				ovr_GetCellularSignalLevel();

// Returns the current power level state.
OVR_VRAPI_EXPORT bool				ovr_GetPowerLevelStateThrottled();
OVR_VRAPI_EXPORT bool				ovr_GetPowerLevelStateMinimum();

// Returns the current volume.
OVR_VRAPI_EXPORT int				ovr_GetSoundVolume();
OVR_VRAPI_EXPORT double				ovr_GetSoundVolumeTimeSinceLastChangeInSeconds();

//-----------------------------------------------------------------
// Device Settings
//-----------------------------------------------------------------

OVR_VRAPI_EXPORT int				ovr_GetSystemBrightness( const ovrJava * java );
OVR_VRAPI_EXPORT void				ovr_SetSystemBrightness( const ovrJava * java, int const brightness );

OVR_VRAPI_EXPORT bool				ovr_GetComfortMode( const ovrJava * java );
OVR_VRAPI_EXPORT void				ovr_SetComfortMode( const ovrJava * java, bool const enable );

OVR_VRAPI_EXPORT bool				ovr_GetDoNotDisturbMode( const ovrJava * java );
OVR_VRAPI_EXPORT void				ovr_SetDoNotDisturbMode( const ovrJava * java, bool const enable );

//-----------------------------------------------------------------
// Activity intents/termination
//-----------------------------------------------------------------

// Sends an intent to another activity.
OVR_VRAPI_EXPORT void ovr_SendIntent( const ovrJava * java,
									const char * actionName, const char * toPackageName,
									const char * toClassName, const char * command, const char * uri );

// Launches another activity with an intent.
OVR_VRAPI_EXPORT void ovr_SendLaunchIntent( const ovrJava * java,
									const char * toPackageName,
									const char * command, const char * uri );

// Triggers the calling activity to be shut down.
OVR_VRAPI_EXPORT void ovr_FinishActivity( const ovrJava * java, ovrFinishType type );

//-----------------------------------------------------------------
// Home / System Activities
//-----------------------------------------------------------------

// Pressing the back button twice within BACK_BUTTON_DOUBLE_TAP_TIME_IN_SECONDS
// is a double tap. A single press and release of the back button in less than
// BACK_BUTTON_SHORT_PRESS_TIME_IN_SECONDS is a short press. Holding the back
// button for longer than BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS is considered
// a long press. Anything in between a short press and a long press is considered
// a cancellation of a long press.
#define BACK_BUTTON_DOUBLE_TAP_TIME_IN_SECONDS		0.25f
#define BACK_BUTTON_SHORT_PRESS_TIME_IN_SECONDS		0.25f
#define BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS		0.75f

// Oculus Home.
#define HOME_CLASS_NAME				"com.oculus.home.HomeActivity"
#define HOME_PACKAGE_NAME			"com.oculus.home"

// Return to Home.
OVR_VRAPI_EXPORT void ovr_ReturnToHome( const ovrJava * java );

// Start up the platform UI for pass-through camera, reorient, exit, etc.
// The current app will be covered up by the platform UI, but will be
// returned to when it finishes.
#define PUI_CLASS_NAME				"com.oculus.systemactivities.PlatformActivity"
#define PUI_PACKAGE_NAME			"com.oculus.systemactivities"

// Version 0 is pre-JSON.
// Version 1 is initial version.
// Version 2 added "exitToHome" event.
//		Apps built with current versions only respond to "returnToLauncher" if 
//		the Systems Activity that sent it is version 1 (meaning they'll never
//		get an "exitToHome" from System Activities).
#define PLATFORM_UI_VERSION			2

// Sends a System Activity intent containing the command and extra JSON text.
OVR_VRAPI_EXPORT bool ovr_StartSystemActivity( const ovrJava * java, const char * command, const char * jsonText );

// Sends a System Activity intent containing the command and extra JSON text.
// This should only be used in the fatal error case.
OVR_VRAPI_EXPORT void ovr_DisplaySystemActivityError( const ovrJava * java, const ovrError error,
														const char * fileName, const char * messageFormat, ... );

// Fills outBuffer with a JSON text object with the required versioning info, the passed command, 
// and embedded extraJsonText.
OVR_VRAPI_EXPORT bool ovr_CreateSystemActivityIntent( const char * command, const char * extraJsonText, 
		char * outBuffer, unsigned long long const outBufferSize, unsigned long long * outRequiredBufferSize );

//-----------------------------------------------------------------
// System Activity Events
//-----------------------------------------------------------------

// This must match the value declared in ProximityReceiver.java / SystemActivityReceiver.java
#define SYSTEM_ACTIVITY_INTENT						"com.oculus.system_activity"
#define	SYSTEM_ACTIVITY_EVENT_REORIENT				"reorient"
#define SYSTEM_ACTIVITY_EVENT_RETURN_TO_LAUNCHER	"returnToLauncher"
#define SYSTEM_ACTIVITY_EVENT_EXIT_TO_HOME			"exitToHome"

// Used by system activities to send an event back to the activity that launched it.
OVR_VRAPI_EXPORT void ovr_BroadcastSystemActivityEvent( const ovrJava * java,
							const char * actionName, const char * toPackageName,
							const char * toClassName, const char * command, const char * jsonExtra, const char * uri );

// Return values for ovr_GetNextPendingEvent.
typedef enum
{
	VRAPI_EVENT_ERROR_INTERNAL = -2,		// queue isn't created, etc.
	VRAPI_EVENT_ERROR_INVALID_BUFFER = -1,	// the buffer passed in was invalid
	VRAPI_EVENT_NOT_PENDING = 0,			// no event is waiting
	VRAPI_EVENT_PENDING,					// an event is waiting
	VRAPI_EVENT_CONSUMED,					// an event was pending but was consumed internally
	VRAPI_EVENT_BUFFER_OVERFLOW,			// an event is being returned, but it could not fit into the buffer
	VRAPI_EVENT_INVALID_JSON				// there was an error parsing the JSON data
} eVrApiEventStatus;

// Returns the next system activity event.
OVR_VRAPI_EXPORT eVrApiEventStatus ovr_GetNextPendingEvent( char * buffer, unsigned int const bufferSize );

//-----------------------------------------------------------------
// Embedded images. Only here to allow easy sharing between engines.
//-----------------------------------------------------------------

// Note that the pointers returned are from data embedded in the executable and should not be freed.
OVR_VRAPI_EXPORT bool ovr_FindEmbeddedImage( const char * imageName, void ** buffer, int * bufferSize );

#if defined( __cplusplus )
}	// extern "C"
#endif

#endif	// OVR_VrApi_Android_h
