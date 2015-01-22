/************************************************************************************

Filename    :   VrApi.h
Content     :   Primary C level interface necessary for VR, App builds on this
Created     :   July, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrApi.h"

#include <unistd.h>						// gettid, usleep, etc
#include <jni.h>

#include "Log.h"
#include "GlUtils.h"
#include "GlTexture.h"
#include "DirectRender.h"
#include "VrCommon.h"
#include "HmdInfo.h"
#include "TimeWarp.h"
#include "JniUtils.h"
#include "VrApi_local.h"
#include "Vsync.h"
#include "Kernel/OVR_String.h"			// for ReadFreq()
#include "OVR_JSON.h"
#include "OVRVersion.h"					// for vrlib build version
#include "../SearchPaths.h"
#include "LocalPreferences.h"			// for testing via local prefs

/*
 * This interacts with the VrLib java class to deal with Android platform issues.
 */

static JavaVM	* 				JavaVm;
static pid_t					OnLoadTid;

// This needs to be looked up by a thread called directly from java,
// not a native pthread.
static jclass	VrLibClass = NULL;
static jclass	VrActivityClass = NULL;
static jclass	PlatformActivityClass = NULL;
static jclass	ProximityReceiverClass = NULL;
static jclass	DockReceiverClass = NULL;

static jmethodID getPowerLevelStateID = NULL;
static jmethodID setActivityWindowFullscreenID = NULL;
static jmethodID notifyMountHandledID = NULL;

static  OVR::NativeBuildStrings * BuildStrings;

static bool alwaysSkipPowerLevelCheck = false;
static double startPowerLevelCheckTime = -1.0;
static double powerLevelCheckLastReportTime = 0.0;

static bool hmtIsMounted = false;

// Register the HMT receivers once, and do
// not unregister in Pause(). We may miss
// important mount or dock events while
// the receiver is unregistered.
static bool registerHMTReceivers = false;

static int BuildVersionSDK = 19;	// default supported version for vrlib is KitKat 19

ovrHmd	OvrHmd;

enum eHMTDockState {
	HMT_DOCK_NONE,			// nothing to do
	HMT_DOCK_DOCKED,		// the device is inserted into the HMT
	HMT_DOCK_UNDOCKED		// the device has been removed from the HMT
};

struct HMTDockState_t
{
	HMTDockState_t() : 
		DockState( HMT_DOCK_NONE )
	{ 
	}

	explicit HMTDockState_t( eHMTDockState const dockState ) :
		DockState( dockState )
	{
	}

	eHMTDockState	DockState;
};

template< typename T, T _initValue_ >
class LocklessVar
{
public:
	LocklessVar() : Value( _initValue_ ) { }
	LocklessVar( T const v ) : Value( v ) { }

	T	Value;
};

class LocklessDouble
{
public:
	LocklessDouble( const double v ) : Value( v ) { };
	LocklessDouble() : Value( -1 ) { };

	double Value;
};

typedef LocklessVar< int, -1> 						volume_t;
OVR::LocklessUpdater< volume_t >					CurrentVolume;
OVR::LocklessUpdater< LocklessDouble >				TimeSinceLastVolumeChange;
OVR::LocklessUpdater< batteryState_t >				BatteryState;
OVR::LocklessUpdater< bool >						HeadsetPluggedState;
OVR::LocklessUpdater< bool >						PowerLevelStateThrottled;
OVR::LocklessUpdater< bool >						PowerLevelStateMinimum;
OVR::LocklessUpdater< HMTMountState_t >				HMTMountState;
OVR::LocklessUpdater< HMTDockState_t >				HMTDockState;

typedef LocklessVar< int, -1> wifiSignalLevel_t;
OVR::LocklessUpdater< wifiSignalLevel_t >			WifiSignalLevel;
typedef LocklessVar< eWifiState, WIFI_STATE_UNKNOWN > wifiState_t;
OVR::LocklessUpdater< wifiState_t >					WifiState;

typedef LocklessVar< int, -1> cellularSignalLevel_t;
OVR::LocklessUpdater< cellularSignalLevel_t >		CellularSignalLevel;
typedef LocklessVar< eCellularState, CELLULAR_STATE_IN_SERVICE > cellularState_t;
OVR::LocklessUpdater< cellularState_t >				CellularState;

typedef LocklessVar< bool, false > externalMount_t;
OVR::LocklessUpdater< externalMount_t >	HandleMountExternally;
OVR::LocklessUpdater< HMTMountState_t >	ExternalHMTMountState;

extern "C"
{
void Java_com_oculusvr_vrlib_VrLib_nativeVolumeEvent(JNIEnv *jni, jclass clazz, jint volume)
{
    LOG( "nativeVolumeEvent(%i)", volume );

    CurrentVolume.SetState( volume );
    double now = ovr_GetTimeInSeconds();

    if ( TimeSinceLastVolumeChange.GetState().Value == -1 )
    {
    	// ignore first volume change
    	now = now - 1000;
    }
    TimeSinceLastVolumeChange.SetState( now );
	double value = TimeSinceLastVolumeChange.GetState().Value;

}

void Java_com_oculusvr_vrlib_VrLib_nativeBatteryEvent(JNIEnv *jni, jclass clazz, jint status, jint level, jint temperature) 
{
    LOG( "nativeBatteryEvent(%i, %i, %i)", status, level, temperature );

    batteryState_t state;
    state.level = level;
    state.temperature = temperature;
    state.status = (eBatteryStatus)status;
    BatteryState.SetState( state );
}

void Java_com_oculusvr_vrlib_VrLib_nativeHeadsetEvent(JNIEnv *jni, jclass clazz, jint state) 
{
    LOG( "nativeHeadsetEvent(%i)", state );
    HeadsetPluggedState.SetState( ( state == 1 ) );
}

void Java_com_oculusvr_vrlib_VrLib_nativeWifiEvent( JNIEnv * jni, jclass clazz, jint state, jint level )
{
	LOG( "nativeWifiSignalEvent( %i, %i )", state, level );
	WifiState.SetState( wifiState_t( static_cast< eWifiState >( state ) ) );
	WifiSignalLevel.SetState( wifiSignalLevel_t( level ) );
}

void Java_com_oculusvr_vrlib_VrLib_nativeCellularStateEvent( JNIEnv * jni, jclass clazz, jint state )
{
	LOG( "nativeCellularStateEvent( %i )", state );
	CellularState.SetState( cellularState_t( static_cast< eCellularState >( state ) ) );
}

void Java_com_oculusvr_vrlib_VrLib_nativeCellularSignalEvent( JNIEnv * jni, jclass clazz, jint level )
{
	LOG( "nativeCellularSignalEvent( %i )", level );
	CellularSignalLevel.SetState( cellularSignalLevel_t( level ) );
}

void Java_com_oculusvr_vrlib_ProximityReceiver_nativeMountHandled(JNIEnv *jni, jclass clazz)
{
	LOG( "Java_com_oculusvr_vrlib_VrLib_nativeMountEventHandled" );

	// If we're received this, the foreground app has already received
	// and processed the mount event.
	if ( HMTMountState.GetState().MountState == HMT_MOUNT_MOUNTED )
	{
		LOG( "RESETTING MOUNT" );
		HMTMountState.SetState( HMTMountState_t( HMT_MOUNT_NONE ) );
	}
}

void Java_com_oculusvr_vrlib_ProximityReceiver_nativeProximitySensor(JNIEnv *jni, jclass clazz, jint state)
{
	LOG( "nativeProximitySensor(%i)", state );
	if ( state == 0 )
	{
		HMTMountState.SetState( HMTMountState_t( HMT_MOUNT_UNMOUNTED ) );
	}
	else
	{
		HMTMountState.SetState( HMTMountState_t( HMT_MOUNT_MOUNTED ) );
	}
}

void Java_com_oculusvr_vrlib_DockReceiver_nativeDockEvent(JNIEnv *jni, jclass clazz, jint state)
{
	LOG( "nativeDockEvent = %s", ( state == 0 ) ? "UNDOCKED" : "DOCKED" );
	
	if ( state == 0 )
	{
		// On undock, we need to do the following 2 things:
		// (1) Provide a means for apps to save their state.
		// (2) Call finish() to kill app.

		// NOTE: act.finish() triggers OnPause() -> OnStop() -> OnDestroy() to 
		// be called on the activity. Apps may place save data and state in
		// OnPause() (or OnApplicationPause() for Unity)

		HMTDockState.SetState( HMTDockState_t( HMT_DOCK_UNDOCKED ) );
	}
	else
	{
		HMTDockState_t dockState = HMTDockState.GetState();
		if ( dockState.DockState == HMT_DOCK_UNDOCKED )
		{
			LOG( "CLEARING UNDOCKED!!!!" );
		}
		HMTDockState.SetState( HMTDockState_t( HMT_DOCK_DOCKED ) );
	}
}

}

