/************************************************************************************

Filename    :   OVR_Win32_SensorDevice.cpp
Content     :   Win32 SensorDevice implementation
Created     :   March 12, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_Win32_SensorDevice.h"

#include "OVR_Win32_HMDDevice.h"
#include "OVR_SensorDeviceImpl.h"
#include "OVR_DeviceImpl.h"

namespace OVR { namespace Win32 {

} // namespace Win32

//-------------------------------------------------------------------------------------
void SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo
    (const SensorDisplayInfoImpl& displayInfo, 
     DeviceFactory::EnumerateVisitor& visitor)
{
    Win32::HMDDeviceCreateDesc hmdCreateDesc(&Win32::HMDDeviceFactory::GetInstance(), String(), String());

	hmdCreateDesc.SetScreenParameters(  0, 0,
                                        displayInfo.Resolution.H, displayInfo.Resolution.V,
                                        displayInfo.ScreenSize.H, displayInfo.ScreenSize.V);

    if ((displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt) == SensorDisplayInfoImpl::Base_Distortion)
    {
        hmdCreateDesc.SetDistortion(displayInfo.DistortionK);
        // TODO: add DistortionEqn
    }
    if (displayInfo.ScreenSize.H > 0.14f)
        hmdCreateDesc.Set7Inch();

    visitor.Visit(hmdCreateDesc);
}

} // namespace OVR


