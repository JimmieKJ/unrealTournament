// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_MESSAGE_H
#define OVR_MESSAGE_H

#include "OVR_Platform_Defs.h"
#include "OVR_AchievementDefinitionArray.h"
#include "OVR_AchievementProgressArray.h"
#include "OVR_AchievementUpdate.h"
#include "OVR_ApplicationVersion.h"
#include "OVR_CloudStorageConflictMetadata.h"
#include "OVR_CloudStorageData.h"
#include "OVR_CloudStorageMetadata.h"
#include "OVR_CloudStorageMetadataArray.h"
#include "OVR_CloudStorageUpdateResponse.h"
#include "OVR_Error.h"
#include "OVR_LeaderboardEntryArray.h"
#include "OVR_LeaderboardUpdateStatus.h"
#include "OVR_MatchmakingBrowseResult.h"
#include "OVR_MatchmakingEnqueueResult.h"
#include "OVR_MatchmakingEnqueueResultAndRoom.h"
#include "OVR_MatchmakingRoomArray.h"
#include "OVR_MatchmakingStats.h"
#include "OVR_MessageType.h"
#include "OVR_NetworkingPeer.h"
#include "OVR_OrgScopedID.h"
#include "OVR_PingResult.h"
#include "OVR_ProductArray.h"
#include "OVR_Purchase.h"
#include "OVR_PurchaseArray.h"
#include "OVR_Room.h"
#include "OVR_RoomArray.h"
#include "OVR_RoomInviteNotificationArray.h"
#include "OVR_Types.h"
#include "OVR_User.h"
#include "OVR_UserArray.h"
#include "OVR_UserProof.h"
#include <stddef.h>

typedef struct ovrMessage *ovrMessageHandle;

OVRP_PUBLIC_FUNCTION(ovrAchievementDefinitionArrayHandle)      ovr_Message_GetAchievementDefinitionArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrAchievementProgressArrayHandle)        ovr_Message_GetAchievementProgressArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrAchievementUpdateHandle)               ovr_Message_GetAchievementUpdate(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrApplicationVersionHandle)              ovr_Message_GetApplicationVersion(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrCloudStorageConflictMetadataHandle)    ovr_Message_GetCloudStorageConflictMetadata(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrCloudStorageDataHandle)                ovr_Message_GetCloudStorageData(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrCloudStorageMetadataHandle)            ovr_Message_GetCloudStorageMetadata(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrCloudStorageMetadataArrayHandle)       ovr_Message_GetCloudStorageMetadataArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrCloudStorageUpdateResponseHandle)      ovr_Message_GetCloudStorageUpdateResponse(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrErrorHandle)                           ovr_Message_GetError(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrLeaderboardEntryArrayHandle)           ovr_Message_GetLeaderboardEntryArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrLeaderboardUpdateStatusHandle)         ovr_Message_GetLeaderboardUpdateStatus(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrMatchmakingBrowseResultHandle)         ovr_Message_GetMatchmakingBrowseResult(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrMatchmakingEnqueueResultHandle)        ovr_Message_GetMatchmakingEnqueueResult(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrMatchmakingEnqueueResultAndRoomHandle) ovr_Message_GetMatchmakingEnqueueResultAndRoom(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrMatchmakingRoomArrayHandle)            ovr_Message_GetMatchmakingRoomArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrMatchmakingStatsHandle)                ovr_Message_GetMatchmakingStats(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrMessageHandle)                         ovr_Message_GetNativeMessage(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrNetworkingPeerHandle)                  ovr_Message_GetNetworkingPeer(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrOrgScopedIDHandle)                     ovr_Message_GetOrgScopedID(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrPingResultHandle)                      ovr_Message_GetPingResult(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrProductArrayHandle)                    ovr_Message_GetProductArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrPurchaseHandle)                        ovr_Message_GetPurchase(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrPurchaseArrayHandle)                   ovr_Message_GetPurchaseArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrRequest)                               ovr_Message_GetRequestID(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrRoomHandle)                            ovr_Message_GetRoom(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrRoomArrayHandle)                       ovr_Message_GetRoomArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrRoomInviteNotificationArrayHandle)     ovr_Message_GetRoomInviteNotificationArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(const char *)                             ovr_Message_GetString(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrMessageType)                           ovr_Message_GetType(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrUserHandle)                            ovr_Message_GetUser(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrUserArrayHandle)                       ovr_Message_GetUserArray(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(ovrUserProofHandle)                       ovr_Message_GetUserProof(const ovrMessageHandle obj);
OVRP_PUBLIC_FUNCTION(bool)                                     ovr_Message_IsError(const ovrMessageHandle obj);

#endif