void ovr_OnLoad( JavaVM * JavaVm_ )
{
	if ( JavaVm_ == NULL )
	{
		FAIL( "JavaVm == NULL" );
	}
	if ( JavaVm != NULL )
	{
		FAIL( "ovr_OnLoad() called twice" );
	}

	JavaVm = JavaVm_;
	OnLoadTid = gettid();

	// We only need a JNIEnv on this thread for class name lookup
	JNIEnv	* jni;
	const jint rtn = JavaVm->AttachCurrentThread( &jni, 0 );
	if ( rtn != JNI_OK )
	{
		FAIL( "AttachCurrentThread returned %i", rtn );
	}

	VrLibClass = ovr_GetGlobalClassReference( jni, "com/oculusvr/vrlib/VrLib" );
	VrActivityClass = ovr_GetGlobalClassReference( jni, "com/oculusvr/vrlib/VrActivity" );
	PlatformActivityClass = ovr_GetGlobalClassReference( jni, "com/oculusvr/vrlib/PlatformActivity" );
	ProximityReceiverClass = ovr_GetGlobalClassReference( jni, "com/oculusvr/vrlib/ProximityReceiver" );
	DockReceiverClass = ovr_GetGlobalClassReference( jni, "com/oculusvr/vrlib/DockReceiver" );

	// initialize Oculus code
	LOG( "Calling ovr_Initialize()" );
	ovr_Initialize();

	OvrHmd = ovrHmd_Create( 0 );
	if ( !OvrHmd )
	{
		FAIL( "ovrHmd_Create(0) failed" );
	}

	// Start the sensor running
	ovrHmd_StartSensor( OvrHmd, ovrHmdCap_Orientation|ovrHmdCap_YawCorrection, 0 );

	// After ovr_Initialize(), because it uses String
	BuildStrings = new OVR::NativeBuildStrings( jni );

	// Get the BuildVersion SDK
	{
		jclass versionClass = jni->FindClass( "android/os/Build$VERSION" );
		if ( versionClass != 0 )
		{
			jfieldID sdkIntFieldID = jni->GetStaticFieldID( versionClass, "SDK_INT", "I" );
			if ( sdkIntFieldID != 0 )
			{
				BuildVersionSDK = jni->GetStaticIntField( versionClass, sdkIntFieldID );
				LOG( "BuildVersionSDK %d", BuildVersionSDK );
			}
			jni->DeleteLocalRef( versionClass );
		}
	}
}

void ovr_StartPackageActivity( ovrMobile * ovr, const char * className, const char * commandString )
{
	LOG( "ovr_StartPackageActivity( %s, %s )", className, commandString );

	// Push black images to the screen and release our clock locks
	ovr_LeaveVrMode( ovr );

	jstring classString = ovr->Jni->NewStringUTF( className );
	jstring cmdString = ovr->Jni->NewStringUTF( commandString );

	const jmethodID method = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
			"startPackageActivity", "(Landroid/app/Activity;Ljava/lang/String;Ljava/lang/String;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, method, ovr->Parms.ActivityObject, classString, cmdString );

	ovr->Jni->DeleteLocalRef( cmdString );
	ovr->Jni->DeleteLocalRef( classString );
}

void ovr_ExitActivity( ovrMobile * ovr, eExitType exitType )
{
	if ( exitType == EXIT_TYPE_FINISH )
	{
		LOG( "ovr_ExitActivity( EXIT_TYPE_FINISH ) - act.finish()" );

		ovr_LeaveVrMode( ovr );

		OVR_ASSERT( ovr != NULL );

	//	const char * name = "finish";
		const char * name = "finishOnUiThread";
		const jmethodID mid = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				name, "(Landroid/app/Activity;)V" );

		if ( ovr->Jni->ExceptionOccurred() )
		{
			ovr->Jni->ExceptionClear();
			LOG("Cleared JNI exception");
		}
		LOG( "Calling activity.finishOnUiThread()" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, mid, *static_cast< jobject* >( &ovr->Parms.ActivityObject ) );
		LOG( "Returned from activity.finishOnUiThread()" );
	}
	else if ( exitType == EXIT_TYPE_FINISH_AFFINITY )
	{
		LOG( "ovr_ExitActivity( EXIT_TYPE_FINISH_AFFINITY ) - act.finishAffinity()" );

		OVR_ASSERT( ovr != NULL );

		ovr_LeaveVrMode( ovr );

		const char * name = "finishAffinityOnUiThread";
		const jmethodID mid = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				name, "(Landroid/app/Activity;)V" );

		if ( ovr->Jni->ExceptionOccurred() )
		{
			ovr->Jni->ExceptionClear();
			LOG("Cleared JNI exception");
		}
		LOG( "Calling activity.finishAffinityOnUiThread()" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, mid, *static_cast< jobject* >( &ovr->Parms.ActivityObject ) );
		LOG( "Returned from activity.finishAffinityOnUiThread()" );
	}
	else if ( exitType == EXIT_TYPE_EXIT )
	{
		LOG( "ovr_ExitActivity( EXIT_TYPE_EXIT ) - exit(0)" );

		// This should only ever be called from the Java thread.
		// ovr_LeaveVrMode() should have been called already from the VrThread.
		OVR_ASSERT( ovr == NULL || ovr->Destroyed );

		if ( OnLoadTid != gettid() )
		{
			FAIL( "ovr_ExitActivity( EXIT_TYPE_EXIT ): Called with tid %d instead of %d", gettid(), OnLoadTid );
		}

		ovr_ShutdownLocalPreferences();
		ovrHmd_Destroy( OvrHmd );
		ovr_Shutdown();
		exit( 0 );
	}
}

// FIXME: Currently duplicating functionality with App.cpp
// SetSchedulingReport

// Always returns a 0 terminated, possibly empty, string.
// Strips any trailing \n.
static const char * ReadSmallFile( const char * path )
{
	static char buffer[1024];
	buffer[0] = 0;

	FILE * f = fopen( path, "r" );
	if ( !f )
	{
		return buffer;
	}
	const int r = fread( buffer, 1, sizeof( buffer ) - 1, f );
	fclose( f );

	// Strip trailing \n that some sys files have.
	for ( int n = r; n > 0 && buffer[n] == '\n'; n-- )
	{
		buffer[n] = 0;
	}
	return buffer;
}

static OVR::String StripLinefeed( const OVR::String s )
{
	OVR::String copy;
	for ( int i = 0; i < s.GetLengthI() && s.GetCharAt( i ) != '\n'; i++ )
	{
		copy += s.GetCharAt( i );
	}
	return copy;
}

static int ReadFreq( const char * pathFormat, ... )
{
	char fullPath[1024] = {};

	va_list argptr;
	va_start( argptr, pathFormat );
	vsnprintf( fullPath, sizeof( fullPath ) - 1, pathFormat, argptr );
	va_end( argptr );

	OVR::String clock = ReadSmallFile( fullPath );
	clock = StripLinefeed( clock );
	if ( clock == "" )
	{
		return -1;
	}
	return atoi( clock.ToCStr() );
}

