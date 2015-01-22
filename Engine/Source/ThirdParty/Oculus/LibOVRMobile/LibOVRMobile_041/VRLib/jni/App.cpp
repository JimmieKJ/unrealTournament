/************************************************************************************

Filename    :   App.cpp
Content     :   Native counterpart to VrActivity
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "App.h"

#include <math.h>
#include <jni.h>

#include <android/native_window_jni.h>	// for native window JNI
#include <android/keycodes.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>						// usleep, etc

#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"

#include "3rdParty/stb/stb_image_write.h"

#include "GlUtils.h"
#include "GlTexture.h"
#include "VrCommon.h"

#include "AppLocal.h"

#include "ModelView.h"
#include "BitmapFont.h"
#include "GazeCursorLocal.h"        // necessary to instantiate the gaze cursor
#include "VRMenu/VRMenuMgr.h"
#include "VRMenu/GuiSysLocal.h"    // necessary to instantiate the gui system
#include "VRMenu/VolumePopup.h"
#include "VrApi/LocalPreferences.h"
#include "VrApi/TimeWarp.h"		// for tid needed by CreateSChedulingReport
#include "VrApi/JniUtils.h"
#include "PackageFiles.h"

#define DELAYED_ONE_TIME_INIT
//#define TEST_TIMEWARP_WATCHDOG

static const char * activityClassName = "com/oculusvr/vrlib/VrActivity";
static const char * platformActivityClassName = "com/oculusvr/vrlib/PlatformActivity";
static const char * passThroughCameraClassName = "com/oculusvr/vrlib/PassThroughCamera";
static const char * vrLibClassName = "com/oculusvr/vrlib/VrLib";

namespace OVR
{

// This is separate from App so UnityPlugin doesn't need the app declaration
JavaVM	* VrLibJavaVM;

extern "C"
{

// This is here to keep the linker from stripping this function
jlong Java_com_oculusvr_vrlib_PlatformActivity_nativeSetAppInterface( JNIEnv * jni, jclass clazz, jobject activity );

jint JNI_OnLoad( JavaVM* vm, void* reserved )
{
	LOG( "JNI_OnLoad" );
	VrLibJavaVM = vm;

	ovr_OnLoad( vm );

	return JNI_VERSION_1_6;
}

void Java_com_oculusvr_vrlib_VrActivity_nativeSurfaceChanged( JNIEnv *jni, jclass clazz,
		jlong appPtr, jobject surface )
{
	LOG( "%p nativeSurfaceChanged( %p )", (void *)appPtr, surface );

	if ( surface == NULL )
	{
		((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "destroyedSurface " );
	}
	else
	{
		((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "createdSurface %p",
				surface ? ANativeWindow_fromSurface( jni, surface ) : NULL );
	}
}

void Java_com_oculusvr_vrlib_VrActivity_nativePopup( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint width, jint height, jfloat seconds )
{
	LOG( "%p nativePopup", (void *)appPtr );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "popup %i %i %f", width, height, seconds );
}

void Java_com_oculusvr_vrlib_VrActivity_nativeActivity( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint width, jint height, jobject activityObject )
{
	LOG( "%p nativeActivity", (void *)appPtr );
	jobject	globalObject = jni->NewGlobalRef( activityObject );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "activity %i %i %p", width, height, globalObject );
}

void Java_com_oculusvr_vrlib_VrActivity_nativeActivityHide( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p nativeActivityHide", (void *)appPtr );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "activityhide " );
}

void Java_com_oculusvr_vrlib_VrActivity_nativeActivityRefresh( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p nativeActivityRefresh", (void *)appPtr );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "activityrefresh ");
}

void Java_com_oculusvr_vrlib_PassThroughCamera_nativeSetCameraFov( JNIEnv *jni, jclass clazz,
		jlong appPtr, jfloat fovHorizontal, jfloat fovVertical )
{
	LOG( "%p nativeSetCameraFov %.2f %.2f", (void *)appPtr, fovHorizontal, fovVertical );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "camera %f %f", fovHorizontal, fovVertical );
}

jobject Java_com_oculusvr_vrlib_PassThroughCamera_nativeGetCameraSurfaceTexture( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p getCameraSurfaceTexture: %i", (void *)appPtr,
			((AppLocal *)appPtr)->GetCameraTexture()->textureId );
	return ((AppLocal *)appPtr)->GetCameraTexture()->javaObject;
}

jobject Java_com_oculusvr_vrlib_VrActivity_nativeGetPopupSurfaceTexture( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p getPopUpSurfaceTexture: %i", (void *)appPtr,
			((AppLocal *)appPtr)->GetDialogTexture()->textureId );
	return ((AppLocal *)appPtr)->GetDialogTexture()->javaObject;
}

jobject Java_com_oculusvr_vrlib_VrActivity_nativeGetActivitySurfaceTexture( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p getActivitySurfaceTexture: %i", (void *)appPtr,
			((AppLocal *)appPtr)->GetActivityPanel().Texture->textureId );
	return ((AppLocal *)appPtr)->GetActivityPanel().Texture->javaObject;
}

void Java_com_oculusvr_vrlib_VrActivity_nativePause( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p Java_com_oculusvr_vrlib_VrActivity_nativePause", (void *)appPtr );
		((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "pause " );
}

void Java_com_oculusvr_vrlib_VrActivity_nativeResume( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p Java_com_oculusvr_vrlib_VrActivity_nativeResume", (void *)appPtr );
		((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "resume " );
}

void Java_com_oculusvr_vrlib_VrActivity_nativeDestroy( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p Java_com_oculusvr_vrlib_VrActivity_nativeDestroy", (void *)appPtr );

	AppLocal * localPtr = (AppLocal *)appPtr;
	const bool exitOnDestroy = localPtr->ExitOnDestroy;

	// First kill the VrThread.
	localPtr->StopVrThread();
	// Then delete the VrAppInterface derived class.
	delete localPtr->appInterface;
	// Last delete AppLocal.
	delete localPtr;

	// Execute ovr_Shutdown() here on the Java thread because ovr_Initialize()
	// was also called from the Java thread through JNI_OnLoad().
	if ( exitOnDestroy )
	{
		LOG( "ExitOnDestroy is true, exiting" );
		ovr_ExitActivity( NULL, EXIT_TYPE_EXIT );
	}
	else
	{
		LOG( "ExitOnDestroy was false, returning normally." );
	}
}

void Java_com_oculusvr_vrlib_VrActivity_nativeJoypadAxis( JNIEnv *jni, jclass clazz,
		jlong appPtr, jfloat lx, jfloat ly, jfloat rx, jfloat ry )
{
	AppLocal * local = ((AppLocal *)appPtr);
	// Suspend input until OneTimeInit() has finished to avoid overflowing the message queue on long loads.
	if ( local->OneTimeInitCalled )
	{
		local->GetMessageQueue().PostPrintf( "joy %f %f %f %f", lx, ly, rx, ry );
	}
}

void Java_com_oculusvr_vrlib_VrActivity_nativeTouch( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint action, jfloat x, jfloat y )
{
	AppLocal * local = ((AppLocal *)appPtr);
	// Suspend input until OneTimeInit() has finished to avoid overflowing the message queue on long loads.
	if ( local->OneTimeInitCalled )
	{
		local->GetMessageQueue().PostPrintf( "touch %i %f %f", action, x, y );
	}
}

void Java_com_oculusvr_vrlib_VrActivity_nativeKeyEvent( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint key, jboolean down, jint repeatCount )
{
	AppLocal * local = ((AppLocal *)appPtr);
	// Suspend input until OneTimeInit() has finished to avoid overflowing the message queue on long loads.
	if ( local->OneTimeInitCalled )
	{
		local->GetMessageQueue().PostPrintf( "key %i %i %i", key, down, repeatCount );
	}
}

void Java_com_oculusvr_vrlib_VrActivity_nativeVolumeChanged( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint current, jint max )
{
	AppLocal * local = ((AppLocal *)appPtr);
	// Suspend input until OneTimeInit() has finished to avoid overflowing the message queue on long loads.
	if ( local->OneTimeInitCalled )
	{
		local->GetMessageQueue().PostPrintf( "volume %d %d", current, max );
	}
}

void Java_com_oculusvr_vrlib_VrActivity_nativeNewIntent( JNIEnv *jni, jclass clazz,
		jlong appPtr, jstring path )
{
	const char * cpath = jni->GetStringUTFChars( path, NULL );
	LOG( "%p nativeNewIntent: %s", (void *)appPtr, cpath );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "intent %s", cpath );
	jni->ReleaseStringUTFChars(path, cpath);
}

}	// extern "C"

//=======================================================================================
// Default handlers for VrAppInterface

VrAppInterface::VrAppInterface() :
	app( NULL ),
	ActivityClass( NULL )
{

}

VrAppInterface::~VrAppInterface()
{
	if ( ActivityClass != NULL )
	{
		// FIXME:
		//jni->DeleteGlobalRef( ActivityClass );
		//ActivityClass = NULL;
	}
}

jlong VrAppInterface::SetActivity( JNIEnv * jni, jclass clazz, jobject activity )
{
	LOG( "VrAppInterface::SetActivity" );
	// Make a permanent global reference for the class
	if ( ActivityClass != NULL )
	{
		jni->DeleteGlobalRef( ActivityClass );
	}
	ActivityClass = (jclass)jni->NewGlobalRef( clazz );

	// fetch the launch intent
	const jmethodID method = jni->GetMethodID( clazz,
			"getLaunchIntent", "()Ljava/lang/String;" );
	if ( method == NULL )
	{
		FAIL( "getLaunchIntent not found" );
	}
	jstring jString = (jstring)jni->CallObjectMethod( activity, method );
	const char * cpath = jni->GetStringUTFChars( jString, NULL );
	LOG( "launchIntent: %s", cpath );
	String launchIntent( cpath );
	jni->ReleaseStringUTFChars( jString, cpath );
	jni->DeleteLocalRef( jString );

	if ( app == NULL )
	{	// First time initialization
		// This will set the VrAppInterface app pointer directly,
		// so it is set when OneTimeInit is called.
		LOG( "new AppLocal( %p %p %p )", jni, activity, this );
		new AppLocal( *jni, activity, *this );

		// Start the VrThread and wait for it to have initialized.
		static_cast< AppLocal * >( app )->StartVrThread();
		static_cast< AppLocal * >( app )->SyncVrThread();
	}
	else
	{	// Just update the activity object.
		LOG( "Update AppLocal( %p %p %p )", jni, activity, this );
		if ( static_cast< AppLocal * >( app )->javaObject != NULL )
		{
			jni->DeleteGlobalRef( static_cast< AppLocal * >( app )->javaObject );
		}
		static_cast< AppLocal * >( app )->javaObject = jni->NewGlobalRef( activity );
		static_cast< AppLocal * >( app )->VrModeParms.ActivityObject = static_cast< AppLocal * >( app )->javaObject;
	}

	// Send the intent and wait for it to complete.
	static_cast< AppLocal * >( app )->GetMessageQueue().PostPrintf( "intent %s", launchIntent.ToCStr() );
	static_cast< AppLocal * >( app )->SyncVrThread();

	return (jlong)app;
}

void VrAppInterface::OneTimeInit( const char * launchIntent )
{
	DROID_ASSERT( !"OneTimeInit() is not overloaded!", "VrApp" );	// every native app must overload this. Why isn't it pure virtual?
}

void VrAppInterface::OneTimeShutdown()
{
}

void VrAppInterface::WindowCreated()
{
	LOG( "VrAppInterface::WindowCreated - default handler called" );
}

void VrAppInterface::WindowDestroyed()
{
	LOG( "VrAppInterface::WindowDestroyed - default handler called" );
}

void VrAppInterface::Command( const char * msg )
{
	LOG( "VrAppInterface::Command - default handler called, msg = '%s'", msg );
}

void VrAppInterface::NewIntent( const char * intent )
{
	LOG( "VrAppInterface::NewIntent - default handler called" );
}

Matrix4f VrAppInterface::Frame( VrFrame vrFrame )
{
	LOG( "VrAppInterface::Frame - default handler called" );
	return Matrix4f();
}

void VrAppInterface::ConfigureVrMode( ovrModeParms & modeParms )
{ 
	LOG( "VrAppInterface::ConfigureVrMode - default handler called" );
}

Matrix4f VrAppInterface::DrawEyeView( const int eye, const float fovDegrees ) 
{ 
	LOG( "VrAppInterface::DrawEyeView - default handler called" );
	return Matrix4f(); 
}

bool VrAppInterface::OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType )
{ 
	LOG( "VrAppInterface::OnKeyEvent - default handler called" );
	return false; 
}

bool VrAppInterface::OnVrWarningDismissed( const bool accepted )
{
	LOG( "VrAppInterface::OnVrWarningDismissed - default handler called" );
	return false;
}

bool VrAppInterface::ShouldShowLoadingIcon() const
{
	return true;
}

bool VrAppInterface::GetWantSrgbFramebuffer() const
{
	return false;
}

//==============================
// WaitForDebuggerToAttach
//
// wait on the debugger... once it is attached, change waitForDebugger to false
void WaitForDebuggerToAttach()
{
	static volatile bool waitForDebugger = true;
	while ( waitForDebugger )
	{
		// put your breakpoint on the usleep to wait
		usleep( 100000 );
	}
}

//=======================================================================================

/*
 * AppLocal
 *
 * Called once at startup.
 *
 * ?still true?: exit() from here causes an immediate app re-launch,
 * move everything to first surface init?
 */
