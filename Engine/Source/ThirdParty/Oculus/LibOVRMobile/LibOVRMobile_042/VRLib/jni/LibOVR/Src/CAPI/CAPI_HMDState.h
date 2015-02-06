/************************************************************************************

Filename    :   CAPI_HMDState.h
Content     :   State associated with a single HMD
Created     :   January 24, 2014
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPI_HMDState_h
#define OVR_CAPI_HMDState_h

#include "../Kernel/OVR_Math.h"
#include "../Kernel/OVR_List.h"
#include "../Kernel/OVR_Log.h"
#include "../OVR_CAPI.h"
#include "../OVR_SensorFusion.h"
#include "../Util/Util_LatencyTest.h"

struct ovrHmdStruct { };

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** HMDState

// Describes a single HMD.
class HMDState : public ListNode<HMDState>,
				 public ovrHmdStruct, public NewOverrideBase
{
public:

	HMDState(HMDDevice* device);
	virtual ~HMDState();


	// Delegated access APIs
	//  ovrHmdDesc GetDesc();  

	bool            StartSensor(unsigned supportedCaps, unsigned requiredCaps);
	void            StopSensor();
	void            ResetSensor();
	void			RecenterYaw();
	ovrSensorState  PredictedSensorState(double absTime, bool allowSensorCreate);

	bool            ProcessLatencyTest(unsigned char rgbColorOut[3]);

	const char* GetLastError()
	{
		const char* p = pLastError;
		pLastError = 0;
		return p;
	}

	void NotifyAddDevice(DeviceType deviceType)
	{
		if (deviceType == Device_Sensor)
			AddSensorCount++;
		else if (deviceType == Device_LatencyTester)
        {
			AddLatencyTestCount++;
        }
	}

	bool checkCreateSensor();

	Ptr<HMDDevice>     pHMD;

	// Wrapper to support 'const'
	struct HMDInfoWrapper
	{
		HMDInfoWrapper(HMDDevice* device) { if (device) device->GetDeviceInfo(&h); }
		OVR::HMDInfo h;
	};

	// HMDInfo shouldn't change, as its string pointers are passed out.
	const HMDInfoWrapper HMDInfoW;
	const OVR::HMDInfo&  HMDInfo;

	// Whether we called StartSensor() and requested sensor caps.
	// pSensor may still be null or non-running after start if it wasn't yet available
	bool                SensorStarted;
	unsigned            SensorCaps;
	// Atomic integer used as a flag that we should check the device.
	AtomicInt<int>      AddSensorCount;
	Ptr<SensorDevice>   pSensor;	// Head
	SensorFusion        SFusion;

	// Latency tester
	Ptr<LatencyTestDevice>  pLatencyTester;
	Util::LatencyTest	    LatencyUtil;
	AtomicInt<int>          AddLatencyTestCount;

	const char*        pLastError;
};


}} // namespace OVR::CAPI


#endif	// OVR_CAPI_HMDState_h


