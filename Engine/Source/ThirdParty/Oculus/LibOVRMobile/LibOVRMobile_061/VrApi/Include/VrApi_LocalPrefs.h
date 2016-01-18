 /************************************************************************************

Filename    :   VrApi_LocalPrefs.h
Content     :   Interface for device-local preferences
Created     :   July 8, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_LocalPrefs_h
#define OVR_VrApi_LocalPrefs_h

#include "VrApi_Config.h"
#include "VrApi_Types.h"

#if defined( __cplusplus )
extern "C" {
#endif

// Local preferences are for storing platform-wide settings that are tied to
// a device instead of an application or user.

// Initially this is just a set of strings stored to /sdcard/.oculusprefs, but it
// may move to some other database.
//
// While it is here, you can easily set one or more values with adb like this:
// adb shell "echo dev_enableCapture 1 > /sdcard/.oculusprefs"
//
// The key / value pairs are just alternate tokens, with no newline required, so
// you can set multiple values at once:
//
// adb shell "echo dev_enableCapture 1 dev_powerLevelState 1 > /sdcard/.oculusprefs"

// Enable support for Oculus Remote Monitor to connect to the application.
#define LOCAL_PREF_VRAPI_ENABLE_CAPTURE					"dev_enableCapture"				// "0" or "1"

// Use the provided cpu and gpu levels for setting
// fixed clock levels.
#define LOCAL_PREF_VRAPI_CPU_LEVEL						"dev_cpuLevel"					// "0", "1", "2", or "3"
#define LOCAL_PREF_VRAPI_GPU_LEVEL						"dev_gpuLevel"					// "0", "1", "2", or "3"

// Shipping applications will always want this on, but if you want to draw
// directly to the screen for debug tasks, you can run synchronously so the
// init thread is still current on the window.
#define LOCAL_PREF_VRAPI_ASYNC_TIMEWARP					"dev_asyncTimewarp"				// "0" or "1"

// Optionally force a specific MinimumVsyncs.
#define LOCAL_PREF_VRAPI_MINIMUM_VSYNCS					"dev_mimumumVsyncs"				// "0", "1", "2", "3"

// For video capture or testing on reference platforms without direct frontbuffer
// rendering, direct frontbuffer can be forced off.
#define LOCAL_PREF_VRAPI_FRONTBUFFER					"dev_frontbuffer"				// "0" or "1"

// Optional distortion file to override built-in distortion.
#define LOCAL_PREF_VRAPI_DISTORTION_FILE_NAME			"dev_distortionFileName"		// default = ""

// Experimental feature that clips the distortion mesh to reduce GPU fill for timewarp+distortion
#define LOCAL_PREF_VRAPI_CLIP_DISTORTION_MESH			"dev_clipDistortionMesh"		// "0" = off, "1" = clip to optics, "2" = clip to framebuffer

// Time in seconds to start drawing before each slice.
// Clamped at 0.014 high and 0.002 low, but the very low
// values will usually result in screen tearing.
#define LOCAL_PREF_VRAPI_SLICE_PRE_SCHEDULE_SECONDS		"dev_slicePreScheduleSeconds"	// "0.002" - "0.014", default = "0.014"

// Controls the collection and display of timing data.
#define LOCAL_PREF_VRAPI_TIMEWARP_DEBUG_GRAPH_MODE		"dev_timeWarpDebugGraphMode"	// "0" = off, "1" = running, "2" = frozen
#define LOCAL_PREF_VRAPI_TIMEWARP_DEBUG_GRAPH_VALUE		"dev_timeWarpDebugGraphValue"	// "0" = draw, "1" = latency

#define LOCAL_PREF_VRAPI_GPU_TIMINGS					"dev_gpuTimings"				// "0" = off, "1" = glBeginQuery/glEndQuery, "2" = glQueryCounter

#define LOCAL_PREF_APP_SHOW_VIGNETTE					"dev_showVignette"				// "0" or "1"

#define LOCAL_PREF_APP_DEBUG_OPTIONS					"dev_debugOptions"				// "0" or "1"

// Temporary preference to verify ovrFrameParms are consistent.
// Should be removed once the WarpProgram member is gone.
#define LOCAL_PREF_VRAPI_VERIFY_FRAME_PARMS				"dev_verifyFrameParms"			// "0" or "1" (default)

// When "1", render each layer as a separate blended draw call.
#define LOCAL_PREF_VRAPI_TIMEWARP_LAYERS_MULTIPASS		"dev_timewarpLayersMultipass"	// "0" (default) or "1"

// Query the in-memory preferences for a (case insensitive) key / value pair.
// If the returned string is not defaultKeyValue, it will remain valid until the next ovr_UpdateLocalPreferences().
OVR_VRAPI_EXPORT const char * ovr_GetLocalPreferenceValueForKey( const char * keyName, const char * defaultKeyValue );

// Updates the in-memory data and synchronously writes it to storage.
OVR_VRAPI_EXPORT void ovr_SetLocalPreferenceValueForKey( const char * keyName, const char * keyValue );

#if defined( __cplusplus )
}	// extern "C"
#endif

#endif	// OVR_VrApi_LocalPrefs_h
