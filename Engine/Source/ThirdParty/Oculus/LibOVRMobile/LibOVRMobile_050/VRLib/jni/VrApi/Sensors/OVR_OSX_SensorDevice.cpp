/************************************************************************************

Filename    :   OVR_OSX_SensorDevice.cpp
Content     :   OSX SensorDevice implementation
Created     :   March 14, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_OSX_HMDDevice.h"
#include "OVR_SensorDeviceImpl.h"
#include "OVR_DeviceImpl.h"

namespace OVR { namespace OSX {

} // namespace OSX

//-------------------------------------------------------------------------------------
void SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo(   const SensorDisplayInfoImpl& displayInfo, 
                                                            DeviceFactory::EnumerateVisitor& visitor)
{

    OSX::HMDDeviceCreateDesc hmdCreateDesc(&OSX::HMDDeviceFactory::GetInstance(), 1, 1, "", 0);    

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


