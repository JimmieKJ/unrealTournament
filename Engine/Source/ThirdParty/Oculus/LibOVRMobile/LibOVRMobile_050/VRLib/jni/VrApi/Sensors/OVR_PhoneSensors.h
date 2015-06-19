/************************************************************************************

Filename    :   OVR_PhoneSensors.h
Content     :   Cross platform interface for non-tracker sensors.
Created     :   December 16, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_PhoneSensors_h
#define OVR_PhoneSensors_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_RefCount.h"

namespace OVR {

class PhoneSensors : public RefCountBase<PhoneSensors>
{
public:
	virtual void GetLatestUncalibratedMagAndBiasValue(Vector3f* mag, Vector3f* bias) = 0;

	static PhoneSensors* Create();
	static PhoneSensors* GetInstance();
	
protected:
	PhoneSensors()
	{}

private:
	static PhoneSensors *Instance;
};

} // namespace OVR

#endif