// System Power Level State
enum ePowerLevelState
{
	POWERLEVEL_NORMAL = 0,		// Device operating at full level
	POWERLEVEL_POWERSAVE = 1,	// Device operating at throttled level
	POWERLEVEL_MINIMUM = 2,		// Device operating at minimum level
	MAX_POWERLEVEL_STATES
};
enum ePowerLevelAction
{
	POWERLEVEL_ACTION_NONE,
	POWERLEVEL_ACTION_WAITING_FOR_UNMOUNT,
	POWERLEVEL_ACTION_WAITING_FOR_UNDOCK,
	POWERLEVEL_ACTION_WAITING_FOR_RESET,
	MAX_POWERLEVEL_ACTIONS
};
static ePowerLevelAction powerLevelAction = POWERLEVEL_ACTION_NONE;

bool ovr_GetPowerLevelStateThrottled()
{
	return PowerLevelStateThrottled.GetState();
}

bool ovr_GetPowerLevelStateMinimum()
{
	return PowerLevelStateMinimum.GetState();
}

void ovr_CheckPowerLevelState( ovrMobile * ovr )
{
	const double timeNow = floor( ovr_GetTimeInSeconds() );
	bool checkThisFrame = false;
	if ( timeNow > powerLevelCheckLastReportTime )
	{
		checkThisFrame = true;
		powerLevelCheckLastReportTime = timeNow;
	}
	if ( !checkThisFrame )
	{
		return;
	}

	ePowerLevelState powerLevelState = POWERLEVEL_NORMAL;
	if ( getPowerLevelStateID != NULL )
	{
		powerLevelState = (ePowerLevelState)ovr->Jni->CallStaticIntMethod( VrLibClass, getPowerLevelStateID, ovr->Parms.ActivityObject );
	}
#if 0
	ovr_UpdateLocalPreferences();
	const char * powerLevelStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_POWER_LEVEL_STATE, "-1" );
	const int powerLevel = atoi( powerLevelStr );
	if ( powerLevel >= 0 )
	{
		powerLevelState = (ePowerLevelState)powerLevel;
	}
	LOG( "powerlevelstate %d powerlevelaction %d", powerLevelState, powerLevelAction );
#endif

	// Skip handling of power level changes (ie, on unmount)
	const bool skipPowerLevelTime = timeNow < startPowerLevelCheckTime;
	const bool skip = skipPowerLevelTime || alwaysSkipPowerLevelCheck;

	const bool isThrottled = PowerLevelStateThrottled.GetState();

	// TODO: Test if we can leave the sys files open and do continuous read or do we always
	// need to read then close?

	// NOTE: Only testing one core for thermal throttling.
	// we won't be able to read the cpu clock unless it has been chmod'd to 0666, but try anyway.
	int cpuCore = 0;
	if ( ( OVR::EglGetGpuType() & OVR::GPU_TYPE_MALI ) != 0 )
	{
		// Use the first BIG core if it is online, otherwise use the first LITTLE core.
		const char * online = ReadSmallFile( "/sys/devices/system/cpu/cpu4/online" );
		cpuCore = ( online[0] != '\0' && atoi( online ) != 0 ) ? 4 : 0;
	}

	const int64_t cpuUnit = ( ( OVR::EglGetGpuType() & OVR::GPU_TYPE_MALI ) != 0 ) ? 1000LL : 1000LL;
	const int64_t cpuFreq = ReadFreq( "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_cur_freq", cpuCore );
	const int64_t gpuUnit = ( ( OVR::EglGetGpuType() & OVR::GPU_TYPE_MALI ) != 0 ) ? 1000000LL : 1000LL;
	const int64_t gpuFreq = ReadFreq( ( ( OVR::EglGetGpuType() & OVR::GPU_TYPE_MALI ) != 0 ) ?
									"/sys/devices/14ac0000.mali/clock" :
									"/sys/class/kgsl/kgsl-3d0/gpuclk" );
	LOG( "CPU%d Clock %lld MHz, GPU Clock %lld MHz, Power Level State %d, %s",
			cpuCore, cpuFreq * cpuUnit / 1000000, gpuFreq * gpuUnit / 1000000,
			powerLevelState, ( isThrottled ) ? "throttled" : "normal" );

	/*
	------------------------------------------------------------
	When the device first transitions from NORMAL to POWERSAVE,
	we display a warning that performance will be degraded and
	force 30Hz TimeWarp operation.
		
	We stay in this state even if the power level state goes back
	to NORMAL after cooling down, because it would be expected to
	rapidly go back up if 60Hz operation were resumed.

	60Hz operation will only be re-enabled after the headset has
	been taken off the user's head.

	If the power level goes to MINIMUM, the platform UI will come
	up with a warning and not allow the game to continue. The user
	will have to take the headset off and wait to get it reset,
	but no state in the app should be lost.
	-------------------------------------------------------------
	*/
	if ( powerLevelState == POWERLEVEL_NORMAL )
	{
		if ( powerLevelAction == POWERLEVEL_ACTION_WAITING_FOR_RESET )
		{
			LOG( "RESET FROM POWERSAVE MODE" );
			PowerLevelStateThrottled.SetState( false );
			powerLevelAction = POWERLEVEL_ACTION_NONE;
		}
	}
	else if ( powerLevelState == POWERLEVEL_POWERSAVE )
	{
		// CPU and GPU have been de-clocked to power-save levels.
		if ( !isThrottled )
		{
			if ( ovr->Parms.AllowPowerSave )
			{
				// If app allows power save, adjust application performance
				// to account for lower clock frequencies and warn about
				// performance degradation.
				LOG( "THERMAL THROTTLING LEVEL 1 - POWER SAVE MODE FORCING 30FPS" );
				PowerLevelStateThrottled.SetState( true );
				powerLevelAction = POWERLEVEL_ACTION_WAITING_FOR_UNMOUNT;
				ovr_StartPackageActivity( ovr, PUI_CLASS_NAME, PUI_THROTTLED1 );
			}
			else
			{
				if ( powerLevelAction != POWERLEVEL_ACTION_WAITING_FOR_UNDOCK )
				{
					LOG( "THERMAL THROTTLING LEVEL 2 - CANNOT CONTINUE" );
					PowerLevelStateThrottled.SetState( true );
					PowerLevelStateMinimum.SetState( true );
					powerLevelAction = POWERLEVEL_ACTION_WAITING_FOR_UNDOCK;
					ovr_StartPackageActivity( ovr, PUI_CLASS_NAME, PUI_THROTTLED2 );
				}
			}
		}
	}
	else if ( powerLevelState == POWERLEVEL_MINIMUM )
	{
		if ( powerLevelAction != POWERLEVEL_ACTION_WAITING_FOR_UNDOCK )
		{
			// Warn that the game cannot continue in minimum mode.
			LOG( "THERMAL THROTTLING LEVEL 2 - CANNOT CONTINUE" );
			PowerLevelStateThrottled.SetState( true );
			PowerLevelStateMinimum.SetState( true );
			powerLevelAction = POWERLEVEL_ACTION_WAITING_FOR_UNDOCK;
			ovr_StartPackageActivity( ovr, PUI_CLASS_NAME, PUI_THROTTLED2 );
		}
	}
}

/*

The July S5 SDK supported a mechanism to lock minimum clock rates
for the CPU and GPU. However, the system could still choose to ramp the
rates up under some internal algorithm. Turning values down wouldn't
necessarily make the app run slower at a steady state -- it will start
out slower, but may ramp back up to nearly the same values.

The Note 4 SDK provides an API to lock a fixed clock level for the CPU and GPU.
For devices with builds which do not support this api, we continue to provide
the minimum clock rate api.

return2EnableFreqLev returns the [min,max] clock levels available for the
CPU and GPU, ie [0,3]. However, not all clock combinations are valid for 
all devices. For instance, the highest GPU level may not be available for
use with the highest CPU levels. If an invalid matrix combination is
provided, the system will not acknowlege the request and clock settings
will go into dynamic mode.

Fixed clock level on Qualcomm Snapdragon 805 based Note 4
GPU MHz: 0 =    240, 1 =   300, 2 =   389, 3 =   500
CPU MHz: 0 =    884, 1 =  1191, 2 =  1498, 3 =  1728

Fixed clock level on ARM Exynos 5433 based Note 4
GPU MHz: 0 =    266, 1 =   350, 2 =   500, 3 =   550
CPU MHz: 0 = L 1000, 1 = B 700, 2 = B 700, 3 = B 800

*/

