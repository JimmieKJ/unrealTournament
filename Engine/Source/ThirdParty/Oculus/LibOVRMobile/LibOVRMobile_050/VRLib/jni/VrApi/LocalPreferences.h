/************************************************************************************

Filename    :   LocalPreferences.h
Content     :   Interface for device-local preferences
Created     :   July 8, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_LocalPreferences_h
#define OVR_LocalPreferences_h

extern "C" {

// Local preferences are for storing platform-wide settings that are tied to
// a device instead of an application or user.

// Initially this is just a set of strings stored to /sdcard/.oculusprefs, but it
// may move to some other database.
//
// While it is here, you can easily set one or more values with adb like this:
// adb shell "echo imageserver 1 > /sdcard/.oculusprefs"
//
// The key / value pairs are just alternate tokens, with no newline required, so
// you can set multiple values at once:
//
// adb shell "echo imageserver 1 dev_powerLevelState 1 > /sdcard/.oculusprefs"

// If the image server is enabled, an external app can request continuous screenshots
// for usability testing.  This is a privacy / security concern if left always
// enabled.
#define LOCAL_PREF_IMAGE_SERVER         "imageServer"               // "0" or "1"

// Enable support for Oculus Remote Monitor to connect to the application.
#define LOCAL_PREF_ENABLE_CAPTURE       "dev_enableCapture"         // "0" or "1"

// Use the provided cpu and gpu levels for setting
// fixed clock levels.
#define LOCAL_PREF_DEV_CPU_LEVEL		"dev_cpuLevel"				// "0", "1", "2", or "3"
#define LOCAL_PREF_DEV_GPU_LEVEL		"dev_gpuLevel"				// "0", "1", "2", or "3"

#define LOCAL_PREF_DEV_SHOW_VIGNETTE	"dev_showVignette"			// "0" or "1"

#define LOCAL_PREF_DEV_DEBUG_OPTIONS	"dev_debugOptions"			// "0" or "1"

#define LOCAL_PREF_DEV_GPU_TIMINGS		"dev_gpuTimings"			// "0" or "1"

// Called on each resume, synchronously fetches the data.
void	ovr_UpdateLocalPreferences();

// Query the in-memory preferences for a (case insensitive) key / value pair.
// If the returned string is not defaultKeyValue, it will remain valid until the next ovr_UpdateLocalPreferences().
const char * ovr_GetLocalPreferenceValueForKey( const char * keyName, const char * defaultKeyValue );

// Updates the in-memory data and synchronously writes it to storage.
void	ovr_SetLocalPreferenceValueForKey( const char * keyName, const char * keyValue );

// Release the local preferences on shutdown.
void	ovr_ShutdownLocalPreferences();

void	ovr_SetAllowLocalPreferencesFile( const bool allow );

}	// extern "C"

#endif	// OVR_LocalPreferences_h
