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
#include "Kernel/OVR_JSON.h"
#include "Android/JniUtils.h"
#include "Android/NativeBuildStrings.h"

#include "3rdParty/stb/stb_image_write.h"

#include "VrApi/VrApi.h"
#include "VrApi/VrApi_Android.h"
#include "VrApi/VrApi_Helpers.h"
#include "VrApi/LocalPreferences.h"		// FIXME:VRAPI move to VrApi_Android.h?

#include "GlSetup.h"
#include "GlTexture.h"
#include "VrCommon.h"
#include "AppLocal.h"
#include "ModelView.h"
#include "BitmapFont.h"
#include "GazeCursorLocal.h"		// necessary to instantiate the gaze cursor
#include "VRMenu/VRMenuMgr.h"
#include "VRMenu/GuiSysLocal.h"		// necessary to instantiate the gui system
#include "VRMenu/VolumePopup.h"
#include "PackageFiles.h"
#include "VrLocale.h"
#include "UserProfile.h"
#include "Console.h"

//#define TEST_TIMEWARP_WATCHDOG

static const char * activityClassName = "com/oculusvr/vrlib/VrActivity";
static const char * vrLibClassName = "com/oculusvr/vrlib/VrLib";

// some parameters from the intent can be empty strings, which cannot be represented as empty strings for sscanf
// so we encode them as EMPTY_INTENT_STR.
// Because the message queue handling uses sscanf() to parse the message, the JSON text is 
// always placed at the end of the message because it can contain spaces while the package
// name and URI cannot. The handler will use sscanf() to parse the first two strings, then
// assume the JSON text is everything immediately following the space after the URI string.
static const char * EMPTY_INTENT_STR = "<EMPTY>";
void ComposeIntentMessage( char const * packageName, char const * uri, char const * jsonText, 
		char * out, size_t outSize )
{
	OVR_sprintf( out, outSize, "intent %s %s %s",
			packageName == NULL || packageName[0] == '\0' ? EMPTY_INTENT_STR : packageName,
			uri == NULL || uri[0] == '\0' ? EMPTY_INTENT_STR : uri,
			jsonText == NULL || jsonText[0] == '\0' ? "" : jsonText );
}