enum {
	LEVEL_GPU_MIN = 0,
	LEVEL_GPU_MAX,
	LEVEL_CPU_MIN,
	LEVEL_CPU_MAX
};

enum {
	CLOCK_CPU_FREQ = 0,
	CLOCK_GPU_FREQ,
	CLOCK_POWERSAVE_CPU_FREQ,
	CLOCK_POWERSAVE_GPU_FREQ
};

enum {
	INVALID_CLOCK_LEVEL = -1,
	RELEASE_CLOCK_LEVEL = -1
};

static bool WriteFreq( const int freq, const char * pathFormat, ... )
{
	char fullPath[1024] = {};

	va_list argptr;
	va_start( argptr, pathFormat );
	vsnprintf( fullPath, sizeof( fullPath ) - 1, pathFormat, argptr );
	va_end( argptr );

	char string[1024] = {};
	sprintf( string, "%d", freq );

	FILE * f = fopen( fullPath, "w" );
	if ( !f )
	{
		LOG( "failed to open %s", fullPath );
		return false;
	}
	const int r = fwrite( string, 1, strlen( string ), f );
	fclose( f );

	LOG( "wrote %s to %s", string, fullPath );
}

/*
 * Set the Fixed CPU/GPU Clock Levels
 *
 * Enter (-1,-1) to release the current clock levels.
 */
static void SetVrPlatformOptions( JNIEnv * VrJni, jclass vrActivityClass, jobject activityObject,
		const int cpuLevel, const int gpuLevel )
{
	// Clear any previous exceptions.
	// NOTE: This can be removed once security exception handling is moved to 
	// Java IF.
	if ( VrJni->ExceptionOccurred() )
	{
		VrJni->ExceptionClear();
		LOG( "SetVrPlatformOptions: Enter: JNI Exception occurred" );
	}

	LOG( "SetVrPlatformOptions( %i, %i )", cpuLevel, gpuLevel );

	// Get the available clock levels for the device.
	const jmethodID getAvailableClockLevelsId = ovr_GetStaticMethodID( VrJni, vrActivityClass,
		"getAvailableFreqLevels", "(Landroid/app/Activity;)[I" ); 
	jintArray jintLevels = (jintArray)VrJni->CallStaticObjectMethod( vrActivityClass,
		getAvailableClockLevelsId, activityObject );

	// TODO: Remove path to support old S5 binaries without fixed clock level api support,
	// only minimum clock level support. Move security exception detection to the java IF.
	// Catch Permission denied
	if ( VrJni->ExceptionOccurred() )
	{
		VrJni->ExceptionClear();
		LOG( "SetVrPlatformOptions: JNI Exception occurred, returning" );
		return;
	}

	jsize length = VrJni->GetArrayLength( jintLevels );
	OVR_ASSERT( length == 4 );		// {GPU MIN, GPU MAX, CPU MIN, CPU MAX}

	jint * levels = VrJni->GetIntArrayElements( jintLevels, NULL );
	if ( levels != NULL )
	{
		// Verify levels are within appropriate range for the device
		if ( cpuLevel >= 0 )
		{
			OVR_ASSERT( cpuLevel >= levels[LEVEL_CPU_MIN] && cpuLevel <= levels[LEVEL_CPU_MAX] );
			LOG( "CPU levels [%d, %d]", levels[LEVEL_CPU_MIN], levels[LEVEL_CPU_MAX] );
		}
		if ( gpuLevel >= 0 )
		{
			OVR_ASSERT( gpuLevel >= levels[LEVEL_GPU_MIN] && gpuLevel <= levels[LEVEL_GPU_MAX] );
			LOG( "GPU levels [%d, %d]", levels[LEVEL_GPU_MIN], levels[LEVEL_GPU_MAX] );
		}

		VrJni->ReleaseIntArrayElements( jintLevels, levels, 0 );
	}
	VrJni->DeleteLocalRef( jintLevels );

	// Set the fixed cpu and gpu clock levels
	const jmethodID setSystemPerformanceId = ovr_GetStaticMethodID( VrJni, vrActivityClass,
			"setSystemPerformanceStatic", "(Landroid/app/Activity;II)[I" );
	jintArray jintClocks = (jintArray)VrJni->CallStaticObjectMethod( vrActivityClass, setSystemPerformanceId,
			activityObject, cpuLevel, gpuLevel );

	length = VrJni->GetArrayLength( jintClocks );
	OVR_ASSERT( length == 4 );		//  {CPU CLOCK, GPU CLOCK, POWERSAVE CPU CLOCK, POWERSAVE GPU CLOCK}

	jint * clocks = VrJni->GetIntArrayElements( jintClocks, NULL );
	if ( clocks != NULL )
	{
		const int64_t cpuUnit = ( ( OVR::EglGetGpuType() & OVR::GPU_TYPE_MALI ) != 0 ) ? 1000LL : 1000LL;
		const int64_t gpuUnit = ( ( OVR::EglGetGpuType() & OVR::GPU_TYPE_MALI ) != 0 ) ? 1000LL : 1LL;
		const int64_t cpuFreq = clocks[CLOCK_CPU_FREQ];
		const int64_t gpuFreq = clocks[CLOCK_GPU_FREQ];
		//const int64_t powerSaveCpuMhz = clocks[CLOCK_POWERSAVE_CPU_FREQ];
		//const int64_t powerSaveGpuMhz = clocks[CLOCK_POWERSAVE_GPU_FREQ];

		LOG( "CPU Clock = %lld MHz", cpuFreq * cpuUnit / 1000000 );
		LOG( "GPU Clock = %lld MHz", gpuFreq * gpuUnit / 1000000 );

		// NOTE: If provided an invalid matrix combination, the system will not
		// acknowledge the request and clock settings will go into dynamic mode.
		// Warn when we've encountered this failure condition.
		if ( ( ( cpuLevel != RELEASE_CLOCK_LEVEL ) && ( cpuFreq == INVALID_CLOCK_LEVEL ) ) || 
			 ( ( gpuLevel != RELEASE_CLOCK_LEVEL ) && ( gpuFreq == INVALID_CLOCK_LEVEL ) ) )
		{
			// NOTE: Mali does not return proper values
			if ( ( OVR::EglGetGpuType() & OVR::GPU_TYPE_MALI ) == 0 )
			{
				OVR_ASSERT( 0 );
				const char * cpuValid = ( ( cpuLevel != RELEASE_CLOCK_LEVEL ) && ( cpuFreq == INVALID_CLOCK_LEVEL ) ) ? "denied" : "accepted";
				const char * gpuValid = ( ( gpuLevel != RELEASE_CLOCK_LEVEL ) && ( gpuFreq == INVALID_CLOCK_LEVEL ) ) ? "denied" : "accepted";
				LOG( "WARNING: Invalid clock level combination for this device (cpu:%d=%s,gpu:%d=%s), forcing dynamic mode on", 
					cpuLevel, cpuValid, gpuLevel, gpuValid );
			}
		}

		VrJni->ReleaseIntArrayElements( jintClocks, clocks, 0 );
	}
	VrJni->DeleteLocalRef( jintClocks );

	/*
	for ( int i = 0; i < 4; i++ )
	{
		WriteFreq( 1200000, "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_min_freq", i );
	}
	for ( int i = 4; i < 8; i++ )
	{
		WriteFreq( 1300000, "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_min_freq", i );
	}
	*/
}

static void SetSchedFifo( ovrMobile * ovr, int tid, int priority )
{
	const jmethodID setSchedFifoMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "setSchedFifoStatic", "(Landroid/app/Activity;II)I" );
	const int r = ovr->Jni->CallStaticIntMethod( VrLibClass, setSchedFifoMethodId, ovr->Parms.ActivityObject, tid, priority );
	LOG( "SetSchedFifo( %i, %i ) = %s", tid, priority,
			( ( r == -1 ) ? "VRManager failed" :
			( ( r == -2 ) ? "API not found" :
			( ( r == -3 ) ? "security exception" : "succeeded" ) ) ) );

	if ( priority != 0 )
	{
		switch ( r )
		{
			case -1: LOG( "VRManager failed to set thread priority." ); break;
			case -2: LOG( "Thread priority API does not exist. Update your device binary." ); break;
			case -3: FAIL( "Thread priority security exception. Make sure the APK is signed." ); break;
		}
	}
}

