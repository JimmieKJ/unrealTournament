// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_REQUESTS_NOTIFICATION_H
#define OVR_REQUESTS_NOTIFICATION_H

#include "OVR_Types.h"
#include "OVR_Platform_Defs.h"

#include "OVR_RoomInviteNotificationArray.h"

/// Get the next page of entries
///
/// A message with type ::ovrMessage_Notification_GetNextRoomInviteNotificationArrayPage will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrRoomInviteNotificationArrayHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetRoomInviteNotificationArray().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Notification_GetNextRoomInviteNotificationArrayPage(ovrRoomInviteNotificationArrayHandle handle);

/// Retrieve a list of all pending room invites for your application.
///
/// A message with type ::ovrMessage_Notification_GetRoomInvites will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrRoomInviteNotificationArrayHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetRoomInviteNotificationArray().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Notification_GetRoomInvites();

/// Mark a notification as read. This causes it to disappear from the Universal
/// Menu, the Oculus App, Oculus Home, and in-app retrieval.
///
/// A message with type ::ovrMessage_Notification_MarkAsRead will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// This response has no payload. If no error occured, the request was successful. Yay!
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Notification_MarkAsRead(ovrID notificationID);

#endif
