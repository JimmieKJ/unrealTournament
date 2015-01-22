/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_GyroTempCalibration.h
Content     :   Used to store gyro temperature calibration to local storage.
Created     :   June 26, 2014
Notes       :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_GyroTempCalibration_h
#define OVR_GyroTempCalibration_h

#include "OVR_Device.h"
#include "Kernel/OVR_Array.h"

namespace OVR {

class GyroTempCalibration
{
public:	
	GyroTempCalibration();

	void Initialize(const String& deviceSerialNumber);
	
	void GetAllTemperatureReports(Array<Array<TemperatureReport> >* tempReports);
	void SetTemperatureReport(const TemperatureReport& tempReport);

private:
	enum { GyroCalibrationNumBins = 7 };
	enum { GyroCalibrationNumSamples = 5 };

	struct GyroCalibrationEntry
	{
		UInt32		Version;
		double	    ActualTemperature;
		UInt32      Time;
		Vector3d    Offset;	
	};
	String GetBaseOVRPath(bool create_dir);
	String GetCalibrationPath(bool create_dir);
	void TokenizeString(Array<String>* tokens, const String& str, char separator);
	void GyroCalibrationFromString(const String& str);
	String GyroCalibrationToString();
	void GetTemperatureReport(int binIndex, int sampleIndex, TemperatureReport* tempReport);

	void LoadFile();
	void SaveFile();

	String DeviceSerialNumber;
	GyroCalibrationEntry GyroCalibration[GyroCalibrationNumBins][GyroCalibrationNumSamples];	
};

} // namespace OVR

#endif // OVR_GyroTempCalibration_h
