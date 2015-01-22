/************************************************************************************

Filename    :   VrApi.h
Content     :   Minimum necessary API for mobile VR
Created     :   June 25, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_VrApi_h
#define OVR_VrApi_h

#include <jni.h>
#include "OVR_CAPI.h"
#include "TimeWarpParms.h"

extern "C" {

struct ovrModeParms
{
	// Shipping applications will almost always want this on,
	// but if you want to draw directly to the screen for
	// debug tasks, you can run synchronously so the init
	// thread is still current on the window.
	bool	AsynchronousTimeWarp;

	// If true, warn and allow the app to continue at 30fps when 
	// throttling occurs.
	// If false, display the level 2 error message which requires
	// the user to undock.
	bool	AllowPowerSave;

	// Optional distortion file to override built in distortion
	const char * DistortionFileName;

	// Set true to enable the image server, which allows a
	// remote device to view frames from the current VR session.
	bool	EnableImageServer;

	// This thread, in addition to the calling one, will
	// get SCHED_FIFO.
	int		GameThreadTid;

	// These are fixed clock levels.
	int		CpuLevel;
	int		GpuLevel;

	// The java Activity object is needed to get the windowManager,
	// packageName, systemService, etc.
	jobject ActivityObject;
};

struct ovrHmdInfo
{
	// Currently returns a conservative 1024
	int		SuggestedEyeResolution;

	// This is a product of the lens distortion and the screen size,
	// but there is no truly correct answer.
	//
	// There is a tradeoff in resolution and coverage.
	// Too small of an fov will leave unrendered pixels visible, but too
	// large wastes resolution or fill rate.  It is unreasonable to
	// increase it until the corners are completely covered, but we do
	// want most of the outside edges completely covered.
	//
	// Applications might choose to render a larger fov when angular
	// acceleration is high to reduce black pull in at the edges by
	// TimeWarp.
	//
	// Currently 90.0.
	float	SuggestedEyeFov;
};

// This must be called by a function called directly from a java thread,
// preferably at JNI_OnLoad().  It will fail if called from a pthread created
// in native code due to the class-lookup issue:
//
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
//
// This calls ovr_Initialize() internally.
void		ovr_OnLoad( JavaVM * JavaVm_ );

// VR context
// To support the meta-UI, which involves two different Android activities
// running in the same process, we need to maintain two separate contexts
// for a lot of the video related systems.
struct ovrMobile;

// Starts up TimeWarp, vsync tracking, sensor reading, clock locking, thread scheduling,
// and sets video options.  The calling thread will be given SCHED_FIFO.
// Should be called when the app is both resumed and has a valid window surface.
// The application must have their preferred OpenGL ES context current so the correct
// version and config can be matched for the background TimeWarp thread.
// On return, the context will be current on an invisible pbuffer, because TimeWarp
// will own the window.
ovrMobile *	ovr_EnterVrMode( ovrModeParms parms, ovrHmdInfo * returnedHmdInfo );

// Shut everything down for window destruction.
//
// The ovrMobile object is freed by this function.
//
// Calling from a thread other than the one that called ovr_EnterVrMode will be
// a fatal error.
void        ovr_LeaveVrMode( ovrMobile * ovr );

// Accepts a new pos + texture set that will be used for future warps.
// The parms are copied, and are not referenced after the function returns.
//
// The application GL context that rendered the eye images must be current,
// but drawing does not need to be completed.  A sync object will be added
// to the current context so the background thread can know when it is ready to use.
//
// This will block until the textures from the previous
// WarpSwap have completed rendering, to allow one frame of overlap for maximum
// GPU utilization, but prevent multiple frames from piling up variable latency.
//
// This will block until at least one vsync has passed since the last
// call to WarpSwap to prevent applications with simple scenes from
// generating completely wasted frames.
//
// IMPORTANT: you must triple buffer the textures that you pass to WarpSwap
// to avoid flickering and performance problems.
//
// Calling from a thread other than the one that called ovr_EnterVrMode will be
// a fatal error.
void		ovr_WarpSwap( ovrMobile * ovr, const TimeWarpParms * parms );

// Warp swap pushing black to the screen.
void		ovr_WarpSwapBlack( ovrMobile * ovr );

// Warp swap showing a spinning loading icon.
void		ovr_WarpSwapLoadingIcon( ovrMobile * ovr );

// Called when the window surface is destroyed to make sure warp swap
// is never called without a valid window surface.
void		ovr_SurfaceDestroyed( ovrMobile * ovr );

// Handle power level state changes and Hmd events, such as
// mount/unmount dock/undock
void		ovr_HandleDeviceStateChanges( ovrMobile * ovr );

// Start up the platform UI for pass-through camera, reorient, exit, etc.
// The current app will be covered up by the platform UI, but will be
// returned to when it finishes.
//
// You must have the following in the AndroidManifest.xml file for this to work:
//<activity
//  android:name="com.oculusvr.vrlib.PlatformActivity"
//	android:theme="@android:style/Theme.Black.NoTitleBar.Fullscreen"
//	android:launchMode="singleTask"
//	android:screenOrientation="landscape"
//	android:configChanges="orientation|keyboardHidden|keyboard">
//</activity>

// Platform UI class name.
#define PUI_CLASS_NAME "com.oculusvr.vrlib.PlatformActivity"

// Platform UI command strings.
#define PUI_GLOBAL_MENU "globalMenu"
#define PUI_GLOBAL_MENU_TUTORIAL "globalMenuTutorial"
#define PUI_CONFIRM_QUIT "confirmQuit"
#define PUI_THROTTLED1	"throttled1"	// Warn that Power Save Mode has been activated
#define PUI_THROTTLED2	"throttled2"	// Warn that Minimum Mode has been activated
#define PUI_HMT_UNMOUNT	"HMT_unmount"	// the HMT has been taken off the head
#define PUI_HMT_MOUNT	"HMT_mount"		// the HMT has been placed on the head
#define PUI_WARNING		"warning"		// the HMT has been placed on the head and a warning message shows

void		ovr_StartPackageActivity( ovrMobile * ovr, const char * className, const char * commandString );

enum eExitType
{
	EXIT_TYPE_FINISH,			// This will finish the current activity.
	EXIT_TYPE_FINISH_AFFINITY,	// This will finish all activities on the stack.
	EXIT_TYPE_EXIT				// This calls ovr_Shutdown() and exit(0).
								// Must be called from the Java thread!
};
void		ovr_ExitActivity( ovrMobile * ovr, eExitType type );

// Handle Hmd Events such as mount/unmount, dock/undock
enum eHMTMountState
{
	HMT_MOUNT_NONE,			// nothing to do
	HMT_MOUNT_MOUNTED,		// the HMT has been placed on the head
	HMT_MOUNT_UNMOUNTED		// the HMT has been removed from the head
};

struct HMTMountState_t
{
	HMTMountState_t() : 
		MountState( HMT_MOUNT_NONE ) 
	{ 
	}

	explicit HMTMountState_t( eHMTMountState const mountState ) :
		MountState( mountState )
	{
	}

	eHMTMountState	MountState;
};

// Call to query the current mount state.
HMTMountState_t ovr_GetExternalHMTMountState();
// Call to reset the current mount state once it's been acted upon.
void ovr_ResetExternalHMTMountState();
// Call to allow the application to handle or not handle HMT mounting.
void ovr_SetExternalHMTMountHandling( bool const handleExternally );

//-----------------------------------------------------------------
// Functions present in the Oculus CAPI
//
// You should not use any other CAPI functions.
//-----------------------------------------------------------------

// There is a single ovrHmd that is initialized at ovr_onLoad() time.
// This is necessary to handle multiple VR activities in a single address
// space, because the second one is started before the first one shuts
// down, so they would not be able to acquire and release the hmd without
// it being double-owned during the hand off.
extern	ovrHmd	OvrHmd;

/*

// Returns global, absolute high-resolution time in seconds. This is the same
// value as used in sensor messages.
double      ovr_GetTimeInSeconds();

// Returns sensor state reading based on the specified absolute system time.
// Pass absTime value of 0.0 to request the most recent sensor reading; in this case
// both PredictedPose and SamplePose will have the same value.
// ovrHmd_GetEyePredictedSensorState relies on this internally.
// This may also be used for more refined timing of FrontBuffer rendering logic, etc.
//
// allowSensorCreate is ignored on mobile.
ovrSensorState ovrHmd_GetSensorState( ovrHmd hmd, double absTime, bool allowSensorCreate );

// Recenters the orientation on the yaw axis.
void        ovrHmd_RecenterYaw( ovrHmd hmd );

*/