AppLocal::AppLocal( JNIEnv & jni_, jobject activityObject_, VrAppInterface & interface_ ) :
			ExitOnDestroy( true ),
			OneTimeInitCalled( false ),
			VrThreadSynced( false ),
			CreatedSurface( false ),
			ReadyToExit( false ),
			vrMessageQueue( 100 ),
			OvrMobile( NULL ),
			EyeTargets( NULL ),
			javaVM( VrLibJavaVM ),
			UiJni( &jni_ ),
			VrJni( NULL ),
			javaObject( NULL ),
			vrActivityClass( NULL ),
			PlatformActivityClass( NULL ),
			passThroughCameraClass( NULL ),
			VrLibClass( NULL ),
			enableCameraPreviewMethodId( NULL ),
			disableCameraPreviewMethodId( NULL ),
			startExposureLockMethodId( NULL ),
			finishActivityMethodId( NULL ),
			createVrToastMethodId( NULL ),
			clearVrToastsMethodId( NULL ),
			playSoundPoolSoundMethodId( NULL ),
			sendIntentMethodId( NULL ),
			gazeEventMethodId( NULL ),
			setSysBrightnessMethodId( NULL ),
			getSysBrightnessMethodId( NULL ),
			enableComfortViewModeMethodId( NULL ),
			getComfortViewModeMethodId( NULL ),
			isPackageInstalledMethodId( NULL ),
			setDoNotDisturbModeMethodId( NULL ),
			getDoNotDisturbModeMethodId( NULL ),
			getBluetoothEnabledMethodId( NULL ),
			isAirplaneModeEnabledMethodId( NULL ),
			PassThroughCameraObject( NULL ),
			launchIntent( NULL ),
			packageCodePath( NULL ),
			demoMode( false ),
			Paused( true ),
			popupDistance( 2.0f ),
			popupScale( 1.0f ),
			appInterface( NULL ),
			dialogTexture( NULL ),
			cameraTexture( NULL ),
			lastCameraUpdateTime( 0 ),
			enableCameraTimeWarp( 2 ),
			cameraLatency( 0.030f ),
			cameraRollTime( 0.008f ),	// 120 fps
			dialogWidth( 0 ),
			dialogHeight( 0 ),
			dialogStopSeconds( 0.0f ),
			cameraFovHorizontal( 0.0f ),
			cameraFovVertical( 0.0f ),
			nativeWindow( NULL ),
			windowSurface( EGL_NO_SURFACE ),
			drawCalibrationLines( false ),
			calibrationLinesDrawn( false ),
			showVignette( true ),
			FramebufferIsSrgb( false ),
			renderMonoMode( false ),
			VrThreadTid( 0 ),
			PassThroughCameraEnabled( false ),
			EnablePassThroughCameraOnResume( false ),
			BatteryLevel( 0 ),
			BatteryStatus( BATTERY_STATUS_UNKNOWN ),
			ShowFPS( false ),
			ShowVolumePopup( true ),
			InfoTextEndFrame( -1 ),
			touchpadTimer( 0.0f ),
			lastTouchpadTime( 0.0f ),
			lastTouchDown( false ),
			touchState( 0 ),
			enableDebugOptions( false ),
			GuiSys( NULL ),
			GazeCursor( NULL ),
			DefaultFont( NULL ),
			WorldFontSurface( NULL ),
			MenuFontSurface( NULL ),
			VRMenuMgr( NULL ),
			VolumePopup( NULL ),
			DebugLines( NULL ),
			BackKeyState( 0.25f, 0.75f )
{
	LOG( "----------------- AppLocal::AppLocal() -----------------");

	//WaitForDebuggerToAttach();

	memset( & cameraFramePose, 0, sizeof( cameraFramePose ) );
	memset( & SensorForNextWarp, 0, sizeof( SensorForNextWarp ) );

	cameraFramePose[0].Predicted.Pose.Orientation = Quatf();
	cameraFramePose[1].Predicted.Pose.Orientation = Quatf();
	SensorForNextWarp.Predicted.Pose.Orientation = Quatf();

	ovr_LoadDevConfig( false );

	// Default vrParms
	vrParms.resolution = 1024;
	vrParms.multisamples = 4;
	vrParms.colorFormat = COLOR_8888;
	vrParms.depthFormat = DEPTH_24;

	// Default vrModeParms
	VrModeParms.AsynchronousTimeWarp = true;
	VrModeParms.AllowPowerSave = true;
	VrModeParms.DistortionFileName = NULL;
	VrModeParms.EnableImageServer = false;
	VrModeParms.CpuLevel = 2;
	VrModeParms.GpuLevel = 2;
	VrModeParms.GameThreadTid = 0;

	javaObject = UiJni->NewGlobalRef( activityObject_ );
	launchIntent = "";	// fixme?

	// A difficulty with JNI is that we can't resolve our (non-Android) package
	// classes on other threads, so lookup everything we need right now.
	vrActivityClass = GetGlobalClassReference( activityClassName );
	PlatformActivityClass = GetGlobalClassReference( platformActivityClassName );
	VrLibClass = GetGlobalClassReference( vrLibClassName );
	passThroughCameraClass = GetGlobalClassReference( passThroughCameraClassName );

	finishActivityMethodId = GetMethodID( "finishActivity", "()V" );
	createVrToastMethodId = GetMethodID( "createVrToastOnUiThread", "(Ljava/lang/String;)V" );
	clearVrToastsMethodId = GetMethodID( "clearVrToasts", "()V" );
	playSoundPoolSoundMethodId = GetMethodID( "playSoundPoolSound", "(Ljava/lang/String;)V" );
	sendIntentMethodId = GetMethodID( "sendIntent", "(Ljava/lang/String;Ljava/lang/String;)V");

	bool const isPlatformActivity = ovr_IsCurrentActivity( UiJni, javaObject, PUI_CLASS_NAME );
	ExitOnDestroy = !isPlatformActivity;
	// the passthrough camera is only on the platform UI activity right now
	if ( isPlatformActivity )
	{
		jfieldID passThroughCamFieldId = UiJni->GetStaticFieldID( PlatformActivityClass, "passThroughCam", "Lcom/oculusvr/vrlib/PassThroughCamera;" );
		jobject localObj = UiJni->GetStaticObjectField( PlatformActivityClass, passThroughCamFieldId );
		PassThroughCameraObject = UiJni->NewGlobalRef( localObj );
		UiJni->DeleteLocalRef( localObj );
	}
	else
	{
		PassThroughCameraObject = NULL;
	}

	enableCameraPreviewMethodId = GetMethodID( passThroughCameraClass, "enableCameraPreview", "(J)V" );
	disableCameraPreviewMethodId = GetMethodID( passThroughCameraClass, "disableCameraPreview", "(J)V" );
	startExposureLockMethodId = GetMethodID( passThroughCameraClass, "startExposureLock", "(JZ)V" );

	gazeEventMethodId = GetStaticMethodID( vrActivityClass, "gazeEventFromNative", "(FFZZLandroid/app/Activity;)V" );
	setSysBrightnessMethodId = GetStaticMethodID( VrLibClass, "setSystemBrightness", "(Landroid/app/Activity;I)V" );
	getSysBrightnessMethodId = GetStaticMethodID( VrLibClass, "getSystemBrightness", "(Landroid/app/Activity;)I" );
	enableComfortViewModeMethodId = GetStaticMethodID( VrLibClass, "enableComfortViewMode", "(Landroid/app/Activity;Z)V" );
	getComfortViewModeMethodId = GetStaticMethodID( VrLibClass, "getComfortViewModeEnabled", "(Landroid/app/Activity;)Z" );
	isPackageInstalledMethodId = GetStaticMethodID( vrActivityClass, "packageIsInstalled", "(Landroid/app/Activity;Ljava/lang/String;)Z" );
	setDoNotDisturbModeMethodId = GetStaticMethodID( VrLibClass, "setDoNotDisturbMode", "(Landroid/app/Activity;Z)V" );
	getDoNotDisturbModeMethodId = GetStaticMethodID( VrLibClass, "getDoNotDisturbMode", "(Landroid/app/Activity;)Z" );
	getBluetoothEnabledMethodId = GetStaticMethodID( VrLibClass, "getBluetoothEnabled", "(Landroid/app/Activity;)Z" );
	isAirplaneModeEnabledMethodId = GetStaticMethodID( VrLibClass, "isAirplaneModeEnabled", "(Landroid/app/Activity;)Z" );

	// Get the path to the .apk
	OpenApplicationPackage();

	// Hook the App and AppInterface together
	appInterface = &interface_;
	appInterface->app = this;

	// This only exists to keep the linker from optimizing the nativeSetAppInterface call away,
	// which is really only ever called from Java.
	if ( VrLibJavaVM == NULL )
	{
		Java_com_oculusvr_vrlib_PlatformActivity_nativeSetAppInterface( NULL, NULL, NULL );
	}
}

AppLocal::~AppLocal()
{
	LOG( "---------- ~AppLocal() ----------" );

	if ( PassThroughCameraObject != 0 )
	{
		UiJni->DeleteGlobalRef( PassThroughCameraObject );
	}

	if ( javaObject != 0 )
	{
		UiJni->DeleteGlobalRef( javaObject );
	}
}

void AppLocal::StartVrThread()
{
	LOG( "StartVrThread" );

	const int createErr = pthread_create( &VrThread, NULL /* default attributes */, &ThreadStarter, this );
	if ( createErr != 0 )
	{
		FAIL( "pthread_create returned %i", createErr );
	}
}

void AppLocal::StopVrThread()
{
	LOG( "StopVrThread" );

	vrMessageQueue.PostPrintf( "quit " );
	const int ret = pthread_join( VrThread, NULL );
	if ( ret != 0 )
	{
		LOG( "failed to joint VrThread (%i)", ret );
	}
}

void AppLocal::SyncVrThread()
{
	MessageQueue result( 1 );
	vrMessageQueue.PostPrintf( "sync %p", &result );
	result.SleepUntilMessage();
	const char * msg = result.GetNextMessage();
	free( (void *)msg );
	VrThreadSynced = true;
}

void AppLocal::InitFonts()
{
	DefaultFont = BitmapFont::Create();
	DefaultFont->Load( "res/raw/clearsans.fnt" );

	WorldFontSurface = BitmapFontSurface::Create();
	MenuFontSurface = BitmapFontSurface::Create();

	WorldFontSurface->Init( 8192 );
	MenuFontSurface->Init( 8192 );
}

