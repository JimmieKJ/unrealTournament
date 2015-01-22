/************************************************************************************

Filename    :   OVR_SensorCalibration.h
Content     :   Calibration data implementation for the IMU messages 
Created     :   January 28, 2014
Authors     :   Max Katsev

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_SensorCalibration_h
#define OVR_SensorCalibration_h

#include "OVR_Device.h"
#include "OVR_SensorFilter.h"
#include "OVR_GyroTempCalibration.h"

#define USE_LOCAL_TEMPERATURE_CALIBRATION_STORAGE 1

namespace OVR {

class OffsetInterpolator
{
public:
    void Initialize(Array<Array<TemperatureReport> > const& temperatureReports, int coord);
    double GetOffset(double targetTemperature, double autoTemperature, double autoValue);

    Array<double> Temperatures;
    Array<double> Values;
};

class SensorCalibration : public NewOverrideBase
{
public:
    SensorCalibration(SensorDevice* pSensor);

    // Load data from the HW and perform the necessary preprocessing
    void Initialize(const String& deviceSerialNumber);
    // Apply the calibration
    void Apply(MessageBodyFrame& msg);
 
protected:
    void StoreAutoOffset();
    void AutocalibrateGyro(MessageBodyFrame const& msg);

	void DebugPrintLocalTemperatureTable();
	void DebugClearHeadsetTemperatureReports();

    SensorDevice* pSensor;

    // Factory calibration data
    Matrix4f AccelMatrix, GyroMatrix;
    Vector3f AccelOffset;
    
    // Temperature based data
    Array<Array<TemperatureReport> > TemperatureReports;
    OffsetInterpolator Interpolators[3];

    // Autocalibration data
    SensorFilterf GyroFilter;
    Vector3f GyroAutoOffset;
    float GyroAutoTemperature;
	
#ifdef USE_LOCAL_TEMPERATURE_CALIBRATION_STORAGE
	GyroTempCalibration GyroCalibration;
#endif	
};

} // namespace OVR
#endif //OVR_SensorCalibration_h
