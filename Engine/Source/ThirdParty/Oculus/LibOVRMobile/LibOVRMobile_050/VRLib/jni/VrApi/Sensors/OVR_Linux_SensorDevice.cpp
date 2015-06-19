/************************************************************************************

Filename    :   OVR_Linux_SensorDevice.cpp
Content     :   Linux SensorDevice implementation
Created     :   June 13, 2013
Authors     :   Brant Lewis

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

//#include "OVR_OSX_HMDDevice.h"
#include "OVR_SensorDeviceImpl.h"
#include "OVR_DeviceImpl.h"

namespace OVR { namespace OSX {

} // namespace OSX

//-------------------------------------------------------------------------------------
void SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo(   const SensorDisplayInfoImpl& displayInfo, 
                                                            DeviceFactory::EnumerateVisitor& visitor)
{
/*
    Linux::HMDDeviceCreateDesc hmdCreateDesc(&Linux::HMDDeviceFactory::GetInstance(), 1, 1, "", 0);
    
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

} // namespace OVR


