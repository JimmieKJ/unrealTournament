/************************************************************************************

Filename    :   SystemActivities.h
Content     :   Event handling for system activities
Created     :   February 23, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_SystemActivities_h
#define OVR_SystemActivities_h

#include "VrApi_Types.h"

// In order to correctly handle System Activities events, an application must do
// the following at a minimum:
//
// - call SystemActivities_Init() when the application initalizes
// - call SystemActivities_Update() from its frame update. This will handle
//   all system-level events sent from System Activities, such as "reorient" and 
//   "returnToLauncher".
// - call SystemActivities_PostUpdate() from its frame function, passing the
//   app event list returned from SystemActivities_Update().
// - call SystemActivities_Shutdown() when the application exits.
// 
// The normal flow for handling SystemActivities events is:
//
// The Java SystemActivitiesReceiver receives an event it adds the event to an 
// internal event queue.
//
// Each frame the application (or app framework) should:
// 1) Call SystemActivities_Update() to handle any internal events (for instance,
//    returntoLauncher) and fill the app event list with the remaining events. 
// 2) If this is a framework, pass the app event list to the application code (for 
//    example, VrAppFramework passes these through ovrFrameInput)
// 2a)The application can modify the app event list, removing any events from the
//    that it handles itself or that it doesn't want the framework to handle.
// 3) Call SystemActivities_PostUpdate() to handle the remaining app events
//    in the list.

#if defined( __cplusplus )
extern "C" {
#endif

// This must match the value declared in SystemActivityReceiver.java
#define SYSTEM_ACTIVITY_INTENT						"com.oculus.system_activity"

// Version 0 is pre-JSON.
// Version 1 is initial version.
// Version 2 added "exitToHome" event.
//		Apps built with current versions only respond to "returnToLauncher" if 
//		the Systems Activity that sent it is version 1 (meaning they'll never
//		get an "exitToHome" from System Activities).
#define PLATFORM_UI_VERSION			2

#define HOME_PACKAGE_NAME			"com.oculus.home"

// action string sent to Oculus Home to tell it to return to the root menu
#define HOME_ACTION_RETURN_TO_ROOT	"return_to_root"

// Start up the platform UI for pass-through camera, reorient, exit, etc.
// The current app will be covered up by the platform UI, but will be
// returned to when it finishes.
#define PUI_CLASS_NAME				"com.oculus.systemactivities.PlatformActivity"
#define PUI_PACKAGE_NAME			"com.oculus.systemactivities"

// System Activities event identifiers
#define	SYSTEM_ACTIVITY_EVENT_REORIENT				"reorient"
#define	SYSTEM_ACTIVITY_EVENT_KEYBOARD_RESULT		"keyboardResult"
#define	SYSTEM_ACTIVITY_EVENT_FILE_DIALOG_RESULT	"fileDialogResult"
#define SYSTEM_ACTIVITY_EVENT_RETURN_TO_LAUNCHER	"returnToLauncher"
#define SYSTEM_ACTIVITY_EVENT_EXIT_TO_HOME			"exitToHome"

//-----------------------------------------------------------------
// Back Button handling
//-----------------------------------------------------------------

// Pressing the back button twice within BUTTON_DOUBLE_TAP_TIME_IN_SECONDS
// is a double tap. A single press and release of the back button in less than
// BUTTON_SHORT_PRESS_TIME_IN_SECONDS is a short press. Holding the back
// button for longer than BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS is considered
// a long press. Anything in between a short press and a long press is considered
// a cancellation of a long press.

// TODO: These really have nothing to do with SystemActivities.h but they're
// used in apps using VrAppFramework and also in apps (VrCubeWorld_NativeActivity,
// etc.) using VrApi directly.
#define BUTTON_DOUBLE_TAP_TIME_IN_SECONDS		0.25f
#define BUTTON_SHORT_PRESS_TIME_IN_SECONDS		0.25f
#define BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS	0.75f
#define MENU_BUTTON_LONG_PRESS_TIME_IN_SECONDS	0.475f // this is != 0.75f because of a bug in S7 Android M firmware that forces an up event at 0.5 seconds.

//==============================================================
// App Events
// Most System Activities events will be passed on to the application
// when SystemActivities_Update() is called. The application (or application
// framework) can decide to handle or ignore these events. In the case
// of VrAppFramework, the framework passes the app event list through 
// VrAppInterface::Frame() first, allowing the application to handle
// and consume any app events it wishes, and then processes the app event
// list that remains after VrAppInterface::Frame() completes.
#define SYSTEM_ACTIVITIES_MAX_APP_EVENTS	16

typedef struct 
{
	char const * 	Events[SYSTEM_ACTIVITIES_MAX_APP_EVENTS];
	int				NumEvents;
} SystemActivitiesAppEventList_t;

// C interface for managing an app event list. If an application wants to consume a System Activities event
// it must remove the event from the SystemActivitiesEventList_t that is passed via VrAppInterface::Frame()
// using SystemActivities_RemoveAppEvent() so that the framework will

// Adds an event to the specified event list.
bool SystemActivities_AppendAppEvent( SystemActivitiesAppEventList_t * eventList, char const * buffer );
// Removes an event from the specified event list.
void SystemActivities_RemoveAppEvent( SystemActivitiesAppEventList_t * eventList, int const index );
// Frees all items in the specified event list. This does not free the eventList structure itself!
//void SystemActivities_FreeAppEventListItems( SystemActivitiesAppEventList_t * eventList );

//==============================================================================================
// System Activities Required Functions
//==============================================================================================

// Initialize SystemActivities_ state.
void SystemActivities_Init( ovrJava * java );

// SystemActivities_Update will return a list of System Activities events that the 
// framework (or app) can handle.
void SystemActivities_Update( ovrMobile * ovr, const ovrJava * java, SystemActivitiesAppEventList_t * appEvents );
void SystemActivities_PostUpdate( ovrMobile * ovr, const ovrJava * java, SystemActivitiesAppEventList_t * appEvents );

// Clean up SystemActivities_ state.
void SystemActivities_Shutdown( ovrJava * java );

//==============================================================================================
// System Activities Optional Functions
// The use of these functions is up to individual applications.
//==============================================================================================

//-----------------------------------------------------------------
// System Activity Commands
//-----------------------------------------------------------------

#define PUI_GLOBAL_MENU				"globalMenu"
#define PUI_GLOBAL_MENU_TUTORIAL	"globalMenuTutorial"
#define PUI_CONFIRM_QUIT			"confirmQuit"
#define PUI_THROTTLED1				"throttled1"	// Warn that Power Save Mode has been activated
#define PUI_THROTTLED2				"throttled2"	// Warn that Minimum Mode has been activated
#define PUI_HMT_UNMOUNT				"HMT_unmount"	// the HMT has been taken off the head
#define PUI_HMT_MOUNT				"HMT_mount"		// the HMT has been placed on the head
#define PUI_WARNING					"warning"		// the HMT has been placed on the head and a warning message shows
#define PUI_FAIL_MENU				"failMenu"		// display a FAIL() message in the System Activities
#define PUI_KEYBOARD_MENU			"keyboardMenu"	// bring up a keyboard to edit a single string, send string back to calling app when done
#define PUI_FILE_DIALOG				"fileDialog"	// bring up a folder browser to select the path to a file or folder.

// TODO: why are both this and CreateSystemActivitiesIntent exposed?
bool SystemActivities_CreateSystemActivitiesCommand( const char * toPackageName, const char * command, 
		const char * jsonExtra, const char * uri, char * outBuffer, unsigned int outBufferSize );

// Used by system activities to send an event back to the activity that launched it.
void SystemActivities_BroadcastSystemActivityEvent( const ovrJava * java, const char * actionName, 
		const char * toPackageName, const char * toClassName, const char * command, const char * jsonExtra, 
		const char * uri );

// Sends a System Activity intent containing the command and extra JSON text.
bool SystemActivities_StartSystemActivity( const ovrJava * java, const char * command, const char * extraJsonText );

// Creates a command for sending to System Activities with optional embedded extra JSON text.
// Fills outBuffer with a JSON text object with the required versioning info, the passed command, 
// and embedded extraJsonText.
// This is normally only used by SystemActivities_StartSystemActivity, but it's also used in the
// SystemActivities app to synthesize an intent when SA is entered without an intent (which can 
// happen in debug runs and also if an app crashes and SA is next on the app stack).
bool SystemActivities_CreateSystemActivityIntent( const char * command, const char * extraJsonText, 
		char * outBuffer, unsigned int const outBufferSize, unsigned int * outRequiredBufferSize );


//-----------------------------------------------------------------
// Error handling
//-----------------------------------------------------------------

typedef enum
{
	SYSTEM_ACTIVITIES_FATAL_ERROR_OUT_OF_MEMORY,
	SYSTEM_ACTIVITIES_FATAL_ERROR_OUT_OF_STORAGE,
	SYSTEM_ACTIVITIES_FATAL_ERROR_OSIG,
	SYSTEM_ACTIVITIES_FATAL_ERROR_MISC
} ovrSystemActivitiesFatalError;

// messages name sent to System Activities via intent.
#define ERROR_MSG_OUT_OF_MEMORY		"failOutOfMemory"
#define ERROR_MSG_OUT_OF_STORAGE	"failOutOfStorage"
#define ERROR_MSG_OSIG				"failOSig"

// Sends a System Activity intent containing the command and extra JSON text.
// This should only be used in the fatal error case.
void SystemActivities_DisplayError( const ovrJava * java, const ovrSystemActivitiesFatalError error, 
		const char * fileName, const char * messageFormat, ... );

// Launches the Oculus Home app, or brings it to the front if already running.
// Issues an intent to start Oculus Home and then exits the application. On Android OS this executes
// a finishAffinity. 
void SystemActivities_ReturnToHome( const ovrJava * java );

// Sends and intent to another application built with VrApi.
void SystemActivities_SendIntent( const ovrJava * java, const char * actionName,
		const char * toPackageName, const char * toClassName, const char * command, 
		const char * uri );

// TODO: this is exposed for the SystemActivities application only right now. It could be removed
// from the interface, but that would require duplication of the Java code in both places because
// this is also used internally in SystemActivities_ReturnToHome.
// Sends a launch intent to the specified package.
// Then the destination package will be queried for its launch intent. If action is not NULL, then
// the intent's action will be set to the string pointed to by action.
void SystemActivities_SendLaunchIntent( const ovrJava * java, const char * toPackageName, 
		const char * command, const char * uri, const char * action );

#if defined( __cplusplus )
} // extern "C"
#endif

#endif // OVR_SystemActivities_h
