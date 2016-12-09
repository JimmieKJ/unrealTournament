// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_PLATFORM_H
#define OVR_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "OVR_Types.h"
#include "OVR_MessageType.h"
#include "OVR_Message.h"
#include "OVR_DataStore.h"
#include "OVR_Error.h"
#include "OVR_Networking.h"
#include "OVR_NetworkingPeer.h"
#include "OVR_UserProof.h"
#include "OVR_Platform_Defs.h"
#include "OVR_PlatformVersion.h"
#include "OVR_PidArray.h"
#include "OVR_InstalledApplicationArray.h"
#include "OVR_Product.h"
#include "OVR_ProductArray.h"
#include "OVR_Purchase.h"
#include "OVR_PurchaseArray.h"
#include "OVR_Room.h"
#include "OVR_RoomJoinPolicy.h"
#include "OVR_RoomArray.h"
#include "OVR_User.h"
#include "OVR_AchievementDefinition.h"
#include "OVR_AchievementDefinitionArray.h"
#include "OVR_AchievementProgress.h"
#include "OVR_AchievementProgressArray.h"
#include "OVR_AchievementUpdate.h"
#include "OVR_ApplicationLifecycle.h"
#include "OVR_CloudStorageConflictMetadata.h"
#include "OVR_CloudStorageData.h"
#include "OVR_CloudStorageMetadataArray.h"
#include "OVR_LeaderboardEntry.h"
#include "OVR_LeaderboardEntryArray.h"
#include "OVR_LeaderboardUpdateStatus.h"
#include "OVR_LeaderboardStartAt.h"
#include "OVR_LeaderboardFilterType.h"
#include "OVR_KeyValuePairType.h"
#include "OVR_MatchmakingAdminSnapshot.h"
#include "OVR_MatchmakingBrowseResult.h"
#include "OVR_MatchmakingEnqueueResult.h"
#include "OVR_MatchmakingEnqueueResultAndRoom.h"
#include "OVR_Voip_LowLevel.h"
#include "OVR_Requests_Achievements.h"
#include "OVR_Requests_Application.h"
#include "OVR_Requests_ApplicationLifecycle.h"
#include "OVR_Requests_CloudStorage.h"
#include "OVR_Requests_Entitlement.h"
#include "OVR_Requests_IAP.h"
#include "OVR_Requests_Leaderboard.h"
#include "OVR_Requests_Matchmaking.h"
#include "OVR_Requests_Notification.h"
#include "OVR_Requests_Room.h"
#include "OVR_Requests_User.h"

#ifdef __ANDROID__
#include <jni.h>
OVRP_PUBLIC_FUNCTION(ovrPlatformInitializeResult) ovr_PlatformInitializeAndroid(const char* appId, jobject activityObject, JNIEnv * jni);
#endif

OVRPL_PUBLIC_FUNCTION(void) ovr_PlatformInitializeStandaloneAccessToken(const char* accessToken);

#if defined(OVRPL_DISABLED)

/// Performs the initialization of the platform for use on Windows. Requires your app
/// ID (not access token). This call may fail for a variety of reasons, and will return
/// an error code in that case. It is critical to respect this error code and either
/// exit or make no further platform calls.
OVRPL_PUBLIC_FUNCTION(ovrPlatformInitializeResult) ovr_PlatformInitializeWindows(const char* appId);

#else

/// Performs the initialization of the platform for use on Windows. Requires your app
/// ID (not access token). This call may fail for a variety of reasons, and will return
/// an error code in that case. It is critical to respect this error code and either
/// exit or make no further platform calls.
OVRPL_PUBLIC_FUNCTION(ovrPlatformInitializeResult) ovr_PlatformInitializeWindowsEx(const char* appId, int productVersion, int majorVersion);

/// Wrapper for ovr_PlatformInitializeWindowsEx that automatically passes the key version
/// information as defined in this header package. This is used to ensure that the version
/// in your headers matches the version of the static library.
#define ovr_PlatformInitializeWindows(appId) ovr_PlatformInitializeWindowsEx((appId), PLATFORM_PRODUCT_VERSION, PLATFORM_MAJOR_VERSION)

#endif

/// Returns the id of the currently logged in user, or a 0 id of there
/// is none.
OVRP_PUBLIC_FUNCTION(ovrID) ovr_GetLoggedInUserID();

/// Return the next message in the queue (FIFO order), or null if none
/// is available.  Safe to call on any thread.  Each returned message
/// should eventually be freed with ovr_FreeMessage.
///
/// TODO: comment on whether it's safe to process messages out of
/// order.
OVRPL_PUBLIC_FUNCTION(ovrMessageHandle) ovr_PopMessage();

OVRP_PUBLIC_FUNCTION(void) ovr_FreeMessage(ovrMessageHandle);

OVRP_PUBLIC_FUNCTION(void) ovr_SetDeveloperAccessToken(const char *accessToken);

#endif
