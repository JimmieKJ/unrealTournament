/************************************************************************************

Filename    :   App.h
Content     :   Native counterpart to VrActivity
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_App_h
#define OVR_App_h


#include <pthread.h>
#include "OVR.h"
#include "Android/GlUtils.h"
#include "Android/LogUtils.h"
#include "VrApi/VrApi.h"
#include "VrApi/VrApi_Android.h"
#include "GlProgram.h"
#include "GlTexture.h"
#include "GlGeometry.h"
#include "SurfaceTexture.h"
#include "EyeBuffers.h"
#include "EyePostRender.h"
#include "VrCommon.h"
#include "MessageQueue.h"
#include "Input.h"
#include "TalkToJava.h"
#include "KeyState.h"

// Avoid including this header file as much as possible in VrLib,
// so individual components are not tied to our native application
// framework, and can more easily be reused by Unity or other
// hosting applications.

namespace OVR
{

//==============================================================
// forward declarations
class EyeBuffers;
struct MaterialParms;
class GlGeometry;
struct GlProgram;
class VRMenuObjectParms;
class OvrGuiSys;
class OvrGazeCursor;
class BitmapFont;
class BitmapFontSurface;
class OvrVRMenuMgr;
class OvrDebugLines;
class App;
class VrViewParms;
class OvrStoragePaths;
class OvrSoundManager;

//==============================================================
// All of these virtual interfaces will be called by the VR application thread.
// Applications that don't care about particular interfaces are not
// required to implement them.
class VrAppInterface
{
public:
							 VrAppInterface();
	virtual					~VrAppInterface();

	// Each onCreate in java will allocate a new java object, but
	// we may reuse a single C++ object for them to make repeated
	// opening of the platformUI faster, and to not require full
	// cleanup on destruction.
	jlong SetActivity( JNIEnv * jni, jclass clazz, jobject activity, 
			jstring javaFromPackageNameString, jstring javaCommandString, 
			jstring javaUriString );

	// All subclasses communicate with App through this member.
	App *	app;				// Allocated in the first call to SetActivity()
	jclass	ActivityClass;		// global reference to clazz passed to SetActivity

	// This will be called one time only, before SetActivity()
	// returns from the first creation.
	//
	// It is called from the VR thread with an OpenGL context current.
	//
	// If the app was launched without a specific intent, launchIntent
	// will be an empty string.
	virtual void OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI );

	// This will be called one time only, when the app is about to shut down.
	//
	// It is called from the VR thread before the OpenGL context is destroyed.
	//
	// If the app needs to free OpenGL resources this is the place to do so.
	virtual void OneTimeShutdown();

	// Frame will only be called if the window surfaces have been created.
	//
	// The application should call DrawEyeViewsPostDistorted() to get
	// the DrawEyeView() callbacks.
	//
	// Any GPU operations that are relevant for both eye views, like
	// buffer updates or dynamic texture rendering, should be done first.
	//
	// Return the view matrix the framework should use for positioning
	// new pop up dialogs.
	virtual Matrix4f Frame( VrFrame vrFrame );

	// Called by DrawEyeViewsPostDisorted().
	//
	// The color buffer will have already been cleared or discarded, as
	// appropriate for the GPU.
	//
	// The viewport and scissor will already be set.
	//
	// Return the MVP matrix for the framework to use for drawing the
	// pop up dialog and debug graphics.
	//
	// fovDegrees may be different on different devices.
	virtual Matrix4f DrawEyeView( const int eye, const float fovDegrees );

	// If the app receives a new intent after launch, it will be sent to
	// this function.
	virtual void NewIntent( const char * fromPackageName, const char * command, const char * uri );

	// This is called on each resume, before VR Mode has been entered, to allow
	// the application to make changes.
	virtual void ConfigureVrMode( ovrModeParms & modeParms );

	// The window has been created and OpenGL is available.
	// This happens every time the user switches back to the app.
	virtual void WindowCreated();

	// The window is about to be destroyed, due to the user switching away.
	// The app will go to sleep until another message arrives.
	// Applications can use this to save game state, etc.
	virtual void WindowDestroyed();

	// The app is about to be paused.
	virtual void Paused();

	// The app is about to be resumed.
	virtual void Resumed();

	// Handle generic commands forwarded from other threads.
	// Commands can be processed even when the window surfaces
	// are not setup and the app would otherwise be sleeping.
	// The msg string will be freed by the framework after
	// command processing.
	virtual void Command( const char * msg );

	// The VrApp should return true if it consumes the key.
	virtual bool OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType );

	// Called by the application framework when the VR warning message is closed by the user.
	virtual bool OnVrWarningDismissed( const bool accepted );

	// Overload this and return false to skip showing the spinning loading icon.
	virtual bool ShouldShowLoadingIcon() const;

	// Overload and return true to have these attributes added to the window surface:
	// EGL_GL_COLORSPACE_KHR,  EGL_GL_COLORSPACE_SRGB_KHR
	virtual bool GetWantSrgbFramebuffer() const;

	// Overload and return true to have these attributes added to the window surface:
	// EGL_PROTECTED_CONTENT_EXT, EGL_TRUE
	virtual bool GetWantProtectedFramebuffer() const;
};

//==============================================================
// App
class App : public TalkToJavaInterface
{
public:
	virtual						~App();

	// App drains this message queue on the VR thread before calling
	// the frame and render functions.
	virtual MessageQueue &		GetMessageQueue() = 0;


	// When VrAppInterface::SetActivity is called, the App member is
	// allocated and hooked up to the VrAppInterface.
	virtual VrAppInterface *	GetAppInterface() = 0;

	// Calls VrAppInterface::DrawEyeView() for drawing to a framebuffer object,
	// then warps it to the screen.
	//
	// The framework can support monoscopic rendering and alternate eye
	// time warp extrapolation automatically.
	//
	// NumPresents 2 = time warp between each eye rendering, 0 = only draw to back buffers,
	// 1 = standard behavior.
	virtual void 				DrawEyeViewsPostDistorted( Matrix4f const & viewMatrix, 
										const int numPresents = 1 ) = 0;

	// Request the java framework to draw a toast popup and
	// present it to the VR framework.  It may be several frames
	// before it is displayed.
	virtual void				CreateToast( const char * fmt, ... ) = 0;

	// Plays a sound located in the application package's assets/ directory
	// through a Java SoundPool.  This is mostly for UI, there is no 3D spatialization.
	// "select.wav" would load <project>/assets/select.wav.
	virtual void				PlaySound( const char * name ) = 0;

	// Switch to SystemActivities application.
	virtual void				StartSystemActivity( const char * command ) = 0;

	// The parms will be initialized to sensible values on startup, and
	// can be freely changed at any time with minimal overhead.
	//
	// This could be implemented by passing and returning them every
	// GetEyeBuffers() call, but separating it allows the framework to manipulate
	// the values based on joypad button sequences without requiring
	// the application to be involved.
	virtual	EyeParms			GetEyeParms() = 0;
	virtual void				SetEyeParms( const EyeParms parms ) = 0;

	// Reorient was triggered - inform all menus etc.
	virtual void				RecenterYaw( const bool showBlack ) = 0;

	// Enables reorient before sensor data is read.  Allows apps to reorient without having invalid orientation information for that frame.
	virtual void				SetRecenterYawFrameStart( const long long frameNumber ) = 0;
	virtual long long			GetRecenterYawFrameStart() const = 0;

    //-----------------------------------------------------------------
    // interfaces
    //-----------------------------------------------------------------
    virtual OvrGuiSys &         	GetGuiSys() = 0;
    virtual OvrGazeCursor &     	GetGazeCursor() = 0;
    virtual BitmapFont &        	GetDefaultFont() = 0;
    virtual BitmapFontSurface & 	GetWorldFontSurface() = 0;
    virtual BitmapFontSurface & 	GetMenuFontSurface() = 0;
    virtual OvrVRMenuMgr &      	GetVRMenuMgr() = 0;
    virtual OvrDebugLines &     	GetDebugLines() = 0;
	virtual const OvrStoragePaths &	GetStoragePaths() = 0;
	virtual OvrSoundManager &		GetSoundMgr() = 0;

	//-----------------------------------------------------------------
	// system settings
	//-----------------------------------------------------------------
	virtual	int					GetSystemBrightness() const = 0;
	virtual void				SetSystemBrightness( int const v ) = 0;

	virtual	bool				GetComfortModeEnabled() const = 0;
	virtual	void				SetComfortModeEnabled( bool const enabled ) = 0;

	virtual	int					GetBatteryLevel() const = 0;
	virtual	eBatteryStatus		GetBatteryStatus() const = 0;

	virtual int					GetWifiSignalLevel() const = 0;
	virtual eWifiState			GetWifiState() const = 0;
	virtual int					GetCellularSignalLevel() const = 0;
	virtual eCellularState		GetCellularState() const = 0;

	virtual bool				IsAirplaneModeEnabled() const = 0;
	virtual bool				IsTime24HourFormat() const = 0;

	virtual void				SetDoNotDisturbMode( bool const enable ) = 0;
	virtual bool				GetDoNotDisturbMode() const = 0;

	virtual bool				GetBluetoothEnabled() const = 0;

	//-----------------------------------------------------------------
	// accessors
	//-----------------------------------------------------------------

	virtual bool				IsAsynchronousTimeWarp() const = 0;
	virtual	bool				GetHasHeadphones() const = 0;
	virtual bool				GetFramebufferIsSrgb() const = 0;
	virtual bool				GetFramebufferIsProtected() const = 0;
	virtual bool				GetRenderMonoMode() const = 0;
	virtual void				SetRenderMonoMode( bool const mono ) = 0;

	virtual char const *		GetPackageCodePath() const = 0;
	virtual char const *		GetLanguagePackagePath() const = 0;
	
	virtual Matrix4f const &	GetLastViewMatrix() const = 0;
	virtual void				SetLastViewMatrix( Matrix4f const & m ) = 0;

	virtual EyeParms &			GetVrParms() = 0;

	// Changing the VrModeParms may cause a visible flicker as VrMode is
	// left and re-entered; it should generally not be used in-game.
	virtual ovrModeParms 		GetVrModeParms() = 0;
	virtual void				SetVrModeParms( ovrModeParms parms ) = 0;

	virtual	void				SetPopupDistance( float const d ) = 0;
	virtual float				GetPopupDistance() const = 0;
	virtual	void				SetPopupScale( float const s ) = 0;
	virtual float				GetPopupScale() const = 0;

	virtual int					GetCpuLevel() const = 0;
	virtual int					GetGpuLevel() const = 0;

	virtual bool				GetPowerSaveActive() const = 0;

	virtual bool				IsGuiOpen() const = 0;

	virtual VrViewParms const &	GetVrViewParms() const = 0;
	virtual void				SetVrViewParms( VrViewParms const & parms ) = 0;

    virtual KeyState &          GetBackKeyState() = 0;

	virtual ovrMobile *			GetOvrMobile() = 0;

	virtual void				SetShowVolumePopup( bool const show ) = 0;
	virtual bool				GetShowVolumePopup() const = 0;

	virtual const char *		GetPackageName() const = 0;

	virtual bool				IsWifiConnected() const = 0;

	//-----------------------------------------------------------------
	// Java accessors
	//-----------------------------------------------------------------

	virtual JavaVM	*			GetJavaVM() = 0;
	virtual JNIEnv	*			GetUiJni() = 0;			// for use by the Java UI thread
	virtual JNIEnv	*			GetVrJni() = 0;			// for use by the VR thread
	virtual jobject	&			GetJavaObject() = 0;
	virtual jclass	&			GetVrActivityClass() = 0;	// need to look up from main thread

	//-----------------------------------------------------------------

	// Every application gets a basic dialog surface.
	virtual SurfaceTexture *	GetDialogTexture() = 0;

	//-----------------------------------------------------------------
	// TimeWarp
	//-----------------------------------------------------------------
	virtual ovrTimeWarpParms const & GetSwapParms() const = 0;			// passed to TimeWarp->WarpSwap()
	virtual ovrTimeWarpParms &		GetSwapParms() = 0;

	virtual ovrSensorState const & GetSensorForNextWarp() const = 0;

	// Overlay plane helper functions

	// Draw a zero to destination alpha
	virtual void				DrawScreenMask( const ovrMatrix4f & mvp, const float fadeFracX, const float fadeFracY ) = 0;
	// Draw a screen to an eye buffer the same way it would be drawn as a
	// time warp overlay.
	virtual void				DrawScreenDirect( const GLuint texid, const ovrMatrix4f & mvp ) = 0;

	//-----------------------------------------------------------------
	// debugging
	//-----------------------------------------------------------------
	virtual void				SetShowFPS( bool const show ) = 0;
	virtual bool				GetShowFPS() const = 0;

	virtual	void				ShowInfoText( float const duration, const char * fmt, ... ) = 0;
	virtual	void				ShowInfoText( float const duration, Vector3f const & offset, Vector4f const & color, const char * fmt, ... ) = 0;
};


}	// namespace OVR



#endif	// OVR_App

