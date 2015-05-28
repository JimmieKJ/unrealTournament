/************************************************************************************

Filename    :   UserProfile.cpp
Content     :   Container for user profile data.
Created     :   November 10, 2014
Authors     :   Caleb Leak

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "UserProfile.h"

#include "Kernel/OVR_JSON.h"
#include "Android/LogUtils.h"

static const char* PROFILE_PATH = "/sdcard/Oculus/userprofile.json";

namespace OVR
{

UserProfile LoadProfile()
{
    // TODO: Switch this over to using a content provider when available.

    Ptr<JSON> root = *JSON::Load(PROFILE_PATH);
    UserProfile profile;

    if (!root)
    {
        WARN("Failed to load user profile \"%s\". Using defaults.", PROFILE_PATH);
    }
    else
    {
        profile.Ipd = root->GetItemByName("ipd")->GetFloatValue();
        profile.EyeHeight = root->GetItemByName("eyeHeight")->GetFloatValue();
        profile.HeadModelHeight = root->GetItemByName("headModelHeight")->GetFloatValue();
        profile.HeadModelDepth = root->GetItemByName("headModelDepth")->GetFloatValue();
    }

    return profile;
}

void SaveProfile(const UserProfile& profile)
{
    Ptr<JSON> root = *JSON::CreateObject();

    root->AddNumberItem("ipd", profile.Ipd);
    root->AddNumberItem("eyeHeight", profile.EyeHeight);
    root->AddNumberItem("headModelHeight", profile.HeadModelHeight);
    root->AddNumberItem("headModelDepth", profile.HeadModelDepth);

    if (root->Save(PROFILE_PATH))
        WARN("Failed to save user profile %s", PROFILE_PATH);
}

}	// namespace OVR
