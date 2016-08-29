// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_MESSAGETYPE_H
#define OVR_MESSAGETYPE_H

#include "OVR_Platform_Defs.h"

typedef enum ovrMessageType_ {
  /// This value should never appear on the message queue. If it does,
  /// it indicates that something has gone horribly, horribly wrong.
  ovrMessage_Unknown,

  ovrMessage_Achievements_AddCount                               = 0x03E76231, ///< Generated in response to ovr_Achievements_AddCount()
  ovrMessage_Achievements_AddFields                              = 0x14AA2129, ///< Generated in response to ovr_Achievements_AddFields()
  ovrMessage_Achievements_GetAllDefinitions                      = 0x03D3458D, ///< Generated in response to ovr_Achievements_GetAllDefinitions()
  ovrMessage_Achievements_GetAllProgress                         = 0x4F9FDE1D, ///< Generated in response to ovr_Achievements_GetAllProgress()
  ovrMessage_Achievements_GetDefinitionsByName                   = 0x629101BC, ///< Generated in response to ovr_Achievements_GetDefinitionsByName()
  ovrMessage_Achievements_GetNextAchievementDefinitionArrayPage  = 0x2A7DD255, ///< Generated in response to ovr_Achievements_GetNextAchievementDefinitionArrayPage()
  ovrMessage_Achievements_GetNextAchievementProgressArrayPage    = 0x2F42E727, ///< Generated in response to ovr_Achievements_GetNextAchievementProgressArrayPage()
  ovrMessage_Achievements_GetProgressByName                      = 0x152663B1, ///< Generated in response to ovr_Achievements_GetProgressByName()
  ovrMessage_Achievements_Unlock                                 = 0x593CCBDD, ///< Generated in response to ovr_Achievements_Unlock()
  ovrMessage_Application_GetVersion                              = 0x68670A0E, ///< Generated in response to ovr_Application_GetVersion()
  ovrMessage_CloudStorage_Delete                                 = 0x28DA456D, ///< Generated in response to ovr_CloudStorage_Delete()
  ovrMessage_CloudStorage_GetNextCloudStorageMetadataArrayPage   = 0x5C07A2EF, ///< Generated in response to ovr_CloudStorage_GetNextCloudStorageMetadataArrayPage()
  ovrMessage_CloudStorage_Load                                   = 0x40846B41, ///< Generated in response to ovr_CloudStorage_Load()
  ovrMessage_CloudStorage_LoadBucketMetadata                     = 0x7327A50D, ///< Generated in response to ovr_CloudStorage_LoadBucketMetadata()
  ovrMessage_CloudStorage_LoadConflictMetadata                   = 0x445A52F2, ///< Generated in response to ovr_CloudStorage_LoadConflictMetadata()
  ovrMessage_CloudStorage_LoadHandle                             = 0x326ADA36, ///< Generated in response to ovr_CloudStorage_LoadHandle()
  ovrMessage_CloudStorage_LoadMetadata                           = 0x03E6A292, ///< Generated in response to ovr_CloudStorage_LoadMetadata()
  ovrMessage_CloudStorage_ResolveKeepLocal                       = 0x30588D05, ///< Generated in response to ovr_CloudStorage_ResolveKeepLocal()
  ovrMessage_CloudStorage_ResolveKeepRemote                      = 0x7525A306, ///< Generated in response to ovr_CloudStorage_ResolveKeepRemote()
  ovrMessage_CloudStorage_Save                                   = 0x4BBB5C2E, ///< Generated in response to ovr_CloudStorage_Save()
  ovrMessage_Entitlement_GetIsViewerEntitled                     = 0x186B58B1, ///< Generated in response to ovr_Entitlement_GetIsViewerEntitled()
  ovrMessage_IAP_ConsumePurchase                                 = 0x1FBB72D9, ///< Generated in response to ovr_IAP_ConsumePurchase()
  ovrMessage_IAP_GetNextProductArrayPage                         = 0x1BD94AAF, ///< Generated in response to ovr_IAP_GetNextProductArrayPage()
  ovrMessage_IAP_GetNextPurchaseArrayPage                        = 0x47570A95, ///< Generated in response to ovr_IAP_GetNextPurchaseArrayPage()
  ovrMessage_IAP_GetProductsBySKU                                = 0x7E9ACAF5, ///< Generated in response to ovr_IAP_GetProductsBySKU()
  ovrMessage_IAP_GetViewerPurchases                              = 0x3A0F8419, ///< Generated in response to ovr_IAP_GetViewerPurchases()
  ovrMessage_IAP_LaunchCheckoutFlow                              = 0x3F9B0D0D, ///< Generated in response to ovr_IAP_LaunchCheckoutFlow()
  ovrMessage_Leaderboard_GetEntries                              = 0x5DB3474C, ///< Generated in response to ovr_Leaderboard_GetEntries()
  ovrMessage_Leaderboard_GetEntriesAfterRank                     = 0x18378BEF, ///< Generated in response to ovr_Leaderboard_GetEntriesAfterRank()
  ovrMessage_Leaderboard_GetNextEntries                          = 0x4E207CD9, ///< Generated in response to ovr_Leaderboard_GetNextEntries()
  ovrMessage_Leaderboard_GetPreviousEntries                      = 0x4901DAC0, ///< Generated in response to ovr_Leaderboard_GetPreviousEntries()
  ovrMessage_Leaderboard_WriteEntry                              = 0x117FC8FE, ///< Generated in response to ovr_Leaderboard_WriteEntry()
  ovrMessage_Matchmaking_Browse                                  = 0x1E6532C8, ///< Generated in response to ovr_Matchmaking_Browse()
  ovrMessage_Matchmaking_Cancel                                  = 0x206849AF, ///< Generated in response to ovr_Matchmaking_Cancel()
  ovrMessage_Matchmaking_Cancel2                                 = 0x10FE8DD4, ///< Generated in response to ovr_Matchmaking_Cancel2()
  ovrMessage_Matchmaking_CreateAndEnqueueRoom                    = 0x604C5DC8, ///< Generated in response to ovr_Matchmaking_CreateAndEnqueueRoom()
  ovrMessage_Matchmaking_CreateRoom                              = 0x033B132A, ///< Generated in response to ovr_Matchmaking_CreateRoom()
  ovrMessage_Matchmaking_Enqueue                                 = 0x40C16C71, ///< Generated in response to ovr_Matchmaking_Enqueue()
  ovrMessage_Matchmaking_EnqueueRoom                             = 0x708A4064, ///< Generated in response to ovr_Matchmaking_EnqueueRoom()
  ovrMessage_Matchmaking_GetStats                                = 0x42FC9438, ///< Generated in response to ovr_Matchmaking_GetStats()
  ovrMessage_Matchmaking_JoinRoom                                = 0x4D32D7FD, ///< Generated in response to ovr_Matchmaking_JoinRoom()
  ovrMessage_Matchmaking_ReportResultInsecure                    = 0x1A36D18D, ///< Generated in response to ovr_Matchmaking_ReportResultInsecure()
  ovrMessage_Matchmaking_StartMatch                              = 0x44D40945, ///< Generated in response to ovr_Matchmaking_StartMatch()
  ovrMessage_Notification_GetNextRoomInviteNotificationArrayPage = 0x0621FB77, ///< Generated in response to ovr_Notification_GetNextRoomInviteNotificationArrayPage()
  ovrMessage_Notification_GetRoomInvites                         = 0x6F916B92, ///< Generated in response to ovr_Notification_GetRoomInvites()
  ovrMessage_Notification_MarkAsRead                             = 0x717259E3, ///< Generated in response to ovr_Notification_MarkAsRead()
  ovrMessage_Room_CreateAndJoinPrivate                           = 0x75D6E377, ///< Generated in response to ovr_Room_CreateAndJoinPrivate()
  ovrMessage_Room_Get                                            = 0x659A8FB8, ///< Generated in response to ovr_Room_Get()
  ovrMessage_Room_GetCurrent                                     = 0x09A6A504, ///< Generated in response to ovr_Room_GetCurrent()
  ovrMessage_Room_GetCurrentForUser                              = 0x0E0017E5, ///< Generated in response to ovr_Room_GetCurrentForUser()
  ovrMessage_Room_GetInvitableUsers                              = 0x1E325792, ///< Generated in response to ovr_Room_GetInvitableUsers()
  ovrMessage_Room_GetModeratedRooms                              = 0x0983FD77, ///< Generated in response to ovr_Room_GetModeratedRooms()
  ovrMessage_Room_GetNextRoomArrayPage                           = 0x4E8379C6, ///< Generated in response to ovr_Room_GetNextRoomArrayPage()
  ovrMessage_Room_InviteUser                                     = 0x4129EC13, ///< Generated in response to ovr_Room_InviteUser()
  ovrMessage_Room_Join                                           = 0x16CA8F09, ///< Generated in response to ovr_Room_Join()
  ovrMessage_Room_KickUser                                       = 0x49835736, ///< Generated in response to ovr_Room_KickUser()
  ovrMessage_Room_LaunchInvitableUserFlow                        = 0x323FE273, ///< Generated in response to ovr_Room_LaunchInvitableUserFlow()
  ovrMessage_Room_Leave                                          = 0x72382475, ///< Generated in response to ovr_Room_Leave()
  ovrMessage_Room_SetDescription                                 = 0x3044852F, ///< Generated in response to ovr_Room_SetDescription()
  ovrMessage_Room_UpdateDataStore                                = 0x026E4028, ///< Generated in response to ovr_Room_UpdateDataStore()
  ovrMessage_Room_UpdateMembershipLockStatus                     = 0x370BB7AC, ///< Generated in response to ovr_Room_UpdateMembershipLockStatus()
  ovrMessage_Room_UpdateOwner                                    = 0x32B63D1D, ///< Generated in response to ovr_Room_UpdateOwner()
  ovrMessage_Room_UpdatePrivateRoomJoinPolicy                    = 0x1141029B, ///< Generated in response to ovr_Room_UpdatePrivateRoomJoinPolicy()
  ovrMessage_User_Get                                            = 0x6BCF9E47, ///< Generated in response to ovr_User_Get()
  ovrMessage_User_GetAccessToken                                 = 0x06A85ABE, ///< Generated in response to ovr_User_GetAccessToken()
  ovrMessage_User_GetLoggedInUser                                = 0x436F345D, ///< Generated in response to ovr_User_GetLoggedInUser()
  ovrMessage_User_GetLoggedInUserFriends                         = 0x587C2A8D, ///< Generated in response to ovr_User_GetLoggedInUserFriends()
  ovrMessage_User_GetNextUserArrayPage                           = 0x267CF743, ///< Generated in response to ovr_User_GetNextUserArrayPage()
  ovrMessage_User_GetOrgScopedID                                 = 0x18F0B01B, ///< Generated in response to ovr_User_GetOrgScopedID()
  ovrMessage_User_GetUserProof                                   = 0x22810483, ///< Generated in response to ovr_User_GetUserProof()

  /// Indicates that a match has been found, for example after calling
  /// ovr_Matchmaking_Enqueue(). Use ovr_Message_GetRoom() to extract the
  /// matchmaking room.
  ///
  /// The message will contain a payload of type ::ovrMatchmakingNotificationHandle.
  /// Extract the payload from the message handle with ::ovr_Message_GetMatchmakingNotification().
  ovrMessage_Notification_Matchmaking_MatchFound = 0x0BC3FCD7,

  /// Indicates that a connection has been established or there's been an error.
  /// Use ovr_NetworkingPeer_GetState() to get the result; as above,
  /// ovr_NetworkingPeer_GetID() returns the ID of the peer this message is for.
  ovrMessage_Notification_Networking_ConnectionStateChange = 0x5E02D49A,

  /// Indicates that another user is attempting to establish a P2P connection
  /// with us. Use ovr_NetworkingPeer_GetID() to extract the ID of the peer.
  ovrMessage_Notification_Networking_PeerConnectRequest = 0x4D31E2CF,

  /// Generated in response to ovr_Net_Ping(). Either contains ping time in
  /// microseconds or indicates that there was a timeout.
  ovrMessage_Notification_Networking_PingResult = 0x51153012,

  /// Indicates that the user has accepted an invitation, for example in Oculus
  /// Home. Use ovr_Message_GetString() to extract the ID of the room that the
  /// user has been inivted to as a string. Then call ovrID_FromString() to parse
  /// it into an ovrID.
  /// 
  /// Note that you must call ovr_Room_Join() if you want to actually join the
  /// room.
  ///
  /// The message will contain a payload of type const char *.
  /// Extract the payload from the message handle with ::ovr_Message_GetString().
  ovrMessage_Notification_Room_InviteAccepted = 0x6D1071B1,

  /// Indicates that the current room has been updated. Use ovr_Message_GetRoom()
  /// to extract the updated room.
  ///
  /// The message will contain a payload of type ::ovrRoomHandle.
  /// Extract the payload from the message handle with ::ovr_Message_GetRoom().
  ovrMessage_Notification_Room_RoomUpdate = 0x60EC3C2F,

  /// Sent when another user is attempting to establish a VoIP connection. Use
  /// ovr_Message_GetNetworkingPeer() to extract information about the user, and
  /// ovr_Voip_Accept() to accept the connection.
  ovrMessage_Notification_Voip_ConnectRequest = 0x36243816,

  /// Sent to indicate that the state of the VoIP connection changed. Use
  /// ovr_Message_GetNetworkingPeer() and ovr_NetworkingPeer_GetState() to
  /// extract the current state.
  ovrMessage_Notification_Voip_StateChange = 0x34EFA660,

} ovrMessageType;

/// Convert an ovrMessageType to a human readable string
///
OVRPL_PUBLIC_FUNCTION(const char*) ovrMessageType_ToString(ovrMessageType value);

#endif
