/************************************************************************************

Filename    :   VrApi_Android.h
Content     :   Android specific API.
Created     :   February 20, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Android_h
#define OVR_VrApi_Android_h

extern "C" {

//-----------------------------------------------------------------
// Query device status
//-----------------------------------------------------------------

// This will be called automatically on ovr_EnterVrMode(), but it can
// be called earlier so hybrid apps can tell when they need to enter VR mode.
// This can't be done in JNI_OnLoad because the ActivityObject is not known yet.
void		ovr_RegisterHmtReceivers( JNIEnv * jni, jobject activityObject );

// Once ovr_RegisterHmtReceivers() has been called, this can be used to detect
// when the device is docked/undocked.
bool		ovr_IsDeviceDocked();

typedef enum
{
	BATTERY_STATUS_CHARGING,
	BATTERY_STATUS_DISCHARGING,
	BATTERY_STATUS_FULL,
	BATTERY_STATUS_NOT_CHARGING,
	BATTERY_STATUS_UNKNOWN
} eBatteryStatus;

typedef struct
{
    int             level;          // in range [0,100]
    int             temperature;    // in tenths of a degree Centigrade
    eBatteryStatus  status;
} batteryState_t;

// While VrMode is active, we get battery state updates from Android.
// This can be used to pop up low battery and temperature warnings.
batteryState_t ovr_GetBatteryState();

// returns the current WIFI signal level
int ovr_GetWifiSignalLevel();

typedef enum
{
	WIFI_STATE_DISABLING,
	WIFI_STATE_DISABLED,
	WIFI_STATE_ENABLING,
	WIFI_STATE_ENABLED,
	WIFI_STATE_UNKNOWN
} eWifiState;

eWifiState ovr_GetWifiState();

// returns the current cellular signal level
int ovr_GetCellularSignalLevel();

typedef enum
{
	CELLULAR_STATE_IN_SERVICE,
	CELLULAR_STATE_OUT_OF_SERVICE,
	CELLULAR_STATE_EMERGENCY_ONLY,
	CELLULAR_STATE_POWER_OFF
} eCellularState;

eCellularState ovr_GetCellularState();

// Power level state.
bool ovr_GetPowerLevelStateThrottled();
bool ovr_GetPowerLevelStateMinimum();

// While in VrMode, we get headset plugged/unplugged updates from Android.
bool ovr_GetHeadsetPluggedState();

// While in VrMode, we get volume updates from Android.
int ovr_GetVolume();
double ovr_GetTimeSinceLastVolumeChange();

//-----------------------------------------------------------------
// Adjust clock levels
//-----------------------------------------------------------------

// Allow the CPU and GPU level to be adjusted while in VR mode.
// Must be called from the same thread that called ovr_EnterVrMode().
void		ovr_AdjustClockLevels( ovrMobile * ovr, int cpuLevel, int gpuLevel );

// Gather information related to CPU and GPU scheduling and clock frequencies.
// The returned string is valid until the next call.
const char * ovr_CreateSchedulingReport( ovrMobile * ovr );

//-----------------------------------------------------------------
// Audio focus
//-----------------------------------------------------------------

// Request/Relaese Audio Focus
// To avoid every music app playing at the same time, Android uses audio focus
// to moderate audio playback. Only apps that hold the audio focus should play audio.
void		ovr_RequestAudioFocus( ovrMobile * ovr );
void		ovr_ReleaseAudioFocus( ovrMobile * ovr );

//-----------------------------------------------------------------
// Activity start/exit
//-----------------------------------------------------------------

// Start up the platform UI for pass-through camera, reorient, exit, etc.
// The current app will be covered up by the platform UI, but will be
// returned to when it finishes.
#define PUI_CLASS_NAME		"com.oculus.systemactivities.PlatformActivity"
#define PUI_PACKAGE_NAME	"com.oculus.systemactivities"

// Platform UI command strings.
#define PUI_GLOBAL_MENU				"globalMenu"
#define PUI_GLOBAL_MENU_TUTORIAL	"globalMenuTutorial"
#define PUI_CONFIRM_QUIT			"confirmQuit"
#define PUI_THROTTLED1				"throttled1"	// Warn that Power Save Mode has been activated
#define PUI_THROTTLED2				"throttled2"	// Warn that Minimum Mode has been activated
#define PUI_HMT_UNMOUNT				"HMT_unmount"	// the HMT has been taken off the head
#define PUI_HMT_MOUNT				"HMT_mount"		// the HMT has been placed on the head
#define PUI_WARNING					"warning"		// the HMT has been placed on the head and a warning message shows

// version 0 is pre-json
// #define PLATFORM_UI_VERSION 1	// initial version
#define PLATFORM_UI_VERSION 2		// added "exitToHome" - apps built with current versions only respond to "returnToLauncher" if 
									// the Systems Activity that sent it is version 1 (meaning they'll never get an "exitToHome" 
									// from System Activities)

typedef enum
{
	EXIT_TYPE_NONE,				// This will not exit the activity at all -- normally used for starting the platform UI activity
	EXIT_TYPE_FINISH,			// This will finish the current activity.
	EXIT_TYPE_FINISH_AFFINITY,	// This will finish all activities on the stack.
	EXIT_TYPE_EXIT				// This calls ovr_Shutdown() and exit(0).
								// Must be called from the Java thread!
} eExitType;

void ovr_ExitActivity( ovrMobile * ovr, eExitType type );
void ovr_ReturnToHome( ovrMobile * ovr );

void ovr_SendIntent( ovrMobile * ovr, const char * actionName, const char * toPackageName, 
		const char * toClassName, const char * command, const char * uri, eExitType exitType );
void ovr_BroadcastSystemActivityEvent( ovrMobile * ovr, const char * actionName, const char * toPackageName, 
		const char * toClassName, const char * command, const char * jsonExtra, const char * uri );
void ovr_SendLaunchIntent( ovrMobile * ovr, const char * toPackageName, const char * command, 
		const char * uri, eExitType exitType );
bool ovr_StartSystemActivity( ovrMobile * ovr, const char * command, const char * jsonText );

//-----------------------------------------------------------------
// Activity start/exit
//-----------------------------------------------------------------

// This must match the value declared in ProximityReceiver.java / SystemActivityReceiver.java
#define SYSTEM_ACTIVITY_INTENT "com.oculus.system_activity"
#define	SYSTEM_ACTIVITY_EVENT_REORIENT "reorient"
#define SYSTEM_ACTIVITY_EVENT_RETURN_TO_LAUNCHER "returnToLauncher"
#define SYSTEM_ACTIVITY_EVENT_EXIT_TO_HOME "exitToHome"

// return values for ovr_GetNextPendingEvent
enum eVrApiEventStatus 
{
	VRAPI_EVENT_ERROR_INTERNAL = -2,		// queue isn't created, etc.
	VRAPI_EVENT_ERROR_INVALID_BUFFER = -1,	// the buffer passed in was invalid
	VRAPI_EVENT_NOT_PENDING = 0,			// no event is waiting
	VRAPI_EVENT_PENDING,					// an event is waiting
	VRAPI_EVENT_CONSUMED,					// an event was pending but was consumed internally
	VRAPI_EVENT_BUFFER_OVERFLOW,			// an event is being returned, but it could not fit into the buffer
	VRAPI_EVENT_INVALID_JSON				// there was an error parsing the JSON data
};

eVrApiEventStatus ovr_GetNextPendingEvent( char * buffer, unsigned int const bufferSize );

//-----------------------------------------------------------------
// Localization
//-----------------------------------------------------------------

// return the current language for the application
void ovr_GetCurrentLanguage( ovrMobile * ovr, char * language, int const maxLanguageLen );

// Note that the pointers returned are from data embedded in the executable and should not be freed.
bool ovr_FindEmbeddedImage( ovrMobile * ovr, const char * imageName, void * & buffer, int & bufferSize );

//-----------------------------------------------------------------
// Device-Local Preferences
//-----------------------------------------------------------------

void ovr_InitLocalPreferences( JNIEnv * jni, jobject activityObject );

}	// extern "C"

#endif	// OVR_VrApi_Android_h
