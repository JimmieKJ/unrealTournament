/************************************************************************************

Filename    :   CAPI_HMDState.cpp
Content     :   State associated with a single HMD
Created     :   January 24, 2014
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "CAPI_HMDState.h"
#include "CAPI_GlobalState.h"

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** HMDState


HMDState::HMDState(HMDDevice* device)
	: pHMD(device), HMDInfoW(device), HMDInfo(HMDInfoW.h),
	SensorStarted(0), SensorCaps(0),
	AddSensorCount(0), AddLatencyTestCount(0)
{
	pLastError = 0;
	OVR::CAPI::GlobalState::pInstance->AddHMD(this);

	//    RenderInfo = GenerateHmdRenderInfoFromHmdInfo ( HMDInfo, NULL );
	//    SConfig.SetHmdRenderInfo(RenderInfo);    

	//    LastFrameTimeSeconds = 0.0f;
}

HMDState::~HMDState()
{
	OVR_ASSERT(OVR::CAPI::GlobalState::pInstance);

	StopSensor();
	OVR::CAPI::GlobalState::pInstance->RemoveHMD(this);
}



//-------------------------------------------------------------------------------------
// *** Sensor 

bool HMDState::StartSensor(unsigned supportedCaps, unsigned requiredCaps)
{
	// TBD: Implement an optimized path that allows you to change caps such as yaw.
	if (SensorStarted)
		StopSensor();

	supportedCaps |= requiredCaps;

	// TBD: In case of sensor not being immediately available, it would be good to check
	//      yaw config availability to match it with ovrHmdCap_YawCorrection requirement.
	// 

	if (requiredCaps & ovrHmdCap_Position)
	{
		pLastError = "ovrHmdCap_Position not supported.";
		return false;
	}

	pSensor = *pHMD->GetSensor();

	if (!pSensor)
	{
		if (requiredCaps & ovrHmdCap_Orientation)
		{
			pLastError = "Failed to create sensor.";
			return false;
		}
		// Succeed, waiting for sensor become available later.
		LogText("StartSensor succeeded - waiting for sensor.\n");
	}
	else
	{
		pSensor->SetReportRate(500);
		SFusion.AttachToSensor(pSensor);

		if (requiredCaps & ovrHmdCap_YawCorrection)
		{
			if (!SFusion.HasMagCalibration())
			{
				pLastError = "ovrHmdCap_YawCorrection not available.";
				SFusion.AttachToSensor(0);
				SFusion.Reset();
				pSensor.Clear();
				return false;
			}
		}

		SFusion.SetYawCorrectionEnabled((supportedCaps & ovrHmdCap_YawCorrection) != 0);
		LogText("Sensor created.\n");
	}

	AddSensorCount = 0;
	SensorStarted = true;
	SensorCaps = supportedCaps;

	return true;
}


// Stops sensor sampling, shutting down internal resources.
void HMDState::StopSensor()
{
	if (SensorStarted)
	{
		LogText("StopSensor suceeded.\n");

		SFusion.AttachToSensor(0);
		SFusion.Reset();
		pSensor.Clear();
		AddSensorCount = 0;
		SensorCaps = 0;
		SensorStarted = false;
	}
}

// Resets sensor orientation.
void HMDState::ResetSensor()
{
	SFusion.Reset();
}

// Re-center's on yaw axis only.
void HMDState::RecenterYaw()
{
	SFusion.RecenterYaw();
}

// Returns prediction for time.
ovrSensorState HMDState::PredictedSensorState(double absTime, bool allowSensorCreate)
{
	SensorState ss;

	if (pSensor || (allowSensorCreate && checkCreateSensor()))
	{
		ss = SFusion.GetPredictionForTime(absTime);

		if (allowSensorCreate && !(ss.Status & ovrStatus_OrientationTracked))
		{
			// Not needed yet; SFusion.AttachToSensor(0);
			// This seems to reset orientation anyway...
			pSensor.Clear();
		}
	}
	else
	{
		// SensorState() defaults to 0s.
		// ss.Pose.Orientation       = Quatf();
		// ..

		// We still want valid times so frames will get a delta-time
		// and allow operation with a joypad when the sensor isn't
		// connected.
		ss.Recorded.TimeInSeconds = absTime;
		ss.Predicted.TimeInSeconds = absTime;
	}

	ss.Status |= ovrStatus_HmdConnected;
	return ss;
}


bool  HMDState::checkCreateSensor()
{
	if (SensorStarted && !pSensor && AddSensorCount)
	{
		AddSensorCount = 0;
		pSensor = *pHMD->GetSensor();

		if (pSensor)
		{
			pSensor->SetReportRate(500);
			SFusion.AttachToSensor(pSensor);
			SFusion.SetYawCorrectionEnabled((SensorCaps & ovrHmdCap_YawCorrection) != 0);
			LogText("Sensor created.\n");
			return true;
		}
	}

	return false;
}

bool HMDState::ProcessLatencyTest(unsigned char rgbColorOut[3])
{
	bool result = false;

	// Check create.
	if (pLatencyTester)
	{
		if (pLatencyTester->IsConnected())
		{
			Color colorToDisplay;

			LatencyUtil.ProcessInputs();
			result = LatencyUtil.DisplayScreenColor(colorToDisplay);
			rgbColorOut[0] = colorToDisplay.R;
			rgbColorOut[1] = colorToDisplay.G;
			rgbColorOut[2] = colorToDisplay.B;
		}
		else
		{
			// Disconnect.
			LatencyUtil.SetDevice(0);
			pLatencyTester = 0;
			LogText("LATENCY SENSOR disconnected.\n");
		}
	}
	else if (AddLatencyTestCount)
	{
		// This might have some unlikely race condition issue which could cause us to miss a device...
		AddLatencyTestCount = 0;

		pLatencyTester = *OVR::CAPI::GlobalState::pInstance->GetManager()->EnumerateDevices<LatencyTestDevice>().CreateDevice();
		if (pLatencyTester)
		{
			LatencyUtil.SetDevice(pLatencyTester);
			LogText("LATENCY TESTER connected\n");
		}
	}

	return result;
}

}} // namespace OVR::CAPI
