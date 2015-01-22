/************************************************************************************

Filename    :   OVR_CAPI.cpp
Content     :   Experimental simple C interface to the HMD - version 1.
Created     :   November 30, 2013
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_CAPI.h"

#include "../Include/OVR.h"
#include "Kernel/OVR_Timer.h"

#include "OVR_PhoneSensors.h"

#include "CAPI/CAPI_GlobalState.h"
#include "CAPI/CAPI_HMDState.h"

//-------------------------------------------------------------------------------------
namespace OVR {

// ***** SensorState

SensorState::SensorState(const ovrSensorState& s)
{
    *this = reinterpret_cast<const SensorState&>(s);
}

SensorState::operator const ovrSensorState& () const
{
    OVR_COMPILER_ASSERT(sizeof(SensorState) == sizeof(ovrSensorState));
    return reinterpret_cast<const ovrSensorState&>(*this);
}

} // namespace OVR

//-------------------------------------------------------------------------------------

double ovr_GetTimeInSeconds()
{
	return OVR::Timer::GetSeconds();
}

//-------------------------------------------------------------------------------------

// 1. Init/shutdown.

bool ovr_Initialize()
{
	using namespace OVR;
	using namespace OVR::CAPI;

	if (GlobalState::pInstance)
        return true;

    // We must set up the system for the plugin to work
    if (!OVR::System::IsInitialized())
    	OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));

	// Constructor detects devices
	GlobalState::pInstance = new GlobalState;
    return true;
}

void ovr_Shutdown()
{
    using namespace OVR;
    using namespace OVR::CAPI;

	if (!GlobalState::pInstance)
        return;
	delete GlobalState::pInstance;
	GlobalState::pInstance = 0;

    // We should clean up the system to be complete    
    OVR::System::Destroy();    
}

int ovrHmd_Detect()
{
    using namespace OVR;
    using namespace OVR::CAPI;

	if (!GlobalState::pInstance)
        return 0;
	return GlobalState::pInstance->EnumerateDevices();
}

ovrHmd ovrHmd_Create(int index)
{
    using namespace OVR;
    using namespace OVR::CAPI;

	if (!GlobalState::pInstance)
        return 0;

	Ptr<HMDDevice> device = *GlobalState::pInstance->CreateDevice(index);
    if (!device)
        return 0;

    HMDState* hmds = new HMDState(device);
    if (!hmds)
        return 0;

  //  if (desc)
  //      ovrHmd_GetDesc(hmds, desc);

    return hmds;
}

void ovrHmd_Destroy(ovrHmd hmd)
{
	if (!hmd)
        return;

    // TBD: Any extra shutdown?
	delete (OVR::CAPI::HMDState*)hmd;
}

const char* ovrHmd_GetLastError(ovrHmd hmd)
{
    using namespace OVR;
    using namespace OVR::CAPI;

    if (!hmd)
    {
		if (!GlobalState::pInstance)
            return "LibOVR not initialized.";
		return GlobalState::pInstance->GetLastError();
    }
    HMDState* p = (HMDState*)hmd;
    return p->GetLastError();
}

//-------------------------------------------------------------------------------------
// *** Sensor

bool ovrHmd_StartSensor(ovrHmd hmd, unsigned supportedCaps, unsigned requiredCaps)
{
    OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
    // TBD: Decide if we null-check arguments.
    return p->StartSensor(supportedCaps, requiredCaps);
}

void ovrHmd_StopSensor(ovrHmd hmd)
{
    OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
    p->StopSensor();
}

void ovrHmd_ResetSensor(ovrHmd hmd)
{
    OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
	p->ResetSensor();
}

void ovrHmd_RecenterYaw(ovrHmd hmd)
{
    OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
	p->RecenterYaw();
}

ovrSensorState ovrHmd_GetSensorState(ovrHmd hmd, double absTime, bool allowSensorCreate)
{
    OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
    return p->PredictedSensorState(absTime, allowSensorCreate );
}


// Returns information about a sensor.
// Only valid after SensorStart.
bool ovrHmd_GetSensorDesc(ovrHmd hmd, ovrSensorDesc* descOut)
{
	OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
    
    if (p->pSensor)
    {
        OVR::SensorInfo si;
        p->pSensor->GetDeviceInfo(&si);
        descOut->VendorId  = si.VendorId;
        descOut->ProductId = si.ProductId;
        OVR_COMPILER_ASSERT(sizeof(si.SerialNumber) == sizeof(descOut->SerialNumber));
        memcpy(descOut->SerialNumber, si.SerialNumber, sizeof(si.SerialNumber) );

        // JDC: needed to disambiguate Samsung headsets
        descOut->Version = si.Version;
        return true;
    }

    return false;
}


//-------------------------------------------------------------------------------------
// ***** Latency Test interface

bool ovrHmd_ProcessLatencyTest(ovrHmd hmd, unsigned char rgbColorOut[3])
{
	OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
    return p->ProcessLatencyTest(rgbColorOut);
}

const char*  ovrHmd_GetLatencyTestResult(ovrHmd hmd)
{
	OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
    return p->LatencyUtil.GetResultsString();
}


//-------------------------------------------------------------------------------------
// ****** Special access for VRConfig

// Return the sensor fusion object for the purposes of magnetometer calibration.  The
// function is private and is only exposed through VRConfig header declarations
OVR::SensorFusion* ovrHmd_GetSensorFusion(ovrHmd hmd)
{
	OVR::CAPI::HMDState* p = (OVR::CAPI::HMDState*)hmd;
	return &p->SFusion;
}

// -----------------------------------------------------------------------------------
// ***** Mobile Additions

#include "OVR_DeviceImpl.h"

int ovr_GetDeviceManagerThreadTid()
{
    using namespace OVR;
    using namespace OVR::CAPI;

    if (!GlobalState::pInstance)
        return 0;
    return static_cast<DeviceManagerImpl*>(GlobalState::pInstance->GetManager())->GetThreadTid();
}

void ovr_SuspendDeviceManagerThread()
{
    using namespace OVR;
    using namespace OVR::CAPI;

    if (!GlobalState::pInstance)
        return;
    static_cast<DeviceManagerImpl*>(GlobalState::pInstance->GetManager())->SuspendThread();
}

void ovr_ResumeDeviceManagerThread()
{
    using namespace OVR;
    using namespace OVR::CAPI;

    if (!GlobalState::pInstance)
        return;
    static_cast<DeviceManagerImpl*>(GlobalState::pInstance->GetManager())->ResumeThread();
}