//-----------------------------------------------------------------
// Miscellaneous functions
//-----------------------------------------------------------------

enum eBatteryStatus
{
	BATTERY_STATUS_CHARGING,
	BATTERY_STATUS_DISCHARGING,
	BATTERY_STATUS_FULL,
	BATTERY_STATUS_NOT_CHARGING,
	BATTERY_STATUS_UNKNOWN
};

struct batteryState_t
{
    int             level;          // in range [0,100]
    int             temperature;    // in tenths of a degree Centigrade
    eBatteryStatus  status;
};

// While VrMode is active, we get battery state updates from Android.
// This can be used to pop up low battery and temperature warnings.
batteryState_t ovr_GetBatteryState();

// While VrMode is active, we get volume updates from Android.
int ovr_GetVolume();
double ovr_GetTimeSinceLastVolumeChange();

// returns the current WIFI signal level
int ovr_GetWifiSignalLevel();

enum eWifiState
{
	WIFI_STATE_DISABLED,
	WIFI_STATE_DISABLING,
	WIFI_STATE_ENABLED,
	WIFI_STATE_ENABLING,
	WIFI_STATE_UNKNOWN
};
eWifiState ovr_GetWifiState();

// returns the current cellular signal level
int ovr_GetCellularSignalLevel();

enum eCellularState
{
	CELLULAR_STATE_IN_SERVICE,
	CELLULAR_STATE_OUT_OF_SERVICE,
	CELLULAR_STATE_EMERGENCY_ONLY,
	CELLULAR_STATE_POWER_OFF
};
eCellularState ovr_GetCellularState();

// While VrMode is active, we get headset plugged/unplugged updates
// from Android.
bool ovr_GetHeadsetPluggedState();

bool ovr_GetPowerLevelStateThrottled();
bool ovr_GetPowerLevelStateMinimum();

void ovr_ResetClockLocks( ovrMobile * ovr );

// returns the value of a specific build string.  Valid names are:
enum eBuildString
{
	BUILDSTR_BRAND,
	BUILDSTR_DEVICE,
	BUILDSTR_DISPLAY,
	BUILDSTR_FINGERPRINT,
	BUILDSTR_HARDWARE,
	BUILDSTR_HOST,
	BUILDSTR_ID,
	BUILDSTR_MODEL,
	BUILDSTR_PRODUCT,
	BUILDSTR_SERIAL,
	BUILDSTR_TAGS,
	BUILDSTR_TYPE,
	BUILDSTR_MAX
};

char const * ovr_GetBuildString( eBuildString const id );

}	// extern "C"

#endif	// OVR_VrApi_h