void AppLocal::ShutdownFonts()
{
	BitmapFont::Free( DefaultFont );
	BitmapFontSurface::Free( WorldFontSurface );
	BitmapFontSurface::Free( MenuFontSurface );
}

MessageQueue & AppLocal::GetMessageQueue()
{
	return vrMessageQueue;
}

// This callback happens from the java thread, after a string has been
// pulled off the message queue
void AppLocal::TtjCommand( JNIEnv & jni, const char * commandString )
{
	if ( MatchesHead( "sound ", commandString ) )
	{
		jni.CallVoidMethod( javaObject, playSoundPoolSoundMethodId,
				jni.NewStringUTF( commandString + 6 ) );
		return;
	}

	if ( MatchesHead( "toast ", commandString ) )
	{
	    jni.CallVoidMethod( javaObject, createVrToastMethodId,
	    		jni.NewStringUTF( commandString + 6 ) );
	    return;
	}

	if ( MatchesHead( "uipanel ", commandString ) )
	{
		float	x, y;
		int		press, release;
		jobject	object;

		sscanf( commandString, "uipanel %f %f %i %i %p",
				&x, &y, &press, &release, &object );

		DROIDLOG( "cursor", "uipanel recieved: %f %f %i %i", x, y, 	press, release );

		jni.CallStaticVoidMethod( vrActivityClass, gazeEventMethodId, x, y, press, release, object );
	}

	if ( MatchesHead( "finish ", commandString ) )
	{
		jni.CallVoidMethod( javaObject, finishActivityMethodId );
	}
}

void AppLocal::CreateToast( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );

	LOG("CreateToast %s", bigBuffer);

	Ttj.GetMessageQueue().PostPrintf( "toast %s", bigBuffer );
}

void AppLocal::PlaySound( const char * name )
{
	// Get sound from SoundManager
	String soundFile;
	
	if ( SoundManager.GetSound( name, soundFile ) )
	{
		// Run on the talk to java thread
		Ttj.GetMessageQueue().PostPrintf( "sound %s", soundFile.ToCStr() );
	}
	else
	{
		LOG( "AppLocal::PlaySound called with non SoundManager defined sound: %s", name );
		// Run on the talk to java thread
		Ttj.GetMessageQueue().PostPrintf( "sound %s", name );
	}
}

void AppLocal::SendIntent( const char * package, const char * data )
{
	LOG( "SendIntent from '%s'", packageCodePath );
	LOG( "SendIntent to '%s' %s", package, data );

	jobject string1Object = VrJni->NewStringUTF( package );
	jobject string2Object = VrJni->NewStringUTF( data );

	VrJni->CallVoidMethod( javaObject, sendIntentMethodId, string1Object, string2Object );

	// We are not regularly returning to java, so we need to delete
	// the local string reference explicitly.
	VrJni->DeleteLocalRef( string1Object );
	VrJni->DeleteLocalRef( string2Object );

	// Do not call finishAffinity() on for Home - 
	// we want the platformActivity to shutdown but not
	// Home itself.
	//OVR_ASSERT( !ovr_IsOculusHomePackage( UiJni, javaObject ) );

	ovr_ExitActivity( OvrMobile, EXIT_TYPE_FINISH_AFFINITY );
}

void AppLocal::ReturnToLauncher()
{
	LOG( "ReturnToLauncher" );

	if ( !ovr_IsOculusHomePackage( VrJni, javaObject ) )
	{
		SendIntent( GetDashboardPackageName(), "" );
	}
	else
	{
		ExitPlatformUI();
	}
}

void AppLocal::ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer )
{
	ovr_ReadFileFromApplicationPackage( nameInZip, length, buffer );
}

void AppLocal::OpenApplicationPackage()
{
	jmethodID getPackageCodePathId = GetMethodID( "getPackageCodePath", "()Ljava/lang/String;" );
	jobject result = UiJni->CallObjectMethod( javaObject, getPackageCodePathId );

	const char * cpath = UiJni->GetStringUTFChars( (jstring)result, NULL );
	LOG( "Package code path: %s", cpath );
	packageCodePath = strdup( cpath );
	UiJni->ReleaseStringUTFChars(( jstring)result, cpath );

	ovr_OpenApplicationPackage( packageCodePath );
}

// Error checks and exits on failure
jmethodID AppLocal::GetMethodID( const char * name, const char * signature )
{
	jmethodID mid = UiJni->GetMethodID( vrActivityClass, name, signature );
	if ( !mid )
	{
		FAIL( "couldn't get %s", name );
	}
	return mid;
}

jmethodID AppLocal::GetMethodID( jclass clazz, const char * name, const char * signature )
{
	jmethodID mid = UiJni->GetMethodID( clazz, name, signature );
	if ( !mid )
	{
		FAIL( "couldn't get %s", name );
	}
	return mid;
}

jmethodID AppLocal::GetStaticMethodID( jclass cls, const char * name, const char * signature )
{
	jmethodID mid = UiJni->GetStaticMethodID( cls, name, signature );
	if ( !mid )
	{
		FAIL( "couldn't get %s", name );
	}

	return mid;
}

void AppLocal::PassThroughCamera( const bool enable )
{
	if ( PassThroughCameraObject == NULL )
	{
		// currently this may happen for the main activity, which doesn't have a passthrough camera object, so ignore it
		return;
	}

	LOG( "PassThroughCamera( %i )", enable );
	PassThroughCameraEnabled = enable;
	
	if ( enable )
	{
		VrJni->CallVoidMethod( PassThroughCameraObject, enableCameraPreviewMethodId, ( long long )this );
	}
	else
	{
		VrJni->CallVoidMethod( PassThroughCameraObject, disableCameraPreviewMethodId, ( long long )this );
	}
}

void AppLocal::PassThroughCameraExposureLock( const bool enable )
{
	LOG( "PassThroughCameraExposureLock( %i )", enable );
	VrJni->CallStaticVoidMethod( passThroughCameraClass, startExposureLockMethodId );
}

jclass AppLocal::GetGlobalClassReference( const char * className ) const
{
	jclass lc = UiJni->FindClass(className);
	if ( lc == 0 )
	{
		FAIL( "FindClass( %s ) failed", className );
	}
	// Turn it into a global ref, so we can safely use it in the VR thread
	jclass gc = (jclass)UiJni->NewGlobalRef( lc );

	UiJni->DeleteLocalRef( lc );

	return gc;
}

// Called only from VrThreadFunction()
void AppLocal::InitVrThread()
{
	LOG( "AppLocal::InitVrThread()" );

	// Get the tid for setting the scheduler
	VrThreadTid = gettid();

	// The Java VM needs to be attached on each thread that will use
	// it.  We need it to call UpdateTexture on surfaceTextures, which
	// must be done on the thread with the openGL context that created
	// the associated texture object current.
	LOG( "javaVM->AttachCurrentThread" );
	const jint rtn = javaVM->AttachCurrentThread( &VrJni, 0 );
	if ( rtn != JNI_OK )
	{
		FAIL( "javaVM->AttachCurrentThread returned %i", rtn );
	}

	// Set up another thread for making longer-running java calls
	// to avoid hitches.
	Ttj.Init( *javaVM, *this );

	// Create a new context and pbuffer surface
	const int windowDepth = 0;
	const int windowSamples = 0;
	const GLuint contextPriority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;
	eglr = EglSetup( EGL_NO_CONTEXT, GL_ES_VERSION,	// no share context,
			8,8,8, windowDepth, windowSamples, // r g b
			contextPriority );

	// Create our GL data objects
	InitGlObjects();

	EyeTargets = new EyeBuffers;
	GuiSys = new OvrGuiSysLocal;
	GazeCursor = new OvrGazeCursorLocal;
	VRMenuMgr = OvrVRMenuMgr::Create();
	DebugLines = OvrDebugLines::Create();

	// Create the SurfaceTexture for dialog rendering and the
	// camera texture for pass through viewing.
	dialogTexture = new SurfaceTexture( VrJni );
	cameraTexture = new SurfaceTexture( VrJni );
	activityPanel.Texture = new SurfaceTexture( VrJni );

	InitFonts();

	SoundManager.LoadSoundAssets();

	GetDebugLines().Init();

	GetGazeCursor().Init();

	GetVRMenuMgr().Init();

	GetGuiSys().Init( this, GetVRMenuMgr(), *DefaultFont );

	VolumePopup = OvrVolumePopup::Create( this, GetVRMenuMgr(), *DefaultFont );

	lastTouchpadTime = ovr_GetTimeInSeconds();

#if !defined( DELAYED_ONE_TIME_INIT )
	LOG( "launchIntent: %s", launchIntent );
	appInterface->OneTimeInit( launchIntent );
	OneTimeInitCalled = true;
#endif
}

// Called only from VrThreadFunction()
void AppLocal::ShutdownVrThread()
{
	LOG( "AppLocal::ShutdownVrThread()" );

	appInterface->OneTimeShutdown();

	GetGuiSys().Shutdown( GetVRMenuMgr() );

	GetVRMenuMgr().Shutdown();

	GetGazeCursor().Shutdown();

	GetDebugLines().Shutdown();

	ShutdownFonts();

	delete dialogTexture;
	dialogTexture = NULL;

	delete cameraTexture;
	cameraTexture = NULL;

	delete activityPanel.Texture;
	activityPanel.Texture = NULL;

	delete EyeTargets;
	EyeTargets = NULL;

	delete GuiSys;
	GuiSys = NULL;

	delete GazeCursor;
	GazeCursor = NULL;

	OvrVRMenuMgr::Free( VRMenuMgr );
	OvrDebugLines::Free( DebugLines );

	ShutdownGlObjects();

	EglShutdown( eglr );

	// Detach from the Java VM before exiting.
	LOG( "javaVM->DetachCurrentThread" );
	const jint rtn = javaVM->DetachCurrentThread();
	if ( rtn != JNI_OK )
	{
		LOG( "javaVM->DetachCurrentThread returned %i", rtn );
	}
}

static EyeParms DefaultVrParmsForRenderer( const eglSetup_t & eglr )
{
	EyeParms vrParms;

	vrParms.resolution = 1024;
	vrParms.multisamples = ( eglr.gpuType == GPU_TYPE_ADRENO_330 ) ? 2 : 4;
	vrParms.colorFormat = COLOR_8888;
	vrParms.depthFormat = DEPTH_24;

	return vrParms;
}

static bool ChromaticAberrationCorrection( const eglSetup_t & eglr )
{
	return ( eglr.gpuType & GPU_TYPE_ADRENO ) != 0 && ( eglr.gpuType >= GPU_TYPE_ADRENO_420 );
}

static const char* vertexShaderSource =
		"uniform mat4 Mvpm;\n"
		"uniform mat4 Texm;\n"
		"attribute vec4 Position;\n"
		"attribute vec4 VertexColor;\n"
		"attribute vec2 TexCoord;\n"
		"uniform mediump vec4 UniformColor;\n"
		"varying  highp vec2 oTexCoord;\n"
		"varying  lowp vec4 oColor;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = vec2( Texm * vec4(TexCoord,1,1) );\n"
		"   oColor = VertexColor * UniformColor;\n"
		"}\n";


// Interpolate between two different matrix based on the T texcoord
static const char* interpolatedVertexShaderSource =
		"uniform mat4 Mvpm;\n"
		"uniform mat4 Texm;\n"
		"attribute vec4 Position;\n"
		"attribute vec4 VertexColor;\n"
		"attribute vec2 TexCoord;\n"
		"uniform mediump vec4 UniformColor;\n"
		"varying  highp vec2 oTexCoord;\n"
		"varying  lowp vec4 oColor;\n"
		"void main()\n"
		"{\n"
	    "   vec4 top = Texm * Position;\n"
	    "   vec4 bottom = Mvpm * Position;\n"
	    "   gl_Position = mix( bottom, top, TexCoord.y );\n"
		"   oTexCoord = TexCoord;\n"
		"   oColor = VertexColor * UniformColor;\n"
		"}\n";