static void UpdateHmdInfo( ovrMobile * ovr )
{
	ovrSensorDesc cleared = {};
	ovrHmd_GetSensorDesc( OvrHmd, &cleared );
	ovr->SensorDesc = cleared;

	LOG( "ovrSensorDesc.ProductId = %i", ovr->SensorDesc.ProductId );
	LOG( "ovrSensorDesc.VendorId = %i", ovr->SensorDesc.VendorId );
	LOG( "ovrSensorDesc.SerialNumber = %s", ovr->SensorDesc.SerialNumber );
	LOG( "ovrSensorDesc.Version = %i", ovr->SensorDesc.Version );

	ovr->HmdInfo = OVR::GetDeviceHmdInfo( ovr->Jni, ovr->Parms.ActivityObject, VrLibClass, OvrHmd,
			BuildStrings->GetBuildString( BUILDSTR_MODEL ) );

	// Update the pixels directly from the window
	ovr->Screen.GetScreenResolution( ovr->HmdInfo.widthPixels, ovr->HmdInfo.heightPixels );

	LOG( "hmdInfo.lensSeparation = %f", ovr->HmdInfo.lensSeparation );
	LOG( "hmdInfo.widthMeters = %f", ovr->HmdInfo.widthMeters );
	LOG( "hmdInfo.heightMeters = %f", ovr->HmdInfo.heightMeters );
	LOG( "hmdInfo.widthPixels = %i", ovr->HmdInfo.widthPixels );
	LOG( "hmdInfo.heightPixels = %i", ovr->HmdInfo.heightPixels );
	LOG( "hmdInfo.eyeTextureFov = %f", ovr->HmdInfo.eyeTextureFov );
}

int	ovr_GetSystemBrightness( ovrMobile * ovr ) 
{
	int cur = 255;
	jmethodID getSysBrightnessMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "getSystemBrightness", "(Landroid/app/Activity;)I" );
	if ( getSysBrightnessMethodId != NULL && OVR::OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		cur = ovr->Jni->CallStaticIntMethod( VrLibClass, getSysBrightnessMethodId, ovr->Parms.ActivityObject );
		LOG( "System brightness = %i", cur );
	}
	return cur;
}

void ovr_SetSystemBrightness(  ovrMobile * ovr, int const v )
{
	int v2 = v < 0 ? 0 : v;
	v2 = v2 > 255 ? 255 : v2;
	jmethodID setSysBrightnessMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "setSystemBrightness", "(Landroid/app/Activity;I)V" );
	if ( setSysBrightnessMethodId != NULL && OVR::OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		ovr->Jni->CallStaticVoidMethod( VrLibClass, setSysBrightnessMethodId, ovr->Parms.ActivityObject, v2 );
		LOG( "Set brightness to %i", v2 );
		ovr_GetSystemBrightness( ovr );
	}
}

bool ovr_GetComfortModeEnabled( ovrMobile * ovr ) 
{
	jmethodID getComfortViewModeMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "getComfortViewModeEnabled", "(Landroid/app/Activity;)Z" );
	bool r = true;
	if ( getComfortViewModeMethodId != NULL && OVR::OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		r = ovr->Jni->CallStaticBooleanMethod( VrLibClass, getComfortViewModeMethodId, ovr->Parms.ActivityObject );
		LOG( "System comfort mode = %s", r ? "true" : "false" );
	}
	return r;
}

void ovr_SetComfortModeEnabled( ovrMobile * ovr, bool const enabled )
{
	jmethodID enableComfortViewModeMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "enableComfortViewMode", "(Landroid/app/Activity;Z)V" );
	if ( enableComfortViewModeMethodId != NULL && OVR::OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		ovr->Jni->CallStaticVoidMethod( VrLibClass, enableComfortViewModeMethodId, ovr->Parms.ActivityObject, enabled );
		LOG( "Set comfort mode to %s", enabled ? "true" : "false" );
		ovr_GetComfortModeEnabled( ovr );
	}
}

void ovr_SetDoNotDisturbMode( ovrMobile * ovr, bool const enabled )
{
	jmethodID setDoNotDisturbModeMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "setDoNotDisturbMode", "(Landroid/app/Activity;Z)V" );
	if ( setDoNotDisturbModeMethodId != NULL && OVR::OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		ovr->Jni->CallStaticVoidMethod( VrLibClass, setDoNotDisturbModeMethodId, ovr->Parms.ActivityObject, enabled );
		LOG( "System DND mode = %s", enabled ? "true" : "false" );
	}
}

bool ovr_GetDoNotDisturbMode( ovrMobile * ovr ) 
{
	bool r = false;
	jmethodID getDoNotDisturbModeMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "getDoNotDisturbMode", "(Landroid/app/Activity;)Z" );
	if ( getDoNotDisturbModeMethodId != NULL && OVR::OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		r = ovr->Jni->CallStaticBooleanMethod( VrLibClass, getDoNotDisturbModeMethodId, ovr->Parms.ActivityObject );
		LOG( "Set DND mode to %s", r ? "true" : "false" );
	}
	return r;
}

