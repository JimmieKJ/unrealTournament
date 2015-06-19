/************************************************************************************

Filename    :   HmdSensors.h
Content     :   State associated with a single HMD
Created     :   January 24, 2014
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_HMDState_h
#define OVR_HMDState_h

#include "Sensors/OVR_SensorFusion.h"
#include "Sensors/OVR_LatencyTest.h"

// HMD capability bits reported by device.
typedef enum
{
    ovrHmdCap_Orientation       = 0x0010,   //  Support orientation tracking (IMU).
    ovrHmdCap_YawCorrection     = 0x0020,   //  Supports yaw correction through magnetometer or other means.
    ovrHmdCap_Position          = 0x0040,   //  Supports positional tracking.
} ovrHmdCapBits;

class HMDState : public OVR::MessageHandler, public OVR::NewOverrideBase
{
public:
							HMDState();
							~HMDState();
					
	bool					InitDevice();

	bool					StartSensor( unsigned supportedCaps, unsigned requiredCaps );
	void					StopSensor();
	void					ResetSensor();
	OVR::SensorInfo			GetSensorInfo();

	float					GetYaw();
	void					SetYaw( float yaw );
	void					RecenterYaw();

	OVR::SensorState		PredictedSensorState( double absTime );

	bool					ProcessLatencyTest( unsigned char rgbColorOut[3] );
	const char *			GetLatencyTestResult() { return LatencyUtil.GetResultsString(); }

	OVR::DeviceManager *	GetDeviceManager() { return DeviceManager.GetPtr(); }

	void					OnMessage( const OVR::Message & msg );

private:
	OVR::Ptr<OVR::DeviceManager>		DeviceManager;
	OVR::Ptr<OVR::HMDDevice>			Device;

	bool								SensorStarted;
	unsigned							SensorCaps;

	OVR::AtomicInt<int>					SensorChangedCount;
	OVR::Mutex							SensorChangedMutex;
	OVR::Ptr<OVR::SensorDevice>			Sensor;
	OVR::SensorFusion					SFusion;

	OVR::AtomicInt<int>					LatencyTesterChangedCount;
	OVR::Mutex							LatencyTesterChangedMutex;
	OVR::Ptr<OVR::LatencyTestDevice>	LatencyTester;
	OVR::LatencyTest					LatencyUtil;

	OVR::SensorState					LastSensorState;
};

#endif	// !OVR_HmdSensors_h