namespace OVR
{

extern "C"
{

void Java_com_oculusvr_vrlib_VrActivity_nativeSurfaceChanged( JNIEnv *jni, jclass clazz,
		jlong appPtr, jobject surface )
{
	LOG( "%p nativeSurfaceChanged( %p )", (void *)appPtr, surface );

	((AppLocal *)appPtr)->GetMessageQueue().SendPrintf( "surfaceChanged %p",
			surface ? ANativeWindow_fromSurface( jni, surface ) : NULL );
}

void Java_com_oculusvr_vrlib_VrActivity_nativeSurfaceDestroyed( JNIEnv *jni, jclass clazz,
		jlong appPtr, jobject surface )
{
	LOG( "%p nativeSurfaceDestroyed()", (void *)appPtr );

	if ( appPtr == 0 )
	{
		// Android may call surfaceDestroyed() after onDestroy().
		LOG( "nativeSurfaceChanged was called after onDestroy. We cannot destroy the surface now because we don't have a valid app pointer." );
		return;
	}

	((AppLocal *)appPtr)->GetMessageQueue().SendPrintf( "surfaceDestroyed " );
}

void Java_com_oculusvr_vrlib_VrActivity_nativePopup( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint width, jint height, jfloat seconds )
{
	LOG( "%p nativePopup", (void *)appPtr );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( "popup %i %i %f", width, height, seconds );
}

jobject Java_com_oculusvr_vrlib_VrActivity_nativeGetPopupSurfaceTexture( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p getPopUpSurfaceTexture: %i", (void *)appPtr,
			((AppLocal *)appPtr)->GetDialogTexture()->textureId );
	return ((AppLocal *)appPtr)->GetDialogTexture()->javaObject;
}

void Java_com_oculusvr_vrlib_VrActivity_nativePause( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p Java_com_oculusvr_vrlib_VrActivity_nativePause", (void *)appPtr );
		((AppLocal *)appPtr)->GetMessageQueue().SendPrintf( "pause " );
}

void Java_com_oculusvr_vrlib_VrActivity_nativeResume( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p Java_com_oculusvr_vrlib_VrActivity_nativeResume", (void *)appPtr );
		((AppLocal *)appPtr)->GetMessageQueue().SendPrintf( "resume " );
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

void Java_com_oculusvr_vrlib_VrActivity_nativeNewIntent( JNIEnv *jni, jclass clazz,
		jlong appPtr, jstring fromPackageName, jstring command, jstring uriString )
{
	LOG( "%p nativeNewIntent", (void*)appPtr );
	JavaUTFChars utfPackageName( jni, fromPackageName );
	JavaUTFChars utfUri( jni, uriString );
	JavaUTFChars utfJson( jni, command );

	char intentMessage[4096];
	ComposeIntentMessage( utfPackageName.ToStr(), utfUri.ToStr(), utfJson.ToStr(), 
			intentMessage, sizeof( intentMessage ) );
	LOG( "nativeNewIntent: %s", intentMessage );
	((AppLocal *)appPtr)->GetMessageQueue().PostPrintf( intentMessage );
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

jlong VrAppInterface::SetActivity( JNIEnv * jni, jclass clazz, jobject activity, jstring javaFromPackageNameString,
		jstring javaCommandString, jstring javaUriString )
{
	// Make a permanent global reference for the class
	if ( ActivityClass != NULL )
	{
		jni->DeleteGlobalRef( ActivityClass );
	}
	ActivityClass = (jclass)jni->NewGlobalRef( clazz );

	JavaUTFChars utfFromPackageString( jni, javaFromPackageNameString );
	JavaUTFChars utfJsonString( jni, javaCommandString );
	JavaUTFChars utfUriString( jni, javaUriString );
	LOG( "VrAppInterface::SetActivity: %s %s %s", utfFromPackageString.ToStr(), utfJsonString.ToStr(), utfUriString.ToStr() );

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
	char intentMessage[4096];
	ComposeIntentMessage( utfFromPackageString.ToStr(), utfUriString.ToStr(), utfJsonString.ToStr(), intentMessage, sizeof( intentMessage ) );
	static_cast< AppLocal * >( app )->GetMessageQueue().PostPrintf( intentMessage );
	static_cast< AppLocal * >( app )->SyncVrThread();

	return (jlong)app;
}

void VrAppInterface::OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI )
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

void VrAppInterface::Paused()
{
	LOG( "VrAppInterface::Paused - default handler called" );
}

void VrAppInterface::Resumed()
{
	LOG( "VrAppInterface::Resumed - default handler called" );
}

void VrAppInterface::Command( const char * msg )
{
	LOG( "VrAppInterface::Command - default handler called, msg = '%s'", msg );
}

void VrAppInterface::NewIntent( const char * fromPackageName, const char * command, const char * uri )
{
	LOG( "VrAppInterface::NewIntent - default handler called - %s %s %s", fromPackageName, command, uri );
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

bool VrAppInterface::GetWantProtectedFramebuffer() const
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

extern void DebugMenuBounds( void * appPtr, const char * cmd );
extern void DebugMenuHierarchy( void * appPtr, const char * cmd );
extern void ShowFPS( void * appPtr, const char * cmd );

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
			loadingIconTexId( 0 ),
			javaVM( VrLibJavaVM ),
			UiJni( &jni_ ),
			VrJni( NULL ),
			javaObject( NULL ),
			vrActivityClass( NULL ),
			VrLibClass( NULL ),
			finishActivityMethodId( NULL ),
			createVrToastMethodId( NULL ),
			clearVrToastsMethodId( NULL ),
			playSoundPoolSoundMethodId( NULL ),
			gazeEventMethodId( NULL ),
			setSysBrightnessMethodId( NULL ),
			getSysBrightnessMethodId( NULL ),
			enableComfortViewModeMethodId( NULL ),
			getComfortViewModeMethodId( NULL ),
			setDoNotDisturbModeMethodId( NULL ),
			getDoNotDisturbModeMethodId( NULL ),
			getBluetoothEnabledMethodId( NULL ),
			isAirplaneModeEnabledMethodId( NULL ),
			isTime24HourFormatId( NULL ),
			LaunchIntentURI(),
			LaunchIntentJSON(),
			LaunchIntentFromPackage(),
			packageCodePath( "" ),
			packageName( "" ),
			Paused( true ),
			popupDistance( 2.0f ),
			popupScale( 1.0f ),
			appInterface( NULL ),
			dialogTexture( NULL ),
			dialogWidth( 0 ),
			dialogHeight( 0 ),
			dialogStopSeconds( 0.0f ),
			nativeWindow( NULL ),
			windowSurface( EGL_NO_SURFACE ),
			drawCalibrationLines( false ),
			calibrationLinesDrawn( false ),
			showVignette( true ),
			FramebufferIsSrgb( false ),
			FramebufferIsProtected( false ),
			renderMonoMode( false ),
			VrThreadTid( 0 ),
			BatteryLevel( 0 ),
			BatteryStatus( BATTERY_STATUS_UNKNOWN ),
			ShowFPS( false ),
			ShowVolumePopup( true ),
			InfoTextColor( 1.0f ),
			InfoTextOffset( 0.0f ),
			InfoTextEndFrame( -1 ),
			touchpadTimer( 0.0f ),
			lastTouchpadTime( 0.0f ),
			lastTouchDown( false ),
			touchState( 0 ),
			enableDebugOptions( false ),
			recenterYawFrameStart( 0 ),
			GuiSys( NULL ),
			GazeCursor( NULL ),
			DefaultFont( NULL ),
			WorldFontSurface( NULL ),
			MenuFontSurface( NULL ),
			VRMenuMgr( NULL ),
			VolumePopup( NULL ),
			DebugLines( NULL ),
			BackKeyState( 0.25f, 0.75f ),
			StoragePaths( NULL ),
			ErrorTextureSize( 0 ),
			ErrorMessageEndTime( -1.0 )
{
	LOG( "----------------- AppLocal::AppLocal() -----------------");

	StoragePaths = new OvrStoragePaths( &jni_, activityObject_);

	//WaitForDebuggerToAttach();

	memset( & SensorForNextWarp, 0, sizeof( SensorForNextWarp ) );

	SensorForNextWarp.Predicted.Pose.Orientation = Quatf();

	ovr_LoadDevConfig( false );

	// Default time warp parms
	SwapParms = InitTimeWarpParms();

	// Default EyeParms
	vrParms.resolution = 1024;
	vrParms.multisamples = 4;
	vrParms.colorFormat = COLOR_8888;
	vrParms.depthFormat = DEPTH_24;

	// Default ovrModeParms
	VrModeParms.AsynchronousTimeWarp = true;
	VrModeParms.AllowPowerSave = true;
	VrModeParms.DistortionFileName = NULL;
	VrModeParms.EnableImageServer = false;
	VrModeParms.SkipWindowFullscreenReset = false;
	VrModeParms.CpuLevel = 2;
	VrModeParms.GpuLevel = 2;
	VrModeParms.GameThreadTid = 0;

	javaObject = UiJni->NewGlobalRef( activityObject_ );

	// A difficulty with JNI is that we can't resolve our (non-Android) package
	// classes on other threads, so lookup everything we need right now.
	vrActivityClass = GetGlobalClassReference( activityClassName );
	VrLibClass = GetGlobalClassReference( vrLibClassName );

	VrLocale::VrActivityClass = vrActivityClass;

	finishActivityMethodId = GetMethodID( "finishActivity", "()V" );
	createVrToastMethodId = GetMethodID( "createVrToastOnUiThread", "(Ljava/lang/String;)V" );
	clearVrToastsMethodId = GetMethodID( "clearVrToasts", "()V" );
	playSoundPoolSoundMethodId = GetMethodID( "playSoundPoolSound", "(Ljava/lang/String;)V" );

	jmethodID isHybridAppMethodId = GetStaticMethodID(VrLibClass, "isHybridApp", "(Landroid/app/Activity;)Z");
	bool const isHybridApp = jni_.CallStaticBooleanMethod(VrLibClass, isHybridAppMethodId, javaObject);

	ExitOnDestroy = !isHybridApp;

	gazeEventMethodId = GetStaticMethodID( vrActivityClass, "gazeEventFromNative", "(FFZZLandroid/app/Activity;)V" );
	setSysBrightnessMethodId = GetStaticMethodID( VrLibClass, "setSystemBrightness", "(Landroid/app/Activity;I)V" );
	getSysBrightnessMethodId = GetStaticMethodID( VrLibClass, "getSystemBrightness", "(Landroid/app/Activity;)I" );
	enableComfortViewModeMethodId = GetStaticMethodID( VrLibClass, "enableComfortViewMode", "(Landroid/app/Activity;Z)V" );
	getComfortViewModeMethodId = GetStaticMethodID( VrLibClass, "getComfortViewModeEnabled", "(Landroid/app/Activity;)Z" );
	setDoNotDisturbModeMethodId = GetStaticMethodID( VrLibClass, "setDoNotDisturbMode", "(Landroid/app/Activity;Z)V" );
	getDoNotDisturbModeMethodId = GetStaticMethodID( VrLibClass, "getDoNotDisturbMode", "(Landroid/app/Activity;)Z" );
	getBluetoothEnabledMethodId = GetStaticMethodID( VrLibClass, "getBluetoothEnabled", "(Landroid/app/Activity;)Z" );
	isAirplaneModeEnabledMethodId = GetStaticMethodID( VrLibClass, "isAirplaneModeEnabled", "(Landroid/app/Activity;)Z" );
	isTime24HourFormatId = GetStaticMethodID( VrLibClass, "isTime24HourFormat", "(Landroid/app/Activity;)Z" );

	// Find the system activities apk so that we can load font data from it.
	LanguagePackagePath = GetInstalledPackagePath( "com.oculus.systemactivities" );
	if ( LanguagePackagePath.IsEmpty() )
	{
		// If we can't find the system activities apk the user has updated this application and tried to run
		// it before getting a new Oculus Home. They may even have the new Oculus Home but it hasn't been
		// run yet and therefore Home hasn't installed the System Activities apk, so try to load fonts directly
		// from Home instead. If for some reason the fonts can't be loaded from Home, apps will default to US
		// English and use their embedded EFIGS font.
		LanguagePackagePath = GetInstalledPackagePath( "com.oculus.home" );
	}

	// Get the path to the .apk and package name
	OpenApplicationPackage();

	// Hook the App and AppInterface together
	appInterface = &interface_;
	appInterface->app = this;

	// Load user profile data relevant to rendering
    UserProfile profile = LoadProfile();
    ViewParms.InterpupillaryDistance = profile.Ipd;
    ViewParms.EyeHeight = profile.EyeHeight;
    ViewParms.HeadModelDepth = profile.HeadModelDepth;
    ViewParms.HeadModelHeight = profile.HeadModelHeight;

	// Register console functions
	InitConsole();
	RegisterConsoleFunction( "print", OVR::DebugPrint );
	RegisterConsoleFunction( "debugMenuBounds", OVR::DebugMenuBounds );
	RegisterConsoleFunction( "debugMenuHierarchy", OVR::DebugMenuHierarchy );
	RegisterConsoleFunction( "showFPS", OVR::ShowFPS );
}

AppLocal::~AppLocal()
{
	LOG( "---------- ~AppLocal() ----------" );

	UnRegisterConsoleFunctions();
	ShutdownConsole();

	if ( javaObject != 0 )
	{
		UiJni->DeleteGlobalRef( javaObject );
	}

	if ( StoragePaths != NULL )
	{
		delete StoragePaths;
		StoragePaths = NULL;
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
		WARN( "failed to join VrThread (%i)", ret );
	}
}

void AppLocal::SyncVrThread()
{
	vrMessageQueue.SendPrintf( "sync " );
	VrThreadSynced = true;
}

void AppLocal::InitFonts()
{
	DefaultFont = BitmapFont::Create();

	String fontName;
	VrLocale::GetString( GetVrJni(), GetJavaObject(), "@string/font_name", "efigs.fnt", fontName );
	fontName.Insert( "res/raw/", 0 );
	if ( !DefaultFont->Load( LanguagePackagePath.ToCStr(), fontName ) )
	{
		// reset the locale to english and try to load the font again
		jmethodID setDefaultLocaleId = VrJni->GetMethodID( vrActivityClass, "setDefaultLocale", "()V" );
		if ( setDefaultLocaleId != NULL )
		{
			VrJni->CallObjectMethod( javaObject, setDefaultLocaleId );
			if ( VrJni->ExceptionOccurred() )
			{
				VrJni->ExceptionClear();
				WARN( "Exception occured in setDefaultLocale" );
			}
			// re-get the font name for the new locale
			VrLocale::GetString( GetVrJni(), GetJavaObject(), "@string/font_name", "efigs.fnt", fontName );
			fontName.Insert( "res/raw/", 0 );
			// try to load the font
			if ( !DefaultFont->Load( LanguagePackagePath.ToCStr(), fontName ) )
			{
				FAIL( "Failed to load font for default locale!" );
			}
		}
	}

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
		jstring cmdString = (jstring) ovr_NewStringUTF( &jni, commandString + 6 );
		jni.CallVoidMethod( javaObject, playSoundPoolSoundMethodId, cmdString );
		jni.DeleteLocalRef( cmdString );
		return;
	}

	if ( MatchesHead( "toast ", commandString ) )
	{
		jstring cmdString = (jstring) ovr_NewStringUTF( &jni, commandString + 6 );
		jni.CallVoidMethod( javaObject, createVrToastMethodId, cmdString );
		jni.DeleteLocalRef( cmdString );
	    return;
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
		WARN( "AppLocal::PlaySound called with non SoundManager defined sound: %s", name );
		// Run on the talk to java thread
		Ttj.GetMessageQueue().PostPrintf( "sound %s", name );
	}
}

void AppLocal::StartSystemActivity( const char * command )
{
	if ( ovr_StartSystemActivity( OvrMobile, command, NULL ) ) {
		return;
	}
	if ( ErrorTexture != 0 )
	{
		// already in an error state
		return;
	}

	// clear any pending exception because to ensure no pending exception causes the error message to fail
	if ( VrJni->ExceptionOccurred() )
	{
		VrJni->ExceptionClear();
	}

	OVR::String imageName = "dependency_error";
	char language[128];
	ovr_GetCurrentLanguage( OvrMobile, language, sizeof( language ) );
	imageName += "_";
	imageName += language;
	imageName += ".png";

	void * imageBuffer = NULL;
	int imageSize = 0;
	if ( !ovr_FindEmbeddedImage( OvrMobile, imageName.ToCStr(), imageBuffer, imageSize ) )
	{
		// try to default to English
		imageName = "dependency_error_en.png";
		if ( !ovr_FindEmbeddedImage( OvrMobile, imageName.ToCStr(), imageBuffer, imageSize ) )
		{
			FAIL( "Failed to load error message texture!" );
		}
	}

	OVR::MemBuffer memBuffer( imageBuffer, static_cast< int >( imageSize ) );
	int h = 0;
	// Note that the extension used on the filename passed here is important! It must match the type
	// of file that was embedded.
	ErrorTexture.texture = LoadTextureFromBuffer( "error_msg.png", memBuffer, OVR::TextureFlags_t(), ErrorTextureSize, h );
	OVR_ASSERT( ErrorTextureSize == h );

	ErrorMessageEndTime = ovr_GetTimeInSeconds() + 7.5f;
}

void AppLocal::ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer )
{
	ovr_ReadFileFromApplicationPackage( nameInZip, length, buffer );
}

void AppLocal::OpenApplicationPackage()
{
	// get package codepath
	char temp[1024];
	ovr_GetPackageCodePath( UiJni, vrActivityClass, javaObject, temp, sizeof( temp ) );
	packageCodePath = strdup( temp );

	ovr_GetCurrentPackageName( UiJni, vrActivityClass, javaObject, temp, sizeof( temp ) );
	packageName = strdup( temp );

	ovr_OpenApplicationPackage( packageCodePath );
}

String AppLocal::GetInstalledPackagePath( const char * packageName ) const
{
	jmethodID getInstalledPackagePathId = GetMethodID( "getInstalledPackagePath", "(Ljava/lang/String;)Ljava/lang/String;" );
	if ( getInstalledPackagePathId != NULL )
	{
		JavaString packageNameObj( UiJni, packageName );
		JavaUTFChars resultStr( UiJni, static_cast< jstring >( UiJni->CallObjectMethod( javaObject, getInstalledPackagePathId, packageNameObj.GetJString() ) ) );
		if ( !UiJni->ExceptionOccurred() )
		{
			return String( resultStr );
		}
	}
	return String();
}

const char * AppLocal::GetLanguagePackagePath() const 
{
	return LanguagePackagePath.ToCStr();
}

// Error checks and exits on failure
jmethodID AppLocal::GetMethodID( const char * name, const char * signature ) const
{
	jmethodID mid = UiJni->GetMethodID( vrActivityClass, name, signature );
	if ( !mid )
	{
		FAIL( "couldn't get %s", name );
	}
	return mid;
}

jmethodID AppLocal::GetMethodID( jclass clazz, const char * name, const char * signature ) const
{
	jmethodID mid = UiJni->GetMethodID( clazz, name, signature );
	if ( !mid )
	{
		FAIL( "couldn't get %s", name );
	}
	return mid;
}

jmethodID AppLocal::GetStaticMethodID( jclass cls, const char * name, const char * signature ) const
{
	jmethodID mid = UiJni->GetStaticMethodID( cls, name, signature );
	if ( !mid )
	{
		FAIL( "couldn't get %s", name );
	}

	return mid;
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
	OverlayScreenFadeMaskProgram = BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 VertexColor;\n"
			"attribute vec4 Position;\n"
			"varying  lowp vec4 oColor;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Mvpm * Position;\n"
			"   oColor = vec4( 1.0, 1.0, 1.0, 1.0 - VertexColor.x );\n"
			"}\n"
		,
			"varying lowp vec4	oColor;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = oColor;\n"
			"}\n"
		);
	OverlayScreenDirectProgram = BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 Position;\n"
			"attribute vec2 TexCoord;\n"
			"varying  highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Mvpm * Position;\n"
			"   oTexCoord = TexCoord;\n"
			"}\n"
		,
			"uniform sampler2D Texture0;\n"
			"varying highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = texture2D( Texture0, oTexCoord );\n"
			"}\n"
		);

	// Build some geometries we need
	panelGeometry = BuildTesselatedQuad( 32, 16 );	// must be large to get faded edge
	unitSquare = BuildTesselatedQuad( 1, 1 );
	unitCubeLines = BuildUnitCubeLines();
	//FadedScreenMaskSquare = BuildFadedScreenMask( 0.0f, 0.0f );	// TODO: clean up: app-specific values are being passed in on DrawScreenMask

	EyeDecorations.Init();
}

