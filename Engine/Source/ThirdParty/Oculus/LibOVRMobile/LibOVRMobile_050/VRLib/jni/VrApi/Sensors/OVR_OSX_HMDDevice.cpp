/************************************************************************************

Filename    :   OVR_OSX_HMDDevice.cpp
Content     :   OSX Interface to HMD - detects HMD display
Created     :   September 21, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_OSX_HMDDevice.h"

#include "OVR_OSX_DeviceManager.h"
#include "Util/Util_Render_Stereo.h"

#include <CoreGraphics/CGDirectDisplay.h>
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <IOKit/graphics/IOGraphicsLib.h>

namespace OVR { namespace OSX {

using namespace OVR::Util::Render;
    
//-------------------------------------------------------------------------------------

HMDDeviceCreateDesc::HMDDeviceCreateDesc(DeviceFactory* factory, 
                                         UInt32 vend, UInt32 prod, const String& displayDeviceName, long dispId)
        : DeviceCreateDesc(factory, Device_HMD),
          DisplayDeviceName(displayDeviceName),
          Contents(0),
          DisplayId(dispId)
{
    /* //??????????
    char idstring[9];
    idstring[0] = 'A'-1+((vend>>10) & 31);
    idstring[1] = 'A'-1+((vend>>5) & 31);
    idstring[2] = 'A'-1+((vend>>0) & 31);
    snprintf(idstring+3, 5, "%04d", prod);
    DeviceId = idstring;*/
    DeviceId = DisplayDeviceName;

    Desktop.X = 0;
    Desktop.Y = 0;
    ResolutionInPixels = Sizei(0);          
    ScreenSizeInMeters = Sizef(0.0f);
}

HMDDeviceCreateDesc::HMDDeviceCreateDesc(const HMDDeviceCreateDesc& other)
        : DeviceCreateDesc(other.pFactory, Device_HMD),
          DeviceId(other.DeviceId), DisplayDeviceName(other.DisplayDeviceName),
          Contents(other.Contents),
          DisplayId(other.DisplayId)
{
    Desktop.X             = other.Desktop.X;
    Desktop.Y             = other.Desktop.Y;
    ResolutionInPixels    = other.ResolutionInPixels;
    ScreenSizeInMeters    = other.ScreenSizeInMeters;          
}

HMDDeviceCreateDesc::MatchResult HMDDeviceCreateDesc::MatchDevice(const DeviceCreateDesc& other,
                                                                  DeviceCreateDesc** pcandidate) const
{
    if ((other.Type != Device_HMD) || (other.pFactory != pFactory))
        return Match_None;

    // There are several reasons we can come in here:
    //   a) Matching this HMD Monitor created desc to OTHER HMD Monitor desc
    //          - Require exact device DeviceId/DeviceName match
    //   b) Matching SensorDisplayInfo created desc to OTHER HMD Monitor desc
    //          - This DeviceId is empty; becomes candidate
    //   c) Matching this HMD Monitor created desc to SensorDisplayInfo desc
    //          - This other.DeviceId is empty; becomes candidate

    const HMDDeviceCreateDesc& s2 = (const HMDDeviceCreateDesc&) other;

    if ((DeviceId == s2.DeviceId) &&
        (DisplayId == s2.DisplayId))
    {
        // Non-null DeviceId may match while size is different if screen size was overwritten
        // by SensorDisplayInfo in prior iteration.
        if (!DeviceId.IsEmpty() ||
            (ScreenSizeInMeters == s2.ScreenSizeInMeters) )
        {            
            *pcandidate = 0;
            return Match_Found;
        }
    }


    // DisplayInfo takes precedence, although we try to match it first.
    if ((ResolutionInPixels == s2.ResolutionInPixels) &&        
        (ScreenSizeInMeters == s2.ScreenSizeInMeters))
    {
        if (DeviceId.IsEmpty() && !s2.DeviceId.IsEmpty())
        {
            *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);
            return Match_Candidate;
        }

        *pcandidate = 0;
        return Match_Found;
    }    
    
    // SensorDisplayInfo may override resolution settings, so store as candidiate.
    if (s2.DeviceId.IsEmpty() && s2.DisplayId == 0)
    {        
        *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);        
        return Match_Candidate;
    }
    // OTHER HMD Monitor desc may initialize DeviceName/Id
    else if (DeviceId.IsEmpty() && DisplayId == 0)
    {
        *pcandidate = const_cast<DeviceCreateDesc*>((const DeviceCreateDesc*)this);        
        return Match_Candidate;
    }
    
    return Match_None;
}