// Starts up TimeWarp, vsync tracking, sensor reading, clock locking, and sets video options.
// Should be called when the app is both resumed and has a window surface.
// The application must have their preferred OpenGL ES context current so the correct
// version and config ccan be configured for the background TimeWarp thread.
// On return, the context will be current on an invisible pbuffer, because TimeWarp
// will own the window.
ovrMobile * ovr_EnterVrMode( ovrModeParms parms, ovrHmdInfo * returnedHmdInfo )
{
	LOG( "---------- ovr_EnterVrMode ----------" );
#if defined( OVR_BUILD_DEBUG )
	char const * buildConfig = "DEBUG";
#else
	char const * buildConfig = "RELEASE";
#endif

	// This returns the existing jni if the caller has already created
	// one, or creates a new one.
	JNIEnv	* Jni = NULL;
	const jint rtn = JavaVm->AttachCurrentThread( &Jni, 0 );
	if ( rtn != JNI_OK )
	{
		FAIL( "AttachCurrentThread returned %i", rtn );
	}

	// log the application name, version, activity, build, model, etc.
	jmethodID logApplicationNameMethodId = ovr_GetStaticMethodID( Jni, VrLibClass, "logApplicationName", "(Landroid/app/Activity;)V" );
	Jni->CallStaticVoidMethod( VrLibClass, logApplicationNameMethodId, parms.ActivityObject );

	jmethodID logApplicationVersionId = ovr_GetStaticMethodID( Jni, VrLibClass, "logApplicationVersion", "(Landroid/app/Activity;)V" );
	Jni->CallStaticVoidMethod( VrLibClass, logApplicationVersionId, parms.ActivityObject );

	char currentClassName[128];
	LOG( "ACTIVITY = %s", ovr_GetCurrentActivityName( Jni, parms.ActivityObject, currentClassName, sizeof( currentClassName ) ) );

	LOG( "BUILD = %s %s", BuildStrings->GetBuildString( BUILDSTR_DISPLAY ), buildConfig );
	LOG( "MODEL = %s", BuildStrings->GetBuildString( BUILDSTR_MODEL ) );
	LOG( "OVR_VERSION = %s", OVR_VERSION_STRING );
	LOG( "ovrModeParms.AsynchronousTimeWarp = %i", parms.AsynchronousTimeWarp );
	LOG( "ovrModeParms.AllowPowerSave = %i", parms.AllowPowerSave );
	LOG( "ovrModeParms.DistortionFileName = %s", parms.DistortionFileName ? parms.DistortionFileName : "" );
	LOG( "ovrModeParms.EnableImageServer = %i", parms.EnableImageServer );
	LOG( "ovrModeParms.GameThreadTid = %i", parms.GameThreadTid );
	LOG( "ovrModeParms.CpuLevel = %i", parms.CpuLevel );
	LOG( "ovrModeParms.GpuLevel = %i", parms.GpuLevel );

	ovrMobile * ovr = new ovrMobile;
	ovr->Jni = Jni;
	ovr->EnterTid = gettid();
	ovr->Parms = parms;
	ovr->Destroyed = false;

	ovrSensorState state = ovrHmd_GetSensorState( OvrHmd, ovr_GetTimeInSeconds(), true );
	if ( state.Status & ovrStatus_OrientationTracked )
	{
		LOG( "HMD sensor attached.");
	}
	else
	{
		LOG( "Operating without a sensor.");
	}

	// Let GlUtils look up extensions
	OVR::GL_FindExtensions();

	// Make the current window into a front-buffer
	// NOTE: 2014-12-05 - Initial Android-L support requires
	// different behavior than KitKat for setting up 
	// front buffer rendering.
   	ovr->Screen.InitForCurrentSurface( ovr->Jni, BuildVersionSDK );

	// Based on sensor ID and platform, determine the HMD
	UpdateHmdInfo( ovr );

	// Start up our vsync callbacks.
	const jmethodID startVsyncId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
    		"startVsync", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, startVsyncId, ovr->Parms.ActivityObject );

	// Register our HMT receivers if they have not already been registered.
	if ( !registerHMTReceivers )
	{
		const jmethodID startProximityReceiverId = ovr_GetStaticMethodID( ovr->Jni, ProximityReceiverClass,
				"startProximityReceiver", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( ProximityReceiverClass, startProximityReceiverId, ovr->Parms.ActivityObject );

		const jmethodID startDockReceiverId = ovr_GetStaticMethodID( ovr->Jni, DockReceiverClass,
				"startDockReceiver", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( DockReceiverClass, startDockReceiverId, ovr->Parms.ActivityObject );

		registerHMTReceivers = true;
	}

	// Register our receivers
#if 1
	const jmethodID startReceiversId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
    		"startReceivers", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, startReceiversId, ovr->Parms.ActivityObject );
#else
	const jmethodID startBatteryReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
    		"startBatteryReceiver", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, startBatteryReceiverId, ovr->Parms.ActivityObject );

	const jmethodID startHeadsetReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
    		"startHeadsetReceiver", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, startHeadsetReceiverId, ovr->Parms.ActivityObject );

	const jmethodID startVolumeReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
    		"startVolumeReceiver", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, startVolumeReceiverId, ovr->Parms.ActivityObject );

	const jmethodID startWifiReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
    		"startWifiReceiver", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, startWifiReceiverId, ovr->Parms.ActivityObject );

	const jmethodID startCellularReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
    		"startCellularReceiver", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, startCellularReceiverId, ovr->Parms.ActivityObject );
#endif

	getPowerLevelStateID = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "getPowerLevelState", "(Landroid/app/Activity;)I" );
	setActivityWindowFullscreenID = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "setActivityWindowFullscreen", "(Landroid/app/Activity;)V" );
	notifyMountHandledID = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "notifyMountHandled", "(Landroid/app/Activity;)V" );

	// get external storage directory
	const jmethodID getExternalStorageDirectoryMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "getExternalStorageDirectory", "()Ljava/lang/String;" );
	jstring externalStorageDirectoryString = (jstring)ovr->Jni->CallStaticObjectMethod( VrLibClass, getExternalStorageDirectoryMethodId );
	const char *externalStorageDirectoryStringUTFChars = ovr->Jni->GetStringUTFChars( externalStorageDirectoryString, NULL );
	OVR::String externalStorageDirectory = externalStorageDirectoryStringUTFChars;
	ovr->Jni->ReleaseStringUTFChars( externalStorageDirectoryString, externalStorageDirectoryStringUTFChars );
	ovr->Jni->DeleteLocalRef( externalStorageDirectoryString );

	// Enable cpu and gpu clock locking
	SetVrPlatformOptions( ovr->Jni, VrLibClass, ovr->Parms.ActivityObject,
			ovr->Parms.CpuLevel, ovr->Parms.GpuLevel );

	if ( ovr->Jni->ExceptionOccurred() )
	{
		ovr->Jni->ExceptionClear();
		LOG( "Cleared JNI exception" );
	}

	// Start the time warp thread
	ovr->Twp.AsynchronousTimeWarp = ovr->Parms.AsynchronousTimeWarp;
	ovr->Twp.DistortionFileName = ovr->Parms.DistortionFileName;
	ovr->Twp.EnableImageServer = ovr->Parms.EnableImageServer;
	ovr->Twp.HmdInfo = ovr->HmdInfo;
	ovr->Twp.Screen = &ovr->Screen;
	ovr->Twp.Hmd = OvrHmd;
	ovr->Twp.JavaVm = JavaVm;
	ovr->Twp.VrLibClass = VrLibClass;
	ovr->Twp.ActivityObject = ovr->Parms.ActivityObject;
	ovr->Twp.GameThreadTid = ovr->Parms.GameThreadTid;
	ovr->Twp.ExternalStorageDirectory = externalStorageDirectory;
	ovr->Warp = OVR::TimeWarp::Factory( ovr->Twp );

	// Enable our real time scheduling.

	// The calling thread, which is presumably the eye buffer drawing thread,
	// will be at the lowest realtime priority.
	SetSchedFifo( ovr, gettid(), SCHED_FIFO_PRIORITY_VRTHREAD );
	if ( ovr->Parms.GameThreadTid )
	{
		SetSchedFifo( ovr, ovr->Parms.GameThreadTid, SCHED_FIFO_PRIORITY_VRTHREAD );
	}

	// The device manager deals with sensor updates, which will happen at 500 hz, and should
	// preempt the main thread, but not timewarp or audio mixing.
	SetSchedFifo( ovr, ovr_GetDeviceManagerThreadTid(), SCHED_FIFO_PRIORITY_DEVICEMNGR );

	// The timewarp thread, if present, will be very high.  It only uses a fraction of
	// a millisecond of CPU, so it can be even higher than audio threads.
	if ( ovr->Parms.AsynchronousTimeWarp )
	{
		SetSchedFifo( ovr, ovr->Warp->GetWarpThreadTid(), SCHED_FIFO_PRIORITY_TIMEWARP );
	}

	// Resume the device manager thread.
//	ovr_ResumeDeviceManagerThread();

	// reset brightness, DND and comfort modes because VRSVC, while maintaining the value, does not actually enforce these settings.
	int brightness = ovr_GetSystemBrightness( ovr );
	ovr_SetSystemBrightness( ovr, brightness );
	
	bool dndMode = ovr_GetDoNotDisturbMode( ovr );
	ovr_SetDoNotDisturbMode( ovr, dndMode );

	bool comfortMode = ovr_GetComfortModeEnabled( ovr );
	ovr_SetComfortModeEnabled( ovr, comfortMode );

	// reset volume popup
	TimeSinceLastVolumeChange.SetState( -1 );

	ovrHmdInfo info = {};
	info.SuggestedEyeFov = ovr->HmdInfo.eyeTextureFov;
	info.SuggestedEyeResolution = 1024;

	*returnedHmdInfo = info;

	double WAIT_AFTER_ENTER_VR_MODE_TIME = 5.0;
	startPowerLevelCheckTime = ovr_GetTimeInSeconds() + WAIT_AFTER_ENTER_VR_MODE_TIME;

	/*

	PERFORMANCE WARNING

	Upon return from the platform activity, an additional full-screen
	HWC layer with the fully-qualified activity name would be active in
	the SurfaceFlinger compositor list, consuming nearly a GB/s of bandwidth.
 
	"adb shell dumpsys SurfaceFlinger":

	HWC			| b84364f0 | 00000002 | 00000000 | 00 | 00100 | 00000001 | [    0.0,    0.0, 1440.0, 2560.0] | [    0,    0, 1440, 2560] SurfaceView
	HWC			| b848e020 | 00000002 | 00000000 | 00 | 00105 | 00000001 | [    0.0,    0.0, 1440.0, 2560.0] | [    0,    0, 1440, 2560] com.oculusvr.vrcene/com.oculusvr.vrscene.MainActivity
	FB TARGET	| b843d6f0 | 00000000 | 00000000 | 00 | 00105 | 00000001 | [    0.0,    0.0, 1440.0, 2560.0] | [    0,    0, 1440, 2560] HWC_FRAMEBUFFER_TARGET

	The additional layer is tied to the decor view of the activity
	and is normally not visible. The decor view becomes visible because
	the SurfaceView window is no longer fullscreen after returning
	from the platform activity.

	By always setting the SurfaceView window as full screen, the decor view
	will not be rendered and won't waste any precious bandwidth.

	*/
	if ( setActivityWindowFullscreenID != NULL )
	{
		//jjh@epic - temporarily removed in order to avoid an infinite loop of window create/destroy events
		//ovr->Jni->CallStaticVoidMethod( VrLibClass, setActivityWindowFullscreenID, ovr->Parms.ActivityObject );
	}

	return ovr;
}