/*
 * InitGlObjects
 *
 * Call once a GL context is created, either by us or a host engine.
 * The Java VM must be attached to this thread to allow SurfaceTexture
 * creation.
 */
void AppLocal::InitGlObjects()
{
	vrParms = DefaultVrParmsForRenderer( eglr );

	SwapParms.WarpProgram = ChromaticAberrationCorrection( eglr ) ? WP_CHROMATIC : WP_SIMPLE;

	// Let glUtils look up extensions
	GL_FindExtensions();

	externalTextureProgram2 = BuildProgram( vertexShaderSource, externalFragmentShaderSource );
	interpolatedCameraWarp = BuildProgram( interpolatedVertexShaderSource, externalFragmentShaderSource );
	untexturedMvpProgram = BuildProgram(
		"uniform mat4 Mvpm;\n"
		"attribute vec4 Position;\n"
		"uniform mediump vec4 UniformColor;\n"
		"varying  lowp vec4 oColor;\n"
		"void main()\n"
		"{\n"
			"   gl_Position = Mvpm * Position;\n"
			"   oColor = UniformColor;\n"
		"}\n"
	,
		"varying lowp vec4	oColor;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = oColor;\n"
		"}\n"
	 );
	untexturedScreenSpaceProgram = BuildProgram( identityVertexShaderSource, untexturedFragmentShaderSource );

	// Build some geometries we need
	panelGeometry = BuildTesselatedQuad( 32, 16 );	// must be large to get faded edge
	unitSquare = BuildTesselatedQuad( 1, 1 );
	unitCubeLines = BuildUnitCubeLines();

	EyeDecorations.Init();
}

void AppLocal::ShutdownGlObjects()
{
	DeleteProgram( externalTextureProgram2 );
	DeleteProgram( interpolatedCameraWarp );
	DeleteProgram( untexturedMvpProgram );
	DeleteProgram( untexturedScreenSpaceProgram );

	panelGeometry.Free();
	unitSquare.Free();
	unitCubeLines.Free();

	EyeDecorations.Shutdown();
}

bool App::MatchesHead( const char * head, const char * check )
{
	const int l = strlen( head );
	return 0 == strncmp( head, check, l );
}

Vector3f ViewOrigin( const Matrix4f & view )
{
	return Vector3f( view.M[0][3], view.M[1][3], view.M[2][3] );
}

Vector3f ViewForward( const Matrix4f & view )
{
	return Vector3f( -view.M[0][2], -view.M[1][2], -view.M[2][2] );
}

Vector3f ViewUp( const Matrix4f & view )
{
	return Vector3f( view.M[0][1], view.M[1][1], view.M[2][1] );
}

Vector3f ViewRight( const Matrix4f & view )
{
	return Vector3f( view.M[0][0], view.M[1][0], view.M[2][0] );
}

void AppLocal::SetVrModeParms( ovrModeParms parms )
{
	if ( OvrMobile )
	{
		ovr_LeaveVrMode( OvrMobile );
		VrModeParms = parms;
		OvrMobile = ovr_EnterVrMode( VrModeParms, &hmdInfo );
	}
	else
	{
		VrModeParms = parms;
	}
}

void AppLocal::Pause()
{
	// always stop the camera on pause
	EnablePassThroughCameraOnResume = IsPassThroughCameraEnabled();
	PassThroughCamera( false );

	ovr_LeaveVrMode( OvrMobile );
}

/*
 * On startup, the resume message happens before our window is created, so
 * it needs to be deferred until after the create so we know the window
 * dimensions for HmdInfo.
 *
 * On pressing the power button, pause/resume happens without destroying
 * the window.
 */
void AppLocal::Resume()
{
	// always reload the dev config on a resume
	ovr_LoadDevConfig( true );

	// Make sure the window surface is current, which it won't be
	// if we were previously in async mode
	// (Not needed now?)
	if ( eglMakeCurrent( eglr.display, windowSurface, windowSurface, eglr.context ) == EGL_FALSE )
	{
		FAIL( "eglMakeCurrent failed: %s", EglErrorString() );
	}

	VrModeParms.ActivityObject = javaObject;

	// Allow the app to override
	appInterface->ConfigureVrMode( VrModeParms );

	// Reload local preferences, in case we are coming back from a
	// switch to the dashboard that changed them.
	ovr_UpdateLocalPreferences();

	// Check for values that effect our mode settings
	{
		const char * imageServerStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_IMAGE_SERVER, "0" );
		VrModeParms.EnableImageServer = ( atoi( imageServerStr ) > 0 );

		const char * cpuLevelStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_CPU_LEVEL, "-1" );
		const int cpuLevel = atoi( cpuLevelStr );
		if ( cpuLevel >= 0 )
		{
			VrModeParms.CpuLevel = cpuLevel;
			LOG( "Local Preferences: Setting cpuLevel %d", VrModeParms.CpuLevel );
		}
		const char * gpuLevelStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_GPU_LEVEL, "-1" );
		const int gpuLevel = atoi( gpuLevelStr );
		if ( gpuLevel >= 0 )
		{
			VrModeParms.GpuLevel = gpuLevel;
			LOG( "Local Preferences: Setting gpuLevel %d", VrModeParms.GpuLevel );
		}

		const char * showVignetteStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_SHOW_VIGNETTE, "1" );
		showVignette = ( atoi( showVignetteStr ) > 0 );

		const char * enableGpuTimingsStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_GPU_TIMINGS, "0" );
		SetAllowGpuTimerQueries( atoi( enableGpuTimingsStr ) > 0 );

		const char * enableDebugOptionsStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_DEBUG_OPTIONS, "0" );
		enableDebugOptions =  ( atoi( enableDebugOptionsStr ) > 0 );
	}

	// Clear cursor trails
	GetGazeCursor().HideCursorForFrames( 10 );

	// Start up TimeWarp and the various performance options
	OvrMobile = ovr_EnterVrMode( VrModeParms, &hmdInfo );

	if ( EnablePassThroughCameraOnResume )
	{
		PassThroughCamera( true );
	}
	EnablePassThroughCameraOnResume = false;
}

// Always make the panel upright, even if the head was tilted when created
Matrix4f PanelMatrix( const Matrix4f & lastViewMatrix, const float popupDistance,
		const float popupScale, const int width, const int height )
{
	// TODO: this won't be valid until a frame has been rendered
	const Matrix4f invView = lastViewMatrix.Inverted();
	const Vector3f forward = ViewForward( invView );
	const Vector3f levelforward = Vector3f( forward.x, 0.0f, forward.z ).Normalized();
	// TODO: check degenerate case
	const Vector3f up( 0.0f, 1.0f, 0.0f );
	const Vector3f right = levelforward.Cross( up );

	const Vector3f center = ViewOrigin( invView ) + levelforward * popupDistance;
	const float xScale = (float)width / 768.0f * popupScale;
	const float yScale = (float)height / 768.0f * popupScale;
	const Matrix4f panelMatrix = Matrix4f(
			xScale * right.x, yScale * up.x, forward.x, center.x,
			xScale * right.y, yScale * up.y, forward.y, center.y,
			xScale * right.z, yScale * up.z, forward.z, center.z,
			0, 0, 0, 1 );

//	LOG( "PanelMatrix center: %f %f %f", center.x, center.y, center.z );
//	LogMatrix( "PanelMatrix", panelMatrix );

	return panelMatrix;
}

/*
 * Command
 *
 * Process commands sent over the message queue for the VR thread.
 *
 */
