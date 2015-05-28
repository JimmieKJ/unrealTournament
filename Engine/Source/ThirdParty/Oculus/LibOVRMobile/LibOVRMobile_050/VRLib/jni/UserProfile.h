/************************************************************************************

Filename    :   UserProfile.h
Content     :   Container for user profile data.
Created     :   November 10, 2014
Authors     :   Caleb Leak

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_UserProfile_h
#define OVR_UserProfile_h

namespace OVR
{

//==============================================================
// UserProfile
// 
class UserProfile
{
public:

    UserProfile() :
        Ipd(             0.0640f ),
        EyeHeight(       1.6750f ),
        HeadModelDepth(  0.0805f ),
        HeadModelHeight( 0.0750f )
    {
    }
     
    float Ipd;
    float EyeHeight;
    float HeadModelDepth;
    float HeadModelHeight;
};

UserProfile     LoadProfile();
void            SaveProfile(const UserProfile& profile);

}	// namespace OVR

#endif	// OVR_UserProfile_h
