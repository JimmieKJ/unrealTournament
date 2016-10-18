// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_ROOM_H
#define OVR_ROOM_H

#include "OVR_Platform_Defs.h"
#include "OVR_DataStore.h"
#include "OVR_RoomJoinPolicy.h"
#include "OVR_RoomJoinability.h"
#include "OVR_RoomType.h"
#include "OVR_Types.h"
#include "OVR_User.h"
#include "OVR_UserArray.h"
#include <stddef.h>

typedef struct ovrRoom *ovrRoomHandle;

OVRP_PUBLIC_FUNCTION(ovrID)              ovr_Room_GetApplicationID(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(ovrDataStoreHandle) ovr_Room_GetDataStore(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(const char *)       ovr_Room_GetDescription(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(ovrID)              ovr_Room_GetID(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(bool)               ovr_Room_GetIsMembershipLocked(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(ovrRoomJoinPolicy)  ovr_Room_GetJoinPolicy(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(ovrRoomJoinability) ovr_Room_GetJoinability(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(unsigned int)       ovr_Room_GetMaxUsers(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(const char *)       ovr_Room_GetName(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(ovrUserHandle)      ovr_Room_GetOwner(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(ovrRoomType)        ovr_Room_GetType(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(ovrUserArrayHandle) ovr_Room_GetUsers(const ovrRoomHandle obj);
OVRP_PUBLIC_FUNCTION(unsigned int)       ovr_Room_GetVersion(const ovrRoomHandle obj);

#endif