void AppLocal::Command( const char *msg )
{
	// Always include the space in MatchesHead to prevent problems
	// with commands that have matching prefixes.

	if ( MatchesHead( "joy ", msg ) )
	{
		sscanf( msg, "joy %f %f %f %f",
				&joypad.sticks[0][0],
				&joypad.sticks[0][1],
				&joypad.sticks[1][0],
				&joypad.sticks[1][1] );
		return;
	}

	if ( MatchesHead( "touch ", msg ) )
	{
		int	action;
		sscanf( msg, "touch %i %f %f",
				&action,
				&joypad.touch[0],
				&joypad.touch[1] );
		if ( action == 0 )
		{
			joypad.buttonState |= BUTTON_TOUCH;
		}
		if ( action == 1 )
		{
			joypad.buttonState &= ~BUTTON_TOUCH;
		}
		return;
	}

	if ( MatchesHead( "key ", msg ) )
	{
		int	key, down, repeatCount;
		sscanf( msg, "key %i %i %i", &key, &down, &repeatCount );
		KeyEvent( key, down, repeatCount );
		// We simply return because KeyEvent will call VrAppInterface->OnKeyEvent to give the app a 
		// chance to handle and consume the key before VrLib gets it. VrAppInterface needs to get the
		// key first and have a chance to consume it completely because keys are context sensitive 
		// and only the app interface can know the current context the key should apply to. For
		// instance, the back key backs out of some current state in the app -- but only the app knows
		// whether or not there is a state to back out of at all. If we were to fall through here, the 
		// "key " message will be sent to VrAppInterface->Command() but only after VrLib has handled 
		// and or consumed the key first, which would break the back key behavior, and probably anything
		// else, like input into an edit control.
		return;	
	}

	if ( MatchesHead( "camera ", msg ) )
	{
		float hfov;
		float vfov;
		sscanf( msg, "camera %f %f", &hfov, &vfov );
		SetCameraFovHorizontal( hfov );
		SetCameraFovVertical( vfov );
		return;
	}

	if ( MatchesHead( "createdSurface ", msg ) )
	{
		LOG( "%s", msg );
		if ( windowSurface != EGL_NO_SURFACE )
		{	// Samsung says this is an Android problem, where surfaces are reported as
			// created multiple times.
			LOG( "Skipping create work because window hasn't been destroyed." );
			return;
		}
		sscanf( msg, "createdSurface %p", &nativeWindow );

		// Optionally force the window to a different resolution, which
		// will be automatically scaled up by the HWComposer.
		//
		//ANativeWindow_setBuffersGeometry( nativeWindow, 1920, 1080, 0 );

		// Set the colorspace on the window
		windowSurface = EGL_NO_SURFACE;
		if ( appInterface->GetWantSrgbFramebuffer() )
		{
			// Android doesn't let the non-standard extensions show up in the
			// extension string, so we need to try it blind.
			const EGLint attribs[] =
			{
				EGL_GL_COLORSPACE_KHR,  EGL_GL_COLORSPACE_SRGB_KHR,
				EGL_NONE
			};
			windowSurface = eglCreateWindowSurface( eglr.display, eglr.config,
					nativeWindow, attribs );
		}
		if ( windowSurface == EGL_NO_SURFACE )
		{
			FramebufferIsSrgb = false;
			// try again without EGL_GL_COLORSPACE_KHR
			const EGLint attribs2[] =
			{
				EGL_NONE
			};
			windowSurface = eglCreateWindowSurface( eglr.display, eglr.config,
					nativeWindow, attribs2 );
			if ( windowSurface == EGL_NO_SURFACE )
			{
				FAIL( "eglCreateWindowSurface failed: %s", EglErrorString() );
			}
		}
		else
		{
			FramebufferIsSrgb = true;
		}
		LOG( "NativeWindow %p gives surface %p", nativeWindow, windowSurface );
		LOG( "FramebufferIsSrgb: %s", FramebufferIsSrgb ? "true" : "false" );

		if ( eglMakeCurrent( eglr.display, windowSurface, windowSurface, eglr.context ) == EGL_FALSE )
		{
			FAIL( "eglMakeCurrent failed: %s", EglErrorString() );
		}

		CreatedSurface = true;

		// Let the client app setup now
		appInterface->WindowCreated();

		// Resume
		if ( !Paused )
		{
			Resume();
		}
		return;
	}

	if ( MatchesHead( "destroyedSurface ", msg ) )
	{
		LOG( "destroyedSurface" );

		// Prevent execution of warp swap on this OvrMobile without a window surface.
		ovr_SurfaceDestroyed( OvrMobile );

		// Let the client app shutdown first.
		appInterface->WindowDestroyed();

		// Handle it ourselves.
		if ( eglMakeCurrent( eglr.display, eglr.pbufferSurface, eglr.pbufferSurface,
				eglr.context ) == EGL_FALSE )
		{
			FAIL( "RC_SURFACE_DESTROYED: eglMakeCurrent pbuffer failed" );
		}

		if ( windowSurface != EGL_NO_SURFACE )
		{
			eglDestroySurface( eglr.display, windowSurface );
			windowSurface = EGL_NO_SURFACE;
		}
		if ( nativeWindow != NULL )
		{
			ANativeWindow_release( nativeWindow );
			nativeWindow = NULL;
		}
		return;
	}

	if ( MatchesHead( "pause ", msg ) )
	{
		LOG( "pause" );
		if ( !Paused )
		{
			Paused = true;
			Pause();
		}
	}

	if ( MatchesHead( "resume ", msg ) )
	{
		LOG( "resume" );
		Paused = false;
		// Don't actually do the resume operations if we don't have
		// a window yet.  They will be done when the window is created.
		if ( windowSurface != EGL_NO_SURFACE )
		{
			Resume();
		}
		else
		{
			LOG( "Skipping resume because windowSurface not set yet");
		}
	}

	if ( MatchesHead( "intent ", msg ) )
	{
		appInterface->NewIntent( msg + 7 );
		return;
	}

	if ( MatchesHead( "popup ", msg ) )
	{
		int width, height;
		float seconds;
		sscanf( msg, "popup %i %i %f", &width, &height, &seconds );

		dialogWidth = width;
		dialogHeight = height;
		dialogStopSeconds = ovr_GetTimeInSeconds() + seconds;

		dialogMatrix = PanelMatrix( lastViewMatrix, popupDistance, popupScale, width, height );

		glActiveTexture( GL_TEXTURE0 );
		LOG( "RC_UPDATE_POPUP dialogTexture %i", dialogTexture->textureId );
		dialogTexture->Update();
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );

		return;
	}

	if ( MatchesHead( "activity ", msg ) )
	{
		sscanf( msg, "activity %i %i %p", &activityPanel.Width, &activityPanel.Height, &activityPanel.Object );

		activityPanel.Matrix = PanelMatrix( lastViewMatrix, popupDistance, popupScale, activityPanel.Width, activityPanel.Height );

		glActiveTexture( GL_TEXTURE0 );
		LOG( "RC_UPDATE_ACTIVITY activityTexture %i", activityPanel.Texture->textureId );
		activityPanel.Texture->Update();
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );

		activityPanel.Visible = true;

		return;
	}

	if ( MatchesHead( "activityhide ", msg ) )
	{
		activityPanel.Visible = false;
	}

	if ( MatchesHead( "activityrefresh ", msg ) )
	{
		glActiveTexture( GL_TEXTURE0 );
		LOG( "RC_UPDATE_ACTIVITY activityTexture %i", activityPanel.Texture->textureId );
		activityPanel.Texture->Update();
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );

		return;
	}

	if ( MatchesHead( "volume ", msg ) )
	{
		if ( ShowVolumePopup )
		{
			int current, max;
			sscanf( msg, "volume %i %i", &current, &max );
			VolumePopup->ShowVolume( this, current, max );
		}
		return;
	}

	if ( AppLocal::MatchesHead( "sync ", msg ) )
	{
		// Let AppLocal know the VrThread is initialized.
		MessageQueue * receiver;
		sscanf( msg, "sync %p", &receiver );
		receiver->PostPrintf( "ack" );
		return;
	}

	if ( AppLocal::MatchesHead( "quit ", msg ) )
	{
		ovr_LeaveVrMode( OvrMobile );
		ReadyToExit = true;
		LOG( "VrThreadSynced=%d CreatedSurface=%d ReadyToExit=%d", VrThreadSynced, CreatedSurface, ReadyToExit );
	}

	// Pass it on to the client app.
	appInterface->Command( msg );
}

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
	const int r = fread( buffer, 1, sizeof( buffer )-1, f );
	fclose( f );

	// Strip trailing \n that some sys files have.
	for ( int n = r ; n > 0 && buffer[n] == '\n' ; n-- )
	{
		buffer[n] = 0;
	}
	return buffer;
}

static String StripLinefeed( const OVR::String s )
{
	OVR::String copy;
	for ( int i = 0 ; i < s.GetLengthI() && s.GetCharAt( i ) != '\n' ; i++ )
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
	va_end( argptr);

	OVR::String clock = ReadSmallFile( fullPath );
	clock = StripLinefeed( clock );
	if ( clock == "" )
	{
		return -1;
	}
	return atoi( clock.ToCStr() );
}

/*
 * CreateSchedulingReport
 *
 * Display information related to CPU scheduling
 */
OVR::String CreateSchedulingReport( pthread_t warpThread )
{
	const pthread_t thisThread = pthread_self();
	int thisPolicy = 0;
	struct sched_param thisSchedParma = {};
	if ( !pthread_getschedparam( thisThread, &thisPolicy, &thisSchedParma ) )
	{
		LOG( "pthread_getschedparam() failed" );
	}

	char timeWarpSched[1024] = {};
	if ( warpThread != 0 )
	{
		int timeWarpPolicy = 0;
		struct sched_param timeWarpSchedParma = {};
		if ( pthread_getschedparam( warpThread, &timeWarpPolicy, &timeWarpSchedParma ) )
		{
			LOG( "pthread_getschedparam() failed" );
	    	snprintf( timeWarpSched, sizeof( timeWarpSched ) - 1, "???" );
		}
		else
		{
	    	snprintf( timeWarpSched, sizeof( timeWarpSched ) - 1, "%s:%i"
	    			, timeWarpPolicy == SCHED_FIFO ? "SCHED_FIFO" : "SCED_NORMAL"
	    			, timeWarpSchedParma.sched_priority );
		}
	}
	else
	{
		snprintf( timeWarpSched, sizeof( timeWarpSched ) - 1, "sync" );
	}

	char path[1024] = {};

	snprintf( path, sizeof( path ) - 1,
			"VrThread:%s:%i WarpThread:%s\n"
			, thisPolicy == SCHED_FIFO ? "SCHED_FIFO" : "SCED_NORMAL"
			, thisSchedParma.sched_priority
			, timeWarpSched );

	OVR::String	str = path;

	static const int maxCores = 8;
	for ( int core = 0 ; core < maxCores ; core++ )
	{
		snprintf( path, sizeof( path ) - 1, "/sys/devices/system/cpu/cpu%i/online", core );

		const char * current = ReadSmallFile( path );
		if ( current[0] == 0 )
		{	// no more files
			break;
		}
		const int online = atoi( current );
		if ( !online )
		{
			continue;
		}

		snprintf( path, sizeof( path ) - 1, "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_governor", core );
		OVR::String governor = ReadSmallFile( path );
		governor = StripLinefeed( governor );

		// we won't be able to read the cpu clock unless it has been chmod'd to 0666, but try anyway.
		//float curFreqGHz = ReadFreq( "/sys/devices/system/cpu/cpu%i/cpufreq/cpuinfo_cur_freq", core ) * ( 1.0f / 1000.0f / 1000.0f );
		float curFreqGHz = ReadFreq( "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_cur_freq", core ) * ( 1.0f / 1000.0f / 1000.0f );
		float minFreqGHz = ReadFreq( "/sys/devices/system/cpu/cpu%i/cpufreq/cpuinfo_min_freq", core ) * ( 1.0f / 1000.0f / 1000.0f );
		float maxFreqGHz = ReadFreq( "/sys/devices/system/cpu/cpu%i/cpufreq/cpuinfo_max_freq", core ) * ( 1.0f / 1000.0f / 1000.0f );

		snprintf( path, sizeof( path ) - 1, "cpu%i: \"%s\" %1.2f GHz (min:%1.2f max:%1.2f)\n", core, governor.ToCStr(), curFreqGHz, minFreqGHz, maxFreqGHz );
		str += path;
	}

	OVR::String governor = ReadSmallFile( "/sys/class/kgsl/kgsl-3d0/pwrscale/trustzone/governor" );
	governor = StripLinefeed( governor );

	const uint64_t gpuUnit = ( ( EglGetGpuType() & GPU_TYPE_MALI ) != 0 ) ? 1000000LL : 1000LL;
	const uint64_t gpuFreq = ReadFreq( ( ( EglGetGpuType() & GPU_TYPE_MALI ) != 0 ) ?
									"/sys/devices/14ac0000.mali/clock" :
									"/sys/class/kgsl/kgsl-3d0/gpuclk" );

	snprintf( path, sizeof( path ) - 1, "gpu: \"%s\" %3.0f MHz", governor.ToCStr(), gpuFreq * gpuUnit * ( 1.0f / 1000.0f / 1000.0f ) );
	str += path;

	return str;
}

void ToggleScreenColor()
{
	static int	color;

	color ^= 1;

	glEnable( GL_WRITEONLY_RENDERING_QCOM );
	glClearColor( color, 1-color, 0, 1 );
	glClear( GL_COLOR_BUFFER_BIT );

	// The Adreno driver has an unfortunate optimization so it doesn't
	// actually flush if all that was done was a clear.
	GL_TimedFinish( "screen toggle" );
	glDisable( GL_WRITEONLY_RENDERING_QCOM );
}

