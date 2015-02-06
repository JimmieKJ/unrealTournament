/************************************************************************************

Filename    :   OVR_Android_PhoneSensors.cpp
Content     :   Android sensor interface.
Created     :
Authors     :	Lee Cooper

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_Android_PhoneSensors.h"

#include <android/sensor.h>
#include <android/looper.h>

#include "Kernel/OVR_Log.h"

#include <jni.h>

namespace OVR { namespace Android {

#define LOOPER_ID 1
#define SAMP_PER_SEC 100
#define UNCALIBRATED_MAG_SENSOR_ID 14	// Not declared in sensor.h for some reason.

PhoneSensors::PhoneSensors()
	: pQueue(NULL)
{
	LatestMagUncalibratedBias.Set(0.0f, 0.0f, 0.0f);
	IsFirstExecution = true;
}

PhoneSensors::~PhoneSensors()
{}

void PhoneSensors::GetLatestUncalibratedMagAndBiasValue(Vector3f* mag, Vector3f* bias)
{
	if (pQueue == NULL)
	{
		// Initialize on same thread as read.
		ASensorManager* sensorManager = ASensorManager_getInstance();

		ALooper* looper = ALooper_forThread();
		if(looper == NULL)
		{
			looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
		}

		MagSensorUncalibrated = ASensorManager_getDefaultSensor(sensorManager, UNCALIBRATED_MAG_SENSOR_ID);

		pQueue = ASensorManager_createEventQueue(sensorManager, looper, LOOPER_ID, NULL, NULL);

		ASensorEventQueue_enableSensor(pQueue, MagSensorUncalibrated);
		ASensorEventQueue_setEventRate(pQueue, MagSensorUncalibrated, (1000L/SAMP_PER_SEC)*1000);
	}


	int ident;	// Identifier.
	int events;

	while ((ident = ALooper_pollAll(0, NULL, &events, NULL) >= 0))
	{
		if (ident == LOOPER_ID)
		{
			ASensorEvent event;
			while (ASensorEventQueue_getEvents(pQueue, &event, 1) > 0)
			{
				if (event.type == UNCALIBRATED_MAG_SENSOR_ID)
				{
					LatestMagUncalibrated = Vector3f(	event.uncalibrated_magnetic.x_uncalib,
														event.uncalibrated_magnetic.y_uncalib,
														event.uncalibrated_magnetic.z_uncalib);

					Vector3f newMagUncalibratedBias =  Vector3f(	event.uncalibrated_magnetic.x_bias,
																	event.uncalibrated_magnetic.y_bias,
																	event.uncalibrated_magnetic.z_bias);

					if (!IsFirstExecution && newMagUncalibratedBias != LatestMagUncalibratedBias)
					{
						LogText("PhoneSensors: detected Android mag calibration (%.3f %.3f %.3f).\n", 	newMagUncalibratedBias.x,
																										newMagUncalibratedBias.y,
																										newMagUncalibratedBias.z);
					}

					LatestMagUncalibratedBias = newMagUncalibratedBias;
					IsFirstExecution = false;
				}
			}
		}
	}

	*mag = LatestMagUncalibrated;
	*bias = LatestMagUncalibratedBias;
}


} // namespace Android

PhoneSensors *PhoneSensors::Instance = NULL;

PhoneSensors* PhoneSensors::Create()
{
	if ( Instance == NULL )
	{
		Instance = new Android::PhoneSensors();
	}
	return Instance;
}

PhoneSensors* PhoneSensors::GetInstance()
{
	return Instance;
}

} // namespace OVR
