// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_PLATFORM_INTERNAL_H
#define OVR_PLATFORM_INTERNAL_H

#include "OVR_Platform.h"

#ifdef __ANDROID__
#include <jni.h>
#endif // __ANDROID__

// Init with a specific request URI
//
OVRP_PUBLIC_FUNCTION(void) ovr_PlatformInitializeStandaloneAccessTokenWithRequestURI(
  const char* requestURIPrefix,
  const char* accessToken);

OVRP_PUBLIC_FUNCTION(void) ovr_CrashApplication();

typedef enum ovrMessageTypeInternal_ {
  ovrMessage_GraphAPI_Get                    = 0x30FF006E,
  ovrMessage_GraphAPI_Post                   = 0x76A5A7C4,
  ovrMessage_HTTP_Get                        = 0x6FB63223,
  ovrMessage_HTTP_GetToFile                  = 0x4E81DC59,
  ovrMessage_HTTP_Post                       = 0x6B36A54F,
  ovrMessage_Room_CreateOrUpdateAndJoinNamed = 0x7C8E0A91,
  ovrMessage_Room_GetNamedRooms              = 0x077D6E8C,
  ovrMessage_Room_GetSocialRooms             = 0x61881D76,
  ovrMessage_User_NewEntitledTestUser        = 0x11741F03,
  ovrMessage_User_NewTestUser                = 0x36E84F8C,
  ovrMessage_User_NewTestUserFriends         = 0x1ED726C7
} ovrMessageTypeInternal;

typedef enum ovrCloudStorageLocation_ {
  ovrCloudStorageLocation_Local = 0,
  ovrCloudStorageLocation_Remote = 1,
} ovrCloudStorageLocation;

// In normal VR apps, apps have to quit and go to Horizon before a user can logout.
// An in-memory cache is sufficient.
//
// Horizon however stays in memory throughout the entire logout process
OVRP_PUBLIC_FUNCTION(void) ovr_LogoutUser();

#ifdef _WIN32
/// DEPRECATED: This method causes deadlocks on Android.
/// Synchronous method that determines whether a user is entitled to the app.
/// Note: this can take ~100 milliseconds. Never call on a perf-critical thread.
/// Use ovr_Entitlement_GetIsViewerEntitled if you want an asynchronous implementation
/// or more detailed information on failure.
OVRPL_PUBLIC_FUNCTION(bool) ovr_IsEntitled();

// Alternative methods to initialize the SDK with Correct User Agent.
// Only used internally by our integrations.
OVRP_PUBLIC_FUNCTION(ovrPlatformInitializeResult) ovr_PlatformInitializeUnityWindows(const char* appId);
OVRP_PUBLIC_FUNCTION(ovrPlatformInitializeResult) ovr_PlatformInitializeUnrealWindows(const char* appId);
#endif // _WIN32

#ifdef __ANDROID__
OVRP_PUBLIC_FUNCTION(ovrPlatformInitializeResult) ovr_PlatformInitializeUnityAndroid(const char* appId, jobject activityObject, JNIEnv * jni);
#endif // _Android

typedef struct {
  const char * key;
  const char * value;
} ovrPlatformLogValue;

// Send an APPLICATION log event to marauder through Horizon or OAF. Only allowed for internal apps.
// This should not be used for PlatformSDK logs. See PlatformIntegration->SendPlatformLogsToMarauder();
//
//  For Windows:
//  the following value keys are special:
//   name is set to "oculus_app_event"
//   source_app_id is set to the ID from initializing the Platform SDK
//   log_channel is set to "OculusApps"
//  OAF expects some other keys to have specific meaning (e.g. log_level, log_to_console, etc)
//  so consult the OAF endpoint docs to use properly.
//
// For Android:
//  Name is set to oculus_mobile_platform_sdk
//  Horizon does all the rest of the magic.
//  Nothing special. Horizon does all the magic.
OVRP_PUBLIC_FUNCTION(void) ovr_Log_NewEvent(const char *eventName, const ovrPlatformLogValue* values, size_t length);

// DEPRECATED DEPRECATED DEPRECATED.
OVRP_PUBLIC_FUNCTION(void) ovr_Log_Event(const ovrPlatformLogValue* values, size_t length);


typedef struct ovrVoipCodecOptions *ovrVoipCodecOptionsHandle;
OVRP_PUBLIC_FUNCTION(ovrVoipEncoderHandle) ovr_Voip_CreateEncoderWithOptions(ovrVoipCodecOptionsHandle options);