void AppLocal::ShutdownGlObjects()
{
	DeleteProgram( externalTextureProgram2 );
	DeleteProgram( untexturedMvpProgram );
	DeleteProgram( untexturedScreenSpaceProgram );
	DeleteProgram( OverlayScreenFadeMaskProgram );
	DeleteProgram( OverlayScreenDirectProgram );

	panelGeometry.Free();
	unitSquare.Free();
	unitCubeLines.Free();
	FadedScreenMaskSquare.Free();

	EyeDecorations.Shutdown();
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
	appInterface->Paused();

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
	DROIDLOG( "OVRTimer", "AppLocal::Resume" );

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

		const char * enableDebugOptionsStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_DEBUG_OPTIONS, "0" );
		enableDebugOptions =  ( atoi( enableDebugOptionsStr ) > 0 );

		const char * enableGpuTimingsStr = ovr_GetLocalPreferenceValueForKey( LOCAL_PREF_DEV_GPU_TIMINGS, "0" );
		SetAllowGpuTimerQueries( atoi( enableGpuTimingsStr ) > 0 );
	}

	// Clear cursor trails
	GetGazeCursor().HideCursorForFrames( 10 );

	// Start up TimeWarp and the various performance options
	OvrMobile = ovr_EnterVrMode( VrModeParms, &hmdInfo );

	appInterface->Resumed();
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

	if ( MatchesHead( "surfaceChanged ", msg ) )
	{
		LOG( "%s", msg );
		if ( windowSurface != EGL_NO_SURFACE )
		{	// Samsung says this is an Android problem, where surfaces are reported as
			// created multiple times.
			WARN( "Skipping create work because window hasn't been destroyed." );
			return;
		}
		sscanf( msg, "surfaceChanged %p", &nativeWindow );

		// Optionally force the window to a different resolution, which
		// will be automatically scaled up by the HWComposer.
		//
		//ANativeWindow_setBuffersGeometry( nativeWindow, 1920, 1080, 0 );

		EGLint attribs[100];
		int		numAttribs = 0;

		// Set the colorspace on the window
		windowSurface = EGL_NO_SURFACE;
		if ( appInterface->GetWantSrgbFramebuffer() )
		{
			attribs[numAttribs++] = EGL_GL_COLORSPACE_KHR;
			attribs[numAttribs++] = EGL_GL_COLORSPACE_SRGB_KHR;
		}
		// Ask for TrustZone rendering support
		if ( appInterface->GetWantProtectedFramebuffer() )
		{
			attribs[numAttribs++] = EGL_PROTECTED_CONTENT_EXT;
			attribs[numAttribs++] = EGL_TRUE;
		}
		attribs[numAttribs++] = EGL_NONE;

		// Android doesn't let the non-standard extensions show up in the
		// extension string, so we need to try it blind.
		windowSurface = eglCreateWindowSurface( eglr.display, eglr.config,
				nativeWindow, attribs );

		if ( windowSurface == EGL_NO_SURFACE )
		{
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
			FramebufferIsSrgb = false;
			FramebufferIsProtected = false;
		}
		else
		{
			FramebufferIsSrgb = appInterface->GetWantSrgbFramebuffer();
			FramebufferIsProtected = appInterface->GetWantProtectedFramebuffer();
		}
		LOG( "NativeWindow %p gives surface %p", nativeWindow, windowSurface );
		LOG( "FramebufferIsSrgb: %s", FramebufferIsSrgb ? "true" : "false" );
		LOG( "FramebufferIsProtected: %s", FramebufferIsProtected ? "true" : "false" );

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

	if ( MatchesHead( "surfaceDestroyed ", msg ) )
	{
		LOG( "surfaceDestroyed" );

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
		char fromPackageName[512];
		char uri[1024];
		// since the package name and URI cannot contain spaces, but JSON can,
		// the JSON string is at the end and will come after the third space.
		sscanf( msg, "intent %s %s", fromPackageName, uri );
		char const * jsonStart = NULL;
		size_t msgLen = OVR_strlen( msg );
		int spaceCount = 0;
		for ( size_t i = 0; i < msgLen; ++i ) {
			if ( msg[i] == ' ' ) {
				spaceCount++;
				if ( spaceCount == 3 ) {
					jsonStart = &msg[i+1];
					break;
				}
			}
		}

		if ( OVR_strcmp( fromPackageName, EMPTY_INTENT_STR ) == 0 )
		{
			fromPackageName[0] = '\0';
		}
		if ( OVR_strcmp( uri, EMPTY_INTENT_STR ) == 0 )
		{
			uri[0] = '\0';
		}

		// assign launchIntent to the intent command
		LaunchIntentFromPackage = fromPackageName;
		LaunchIntentJSON = jsonStart;
		LaunchIntentURI = uri;

		// when the PlatformActivity is launched, this is how it gets its command to start
		// a particular UI.
		appInterface->NewIntent( fromPackageName, jsonStart, uri );
		
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

	if ( MatchesHead( "sync ", msg ) )
	{
		return;
	}

	if ( MatchesHead( "quit ", msg ) )
	{
		ovr_LeaveVrMode( OvrMobile );
		ReadyToExit = true;
		LOG( "VrThreadSynced=%d CreatedSurface=%d ReadyToExit=%d", VrThreadSynced, CreatedSurface, ReadyToExit );
	}

	// Pass it on to the client app.
	appInterface->Command( msg );
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
	GL_Finish();
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
	// Toggle calibration lines
	bool const rightTrigger = input.buttonState & BUTTON_RIGHT_TRIGGER;
	bool const leftTrigger = input.buttonState & BUTTON_LEFT_TRIGGER;
	if ( leftTrigger && rightTrigger && ( input.buttonPressed & BUTTON_START ) != 0 )
	{
		time_t rawTime;
		time( &rawTime );
		struct tm * timeInfo = localtime( &rawTime );
		char timeStr[128];
		strftime( timeStr, sizeof( timeStr ), "%H:%M:%S", timeInfo );
		DROIDLOG( "QAEvent", "%s (%.3f) - QA event occurred", timeStr, ovr_GetTimeInSeconds() );
	}

	// Display tweak testing, only when holding right trigger
	if ( enableDebugOptions && rightTrigger )
	{
#if 0
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
			SwapParms.DebugGraphMode = (ovrTimeWarpDebugPerfMode)debugMode;
			CreateToast( "debug graph %s: %s", modeNames[ debugMode ], valueNames[ debugValue ] );
		}
#endif

#if 0
		if ( input.buttonPressed & BUTTON_DPAD_RIGHT )
		{
			SwapParms.MinimumVsyncs = SwapParms.MinimumVsyncs > 3 ? 1 : SwapParms.MinimumVsyncs + 1;
			CreateToast( "MinimumVsyncs: %i", SwapParms.MinimumVsyncs );
		}
#endif

#if 0
		if ( input.buttonPressed & BUTTON_DPAD_RIGHT )
		{
			debugValue = ( debugValue + 1 ) % DEBUG_VALUE_MAX;
			SwapParms.DebugGraphValue = (ovrTimeWarpDebugPerfValue)debugValue;
			CreateToast( "debug graph %s: %s", modeNames[ debugMode ], valueNames[ debugValue ] );
		}
#endif
		if ( input.buttonPressed & BUTTON_DPAD_RIGHT )
		{
			jclass vmDebugClass = VrJni->FindClass( "dalvik/system/VMDebug" );
			jmethodID dumpId = VrJni->GetStaticMethodID( vmDebugClass, "dumpReferenceTables", "()V" );
			VrJni->CallStaticVoidMethod( vmDebugClass, dumpId );
			VrJni->DeleteLocalRef( vmDebugClass );
		}

		if ( input.buttonPressed & BUTTON_Y )
		{	// display current scheduler state and clock rates
			//const char * str = ovr_CreateSchedulingReport( OvrMobile );
			//CreateToast( "%s", str );
		}

		if ( input.buttonPressed & BUTTON_B )
		{
			if ( SwapParms.WarpOptions & SWAP_OPTION_USE_SLICED_WARP )
			{
				SwapParms.WarpOptions &= ~SWAP_OPTION_USE_SLICED_WARP;
				CreateToast( "eye warp" );
			}
			else
			{
				SwapParms.WarpOptions |= SWAP_OPTION_USE_SLICED_WARP;
				CreateToast( "slice warp" );
			}
		}

		if ( SwapParms.WarpOptions & SWAP_OPTION_USE_SLICED_WARP )
		{
			extern float calibrateFovScale;

			if ( input.buttonPressed & BUTTON_DPAD_LEFT )
			{
				SwapParms.PreScheduleSeconds -= 0.001f;
				CreateToast( "Schedule: %f", SwapParms.PreScheduleSeconds );
			}
			if ( input.buttonPressed & BUTTON_DPAD_RIGHT )
			{
				SwapParms.PreScheduleSeconds += 0.001f;
				CreateToast( "Schedule: %f", SwapParms.PreScheduleSeconds );
			}
			if ( input.buttonPressed & BUTTON_DPAD_UP )
			{
				calibrateFovScale -= 0.01f;
				CreateToast( "calibrateFovScale: %f", calibrateFovScale );
				Pause();
				Resume();
			}
			if ( input.buttonPressed & BUTTON_DPAD_DOWN )
			{
				calibrateFovScale += 0.01f;
				CreateToast( "calibrateFovScale: %f", calibrateFovScale );
				Pause();
				Resume();
			}
		}
	}
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

	// Initialize the VR thread
	{
		LOG( "AppLocal::VrThreadFunction - init" );

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

		int w = 0;
		int h = 0;
		loadingIconTexId = LoadTextureFromApplicationPackage( "res/raw/loading_indicator.png",
										TextureFlags_t( TEXTUREFLAG_NO_MIPMAPS ), w, h );

		// Create the SurfaceTexture for dialog rendering.
		dialogTexture = new SurfaceTexture( VrJni );

		InitFonts();

		SoundManager.LoadSoundAssets();

		GetDebugLines().Init();

		GetGazeCursor().Init();

		GetVRMenuMgr().Init();

		GetGuiSys().Init( this, GetVRMenuMgr(), *DefaultFont );

		VolumePopup = OvrVolumePopup::Create( this, GetVRMenuMgr(), *DefaultFont );

		ovr_InitLocalPreferences( VrJni, javaObject );

		lastTouchpadTime = ovr_GetTimeInSeconds();
	}

	// FPS counter information
	int countApplicationFrames = 0;
	double lastReportTime = ceil( ovr_GetTimeInSeconds() );

	while( !( VrThreadSynced && CreatedSurface && ReadyToExit ) )
	{
		//SPAM( "FRAME START" );

		GetGazeCursor().BeginFrame();

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

		// handle any pending system activity events
		size_t const MAX_EVENT_SIZE = 4096;
		char eventBuffer[MAX_EVENT_SIZE];

		for ( eVrApiEventStatus status = ovr_GetNextPendingEvent( eventBuffer, MAX_EVENT_SIZE ); 
			status >= VRAPI_EVENT_PENDING; 
			status = ovr_GetNextPendingEvent( eventBuffer, MAX_EVENT_SIZE ) )
		{
			if ( status != VRAPI_EVENT_PENDING )
			{
				if ( status != VRAPI_EVENT_CONSUMED )
				{
					WARN( "Error %i handing System Activities Event", status );
				}
				continue;
			}

			char const * jsonError;
			JSON * jsonObj = JSON::Parse( eventBuffer, &jsonError );
			JsonReader reader( jsonObj );
			if ( jsonObj != NULL && reader.IsObject() ) 
			{
				String command = reader.GetChildStringByName( "Command" );
				if ( OVR_stricmp( command, SYSTEM_ACTIVITY_EVENT_REORIENT ) == 0 )
				{
					// for reorient, we recenter yaw natively, then pass the event along so that the client
					// application can also handle the event (for instance, to reposition menus)
					LOG( "VtThreadFunction: Acting on System Activity reorient event." );
					RecenterYaw( false );
				}
				else 
				{
					// In the case of the returnToLauncher event, we always handler it internally and pass 
					// along an empty buffer so that any remaining events still get processed by the client.
					LOG( "Unhandled System Activity event: '%s'", command.ToCStr() );
				}
				// free the JSON object
				jsonObj->Release();
			}
			else
			{
				// a malformed event string was pushed! This implies an error in the native code somewhere.
				WARN( "Error parsing System Activities Event: %s", jsonError );
			}
		}

		// update volume popup
		if ( ShowVolumePopup )
		{
			VolumePopup->CheckForVolumeChange( this );
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

		// if there is an error condition, warp swap and nothing else
		if ( ErrorTexture != 0 )
		{
			if ( ovr_GetTimeInSeconds() >= ErrorMessageEndTime )
			{
				ovr_ReturnToHome( OvrMobile );
			} 
			else 
			{
				ovrTimeWarpParms warpSwapMessageParms = InitTimeWarpParms( WARP_INIT_MESSAGE, ErrorTexture.texture );
				warpSwapMessageParms.ProgramParms[0] = 0.0f;						// rotation in radians
				warpSwapMessageParms.ProgramParms[1] = 1024.0f / ErrorTextureSize;	// message size factor
				ovr_WarpSwap( OvrMobile, &warpSwapMessageParms );
			}
			continue;
		}

		// Let the client app initialize only once by calling OneTimeInit() when the windowSurface is valid.
		if ( !OneTimeInitCalled )
		{
			if ( appInterface->ShouldShowLoadingIcon() )
			{
				const ovrTimeWarpParms warpSwapLoadingIconParms = InitTimeWarpParms( WARP_INIT_LOADING_ICON, loadingIconTexId );
				ovr_WarpSwap( OvrMobile, &warpSwapLoadingIconParms );
			}
			LOG( "launchIntentJSON: %s", LaunchIntentJSON.ToCStr() );
			LOG( "launchIntentURI: %s", LaunchIntentURI.ToCStr() );

			appInterface->OneTimeInit( LaunchIntentFromPackage.ToCStr(), LaunchIntentJSON.ToCStr(), LaunchIntentURI.ToCStr() );
			OneTimeInitCalled = true;
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

		if ( recenterYawFrameStart != 0 )
		{
			// Perform a reorient before sensor data is read.  Allows apps to reorient without having invalid orientation information for that frame.
			// Do a warp swap black on the frame the recenter started.
			RecenterYaw( recenterYawFrameStart == ( vrFrame.FrameNumber + 1 ) );  // vrFrame.FrameNumber hasn't been incremented yet, so add 1.
		}

		// Get the latest head tracking state, predicted ahead to the midpoint of the time
		// it will be displayed.  It will always be corrected to the real values by
		// time warp, but the closer we get, the less black will be pulled in at the edges.
		const double now = ovr_GetTimeInSeconds();
		static double prev;
		const double rawDelta = now - prev;
		prev = now;
		const double clampedPrediction = Alg::Min( 0.1, rawDelta * 2 );
		SensorForNextWarp = ovr_GetPredictedSensorState( OvrMobile, now + clampedPrediction );

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

#if 0
		// Joypad latency test
		// When this is enabled, each tap of a button will toggle the screen
		// color, allowing the tap-to-photons latency to be measured.
		if ( 0 )
		{
			if ( vrFrame.Input.buttonPressed )
			{
				LOG( "Input.buttonPressed" );
				static bool shut;
				if ( !shut )
				{
					// shut down timewarp, then go back into frontbuffer mode
					shut = true;
					ovr_LeaveVrMode( OvrMobile );
					static DirectRender	dr;
					dr.InitForCurrentSurface( VrJni, true, 19 );
				}
				ToggleScreenColor();
			}
			vrMessageQueue.SleepUntilMessage();
			continue;
		}
		// IMU latency test
		if ( 0 )
		{
			static bool buttonDown;
			const ovrSensorState sensor = ovr_GetPredictedSensorState( OvrMobile, now + clampedPrediction );
			const float acc = Vector3f( sensor.Predicted.AngularVelocity ).Length();
//			const float acc = fabs( Vector3f( sensor.Predicted.LinearAcceleration ).Length() - 9.8f );

			static int count;
			if ( ++count % 10 == 0 )
			{
				LOG( "acc: %f", acc );
			}
			const bool buttonNow = ( acc > 0.1f );
			if ( buttonNow != buttonDown )
			{
				LOG( "accel button" );
				buttonDown = buttonNow;
				static bool shut;
				if ( !shut )
				{
					// shut down timewarp, then go back into frontbuffer mode
					shut = true;
					ovr_LeaveVrMode( OvrMobile );
					static DirectRender	dr;
					dr.InitForCurrentSurface( VrJni, true, 19 );
				}
				ToggleScreenColor();
			}
			usleep( 1000 );
			continue;
		}
#endif

		FrameworkButtonProcessing( vrFrame.Input );

		KeyState::eKeyEventType event = BackKeyState.Update( ovr_GetTimeInSeconds() );
		if ( event != KeyState::KEY_EVENT_NONE )
		{
			//LOG( "BackKey: event %s", KeyState::EventNames[ event ] );
			// always allow the gaze cursor to peek at the event so it can start the gaze timer if necessary
			if ( !ovr_IsCurrentActivity( VrJni, javaObject, PUI_CLASS_NAME ) )
			{
				// update the gaze cursor timer
				if ( event == KeyState::KEY_EVENT_DOWN )
				{
					GetGazeCursor().StartTimer( GetBackKeyState().GetLongPressTime(), GetBackKeyState().GetDoubleTapTime() );
				} 
				else if ( event == KeyState::KEY_EVENT_DOUBLE_TAP || event == KeyState::KEY_EVENT_SHORT_PRESS )
				{
					GetGazeCursor().CancelTimer();
				}
				else if ( event == KeyState::KEY_EVENT_LONG_PRESS )
				{
					StartSystemActivity( PUI_GLOBAL_MENU );
				}
			}

			// let the menu handle it if it's open
			bool consumedKey = GetGuiSys().OnKeyEvent( this, AKEYCODE_BACK, event );
	
			// pass to the app if nothing handled it before this
			if ( !consumedKey )
			{				
				consumedKey = appInterface->OnKeyEvent( AKEYCODE_BACK, event );
			}
			// if nothing consumed the key and it's a short-press, exit the application to OculusHome
			if ( !consumedKey )
			{
				if ( event == KeyState::KEY_EVENT_SHORT_PRESS )
				{
					consumedKey = true;
					LOG( "BUTTON_BACK: confirming quit in platformUI" );
					StartSystemActivity( PUI_CONFIRM_QUIT );
				}
			}
		}

		if ( ShowFPS )
		{
			const int FPS_NUM_FRAMES_TO_AVERAGE = 30;
			static double  LastFrameTime = ovr_GetTimeInSeconds();
			static double  AccumulatedFrameInterval = 0.0;
			static int   NumAccumulatedFrames = 0;
			static float LastFrameRate = 60.0f;

			double currentFrameTime = ovr_GetTimeInSeconds();
			double frameInterval = currentFrameTime - LastFrameTime;
			AccumulatedFrameInterval += frameInterval;
			NumAccumulatedFrames++;
			if ( NumAccumulatedFrames > FPS_NUM_FRAMES_TO_AVERAGE ) {
				double interval = ( AccumulatedFrameInterval / NumAccumulatedFrames );  // averaged
				AccumulatedFrameInterval = 0.0;
				NumAccumulatedFrames = 0;
				LastFrameRate = 1.0f / float( interval > 0.000001 ? interval : 0.00001 );
			}    

			Vector3f viewPos = GetViewMatrixPosition( lastViewMatrix );
			Vector3f viewFwd = GetViewMatrixForward( lastViewMatrix );
			Vector3f newPos = viewPos + viewFwd * 1.5f;
			FPSPointTracker.Update( ovr_GetTimeInSeconds(), newPos );

			fontParms_t fp;
			fp.AlignHoriz = HORIZONTAL_CENTER;
			fp.Billboard = true;
			fp.TrackRoll = false;
			GetWorldFontSurface().DrawTextBillboarded3Df( GetDefaultFont(), fp, FPSPointTracker.GetCurPosition(), 
					0.8f, Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), "%.1f fps", LastFrameRate );
			LastFrameTime = currentFrameTime;
		}


		// draw info text
		if ( InfoTextEndFrame >= vrFrame.FrameNumber )
		{
			Vector3f viewPos = GetViewMatrixPosition( lastViewMatrix );
			Vector3f viewFwd = GetViewMatrixForward( lastViewMatrix );
			Vector3f viewUp( 0.0f, 1.0f, 0.0f );
			Vector3f viewLeft = viewUp.Cross( viewFwd );
			Vector3f newPos = viewPos + viewFwd * InfoTextOffset.z + viewUp * InfoTextOffset.y + viewLeft * InfoTextOffset.x;
			InfoTextPointTracker.Update( ovr_GetTimeInSeconds(), newPos );

			fontParms_t fp;
			fp.AlignHoriz = HORIZONTAL_CENTER;
			fp.AlignVert = VERTICAL_CENTER;
			fp.Billboard = true;
			fp.TrackRoll = false;
			GetWorldFontSurface().DrawTextBillboarded3Df( GetDefaultFont(), fp, InfoTextPointTracker.GetCurPosition(), 
					1.0f, InfoTextColor, InfoText.ToCStr() );
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
#if 1	// it is sometimes handy to remove this spam from the log
			LOG( "FPS: %i GPU time: %3.1f ms ", countApplicationFrames, EyeTargets->LogEyeSceneGpuTime.GetTotalTime() );
#endif
			countApplicationFrames = 0;
			lastReportTime = timeNow;
		}

		//SPAM( "FRAME END" );
	}

	// Shutdown the VR thread
	{
		LOG( "AppLocal::VrThreadFunction - shutdown" );

		// Shut down the message queue so it cannot overflow.
		vrMessageQueue.Shutdown();

		if ( ErrorTexture != 0 )
		{
			FreeTexture( ErrorTexture );
		}

		appInterface->OneTimeShutdown();

		GetGuiSys().Shutdown( GetVRMenuMgr() );

		GetVRMenuMgr().Shutdown();

		GetGazeCursor().Shutdown();

		GetDebugLines().Shutdown();

		ShutdownFonts();

		delete dialogTexture;
		dialogTexture = NULL;

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

		if ( down && keyCode == AKEYCODE_RIGHT_BRACKET )
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
const OvrStoragePaths & AppLocal::GetStoragePaths()
{
	return *StoragePaths;
}
OvrSoundManager & AppLocal::GetSoundMgr()
{
	return SoundManager;
}

bool AppLocal::IsGuiOpen() const 
{
	return GuiSys->IsAnyMenuOpen();
}

bool AppLocal::IsTime24HourFormat() const 
{
	bool r = false;
	if ( isTime24HourFormatId != NULL )
	{
		r = VrJni->CallStaticBooleanMethod( VrLibClass, isTime24HourFormatId, javaObject );
	}
	return r;
}

int AppLocal::GetSystemBrightness() const
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

bool AppLocal::GetFramebufferIsProtected() const
{
	return FramebufferIsProtected;
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

ovrTimeWarpParms const & AppLocal::GetSwapParms() const
{
	return SwapParms; 
}

ovrTimeWarpParms & AppLocal::GetSwapParms()
{
	return SwapParms; 
}

ovrSensorState const & AppLocal::GetSensorForNextWarp() const
{
	return SensorForNextWarp;
}

void AppLocal::SetShowFPS( bool const show )
{
	bool wasShowing = ShowFPS;
	ShowFPS = show;
	if ( !wasShowing && ShowFPS )
	{
		FPSPointTracker.Reset();
	}
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
	InfoTextColor = Vector4f( 1.0f );
	InfoTextOffset = Vector3f( 0.0f, 0.0f, 1.5f );
	InfoTextPointTracker.Reset();
	InfoTextEndFrame = vrFrame.FrameNumber + (long long)( duration * 60.0f ) + 1;
}

void AppLocal::ShowInfoText( float const duration, Vector3f const & offset, Vector4f const & color, const char * fmt, ... )
{
	char buffer[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buffer, sizeof( buffer ), fmt, args );
	va_end( args );
	InfoText = buffer;
	InfoTextColor = color;
	if ( offset != InfoTextOffset || InfoTextEndFrame < vrFrame.FrameNumber )
	{
		InfoTextPointTracker.Reset();
	}
	InfoTextOffset = offset;
	InfoTextEndFrame = vrFrame.FrameNumber + (long long)( duration * 60.0f ) + 1;
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

const char * AppLocal::GetPackageName( ) const
{
	return packageName;
}

bool AppLocal::IsWifiConnected() const
{
	jmethodID isWififConnectedMethodId = ovr_GetStaticMethodID( VrJni, VrLibClass, "isWifiConnected", "(Landroid/app/Activity;)Z" );
	return VrJni->CallStaticBooleanMethod( VrLibClass, isWififConnectedMethodId, javaObject );
}

void AppLocal::RecenterYaw( const bool showBlack )
{
	if ( showBlack )
	{
		const ovrTimeWarpParms warpSwapBlackParms = InitTimeWarpParms( WARP_INIT_BLACK );
		ovr_WarpSwap( OvrMobile, &warpSwapBlackParms );
	}
	ovr_RecenterYaw( OvrMobile );

	GetGuiSys().ResetMenuOrientations( this );
}

void AppLocal::SetRecenterYawFrameStart( const long long frameNumber )
{
	recenterYawFrameStart = frameNumber;
}

long long AppLocal::GetRecenterYawFrameStart() const
{
	return recenterYawFrameStart;
}

void ShowFPS( void * appPtr, const char * cmd ) {
	int show = 0;
	sscanf( cmd, "%i", &show );
	OVR_ASSERT( appPtr != NULL );	// something changed / broke in the OvrConsole code if this is NULL
	( ( App* )appPtr )->SetShowFPS( show != 0 );
}

}	// namespace OVR