void ovr_notifyMountHandled( ovrMobile * ovr )
{
	if ( ovr == NULL )
	{
		return;
	}

	if ( notifyMountHandledID != NULL )
	{
		ovr->Jni->CallStaticVoidMethod( VrLibClass, notifyMountHandledID, ovr->Parms.ActivityObject );
	}
}

// Should be called before the window is destroyed.
void ovr_LeaveVrMode( ovrMobile * ovr )
{
	LOG( "---------- ovr_LeaveVrMode ----------" );

	if ( !ovr )
	{
		LOG( "NULL ovr");
		return;
	}

	if ( ovr->Destroyed )
	{
		LOG( "Skipping ovr_LeaveVrMode: ovr already Destroyed");
		return;
	}

	if ( gettid() != ovr->EnterTid )
	{
		FAIL( "ovr_LeaveVrMode: Called with tid %i instead of %i", gettid(), ovr->EnterTid );
	}

	// log the application name, version, activity, build, model, etc.
	jmethodID logApplicationNameMethodId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "logApplicationName", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, logApplicationNameMethodId, ovr->Parms.ActivityObject );

	jmethodID logApplicationVersionId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass, "logApplicationVersion", "(Landroid/app/Activity;)V" );
	ovr->Jni->CallStaticVoidMethod( VrLibClass, logApplicationVersionId, ovr->Parms.ActivityObject );

	char currentClassName[128];
	LOG( "ACTIVITY = %s", ovr_GetCurrentActivityName( ovr->Jni, ovr->Parms.ActivityObject, currentClassName, sizeof( currentClassName ) ) );

	// There are cases where the app gets a "surfaceDestroyed" before
	// getting an "onPause" or "onDestroy" so we have to make sure WarpSwap
	// is not called with an invalid window surface.
	if ( ovr->Screen.windowSurface != EGL_NO_SURFACE )
	{
		// Push black textures to TimeWarp, so the current scene doesn't
		// get "stuck to the user's face" during the transition.
		ovr->Warp->WarpSwapBlack( ovr->HmdInfo.eyeTextureFov );
	}

	delete ovr->Warp;
	ovr->Warp = 0;

	// This must be after pushing the textures to timewarp.
	ovr->Destroyed = true;

	// Remove SCHED_FIFO from the remaining threads.
	SetSchedFifo( ovr, gettid(), SCHED_FIFO_PRIORITY_NONE );
	if ( ovr->Parms.GameThreadTid )
	{
		SetSchedFifo( ovr, ovr->Parms.GameThreadTid, SCHED_FIFO_PRIORITY_NONE );
	}
	SetSchedFifo( ovr, ovr_GetDeviceManagerThreadTid(), SCHED_FIFO_PRIORITY_NONE );

	// Suspend the device manager thread.
//	ovr_SuspendDeviceManagerThread();

	getPowerLevelStateID = NULL;

	if ( 1 )
	{
		// Always release clock locks.
		SetVrPlatformOptions( ovr->Jni, VrLibClass, ovr->Parms.ActivityObject,
				RELEASE_CLOCK_LEVEL, RELEASE_CLOCK_LEVEL );

		// Stop our vsync callbacks.
		const jmethodID stopVsync = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				"stopVsync", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, stopVsync, ovr->Parms.ActivityObject );

		// Unregister our receivers
#if 1
		const jmethodID stopReceiversId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				"stopReceivers", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, stopReceiversId, ovr->Parms.ActivityObject );
#else
		const jmethodID stopCellularReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				"stopCellularReceiver", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, stopCellularReceiverId, ovr->Parms.ActivityObject );

		const jmethodID stopWifiReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				"stopWifiReceiver", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, stopWifiReceiverId, ovr->Parms.ActivityObject );

		const jmethodID stopVolumeReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				"stopVolumeReceiver", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, stopVolumeReceiverId, ovr->Parms.ActivityObject );

		const jmethodID stopBatteryReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				"stopBatteryReceiver", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, stopBatteryReceiverId, ovr->Parms.ActivityObject );

		const jmethodID stopHeadsetReceiverId = ovr_GetStaticMethodID( ovr->Jni, VrLibClass,
				"stopHeadsetReceiver", "(Landroid/app/Activity;)V" );
		ovr->Jni->CallStaticVoidMethod( VrLibClass, stopHeadsetReceiverId, ovr->Parms.ActivityObject );
#endif
	}
	else
	{
		LOG( "Skipping platform reset because another activity is current");
	}

	ovr->Screen.Shutdown();
}

static void ResetTimeWarp( ovrMobile * ovr )
{
	// restart TimeWarp to generate new distortion meshes
	ovr->Twp.HmdInfo = ovr->HmdInfo;
	delete ovr->Warp;
	ovr->Warp = OVR::TimeWarp::Factory( ovr->Twp );
	if ( ovr->Parms.AsynchronousTimeWarp )
	{
		SetSchedFifo( ovr, ovr->Warp->GetWarpThreadTid(), SCHED_FIFO_PRIORITY_TIMEWARP );
	}
}

void ovr_HandleHmdEvents( ovrMobile * ovr )
{
	if ( ovr == NULL )
	{
		return;
	}

	// check if the HMT has been undocked
	HMTDockState_t dockState = HMTDockState.GetState();
	if ( dockState.DockState == HMT_DOCK_UNDOCKED )
	{
		LOG( "ovr_HandleHmdEvents::Hmt was disconnected" );

		// reset the real dock state since we're handling the change
		HMTDockState.SetState( HMTDockState_t( HMT_DOCK_NONE ) );

		// ovr_ExitActivity() with EXIT_TYPE_FINISH_AFFINITY will finish
		// all activities in the stack, ie if we're currently in the platform
		// UI activity, we need to finish it as well as the main activity.
		ovr_ExitActivity( ovr, EXIT_TYPE_FINISH_AFFINITY );

		return;
	}

	// check if the HMT has been mounted or unmounted
	HMTMountState_t mountState = HMTMountState.GetState();
	if ( mountState.MountState != HMT_MOUNT_NONE )
	{
		// reset the real mount state since we're handling the change
		HMTMountState.SetState( HMTMountState_t( HMT_MOUNT_NONE ) );

		if ( mountState.MountState == HMT_MOUNT_MOUNTED )
		{
			if ( hmtIsMounted )
			{
				LOG( "ovr_HandleHmtEvents: HMT is already mounted" );
			}
			else
			{
				LOG( "ovr_HandleHmdEvents: HMT was mounted" );
				hmtIsMounted = true;
				alwaysSkipPowerLevelCheck = false;
				double WAIT_AFTER_MOUNT_TIME = 5.0;
				startPowerLevelCheckTime = ovr_GetTimeInSeconds() + WAIT_AFTER_MOUNT_TIME;

				// broadcast to background apps that mount has been handled
				ovr_notifyMountHandled( ovr );

				// reorient, then exit... 
				// we really should be at the UNMOUNT screen when this happens
				ovrHmd_RecenterYaw( OvrHmd );

				if ( ovr_IsOculusHomePackage( ovr->Jni, ovr->Parms.ActivityObject ) && HandleMountExternally.GetState().Value )
				{
					DROIDLOG( "VrSplash", "External mount" );
					ExternalHMTMountState.SetState( HMTMountState_t( HMT_MOUNT_MOUNTED ) );
				}
				else
				{
					DROIDLOG( "VrSplash", "Starting H&S in platform UI - not really!" );
					//ovr_StartPackageActivity( ovr, PUI_CLASS_NAME, PUI_WARNING );
				}
			}
		}
		else if ( mountState.MountState == HMT_MOUNT_UNMOUNTED )
		{
			LOG( "ovr_HandleHmdEvents: HMT was UNmounted" );
			if ( ovr_IsOculusHomePackage( ovr->Jni, ovr->Parms.ActivityObject ) && HandleMountExternally.GetState().Value )
			{
				DROIDLOG( "VrSplash", "External Unmount" );
				ExternalHMTMountState.SetState( HMTMountState_t( HMT_MOUNT_UNMOUNTED ) );
			}

			alwaysSkipPowerLevelCheck = true;
			hmtIsMounted = false;

			// Reset power level state action if waiting for an unmount event
			if ( powerLevelAction == POWERLEVEL_ACTION_WAITING_FOR_UNMOUNT )
			{
				LOG( "powerLevelAction = POWERLEVEL_ACTION_WAITING_FOR_RESET" );
				powerLevelAction = POWERLEVEL_ACTION_WAITING_FOR_RESET;
			}
		}
	}
}