OVRP_PUBLIC_FUNCTION(void) ovr_Voip_SetMicrophoneFilterCallback(VoipFilterCallback cb);

//Declared for the ShimWow Loader
#ifdef __ANDROID__
OVRP_PUBLIC_FUNCTION(bool) ovr_UnityInitWrapperInternal(const char *appId, jobject activity, JNIEnv *env);
#endif // __ANDROID__
OVRP_PUBLIC_FUNCTION(bool) ovr_UnityInitWrapper(const char *appId);
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_HTTP_GetWithMessageType(const char *url, int messageType);
OVRP_PUBLIC_FUNCTION(void) ovr_Voip_Start(ovrID user);
OVRP_PUBLIC_FUNCTION(void) ovr_Voip_Stop(ovrID user);
OVRP_PUBLIC_FUNCTION(void) ovr_Voip_Accept(ovrID user);
OVRP_PUBLIC_FUNCTION(size_t) ovr_Voip_GetPCM(ovrID senderID, int16_t *outputBuffer, size_t outputBufferNumElements);
OVRP_PUBLIC_FUNCTION(size_t) ovr_Voip_GetPCMFloat(ovrID senderID, float *outputBuffer, size_t outputBufferNumElements);
OVRP_PUBLIC_FUNCTION(bool) ovr_UnityInitWrapperStandalone(const char *appID, LogFunctionPtr logptr);
OVRP_PUBLIC_FUNCTION(void) ovr_UnityResetTestPlatform();

// *** Graph API Helpers
//
// Used to help 1st Party apps make GraphAPI calls without needing their own network
// stack. This automatically appends an Oculus access token to the http request.

// Make an authenticated HTTP GET request to graph.oculus.com/{url}
// 
// Example "me"
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_GraphAPI_Get(const char *url);

// Make an authenticated HTTP POST request to graph.oculus.com/{url}
// 
// Example "room_moderated_create"
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_GraphAPI_Post(const char *url);

// *** Http Helpers
//
// Used to help 1st Party apps make arbitrary API calls without needing their own
// network stack. This is very useful for APIs not hosted still hosted on FB infra
// but not exposed via GraphAPI/GraphQL. This is not to be used for arbitrary APIs
// because it still has an Oculus Access token attached.

// Make an authenticated HTTP GET call to the specified complete URL.
// 
// Example: www2.oculus.com/cinema_video_proxy?access_token=$TOKEN
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_HTTP_Get(const char *url);

// Make an authenticated HTTP GET call to the specified complete URL. Save URL
// contents to diskFile.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_HTTP_GetToFile(const char *url, const char *diskFile);

// Make an authenticated HTTP POST call to the specified complete URL.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_HTTP_Post(const char *url);

// Creates a new named (client controlled) room or updates it (if it already
// exists) and adds the caller to it. This type of room is not deleted when
// empty; the owner is always the user that created it.
// \param name The name of the room.
// \param joinPolicy Specifies who can join the room without an invite.
// \param maxUsers The maximum number of users allowed in the room, including the creator.
// \param subscribeToUpdates If true, sends a message with type ovrMessage_RoomUpdateNotification when room data changes, such as when users join or leave.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Room_CreateOrUpdateAndJoinNamed(const char *name, ovrRoomJoinPolicy joinPolicy, unsigned int maxUsers, bool subscribeToUpdates);

// Get the list of named rooms created by a user with a given ID for the
// application.
// 
// NOTE: Users will have a unique ID per application.
// \param userID User ID retrieved with this application.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Room_GetNamedRooms(ovrID userID);

// Does the same thing as GetModeratedRooms, but lets you specify which app.
// Used in Social to read rooms across applications.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Room_GetSocialRooms(ovrID appID);

// Return a new test user, as above, that is entitled to all of the app's IAP
// items.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_NewEntitledTestUser();

// Return a new omni test user on a temporary db. Because these are transient,
// these users will eventually be automatically cleaned up. These users should
// only be used in tests.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_NewTestUser();

// Return an array of omni test user on a temporary db. Because these are
// transient, these users will eventually be automatically cleaned up. These
// users should only be used in tests. These two users are friends.
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_User_NewTestUserFriends();


#endif  // OVR_PLATFORM_INTERNAL_H