bool HMDDeviceCreateDesc::UpdateMatchedCandidate(const DeviceCreateDesc& other, bool* newDeviceFlag)
{
    // This candidate was the the "best fit" to apply sensor DisplayInfo to.
    OVR_ASSERT(other.Type == Device_HMD);
    
    const HMDDeviceCreateDesc& s2 = (const HMDDeviceCreateDesc&) other;

    // Force screen size on resolution from SensorDisplayInfo.
    // We do this because USB detection is more reliable as compared to HDMI EDID,
    // which may be corrupted by splitter reporting wrong monitor 
    if (s2.DeviceId.IsEmpty() && s2.DisplayId == 0)
    {
        ScreenSizeInMeters = s2.ScreenSizeInMeters;        
        Contents |= Contents_Screen;

        if (s2.Contents & HMDDeviceCreateDesc::Contents_Distortion)
        {
            memcpy(DistortionK, s2.DistortionK, sizeof(float)*4);
            // TODO: DistortionEqn
            Contents |= Contents_Distortion;
        }
        DeviceId          = s2.DeviceId;
        DisplayId         = s2.DisplayId;
        DisplayDeviceName = s2.DisplayDeviceName;
        Desktop.X         = s2.Desktop.X;
        Desktop.Y         = s2.Desktop.Y;
        if (newDeviceFlag) *newDeviceFlag = true;
    }
    else if (DeviceId.IsEmpty())
    {
        // This branch is executed when 'fake' HMD descriptor is being replaced by
        // the real one.
        DeviceId          = s2.DeviceId;
        DisplayId         = s2.DisplayId;
        DisplayDeviceName = s2.DisplayDeviceName;
        Desktop.X         = s2.Desktop.X;
        Desktop.Y         = s2.Desktop.Y;

		// ScreenSize and Resolution are NOT assigned here, since they may have
		// come from a sensor DisplayInfo (which has precedence over HDMI).

        if (newDeviceFlag) *newDeviceFlag = true;
    }
    else
    {
        if (newDeviceFlag) *newDeviceFlag = false;
    }

    return true;
}

    
//-------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------
// ***** HMDDeviceFactory

static HMDDeviceFactory &HMDDeviceFactory::GetInstance()
{
	static HMDDeviceFactory instance;
	return instance;
}

void HMDDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{
    CGDirectDisplayID Displays[32];
    uint32_t NDisplays = 0;
    CGGetOnlineDisplayList(32, Displays, &NDisplays);

    for (int i = 0; i < NDisplays; i++)
    {
        io_service_t port = CGDisplayIOServicePort(Displays[i]);
        CFDictionaryRef DispInfo = IODisplayCreateInfoDictionary(port, kIODisplayMatchingInfo);

        uint32_t vendor = CGDisplayVendorNumber(Displays[i]);
        uint32_t product = CGDisplayModelNumber(Displays[i]);
        unsigned mwidth = (unsigned)CGDisplayPixelsWide(Displays[i]);
        unsigned mheight = (unsigned)CGDisplayPixelsHigh(Displays[i]);
        CGRect desktop = CGDisplayBounds(Displays[i]);
        
        if (vendor == 16082 && ( (product == 1)||(product == 2) ) ) // 7" or HD
        {
            char idstring[9];
            idstring[0] = 'A'-1+((vendor>>10) & 31);
            idstring[1] = 'A'-1+((vendor>>5) & 31);
            idstring[2] = 'A'-1+((vendor>>0) & 31);
            snprintf(idstring+3, 5, "%04d", product);

            HMDDeviceCreateDesc hmdCreateDesc(this, vendor, product, idstring, Displays[i]);
            
			if (product == 2)
			{
                hmdCreateDesc.SetScreenParameters(desktop.origin.x, desktop.origin.y,
                                                  mwidth, mheight, 0.12096f, 0.06804f);
			}
			else
			{
				if (hmdCreateDesc.Is7Inch())
				{
					// Physical dimension of SLA screen.
					hmdCreateDesc.SetScreenParameters(desktop.origin.x, desktop.origin.y,
                                                      mwidth, mheight, 0.14976f, 0.0936f);
				}
				else
				{
					hmdCreateDesc.SetScreenParameters(desktop.origin.x, desktop.origin.y,
                                                      mwidth, mheight, 0.12096f, 0.0756f);
				}
			}

            OVR_DEBUG_LOG_TEXT(("DeviceManager - HMD Found %x:%x\n", vendor, product));
            
            // Notify caller about detected device. This will call EnumerateAddDevice
            // if the this is the first time device was detected.
            visitor.Visit(hmdCreateDesc);
        }
        CFRelease(DispInfo);
    }
}

#include "OVR_Common_HMDDevice.cpp"

}} // namespace OVR::OSX