void ovr_HandleDeviceStateChanges( ovrMobile * ovr )
{
	if ( ovr == NULL )
	{
		return;
	}

	// Test for Hmd Events such as mount/unmount, dock/undock
	ovr_HandleHmdEvents( ovr );

	// Check for device power level state changes
	ovr_CheckPowerLevelState( ovr );
}

void ovr_WarpSwap( ovrMobile * ovr, const TimeWarpParms * parms )
{
	if ( ovr == NULL )
	{
		return;
	}

	if ( ovr->Warp == NULL )
	{
		return;
	}
	// If we are changing to a new activity, leave the screen black
	if ( ovr->Destroyed )
	{
		LOG( "ovr_WarpSwap: Returning due to Destroyed" );
		return;
	}

	if ( gettid() != ovr->EnterTid )
	{
		FAIL( "ovr_WarpSwap: Called with tid %i instead of %i", gettid(), ovr->EnterTid );
	}

	// Check for a device change
	ovrSensorDesc newSensorDesc;
	if ( ovrHmd_GetSensorDesc( OvrHmd, &newSensorDesc ) &&
			(	newSensorDesc.ProductId != ovr->SensorDesc.ProductId ||
				newSensorDesc.VendorId != ovr->SensorDesc.VendorId ||
				newSensorDesc.Version != ovr->SensorDesc.Version ) )
	{
		UpdateHmdInfo( ovr );
		ResetTimeWarp( ovr );
	}

	// Push the new data
	ovr->Warp->WarpSwap( *parms );
}

void ovr_WarpSwapBlack( ovrMobile * ovr )
{
	if ( ovr == NULL )
	{
		return;
	}

	if ( ovr->Warp == NULL )
	{
		return;
	}
	// If we are changing to a new activity, leave the screen black
	if ( ovr->Destroyed )
	{
		LOG( "ovr_WarpSwapBlack: Returning due to Destroyed" );
		return;
	}

	if ( gettid() != ovr->EnterTid )
	{
		FAIL( "ovr_WarpSwapBlack: Called with tid %i instead of %i", gettid(), ovr->EnterTid );
	}

	ovr->Warp->WarpSwapBlack( ovr->HmdInfo.eyeTextureFov );
}

void ovr_WarpSwapLoadingIcon( ovrMobile * ovr )
{
	if ( ovr == NULL )
	{
		return;
	}

	if ( ovr->Warp == NULL )
	{
		return;
	}
	// If we are changing to a new activity, leave the screen black
	if ( ovr->Destroyed )
	{
		LOG( "ovr_WarpSwapLoadingIcon: Returning due to Destroyed" );
		return;
	}

	if ( gettid() != ovr->EnterTid )
	{
		FAIL( "ovr_WarpSwapLoadingIcon: Called with tid %i instead of %i", gettid(), ovr->EnterTid );
	}

	ovr->Warp->WarpSwapLoadingIcon( ovr->HmdInfo.eyeTextureFov );
}

void ovr_SurfaceDestroyed( ovrMobile * ovr )
{
	if ( ovr == NULL )
	{
		return;
	}

	ovr->Screen.Shutdown();
}

void ovr_SetDistortionTuning( ovrMobile * ovr, OVR::DistortionEqnType type, float scale, float* distortionK)
{
	// Dynamic adjustment of distortion
	ovr->HmdInfo.lens.Eqn = type;
	ovr->HmdInfo.lens.MetersPerTanAngleAtCenter = scale;
	if ( type == OVR::Distortion_RecipPoly4 )
	{
		for ( int i = 0; i < 4; i++ )
		{
			ovr->HmdInfo.lens.K[i] = distortionK[i];
		}
	}
	else if ( type == OVR::Distortion_CatmullRom10 )
	{
		for ( int i = 0; i < 11; i++ )
		{
			ovr->HmdInfo.lens.K[i] = distortionK[i];
		}
	}
	else
	{
		for ( int i = 0; i < OVR::LensConfig::MaxCoefficients; i++ )
		{
			ovr->HmdInfo.lens.K[i] = distortionK[i];
		}
	}

	ResetTimeWarp( ovr );
}

void ovr_ResetClockLocks( ovrMobile * ovr )
{
	if ( ovr == NULL )
	{
		return;
	}

	// release the existing clock locks
	SetVrPlatformOptions( ovr->Jni, VrLibClass, ovr->Parms.ActivityObject,
				RELEASE_CLOCK_LEVEL, RELEASE_CLOCK_LEVEL );

	// set to new values
	SetVrPlatformOptions( ovr->Jni, VrLibClass, ovr->Parms.ActivityObject,
			ovr->Parms.CpuLevel, ovr->Parms.GpuLevel );
}

int ovr_GetVolume()
{
	return CurrentVolume.GetState().Value;
}

double ovr_GetTimeSinceLastVolumeChange()
{
	double value = TimeSinceLastVolumeChange.GetState().Value;
	if ( value == -1 )
	{
		//LOG( "ovr_GetTimeSinceLastVolumeChange() : Not initialized.  Returning -1" );
		return -1;
	}
	return ovr_GetTimeInSeconds() - value;
}

int ovr_GetWifiSignalLevel()
{
	return WifiSignalLevel.GetState().Value;
}

eWifiState ovr_GetWifiState()
{
	return WifiState.GetState().Value;
}

int ovr_GetCellularSignalLevel()
{
	return CellularSignalLevel.GetState().Value;
}

eCellularState ovr_GetCellularState()
{
	return CellularState.GetState().Value;
}

batteryState_t ovr_GetBatteryState()
{
	return BatteryState.GetState();
}

bool ovr_GetHeadsetPluggedState()
{
	return HeadsetPluggedState.GetState();
}

char const * ovr_GetBuildString( eBuildString const str )
{
	if ( BuildStrings == NULL )
	{
		OVR_ASSERT( BuildStrings != NULL );	// called too early?
		return "";
	}
	return BuildStrings->GetBuildString( str );
}

void ovr_SetExternalHMTMountHandling( bool const handleExternally )
{
	DROIDLOG( "VrSplash", "Handle HMT mount externally = %s", handleExternally ? "true" : "false" );
	// reset the mount state so that we don't instantly act on any pending state
	HMTMountState.SetState( HMTMountState_t( HMT_MOUNT_NONE ) );
	HandleMountExternally.SetState( externalMount_t( handleExternally ) );
}

HMTMountState_t ovr_GetExternalHMTMountState()
{
	return ExternalHMTMountState.GetState();
}

void ovr_ResetExternalHMTMountState()
{
	ExternalHMTMountState.SetState( HMTMountState_t() );
}
