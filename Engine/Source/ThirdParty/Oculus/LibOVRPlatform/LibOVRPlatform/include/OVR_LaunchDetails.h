// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_LAUNCHDETAILS_H
#define OVR_LAUNCHDETAILS_H

#include "OVR_Platform_Defs.h"
#include "OVR_LaunchType.h"
#include "OVR_UserArray.h"
#include <stddef.h>

typedef struct ovrLaunchDetails *ovrLaunchDetailsHandle;

OVRP_PUBLIC_FUNCTION(ovrLaunchType)      ovr_LaunchDetails_GetLaunchType(const ovrLaunchDetailsHandle obj);
OVRP_PUBLIC_FUNCTION(ovrID)              ovr_LaunchDetails_GetRoomID(const ovrLaunchDetailsHandle obj);
OVRP_PUBLIC_FUNCTION(ovrUserArrayHandle) ovr_LaunchDetails_GetUsers(const ovrLaunchDetailsHandle obj);

#endif
