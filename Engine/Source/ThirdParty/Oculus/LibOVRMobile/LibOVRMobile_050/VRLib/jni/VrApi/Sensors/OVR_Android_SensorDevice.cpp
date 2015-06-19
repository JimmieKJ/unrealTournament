/************************************************************************************

Filename    :   OVR_Android_SensorDevice.cpp
Content     :   Android SensorDevice implementation
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

//#include "OVR_Android_HMDDevice.h"
#include "OVR_SensorDeviceImpl.h"
#include "OVR_DeviceImpl.h"

namespace OVR { namespace Android {

} // namespace Android

//-------------------------------------------------------------------------------------
void SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo(   const SensorDisplayInfoImpl& displayInfo, 
                                                            DeviceFactory::EnumerateVisitor& visitor)
{
/*
    Android::HMDDeviceCreateDesc hmdCreateDesc(&Android::HMDDeviceFactory::GetInstance(), 1, 1, "", 0);
    
    hmdCreateDesc.SetScreenParameters(  0, 0,
                                        displayInfo.HResolution, displayInfo.VResolution,
                                        displayInfo.HScreenSize, displayInfo.VScreenSize);

    if ((displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt) == SensorDisplayInfoImpl::Base_Distortion)
        hmdCreateDesc.SetDistortion(displayInfo.DistortionK);
    if (displayInfo.HScreenSize > 0.14f)
        hmdCreateDesc.Set7Inch();

    visitor.Visit(hmdCreateDesc);
    */


}

} // namespace Android


