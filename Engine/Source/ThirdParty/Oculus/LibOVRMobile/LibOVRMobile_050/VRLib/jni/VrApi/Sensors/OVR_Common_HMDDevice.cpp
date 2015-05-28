/************************************************************************************

Filename    :   OVR_Common_HMDDevice.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

// Should be #included from the relevant OVR_YourPlatformHere_HMDDevice.cpp


//-------------------------------------------------------------------------------------
// ***** HMDDeviceCreateDesc

DeviceBase* HMDDeviceCreateDesc::NewDeviceInstance()
{
    return new HMDDevice(this);
}

bool HMDDeviceCreateDesc::Is7Inch() const
{
    return (strstr(DeviceId.ToCStr(), "OVR0001") != 0) || (Contents & Contents_7Inch);
}

Profile* HMDDeviceCreateDesc::GetProfileAddRef() const
{
    // Create device may override profile name, so get it from there is possible.
    ProfileManager* profileManager = GetManagerImpl()->GetProfileManager();
    ProfileType     profileType    = GetProfileType();
    const char *    profileName    = pDevice ?
                        ((HMDDevice*)pDevice)->GetProfileName() :
                        profileManager->GetDefaultProfileName(profileType);
    
    return profileName ? 
        profileManager->LoadProfile(profileType, profileName) :
        profileManager->GetDeviceDefaultProfile(profileType);
}



bool HMDDeviceCreateDesc::GetDeviceInfo(DeviceInfo* info) const
{
    if ((info->InfoClassType != Device_HMD) &&
        (info->InfoClassType != Device_None))
        return false;

#if defined(OVR_OS_ANDROID)
	// LDC - Use zero data for now.
	info->Version = 0;
	info->ProductName[0] = 0;
	info->Manufacturer[0] = 0;

	if (info->InfoClassType == Device_HMD)
    {
		HMDInfo* hmdInfo = static_cast<HMDInfo*>(info);
		*hmdInfo = HMDInfo();
	}
#else

    HmdTypeEnum hmdType = HmdType_DKProto;
    char const *deviceName = "Oculus Rift DK1-Prototype";
    if ( Is7Inch() )
    {
        hmdType = HmdType_DK1;
        deviceName = "Oculus Rift DK1";
    }
    else if ( ResolutionInPixels.Width == 1920 )
    {
        // DKHD protoypes, all 1920x1080
        if ( ScreenSizeInMeters.Width < 0.121f )
        {
            // Screen size 0.12096 x 0.06804
            hmdType = HmdType_DKHDProto;
            deviceName = "Oculus Rift DK HD";
        }
        else if ( ScreenSizeInMeters.Width < 0.126f )
        {
            // Screen size 0.125 x 0.071
            hmdType = HmdType_DKHDProto566Mi;
            deviceName = "Oculus Rift DK HD 566 Mi";
        }
        else
        {
            // Screen size 0.1296 x 0.0729
            hmdType = HmdType_DKHDProto585;
            deviceName = "Oculus Rift DK HD 585";
        }
    }
    else
    {
        OVR_ASSERT ( "Unknown HMD" );
    }

    OVR_strcpy(info->ProductName,  DeviceInfo::MaxNameLength, deviceName );
    OVR_strcpy(info->Manufacturer, DeviceInfo::MaxNameLength, "Oculus VR");
    info->Type    = Device_HMD;
    info->Version = 0;

    // Display detection.
    if (info->InfoClassType == Device_HMD)
    {
        HMDInfo* hmdInfo = static_cast<HMDInfo*>(info);

        hmdInfo->HmdType                = hmdType;
        hmdInfo->DesktopX               = Desktop.X;
        hmdInfo->DesktopY               = Desktop.Y;
        hmdInfo->ResolutionInPixels     = ResolutionInPixels;                
        hmdInfo->ScreenSizeInMeters     = ScreenSizeInMeters;        // Includes ScreenGapSizeInMeters
        hmdInfo->ScreenGapSizeInMeters  = 0.0f;
        hmdInfo->CenterFromTopInMeters  = ScreenSizeInMeters.Height * 0.5f;
        hmdInfo->LensSeparationInMeters = 0.0635f;
        // TODO: any other information we get from the hardware itself should be added to this list

        switch ( hmdInfo->HmdType )
        {
        case HmdType_DKProto:
            // WARNING - estimated.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingTopToBottom;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.000052f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.016580f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.015f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DK1:
            // Data from specs.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingTopToBottom;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.00018226f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.01620089f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.017f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DKHDProto:
            // Data from specs.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingLeftToRight;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000859f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.0164948f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.012f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DKHDProto585:
            // Data from specs.
            hmdInfo->Shutter.Type                             = HmdShutter_RollingLeftToRight;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 60.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.000052f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.016580f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.015f; // estimated.
            hmdInfo->Shutter.PixelPersistence                 = hmdInfo->Shutter.VsyncToNextVsync; // Full persistence
            break;
        case HmdType_DKHDProto566Mi:
#if 0
            // Low-persistence global shutter
            hmdInfo->Shutter.Type                             = HmdShutter_Global;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 76.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000273f + 0.0131033f;    // Global shutter - first visible scan line is actually the last!
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.000f;                     // Global shutter - all visible at once.
            hmdInfo->Shutter.PixelSettleTime                  = 0.0f;                       // <100us
            hmdInfo->Shutter.PixelPersistence                 = 0.18f * hmdInfo->Shutter.VsyncToNextVsync;     // Confgurable - currently set to 18% of total frame.
#else
            // Low-persistence rolling shutter
            hmdInfo->Shutter.Type                             = HmdShutter_RollingLeftToRight;
            hmdInfo->Shutter.VsyncToNextVsync                 = ( 1.0f / 76.0f );
            hmdInfo->Shutter.VsyncToFirstScanline             = 0.0000273f;
            hmdInfo->Shutter.FirstScanlineToLastScanline      = 0.0131033f;
            hmdInfo->Shutter.PixelSettleTime                  = 0.0f;                       // <100us
            hmdInfo->Shutter.PixelPersistence                 = 0.18f * hmdInfo->Shutter.VsyncToNextVsync;     // Confgurable - currently set to 18% of total frame.
#endif
            break;
        default: OVR_ASSERT ( false ); break;
        }


        OVR_strcpy(hmdInfo->DisplayDeviceName, sizeof(hmdInfo->DisplayDeviceName),
                   DisplayDeviceName.ToCStr());
#if   defined(OVR_OS_WIN32)
        // Nothing special for Win32.
#elif defined(OVR_OS_MAC)
        hmdInfo->DisplayId = DisplayId;
#elif defined(OVR_OS_LINUX)
        hmdInfo->DisplayId = DisplayId;
#elif defined(OVR_OS_ANDROID)
#else
#error Unknown platform
#endif

    }
#endif

    return true;
}


//-------------------------------------------------------------------------------------
// ***** HMDDevice

HMDDevice::HMDDevice(HMDDeviceCreateDesc* createDesc)
    : OVR::DeviceImpl<OVR::HMDDevice>(createDesc, 0)
{
}
HMDDevice::~HMDDevice()
{
}

bool HMDDevice::Initialize(DeviceBase* parent)
{
    pParent = parent;

    // Initialize user profile to default for device.
    ProfileManager* profileManager = GetManager()->GetProfileManager();    
    ProfileName = profileManager->GetDefaultProfileName(getDesc()->GetProfileType());

    return true;
}
void HMDDevice::Shutdown()
{
    ProfileName.Clear();
    pCachedProfile.Clear();
    pParent.Clear();
}

Profile* HMDDevice::GetProfile() const
{    
    if (!pCachedProfile)
        pCachedProfile = *getDesc()->GetProfileAddRef();
    return pCachedProfile.GetPtr();
}

const char* HMDDevice::GetProfileName() const
{
    return ProfileName.ToCStr();
}

bool HMDDevice::SetProfileName(const char* name)
{
    pCachedProfile.Clear();
    if (!name)
    {
        ProfileName.Clear();
        return 0;
    }
    if (GetManager()->GetProfileManager()->HasProfile(getDesc()->GetProfileType(), name))
    {
        ProfileName = name;
        return true;
    }
    return false;
}