void AppLocal::InterpretTouchpad( VrInput & input )
{
	// 1) Down -> Up w/ Motion = Slide
	// 2) Down -> Up w/out Motion -> Timeout = Single Tap
	// 3) Down -> Up w/out Motion -> Down -> Timeout = Nothing
	// 4) Down -> Up w/out Motion -> Down -> Up = Double Tap
	static const float timer_finger_down = 0.3f;
	static const float timer_finger_up = 0.3f;
	static const float min_swipe_distance = 100.0f;

	float currentTime = ovr_GetTimeInSeconds();
	float deltaTime = currentTime - lastTouchpadTime;
	lastTouchpadTime = currentTime;
	touchpadTimer = touchpadTimer + deltaTime;

	bool down = false, up = false;
	bool currentTouchDown = input.buttonState & BUTTON_TOUCH;

	if ( currentTouchDown && !lastTouchDown )
	{
		//CreateToast("DOWN");
		down = true;
		touchOrigin = input.touch;
	}

	if ( !currentTouchDown && lastTouchDown )
	{
		//CreateToast("UP");
		up = true;
	}

	lastTouchDown = currentTouchDown;

	input.touchRelative = input.touch - touchOrigin;
	float touchMagnitude = input.touchRelative.Length();
	input.swipeFraction = touchMagnitude / min_swipe_distance;

	switch ( touchState )
	{
	case 0:
		//CreateToast("0 - %f", touchpadTimer);
		if ( down )
		{
			touchState = 1;
			touchpadTimer = 0.0f;
		}
		break;
	case 1:
		//CreateToast("1 - %f", touchpadTimer);
		//CreateToast("1 - %f", touchMagnitude);
		if ( touchMagnitude >= min_swipe_distance )
		{
			int dir = 0;
			if ( fabs(input.touchRelative[0]) > fabs(input.touchRelative[1]) )
			{
				if ( input.touchRelative[0] < 0 )
				{
					//CreateToast("SWIPE FORWARD");
					dir = BUTTON_SWIPE_FORWARD | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//CreateToast("SWIPE BACK");
					dir = BUTTON_SWIPE_BACK | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			else
			{
				if ( input.touchRelative[1] > 0 )
				{
					//CreateToast("SWIPE DOWN");
					dir = BUTTON_SWIPE_DOWN | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//CreateToast("SWIPE UP");
					dir = BUTTON_SWIPE_UP | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			input.buttonPressed |= dir;
			input.buttonReleased |= dir & ~BUTTON_TOUCH_WAS_SWIPE;
			input.buttonState |= dir;
			touchState = 0;
			touchpadTimer = 0.0f;
		}
		else if ( up )
		{
			if ( touchpadTimer < timer_finger_down )
			{
				touchState = 2;
				touchpadTimer = 0.0f;
			}
			else
			{
				//CreateToast("SINGLE TOUCH");
				input.buttonPressed |= BUTTON_TOUCH_SINGLE;
				input.buttonReleased |= BUTTON_TOUCH_SINGLE;
				input.buttonState |= BUTTON_TOUCH_SINGLE;
				touchState = 0;
				touchpadTimer = 0.0f;
			}
		}
		break;
	case 2:
		//CreateToast("2 - %f", touchpadTimer);
		if ( touchpadTimer >= timer_finger_up )
		{
			//CreateToast("SINGLE TOUCH");
			input.buttonPressed |= BUTTON_TOUCH_SINGLE;
			input.buttonReleased |= BUTTON_TOUCH_SINGLE;
			input.buttonState |= BUTTON_TOUCH_SINGLE;
			touchState = 0;
			touchpadTimer = 0.0f;
		}
		else if ( down )
		{
			touchState = 3;
			touchpadTimer = 0.0f;
		}
		break;
	case 3:
		//CreateToast("3 - %f", touchpadTimer);
		if ( touchpadTimer >= timer_finger_down )
		{
			touchState = 0;
			touchpadTimer = 0.0f;
		}
		else if ( up )
		{
			//CreateToast("DOUBLE TOUCH");
			input.buttonPressed |= BUTTON_TOUCH_DOUBLE;
			input.buttonReleased |= BUTTON_TOUCH_DOUBLE;
			input.buttonState |= BUTTON_TOUCH_DOUBLE;
			touchState = 0;
			touchpadTimer = 0.0f;
		}
		break;
	}
}

void AppLocal::FrameworkButtonProcessing( const VrInput & input )
{
	// Toggle calibration lines or camera preview
	bool rightTrigger = input.buttonState & BUTTON_RIGHT_TRIGGER;

	// Display tweak testing, only when holding right trigger
	if ( enableDebugOptions && rightTrigger )
	{
#if 1
		// Cycle debug options
		static int debugMode = 0;
		static int debugValue = 0;
		static const char * modeNames[] = {
			"OFF",
			"RUNNING",
			"FROZEN"
		};
		static const char * valueNames[] = {
			"VALUE_DRAW",
			"VALUE_LATENCY"
		};
		if ( input.buttonPressed & BUTTON_DPAD_UP )
		{
			debugMode = ( debugMode + 1 ) % DEBUG_PERF_MAX;
			SwapParms.DebugGraphMode = (debugPerfMode_t)debugMode;
			CreateToast( "debug graph %s: %s", modeNames[ debugMode ], valueNames[ debugValue ] );
		}
#endif

#if 1
		if ( input.buttonPressed & BUTTON_DPAD_RIGHT )
		{
			SwapParms.MinimumVsyncs = SwapParms.MinimumVsyncs > 3 ? 1 : SwapParms.MinimumVsyncs + 1;
			CreateToast( "MinimumVsyncs: %i", SwapParms.MinimumVsyncs );
		}
#endif

		if ( input.buttonPressed & BUTTON_Y )
		{	// display current scheduler state and clock rates
			//OVR::String s = CreateSchedulingReport( OvrMobile->Warp->GetWarpThread() );
			//CreateToast( "%s", s.ToCStr() );
		}

	}
}

static bool InsideUnit( const Vector2f v )
{
	return v.x > -1.0f && v.x < 1.0f && v.y > -1.0f && v.y < 1.0f;
}

/*
 * VrThreadFunction
 *
 * Continuously renders frames when active, checking for commands
 * from the main thread between frames.
 */
void AppLocal::VrThreadFunction()
{
	// Set the name that will show up in systrace
	pthread_setname_np( pthread_self(), "OVR::VrThread" );

	InitVrThread();

	// FPS counter information
	int countApplicationFrames = 0;
	double lastReportTime = ceil( ovr_GetTimeInSeconds() );

	while( !( VrThreadSynced && CreatedSurface && ReadyToExit ) )
	{
		//SPAM( "FRAME START" );

		// Process incoming messages until queue is empty
		for ( ; ; )
		{
			const char * msg = vrMessageQueue.GetNextMessage();
			if ( !msg )
			{
				break;
			}
			Command( msg );
			free( (void *)msg );
		}

		// If we don't have a surface yet, or we are paused, sleep until
		// something shows up on the message queue.
		if ( windowSurface == EGL_NO_SURFACE || Paused )
		{
			if ( !( VrThreadSynced && CreatedSurface && ReadyToExit ) )
			{
				vrMessageQueue.SleepUntilMessage();
			}
			continue;
		}

#if defined( DELAYED_ONE_TIME_INIT )
		// Let the client app initialize only once by calling OneTimeInit() when the windowSurface is valid.
		if ( !OneTimeInitCalled )
		{
			if ( appInterface->ShouldShowLoadingIcon() && !ovr_IsCurrentActivity( VrJni, javaObject, PUI_CLASS_NAME ) )
			{
				ovr_WarpSwapLoadingIcon( OvrMobile );
			}
			LOG( "launchIntent: %s", launchIntent );
			appInterface->OneTimeInit( launchIntent );
			OneTimeInitCalled = true;
		}
#endif

		// Always check for updates to the camera texture, even if it isn't active,
		// because if we don't flush them all out, we won't get an update when it
		// is turned back on.
		for ( ; ; )
		{
			cameraTexture->Update();
			if ( cameraTexture->timestamp == lastCameraUpdateTime )
			{
				break;
			}
			lastCameraUpdateTime = cameraTexture->timestamp;
			// this would be better as an interpolation from history instead of a prediction
			cameraFramePose[0] = ovrHmd_GetSensorState( OvrHmd,
									lastCameraUpdateTime * 0.000000001 - cameraLatency, true );
			cameraFramePose[1] = ovrHmd_GetSensorState( OvrHmd,
									lastCameraUpdateTime * 0.000000001 - cameraLatency + cameraRollTime, true );
		}

		// check updated battery status
		{
			batteryState_t state = ovr_GetBatteryState();
			BatteryStatus = static_cast< eBatteryStatus >( state.status );
			BatteryLevel = state.level;
		}

		// latch the current joypad state and note transitions
		vrFrame.Input = joypad;
		vrFrame.Input.buttonPressed = joypad.buttonState & ( ~lastVrFrame.Input.buttonState );
		vrFrame.Input.buttonReleased = ~joypad.buttonState & ( lastVrFrame.Input.buttonState & ~BUTTON_TOUCH_WAS_SWIPE );

		if ( lastVrFrame.Input.buttonState & BUTTON_TOUCH_WAS_SWIPE )
		{
			if ( lastVrFrame.Input.buttonReleased & BUTTON_TOUCH )
			{
				vrFrame.Input.buttonReleased |= BUTTON_TOUCH_WAS_SWIPE;
			}
			else
			{
				// keep it around this frame
				vrFrame.Input.buttonState |= BUTTON_TOUCH_WAS_SWIPE;
			}
		}

		// Synthesize swipe gestures
		InterpretTouchpad( vrFrame.Input );

		// Get the latest head tracking state, predicted ahead to the midpoint of the time
		// it will be displayed.  It will always be corrected to the real values by
		// time warp, but the closer we get, the less black will be pulled in at the edges.
		const double now = ovr_GetTimeInSeconds();
		static double prev;
		const double rawDelta = now - prev;
		prev = now;
		const double clampedPrediction = Alg::Min( 0.1, rawDelta * 2 );
		SensorForNextWarp = ovrHmd_GetSensorState(OvrHmd, now + clampedPrediction, true );

		vrFrame.PoseState = SensorForNextWarp.Predicted;
		vrFrame.OvrStatus = SensorForNextWarp.Status;
		vrFrame.DeltaSeconds   = Alg::Min( 0.1, rawDelta );
		vrFrame.FrameNumber++;

		// Don't allow this to be excessively large, which can cause application problems.
		if ( vrFrame.DeltaSeconds > 0.1f )
		{
			vrFrame.DeltaSeconds = 0.1f;
		}

		lastVrFrame = vrFrame;

		// resend any debug lines that have expired
		GetDebugLines().BeginFrame( vrFrame.FrameNumber );

		// reset any VR menu submissions from previous frame
		GetVRMenuMgr().BeginFrame();

		// Joypad latency test
		// When this is enabled, each tap of a button will toggle the screen
		// color, allowing the tap-to-photons latency to be measured.
		if ( 0 )
		{
			if ( vrFrame.Input.buttonPressed )
			{
				LOG( "Input.buttonPressed" );
				ToggleScreenColor();
			}
			vrMessageQueue.SleepUntilMessage();
			continue;
		}

		FrameworkButtonProcessing( vrFrame.Input );

		KeyState::eKeyEventType event = BackKeyState.Update( ovr_GetTimeInSeconds() );
		if ( event != KeyState::KEY_EVENT_NONE )
		{
			//LOG( "BackKey: event %s", KeyState::EventNames[ event ] );
			// always allow the gaze cursor to peek at the event so it can start the gaze timer if necessary
			{
				// update the gaze cursor timer
				if ( event == KeyState::KEY_EVENT_DOWN )
				{
					// only start the timer if the platform UI is not up
					if ( !ovr_IsCurrentActivity( VrJni, javaObject, PUI_CLASS_NAME ) )
					{
						GetGazeCursor().StartTimer( GetBackKeyState().GetLongPressTime(), GetBackKeyState().GetDoubleTapTime() );
					}
				} 
				else if ( event == KeyState::KEY_EVENT_DOUBLE_TAP || event == KeyState::KEY_EVENT_SHORT_PRESS )
				{
					GetGazeCursor().CancelTimer();
				}
				else if ( event == KeyState::KEY_EVENT_LONG_PRESS )
				{
					// start the platform UI if this isn't already the platform activity
					if ( !ovr_IsCurrentActivity( VrJni, javaObject, PUI_CLASS_NAME ) )
					{
						StartPlatformUI();
					}
				}
			}

			// let the menu handle it if it's open
			bool consumedKey = GetGuiSys().OnKeyEvent( this, AKEYCODE_BACK, event );
	
			// pass to the app if nothing handled it before this
			if ( !consumedKey )
			{				
				consumedKey = appInterface->OnKeyEvent( AKEYCODE_BACK, event );
			}
			// if nothing consumed the key and it's a short-press, exit the application to the dashboard
			if ( !consumedKey )
			{
				if ( event == KeyState::KEY_EVENT_SHORT_PRESS )
				{
					consumedKey = true;
					LOG( "BUTTON_BACK: confirming quit in platformUI" );
					ovr_StartPackageActivity( OvrMobile, PUI_CLASS_NAME, PUI_CONFIRM_QUIT );
				}
			}
		}

		// Pass gaze cursor events back to java
		//
		// If the gaze is on a UI panel, we will steal BUTTON_A from the
		// application interface.
		if ( activityPanel.Visible || activityPanel.TouchDown )
		{
			const bool press = ( vrFrame.Input.buttonState & ( BUTTON_A | BUTTON_TOUCH ) ) != 0;
			if ( press || activityPanel.TouchDown )
			{
				// -1 to 1 range on panelMatrix, returns -2,-2 if looking away from the panel
				const Vector2f	cursor = GazeCoordinatesOnPanel( lastViewMatrix, activityPanel.Matrix, activityPanel.AlternateGazeCheck );
				const bool inside = InsideUnit( cursor );
				const bool press = ( vrFrame.Input.buttonState & ( BUTTON_A | BUTTON_TOUCH ) ) != 0;

				// always release when !activityPanel.Visible
				bool release = !activityPanel.Visible || ( ( vrFrame.Input.buttonReleased & ( BUTTON_A | BUTTON_TOUCH ) ) != 0 );

				if ( press && inside )
				{
					activityPanel.TouchDown = true;
					release = false;
				}

				// Always send releases if the panel thinks it is down, otherwise
				// only send events if the gaze cursor is on the panel or on release.
				if ( activityPanel.TouchDown )
				{
					if ( inside || release )
					{
						Ttj.GetMessageQueue().PostPrintf( "uipanel %f %f %i %i %p",
								( cursor.x * 0.5 + 0.5 ) * activityPanel.Width,
								( 0.5 - cursor.y * 0.5 ) * activityPanel.Height,
								press, release, activityPanel.Object );
					}

					if ( release )
					{
						activityPanel.TouchDown = false;
					}
				}
			}
		}

		// Main loop logic / draw code
		if ( !ReadyToExit )
		{
			this->lastViewMatrix = appInterface->Frame( vrFrame );
		}

		ovr_HandleDeviceStateChanges( OvrMobile );

		// MWC demo hack to allow keyboard swipes
		joypad.buttonState &= ~(BUTTON_SWIPE_FORWARD|BUTTON_SWIPE_BACK);

		// Report frame counts once a second
		countApplicationFrames++;
		const double timeNow = floor( ovr_GetTimeInSeconds() );
		if ( timeNow > lastReportTime )
		{
			LOG( "FPS: %i GPU time: %3.1f ms ", countApplicationFrames, EyeTargets->LogEyeSceneGpuTime.GetTotalTime() );
			countApplicationFrames = 0;
			lastReportTime = timeNow;
		}

		//SPAM( "FRAME END" );
	}

	// Shut down the message queue so it cannot overflow.
	vrMessageQueue.Shutdown();

	ShutdownVrThread();
}

// Shim to call a C++ object from a posix thread start.
void *AppLocal::ThreadStarter( void * parm )
{
	((AppLocal *)parm)->VrThreadFunction();

	return NULL;
}

/*
 * GetEyeParms()
 */
EyeParms AppLocal::GetEyeParms()
{
	return vrParms;
}

/*
 * SetVrParms()
 */
void AppLocal::SetEyeParms( const EyeParms parms )
{
	vrParms = parms;
}

/*
 * KeyEvent
 */
static int buttonMappings[] = {
	96, 	// BUTTON_A
	97,		// BUTTON_B
	99, 	// BUTTON_X
	100,	// BUTTON_Y
	108, 	// BUTTON_START
	4,		// BUTTON_BACK
	109, 	// BUTTON_SELECT
	82, 	// BUTTON_MENU
	103, 	// BUTTON_RIGHT_TRIGGER
	102, 	// BUTTON_LEFT_TRIGGER
	19,		// BUTTON_DPAD_UP
	20,		// BUTTON_DPAD_DOWN
	21,		// BUTTON_DPAD_LEFT
	22,		// BUTTON_DPAD_RIGHT
	200,	// BUTTON_LSTICK_UP
	201,	// BUTTON_LSTICK_DOWN
	202,	// BUTTON_LSTICK_LEFT
	203,	// BUTTON_LSTICK_RIGHT
	204,	// BUTTON_RSTICK_UP
	205,	// BUTTON_RSTICK_DOWN
	206,	// BUTTON_RSTICK_LEFT
	207,	// BUTTON_RSTICK_RIGHT
	9999,	// touch is handled separately

	-1
};

static const char * ParseToSemi( FILE * f )
{
	String	s;

	for( ;; )
	{
		const int c = fgetc( f );
		if ( c == EOF || c == ';' )
		{
			break;
		}
		if ( c < ' ' )
		{	// ignore extra whitespace
			continue;
		}
		s.AppendChar( c );
	}
	LOG( "Parsed: '%s'", s.ToCStr() );
	return strdup( s.ToCStr() );
}

void LaunchDemoIntent( AppLocal * apl, int index )
{
#if !defined( RETAIL )
	const char * filename = "/sdcard/Oculus/keyintents.txt";
	FILE * f = fopen( filename, "r" );
	if ( !f )
	{
		LOG( "couldn't open %s", filename );
		return;
	}
	for ( int i = 0 ; i <= index ; i++ )
	{
		const char * package = ParseToSemi( f );
		const char * data = ParseToSemi( f );
		if ( package[0] == 0 )
		{
			LOG( "not enough intents in %s for index %i", filename, index );
			break;
		}

		if ( i == index )
		{
			LOG( "Launching intent %i: %s + %s", i, package, data );
			apl->SendIntent( package, data );
			break;
		}
		free( (void *)package );
		free( (void *)data );
	}
	fclose( f );
#endif
}

#if defined( TEST_TIMEWARP_WATCHDOG )
static float test = 0.0f;
#endif

void AppLocal::KeyEvent( const int keyCode, const bool down, const int repeatCount )
{
	// the back key is special because it has to handle long-press and double-tap
	if ( keyCode == AKEYCODE_BACK )
	{
		//DROIDLOG( "BackKey", "BACK KEY PRESSED" );
		// back key events, because of special handling for double-tap, short-press and long-press,
		// are handled in AppLocal::VrThreadFunction.
		BackKeyState.HandleEvent( ovr_GetTimeInSeconds(), down, repeatCount );
		return;
	}
	
	// the app menu is always the first consumer so it cannot be usurped
	bool consumedKey = false;
	if ( repeatCount == 0 )
	{
		consumedKey = GetGuiSys().OnKeyEvent( this, keyCode, down ? KeyState::KEY_EVENT_DOWN : KeyState::KEY_EVENT_UP );
	}

	// for all other keys, allow VrAppInterface the chance to handle and consume the key first
	if ( !consumedKey )
	{
		consumedKey = appInterface->OnKeyEvent( keyCode, down ? KeyState::KEY_EVENT_DOWN : KeyState::KEY_EVENT_UP );
	}

	// ALL VRLIB KEY HANDLING OTHER THAN APP MENU SHOULD GO HERE
	if ( !consumedKey && enableDebugOptions )
	{
		float const IPD_STEP = 0.001f;

		// FIXME: this should set consumedKey = true now that we pass key events via appInterface first,
		// but this would likely break some apps right now that rely on the old behavior
		// consumedKey = true;

		// MWC demo: launch specific intents with number keys
		// and force a swipe from the remote keyboard
		if ( down && keyCode >= AKEYCODE_1 && keyCode <= AKEYCODE_9 )
		{
			LaunchDemoIntent( this, keyCode - AKEYCODE_1 );
			return;
		}
		else if ( down && keyCode == AKEYCODE_RIGHT_BRACKET )
		{
			LOG( "BUTTON_SWIPE_FORWARD");
			joypad.buttonState |= BUTTON_SWIPE_FORWARD;
			return;
		} 
		else if ( down && keyCode == AKEYCODE_LEFT_BRACKET )
		{
			LOG( "BUTTON_SWIPE_BACK");
			joypad.buttonState |= BUTTON_SWIPE_BACK;
			return;
		}
		else if ( keyCode == AKEYCODE_S )
		{
			if ( repeatCount == 0 && down ) // first down only
			{
				EyeTargets->ScreenShot();
				CreateToast( "screenshot" );
				return;
			}
		}
		else if ( keyCode == AKEYCODE_F && down && repeatCount == 0 )
		{
			SetShowFPS( !GetShowFPS() );
			return;
		}
		else if ( keyCode == AKEYCODE_COMMA && down && repeatCount == 0 )
		{
			float const IPD_MIN_CM = 0.0f;
			ViewParms.InterpupillaryDistance = Alg::Max( IPD_MIN_CM * 0.01f, ViewParms.InterpupillaryDistance - IPD_STEP );
			ShowInfoText( 1.0f, "%.3f", ViewParms.InterpupillaryDistance );
			return;
		}
		else if ( keyCode == AKEYCODE_PERIOD && down && repeatCount == 0 )
		{
			float const IPD_MAX_CM = 8.0f;
			ViewParms.InterpupillaryDistance = Alg::Min( IPD_MAX_CM * 0.01f, ViewParms.InterpupillaryDistance + IPD_STEP );
			ShowInfoText( 1.0f, "%.3f", ViewParms.InterpupillaryDistance );
			return;
		}
		else if ( keyCode == AKEYCODE_C && down && repeatCount == 0 )
		{
			bool enabled = !GetComfortModeEnabled();
			SetComfortModeEnabled( enabled );
		}
#if defined( TEST_TIMEWARP_WATCHDOG )	// test TimeWarp sched_fifo watchdog
		else if ( keyCode == AKEYCODE_T && down && repeatCount == 0 )
		{
			const double SPIN_TIME = 60.0;
			DROIDLOG( "TimeWarp", "Spinning on VrThread for %f seconds...", SPIN_TIME );
			double start = ovr_GetTimeInSeconds();
			while ( ovr_GetTimeInSeconds() < start + SPIN_TIME )
			{
				test = cos( test );
			}
		}
#endif
	}

	// Keys always map to joystick buttons right now even if consumed otherwise.
	// This probably should change but it's unclear what this might break right now.
	for ( int i = 0 ; buttonMappings[i] != -1 ; i++ )
	{
		// joypad buttons come in with 0x10000 as a flag
		if ( ( keyCode & ~0x10000 ) == buttonMappings[i] )
		{
			if ( down )
			{
				joypad.buttonState |= 1<<i;
			}
			else
			{
				joypad.buttonState &= ~(1<<i);
			}
			return;
		}
	}
}

Matrix4f AppLocal::MatrixInterpolation( const Matrix4f & startMatrix, const Matrix4f & endMatrix, double t )
{
	Matrix4f result;
	Quat<float> startQuat = (Quat<float>) startMatrix ;
	Quat<float> endQuat = (Quat<float>) endMatrix ;
	Quat<float> quatResult;

	double cosHalfTheta = startQuat.w * endQuat.w +
						  startQuat.x * endQuat.x +
						  startQuat.y * endQuat.y +
						  startQuat.z * endQuat.z;

	if( fabs( cosHalfTheta ) >= 1.0 )
	{
		result = startMatrix;

		Vector3<float> startTrans( startMatrix.M[0][3], startMatrix.M[1][3], startMatrix.M[2][3] );
		Vector3<float> endTrans( endMatrix.M[0][3], endMatrix.M[1][3], endMatrix.M[2][3] );

		Vector3<float> interpolationVector = startTrans.Lerp( endTrans, t );

		result.M[0][3] = interpolationVector.x;
		result.M[1][3] = interpolationVector.y;
		result.M[2][3] = interpolationVector.z;
		return result;
	}


	bool reverse_q1 = false;
	if ( cosHalfTheta < 0 )
	{
		reverse_q1 = true;
		cosHalfTheta = -cosHalfTheta;
	}

	// Calculate intermediate rotation.
	const double halfTheta = acos( cosHalfTheta );
	const double sinHalfTheta = sqrt( 1.0 - ( cosHalfTheta * cosHalfTheta ) );

	// if theta = 180 degrees then result is not fully defined
	// we could rotate around any axis normal to qa or qb
	if( fabs(sinHalfTheta) < 0.001 )
	{
		if (!reverse_q1)
		{
			quatResult.w = ( 1 - t ) * startQuat.w + t * endQuat.w;
			quatResult.x = ( 1 - t ) * startQuat.x + t * endQuat.x;
			quatResult.y = ( 1 - t ) * startQuat.y + t * endQuat.y;
			quatResult.z = ( 1 - t ) * startQuat.z + t * endQuat.z;
		}
		else
		{
			quatResult.w = ( 1 - t ) * startQuat.w - t * endQuat.w;
			quatResult.x = ( 1 - t ) * startQuat.x - t * endQuat.x;
			quatResult.y = ( 1 - t ) * startQuat.y - t * endQuat.y;
			quatResult.z = ( 1 - t ) * startQuat.z - t * endQuat.z;
		}
		result = (Matrix4f) quatResult;
	}
	else
	{

		const double A = sin( ( 1 - t ) * halfTheta ) / sinHalfTheta;
		const double B = sin( t *halfTheta ) / sinHalfTheta;
		if (!reverse_q1)
		{
			quatResult.w =  A * startQuat.w + B * endQuat.w;
			quatResult.x =  A * startQuat.x + B * endQuat.x;
			quatResult.y =  A * startQuat.y + B * endQuat.y;
			quatResult.z =  A * startQuat.z + B * endQuat.z;
		}
		else
		{
			quatResult.w =  A * startQuat.w - B * endQuat.w;
			quatResult.x =  A * startQuat.x - B * endQuat.x;
			quatResult.y =  A * startQuat.y - B * endQuat.y;
			quatResult.z =  A * startQuat.z - B * endQuat.z;
		}

		result = (Matrix4f) quatResult;
	}

	Vector3<float> startTrans( startMatrix.M[0][3], startMatrix.M[1][3], startMatrix.M[2][3] );
	Vector3<float> endTrans( endMatrix.M[0][3], endMatrix.M[1][3], endMatrix.M[2][3] );

	Vector3<float> interpolationVector = startTrans.Lerp( endTrans, t);

	result.M[0][3] = interpolationVector.x;
	result.M[1][3] = interpolationVector.y;
	result.M[2][3] = interpolationVector.z;

	return result;
}


App::~App() { }	// avoids "undefined reference to 'vtable for OVR::App'" error

OvrGuiSys & AppLocal::GetGuiSys() 
{ 
    return *GuiSys; 
}
OvrGazeCursor & AppLocal::GetGazeCursor() 
{ 
    return *GazeCursor;
}
BitmapFont & AppLocal::GetDefaultFont() 
{ 
    return *DefaultFont; 
}
BitmapFontSurface & AppLocal::GetWorldFontSurface() 
{ 
    return *WorldFontSurface; 
}
BitmapFontSurface & AppLocal::GetMenuFontSurface() 
{ 
    return *MenuFontSurface; 
}   // TODO: this could be in the menu system now

OvrVRMenuMgr & AppLocal::GetVRMenuMgr() 
{ 
    return *VRMenuMgr; 
}
OvrDebugLines & AppLocal::GetDebugLines() 
{ 
    return *DebugLines; 
}

bool AppLocal::IsGuiOpen() const 
{
	return GuiSys->IsAnyMenuOpen();
}

bool AppLocal::IsPassThroughCameraEnabled() const 
{ 
	return PassThroughCameraEnabled; 
}

int	 AppLocal::GetSystemBrightness() const
{
	int cur = 255;
	// FIXME: this specifically checks for Note4 before calling the function because calling it on other
	// models right now can break rendering. Eventually this needs to be supported on all models.
	if ( getSysBrightnessMethodId != NULL && OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		cur = VrJni->CallStaticIntMethod( VrLibClass, getSysBrightnessMethodId, javaObject );
	}
	return cur;
}

void AppLocal::SetSystemBrightness( int const v )
{
	int v2 = Alg::Clamp( v, 0, 255 );
	// FIXME: this specifically checks for Note4 before calling the function because calling it on other
	// models right now can break rendering. Eventually this needs to be supported on all models.
	if ( setSysBrightnessMethodId != NULL && OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		VrJni->CallStaticVoidMethod( VrLibClass, setSysBrightnessMethodId, javaObject, v2 );
	}
}

bool AppLocal::GetComfortModeEnabled() const
{
	bool r = true;
	if ( getComfortViewModeMethodId != NULL && OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		r = VrJni->CallStaticBooleanMethod( VrLibClass, getComfortViewModeMethodId, javaObject );
	}
	return r;
}

void AppLocal::SetComfortModeEnabled( bool const enabled )
{
	if ( enableComfortViewModeMethodId != NULL && OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		VrJni->CallStaticVoidMethod( VrLibClass, enableComfortViewModeMethodId, javaObject, enabled );
	}
}

void AppLocal::SetDoNotDisturbMode( bool const enable )
{
	if ( setDoNotDisturbModeMethodId != NULL && OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		VrJni->CallStaticVoidMethod( VrLibClass, setDoNotDisturbModeMethodId, javaObject, enable );
	}
}

bool AppLocal::GetDoNotDisturbMode() const
{
	bool r = false;
	if ( getDoNotDisturbModeMethodId != NULL && OVR_stricmp( ovr_GetBuildString( BUILDSTR_MODEL ), "SM-G906S" ) != 0 )
	{
		r = VrJni->CallStaticBooleanMethod( VrLibClass, getDoNotDisturbModeMethodId, javaObject );
	}
	return r;
}

int AppLocal::GetWifiSignalLevel() const
{
	int level = ovr_GetWifiSignalLevel();
	return level;
}

eWifiState AppLocal::GetWifiState() const
{
	return ovr_GetWifiState();
}

int AppLocal::GetCellularSignalLevel() const
{
	int level = ovr_GetCellularSignalLevel();
	return level;
}

eCellularState AppLocal::GetCellularState() const
{
	return ovr_GetCellularState();
}

bool AppLocal::IsAirplaneModeEnabled() const
{
	bool r = false;
	if ( isAirplaneModeEnabledMethodId != NULL  )
	{
		r = VrJni->CallStaticBooleanMethod( VrLibClass, isAirplaneModeEnabledMethodId, javaObject );
	}
	return r;
}

bool AppLocal::GetBluetoothEnabled() const
{
	bool r = false;
	if ( getBluetoothEnabledMethodId != NULL )
	{
		r = VrJni->CallStaticBooleanMethod( VrLibClass, getBluetoothEnabledMethodId, javaObject );
	}
	return r;
}

bool AppLocal::IsAsynchronousTimeWarp() const
{
	return VrModeParms.AsynchronousTimeWarp;
}

bool AppLocal::GetHasHeadphones() const {
	return ovr_GetHeadsetPluggedState();
}

bool AppLocal::GetFramebufferIsSrgb() const
{
	return FramebufferIsSrgb;
}

bool AppLocal::GetRenderMonoMode() const
{
	return renderMonoMode; 
}

void AppLocal::SetRenderMonoMode( bool const mono )
{
	renderMonoMode = mono; 
}

char const * AppLocal::GetPackageCodePath() const
{
	return packageCodePath; 
}

bool AppLocal::IsPackageInstalled( const char * packageName ) const {
	LOG( "IsPackageInstalled %s", packageName );

	jobject stringObject = VrJni->NewStringUTF( packageName );

	bool isInstalled = VrJni->CallStaticBooleanMethod( vrActivityClass, isPackageInstalledMethodId, javaObject, stringObject );

	// We are not regularly returning to java, so we need to delete
	// the local string reference explicitly.
	VrJni->DeleteLocalRef( stringObject );

	return isInstalled;
}

Matrix4f const & AppLocal::GetLastViewMatrix() const
{
	return lastViewMatrix; 
}

void AppLocal::SetLastViewMatrix( Matrix4f const & m )
{
	lastViewMatrix = m; 
}

EyeParms & AppLocal::GetVrParms()
{
	return vrParms; 
}

ovrModeParms AppLocal::GetVrModeParms()
{
	return VrModeParms;
}

float AppLocal::GetCameraFovHorizontal() const
{
	return cameraFovHorizontal; 
}

void AppLocal::SetCameraFovHorizontal( float const f )
{
	cameraFovHorizontal = f; 
}

float AppLocal::GetCameraFovVertical() const
{
	return cameraFovVertical; 
}

void AppLocal::SetCameraFovVertical( float const f )
{
	cameraFovVertical = f; 
}

void AppLocal::SetPopupDistance( float const d )
{
	popupDistance = d; 
}

float AppLocal::GetPopupDistance() const
{
	return popupDistance; 
}

void AppLocal::SetPopupScale( float const s )
{
	popupScale = s; 
}

float AppLocal::GetPopupScale() const
{
	return popupScale; 
}

bool AppLocal::GetPowerSaveActive() const
{
	return ovr_GetPowerLevelStateThrottled();
}

int AppLocal::GetCpuLevel() const
{
	return VrModeParms.CpuLevel;
}

int AppLocal::GetGpuLevel() const
{
	return VrModeParms.GpuLevel;
}

int AppLocal::GetBatteryLevel() const 
{
	return BatteryLevel;
}

eBatteryStatus AppLocal::GetBatteryStatus() const 
{
	return BatteryStatus;
}

JavaVM	* AppLocal::GetJavaVM()
{
	return javaVM; 
}

JNIEnv * AppLocal::GetUiJni()
{
	return UiJni; 
}

JNIEnv * AppLocal::GetVrJni()
{
	return VrJni; 
}

jobject	& AppLocal::GetJavaObject()
{
	return javaObject; 
}

jclass & AppLocal::GetVrActivityClass()
{
	return vrActivityClass; 
}

SurfaceTexture * AppLocal::GetDialogTexture()
{
	return dialogTexture; 
}

SurfaceTexture * AppLocal::GetCameraTexture()
{
	return cameraTexture;
}

UiPanel	& AppLocal::GetActivityPanel()
{
	return activityPanel; 
}

GlGeometry const & AppLocal::GetUnitSquare() const
{
	return unitSquare; 
}

GlProgram & AppLocal::GetExternalTextureProgram2()
{
	return externalTextureProgram2; 
}

ovrHmdInfo const & AppLocal::GetHmdInfo() const
{
	return hmdInfo; 
}

TimeWarpParms const & AppLocal::GetSwapParms() const
{
	return SwapParms; 
}

TimeWarpParms & AppLocal::GetSwapParms()
{
	return SwapParms; 
}

ovrSensorState const & AppLocal::GetSensorForNextWarp() const
{
	return SensorForNextWarp;
}

void AppLocal::SetShowFPS( bool const show )
{
	ShowFPS = show;
}

bool AppLocal::GetShowFPS() const
{
	return ShowFPS;
}

VrAppInterface * AppLocal::GetAppInterface() 
{
	return appInterface;
}

VrViewParms const &	AppLocal::GetVrViewParms() const
{
	return ViewParms;
}

void AppLocal::SetVrViewParms( VrViewParms const & parms ) 
{
	ViewParms = parms;
}

void AppLocal::ShowInfoText( float const duration, const char * fmt, ... )
{
	char buffer[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buffer, sizeof( buffer ), fmt, args );
	va_end( args );
	InfoText = buffer;
	InfoTextEndFrame = vrFrame.FrameNumber + (long long)( duration * 60.0f ) + 1;
}

char const * AppLocal::GetDashboardPackageName() const 
{
	static char homePackageName[128];
	ovr_GetHomePackageName(homePackageName, sizeof( homePackageName ) );
	return homePackageName;
}

KeyState & AppLocal::GetBackKeyState()
{
	return BackKeyState;
}

ovrMobile * AppLocal::GetOvrMobile()
{
	return OvrMobile;
}

void AppLocal::SetShowVolumePopup( bool const show )
{
	ShowVolumePopup = show;
}

bool AppLocal::GetShowVolumePopup() const
{
	return ShowVolumePopup;
}

void AppLocal::StartPlatformUI( const char * intent )
{
	LOG( "ovr_StartPlatformUI" );
	if ( intent == NULL || intent[0] == '\0' )
	{
		ovr_StartPackageActivity( OvrMobile, PUI_CLASS_NAME, PUI_GLOBAL_MENU );
	}
	else
	{
		ovr_StartPackageActivity( OvrMobile, PUI_CLASS_NAME, intent );
	}
}

void AppLocal::ExitPlatformUI()
{
	LOG( "ExitPlatformUI" );
	ovr_ExitActivity( OvrMobile, EXIT_TYPE_FINISH );
}

void AppLocal::RecenterYaw( const bool showBlack )
{
	if ( showBlack )
	{
		ovr_WarpSwapBlack( OvrMobile );
	}
	ovrHmd_RecenterYaw( OvrHmd );
	GetGuiSys().RepositionMenus( this );
}

}	// namespace OVR