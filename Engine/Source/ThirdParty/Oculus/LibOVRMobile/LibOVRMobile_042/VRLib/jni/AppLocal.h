/************************************************************************************

Filename    :   App.h
Content     :   Native counterpart to VrActivity
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_AppLocal_h
#define OVR_AppLocal_h

#include "App.h"
#include "SoundManager.h"

namespace OVR {

class OvrGuiSys;
class GazeCursor;
class OvrVolumePopup;

//==============================================================
// AppLocal
//
// NOTE: do not define any of the functions in here inline (inside of the class 
// definition).  When AppLocal.h is included in multiple files (App.cpp and AppRender.cpp)
// this causes bugs with accessor functions.
class AppLocal : public App
{
public:
								AppLocal( JNIEnv & jni_, jobject activityObject_,
										VrAppInterface & interface_ );
	virtual						~AppLocal();

	virtual MessageQueue &		GetMessageQueue();

	virtual VrAppInterface *	GetAppInterface();

	virtual	void 				DrawEyeViewsPostDistorted( Matrix4f const & viewMatrix, const int numPresents = 1);

	virtual void				CreateToast( const char * fmt, ... );

	virtual void				PlaySound( const char * name );

	virtual void				SendIntent( const char * package, const char * data, eExitType exitType = EXIT_TYPE_FINISH_AFFINITY );
	virtual void				ReturnToLauncher();
	virtual void				StartPlatformUI( const char * intent = NULL );
	virtual void				ExitPlatformUI();
	virtual void				RecenterYaw( const bool showBlack );

	//-----------------------------------------------------------------

	virtual	EyeParms			GetEyeParms();
	virtual void				SetEyeParms( const EyeParms parms );

	//-----------------------------------------------------------------
	// interfaces
	//-----------------------------------------------------------------
	virtual OvrGuiSys &         	GetGuiSys();
	virtual OvrGazeCursor  &    	GetGazeCursor();
	virtual BitmapFont &        	GetDefaultFont();
	virtual BitmapFontSurface & 	GetWorldFontSurface();
	virtual BitmapFontSurface & 	GetMenuFontSurface();
	virtual OvrVRMenuMgr &      	GetVRMenuMgr();
	virtual OvrDebugLines &     	GetDebugLines();
	virtual const OvrStoragePaths & GetStoragePaths();

	//-----------------------------------------------------------------
	// pass-through camera
	//-----------------------------------------------------------------
	virtual void				PassThroughCamera( bool enable );
	virtual void				PassThroughCameraExposureLock( bool enable );
	virtual bool				IsPassThroughCameraEnabled() const;

	//-----------------------------------------------------------------
	// system settings
	//-----------------------------------------------------------------
	virtual	int					GetSystemBrightness() const;
	virtual void				SetSystemBrightness( int const v );

	virtual	bool				GetComfortModeEnabled() const;
	virtual	void				SetComfortModeEnabled( bool const enabled );

	virtual int					GetWifiSignalLevel() const;
	virtual eWifiState			GetWifiState() const;
	virtual int					GetCellularSignalLevel() const;
	virtual eCellularState		GetCellularState() const;

	virtual bool				IsAirplaneModeEnabled() const;

	virtual void				SetDoNotDisturbMode( bool const enable );
	virtual bool				GetDoNotDisturbMode() const;

	virtual bool				GetBluetoothEnabled() const;
	
	//-----------------------------------------------------------------
	// accessors
	//-----------------------------------------------------------------

	virtual bool				IsAsynchronousTimeWarp() const;
	virtual	bool				GetHasHeadphones() const;
	virtual	bool				GetFramebufferIsSrgb() const;
	virtual bool				GetRenderMonoMode() const;
	virtual void				SetRenderMonoMode( bool const mono );

	virtual char const *		GetPackageCodePath() const;
	virtual bool				IsPackageInstalled( const char * packageName ) const;
	virtual char const *		GetLanguagePackagePath() const;

	virtual Matrix4f const &	GetLastViewMatrix() const;
	virtual void				SetLastViewMatrix( Matrix4f const & m );

	virtual EyeParms &			GetVrParms();
	virtual ovrModeParms 		GetVrModeParms();
	virtual void				SetVrModeParms( ovrModeParms parms );

	virtual VrViewParms const &	GetVrViewParms() const;
	virtual void				SetVrViewParms( VrViewParms const & parms );

	virtual float				GetCameraFovHorizontal() const;
	virtual void				SetCameraFovHorizontal( float const f );
	virtual float				GetCameraFovVertical() const;
	virtual void				SetCameraFovVertical( float const f );

	virtual	void				SetPopupDistance( float const d );
	virtual float				GetPopupDistance() const;
	virtual	void				SetPopupScale( float const s );
	virtual float				GetPopupScale() const;

	virtual int					GetCpuLevel() const;
	virtual int					GetGpuLevel() const;

	virtual bool				GetPowerSaveActive() const;

	virtual	int					GetBatteryLevel() const;
	virtual	eBatteryStatus		GetBatteryStatus() const;

	virtual bool				IsGuiOpen() const;

	virtual char const *		GetDashboardPackageName() const;

	virtual KeyState &          GetBackKeyState();

	virtual ovrMobile *			GetOvrMobile();

	virtual void				SetShowVolumePopup( bool const show );
	virtual bool				GetShowVolumePopup() const;

	virtual const char *		GetPackageName() const;

	//-----------------------------------------------------------------
	// Java accessors
	//-----------------------------------------------------------------

	virtual JavaVM	*			GetJavaVM();
	virtual JNIEnv	*			GetUiJni();
	virtual JNIEnv	*			GetVrJni();
	virtual jobject	&			GetJavaObject();
	virtual jclass	&			GetVrActivityClass();
	virtual jclass  &			GetSpeechClass();
	virtual jmethodID &			GetSpeechStartMethodId();
	virtual jmethodID &			GetSpeechStopMethodId();

	//-----------------------------------------------------------------

	// Every application gets a basic dialog
	// and a pass through camera surface.
	virtual SurfaceTexture *	GetDialogTexture();
	virtual SurfaceTexture *	GetCameraTexture();

	//----------------------------------------------------------

	// Android activity display parameters
	virtual UiPanel	&			GetActivityPanel();
	virtual GlGeometry const &	GetUnitSquare() const;

	virtual GlProgram &			GetExternalTextureProgram2();

	//-----------------------------------------------------------------

	virtual ovrHmdInfo const &	GetHmdInfo() const;

	//-----------------------------------------------------------------
	// TimeWarp
	//-----------------------------------------------------------------
	virtual TimeWarpParms const & GetSwapParms() const;
	virtual TimeWarpParms &		GetSwapParms();

	//-----------------------------------------------------------------
	// debugging
	//-----------------------------------------------------------------
	virtual void				SetShowFPS( bool const show );
	virtual bool				GetShowFPS() const;

	virtual	void				ShowInfoText( float const duration, const char * fmt, ... );

	virtual ovrSensorState const & GetSensorForNextWarp() const;

	// Primary apps will exit(0) when they get an onDestroy() so we
	// never leave any cpu-sucking process running, but the platformUI
	// needs to just return to the primary activity.
	bool			ExitOnDestroy;
	volatile bool	OneTimeInitCalled;
	volatile bool	VrThreadSynced;
	volatile bool	CreatedSurface;
	volatile bool	ReadyToExit;		// start exit procedure

	// Most calls in from java should communicate through this.
	MessageQueue	vrMessageQueue;

	// From EnterVrMode, used for WarpSwap and LeaveVrMode
	ovrMobile *		OvrMobile;

	// Egl context and surface for rendering
	eglSetup_t		eglr;

	// Handles creating, destroying, and re-configuring the buffers
	// for drawing the eye views, which might be in different texture
	// configurations for CPU warping, etc.
	EyeBuffers *	EyeTargets;

	JavaVM *		javaVM;

	JNIEnv *		UiJni;			// for use by the Java UI thread
	JNIEnv *		VrJni;			// for use by the VR thread

	jobject			javaObject;
	jobject			PassThroughCameraObject;

	jclass			vrActivityClass;		// must be looked up from main thread or FindClass() will fail
	jclass			PlatformActivityClass;	// must be looked up from main thread or FindClass() will fail
	jclass			passThroughCameraClass;	// must be looked up from main thread or FindClass() will fail
	jclass			speechClass;			// must be looked up from main thread or FindClass() will fail
	jclass			VrLibClass;				// must be looked up from main thread or FindClass() will fail

	jmethodID		enableCameraPreviewMethodId;
	jmethodID		disableCameraPreviewMethodId;
	jmethodID		startExposureLockMethodId;
	jmethodID		finishActivityMethodId;
	jmethodID		createVrToastMethodId;
	jmethodID		clearVrToastsMethodId;
	jmethodID		playSoundPoolSoundMethodId;
	jmethodID		sendIntentMethodId;
	jmethodID		gazeEventMethodId;
	jmethodID		speechStartMethodId;
	jmethodID		speechStopMethodId;
	jmethodID		setSysBrightnessMethodId;
	jmethodID		getSysBrightnessMethodId;
	jmethodID		enableComfortViewModeMethodId;
	jmethodID		getComfortViewModeMethodId;
	jmethodID		isPackageInstalledMethodId;
	jmethodID		setDoNotDisturbModeMethodId;
	jmethodID		getDoNotDisturbModeMethodId;
	jmethodID		getBluetoothEnabledMethodId;
	jmethodID		isAirplaneModeEnabledMethodId;

	String			launchIntent;		// empty string if launched without data
	String			packageCodePath;	// path to apk to open as zip and load resources
	String			packageName;		// package name 

	bool			demoMode;			// limit commands for best public demo
	bool			Paused;				// set/cleared by onPause / onResume

	float 			popupDistance;
	float 			popupScale;

	VrAppInterface * appInterface;

	// Every application gets a basic dialog
	// and a pass through camera surface.
	SurfaceTexture * dialogTexture;
	SurfaceTexture * cameraTexture;

	// Update for every camera frame, accounting for an
	// estimate of the camera latency.
	long long		lastCameraUpdateNanoTime;
	ovrSensorState	cameraFramePose[2];		// top / bottom of frame
	int				enableCameraTimeWarp;	// 0 = none, 1 = full frame, 2 = interpolated
	float			cameraLatency;			// assumed offset from timestamp
	float			cameraRollTime;			// amount to de-waggle the rolling shutter effect

	// Current joypad state, without pressed / released calculation
	VrInput			joypad;

	// drawing parameters
	int				dialogWidth;
	int				dialogHeight;
	float			dialogStopSeconds;

	// Dialogs will be oriented base down in the view when they
	// were generated.
	Matrix4f		dialogMatrix;

	// Android activity display parameters
	UiPanel			activityPanel;

	Matrix4f		lastViewMatrix;

	float			cameraFovHorizontal;
	float			cameraFovVertical;

	ANativeWindow * nativeWindow;
	EGLSurface 		windowSurface;

	bool			drawCalibrationLines;	// currently toggled by right trigger
	bool			calibrationLinesDrawn;	// after draw, go to static time warp test
	bool			showVignette;			// render the vignette

	ovrHmdInfo		hmdInfo;

	bool			FramebufferIsSrgb;		// requires KHR_gl_colorspace

	// Only render a single eye view, which will get warped for both
	// screen eyes.
	bool			renderMonoMode;

	VrFrame			vrFrame;
	VrFrame			lastVrFrame;

	EyeParms		vrParms;
	ovrModeParms	VrModeParms;

	TimeWarpParms	SwapParms;			// passed to TimeWarp->WarpSwap()

	GlProgram		externalTextureProgram2;
	GlProgram		interpolatedCameraWarp;
	GlProgram		untexturedMvpProgram;
	GlProgram		untexturedScreenSpaceProgram;

	GlGeometry		unitCubeLines;		// 12 lines that outline a 0 to 1 unit cube, intended to be scaled to cover bounds.
	GlGeometry		panelGeometry;		// used for dialogs and camera pass through
	GlGeometry		unitSquare;			// -1 to 1 in x and Y, 0 to 1 in texcoords

	EyePostRender	EyeDecorations;

	ovrSensorState	SensorForNextWarp;

	pthread_t		VrThread;			// posix pthread
	int				VrThreadTid;		// linux tid

	bool			PassThroughCameraEnabled;	// true if the pass-through camera is on
	bool			EnablePassThroughCameraOnResume;	// true if the pass-through camera was enabled when we paused

	// For running java commands on another thread to
	// avoid hitches.
	TalkToJava		Ttj;

	int				BatteryLevel;		// charge level of the batter as reported from Java
	eBatteryStatus	BatteryStatus;		// battery status as reported from Java

	bool			ShowFPS;			// true to show FPS on screen
	bool			ShowVolumePopup;	// true to show volume popup when volume changes

	VrViewParms		ViewParms;

	String			InfoText;			// informative text to show in front of the view
	long long		InfoTextEndFrame;	// time to stop showing text

	float 			touchpadTimer;
	Vector2f		touchOrigin;
	float 			lastTouchpadTime;
	bool 			lastTouchDown;
	int 			touchState;

	bool			enableDebugOptions;	// enable debug key-commands for development testing

	// Manages sound assets
	OvrSoundManager	SoundManager;

	//-----------------------------------------------------------------

	// Read a file from the apk zip file.  Free buffer with free() when done.
	// Files put in the eclipse res/raw directory will be found as "res/raw/<NAME>"
	// Files put in the eclipse assets directory will be found as "assets/<name>"
	// The filename comparison is case insensitive.
	void 			ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer );

	//-----------------------------------------------------------------

	Matrix4f		 MatrixInterpolation( const Matrix4f & startMatrix, const Matrix4f & endMatrix, double t );

	// Handle development input options
	void 			FrameworkButtonProcessing( const VrInput & input );

	// Called by jni functions
	void			KeyEvent( const int keyCode, const bool down, const int keyCount );

	// One time init of GL objects.
	void			InitGlObjects();
	void			ShutdownGlObjects();

	void			DrawDialog( const Matrix4f & mvp );
	void			DrawActivity( const Matrix4f & mvp );
	void			DrawPanel( const GLuint externalTextureId, const Matrix4f & dialogMvp, const float alpha );

	void			DrawBounds( const Vector3f &mins, const Vector3f &maxs, const Matrix4f &mvp, const Vector3f &color );
	void			DrawPassThroughCamera( const float bufferFov, const Quatf &orientation );

	static void *	ThreadStarter( void * parm );
	void 			VrThreadFunction();

	// Process commands forwarded from other threads.
	// Commands can be processed even when the window surfaces
	// are not setup.
	//
	// The msg string will be freed by the framework after
	// command processing.
	void    		Command( const char * msg );

	// Shut down various subsystems
	void			Pause();

	// Restart various subsusyems, possibly with new parameters
	// Called on surface creation and on resume from pause
	void			Resume();

	// Start, stop and sync the VrThread.
	void			StartVrThread();
	void			StopVrThread();
	void			SyncVrThread();

	// One time init for App framework on the VR thread.
	void 			InitVrThread();

	// One time shutdown for App framework on the VR thread.
	void 			ShutdownVrThread();

	// Open the apk on startup for resource loading
	void			OpenApplicationPackage();

	void			InterpretTouchpad( VrInput & input );
	
	jmethodID 		GetMethodID( const char * name, const char * signature ) const;
	jmethodID		GetMethodID( jclass clazz, const char * name, const char * signature ) const;
	jmethodID		GetStaticMethodID( jclass cls, const char * name, const char * signature ) const;

	// TalkToJavaInterface
	virtual void	TtjCommand( JNIEnv & jni, const char * commandString );

	jclass			GetGlobalClassReference( const char * className ) const;

	String			GetInstalledPackagePath( const char * packageName ) const;

    OvrGuiSys *         GuiSys;
    OvrGazeCursor *     GazeCursor;
    BitmapFont *        DefaultFont;
    BitmapFontSurface * WorldFontSurface;
    BitmapFontSurface * MenuFontSurface;
    OvrVRMenuMgr *      VRMenuMgr;
    OvrVolumePopup *	VolumePopup;
    OvrDebugLines *     DebugLines;
    KeyState            BackKeyState;
	OvrStoragePaths*	StoragePaths;
	String				LanguagePackagePath;

private:
    void                InitFonts();
    void                ShutdownFonts();
};

} // namespace OVR

#endif // OVR_AppLocal_h
