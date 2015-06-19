/************************************************************************************

PublicHeader:   None
Filename    :   OVR_GyroTempCalibration.cpp
Content     :   Used to store gyro temperature calibration to local storage.
Created     :   June 26, 2014
Notes       :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_GyroTempCalibration.h"

#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_String_Utils.h"

#ifdef OVR_OS_WIN32
#include <Shlobj.h>
#else
#include <dirent.h>
#include <sys/stat.h>

#ifdef OVR_OS_LINUX
#include <unistd.h>
#include <pwd.h>
#endif
#endif

#define TEMP_CALIBRATION_FILE_VERSION_1		1	// Initial version.
#define TEMP_CALIBRATION_FILE_VERSION_2		2	// Introduced 2/9/15. Bug in OVR_SensorCalibration.cpp whereby the 
												// zero motion threshold is set badly means that we should overwrite
												// all version 1 files.
namespace OVR {

GyroTempCalibration::GyroTempCalibration()
{

	// Set default values.
	for (int binIndex = 0; binIndex < GyroCalibrationNumBins; binIndex++)
	{
		for (int sampleIndex = 0; sampleIndex < GyroCalibrationNumSamples; sampleIndex++)
		{
			GyroCalibrationEntry gce;			
			gce.Version = 0;					// This is the calibration entry version not the file version.
			gce.ActualTemperature = 0.0;
			gce.Time = 0;
			gce.Offset = Vector3d(0.0, 0.0, 0.0);

			GyroCalibration[binIndex][sampleIndex] = gce;
		}
	}
}

String GyroTempCalibration::GetBaseOVRPath(bool create_dir)
{
    String path;

#if defined(OVR_OS_WIN32)

    TCHAR data_path[MAX_PATH];
    SHGetFolderPath(0, CSIDL_LOCAL_APPDATA, NULL, 0, data_path);
    path = String(data_path);
    
    path += "/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        WCHAR wpath[128];
        OVR::UTF8Util::DecodeString(wpath, path.ToCStr());

        DWORD attrib = GetFileAttributes(wpath);
        bool exists = attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY);
        if (!exists)
        {   
            CreateDirectory(wpath, NULL);
        }
    }
        
#elif defined(OVR_OS_MAC)

    const char* home = getenv("HOME");
    path = home;
    path += "/Library/Preferences/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DIR* dir = opendir(path);
        if (dir == NULL)
        {
            mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        else
        {
            closedir(dir);
        }
    }

#elif defined(OVR_OS_ANDROID)

    // TODO: We probably should use the location of Environment.getExternalStoragePublicDirectory()
    const char* home = "/sdcard";
    path = home;
    path += "/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DIR* dir = opendir(path);
        if (dir == NULL)
        {
            mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        else
        {
            closedir(dir);
        }
    }

#else

    // Note that getpwuid is not safe - it sometimes returns NULL for users from LDAP.
    const char* home = getenv("HOME");
    path = home;
    path += "/.config/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DIR* dir = opendir(path);
        if (dir == NULL)
        {
            mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        else
        {
            closedir(dir);
        }
    }

#endif

    return path;
}

void GyroTempCalibration::Initialize(const String& deviceSerialNumber)
{
	DeviceSerialNumber = deviceSerialNumber;

	LoadFile();
}

String GyroTempCalibration::GetCalibrationPath(bool create_dir)
{
    String path = GetBaseOVRPath(create_dir);
    path += "/";
	path += "GyroCalibration_";
	path += DeviceSerialNumber;
	path += ".json";
	
    return path;
}

void GyroTempCalibration::TokenizeString(Array<String>* tokens, const String& str, char separator)
{
//	OVR_ASSERT(tokens != NULL);	// LDC - Asserts are currently not handled well on mobile.

	tokens->Clear();

	int tokenStart = 0;
	bool foundToken = false;
	
	for (int i=0; i <= str.GetLengthI(); i++)
	{
		if (i == str.GetLengthI() || str.GetCharAt(i) == separator)
		{
			if (foundToken)
			{
				// Found end of token.
				String token = str.Substring(tokenStart, i);
				tokens->PushBack(token);
				foundToken = false;
			}
		}
		else if (!foundToken)
		{
			foundToken = true;
			tokenStart = i;
		}
	}
}

void GyroTempCalibration::GyroCalibrationFromString(const String& str)
{

	Array<String> tokens;
	TokenizeString(&tokens, str, ' ');

	if (tokens.GetSize() != GyroCalibrationNumBins * GyroCalibrationNumSamples * 6)
	{
		LogError("Format of gyro calibration string in profile is incorrect.");
//		OVR_ASSERT(false);	// LDC - Asserts are currently not handled well on mobile.
		return;
	}

	for (int binIndex = 0; binIndex < GyroCalibrationNumBins; binIndex++)
	{
		for (int sampleIndex = 0; sampleIndex < GyroCalibrationNumSamples; sampleIndex++)
		{
			GyroCalibrationEntry& entry = GyroCalibration[binIndex][sampleIndex];
			
			int index = binIndex * GyroCalibrationNumSamples + sampleIndex;

			StringUtils::StringTo<UInt32>(entry.Version, tokens[index * 6 + 0].ToCStr());
			StringUtils::StringTo<double>(entry.ActualTemperature, tokens[index * 6 + 1].ToCStr());
			StringUtils::StringTo<UInt32>(entry.Time, tokens[index * 6 + 2].ToCStr());
			StringUtils::StringTo<double>(entry.Offset.x, tokens[index * 6 + 3].ToCStr());
			StringUtils::StringTo<double>(entry.Offset.y, tokens[index * 6 + 4].ToCStr());
			StringUtils::StringTo<double>(entry.Offset.z, tokens[index * 6 + 5].ToCStr());
		}	
	}
}

String GyroTempCalibration::GyroCalibrationToString()
{
	StringBuffer sb;
	for (int binIndex = 0; binIndex < GyroCalibrationNumBins; binIndex++)
	{
		for (int sampleIndex = 0; sampleIndex < GyroCalibrationNumSamples; sampleIndex++)
		{
			const GyroCalibrationEntry& entry = GyroCalibration[binIndex][sampleIndex];
			sb.AppendFormat("%d %lf %d %lf %lf %lf ", entry.Version, entry.ActualTemperature, entry.Time, entry.Offset.x, entry.Offset.y, entry.Offset.z);
		}	
	}

	return String(sb);
}

void GyroTempCalibration::LoadFile()
{
    String path = GetCalibrationPath(false);

    Ptr<JSON> root = *JSON::Load(path);
    if (!root || root->GetItemCount() < 2)
        return;

    JSON* versionItem = root->GetFirstItem();
	
    if (versionItem->Name != "Calibration Version")
    {
		LogError("Bad calibration file format.");
//		OVR_ASSERT(false);	// LDC - Asserts are currently not handled well on mobile.
		return;
	}

    int version = atoi(versionItem->Value.ToCStr());

	if (version == TEMP_CALIBRATION_FILE_VERSION_1)
	{
		// Old version. This data is potentially bad data so we ignore it.
		return;
	}

	if (version != TEMP_CALIBRATION_FILE_VERSION_2)
	{
		LogError("Bad calibration file version.");
//		OVR_ASSERT(false);	// LDC - Asserts are currently not handled well on mobile.
		return;
	}

	// Parse calibration data.
    JSON* dataItem = root->GetNextItem(versionItem);

    if (dataItem->Name != "Data")
    {
		LogError("Bad calibration file format.");
//		OVR_ASSERT(false);	// LDC - Asserts are currently not handled well on mobile.
		return;
	}
	
	String calibData(dataItem->Value.ToCStr());
	GyroCalibrationFromString(calibData);
}

void GyroTempCalibration::SaveFile()
{
    Ptr<JSON> root = *JSON::CreateObject();
	root->AddNumberItem("Calibration Version", TEMP_CALIBRATION_FILE_VERSION_2);

	String str = GyroCalibrationToString();
    root->AddStringItem("Data", str.ToCStr());
	
	String path = GetCalibrationPath(true);
    root->Save(path);
}
	
void GyroTempCalibration::GetAllTemperatureReports(Array<Array<TemperatureReport> >* tempReports)
{		
    TemperatureReport t;
	
    int bins = GyroCalibrationNumBins;
	int samples = GyroCalibrationNumSamples;

    tempReports->Clear();
    tempReports->Resize(bins);
    for (int i = 0; i < bins; i++)
	{
        (*tempReports)[i].Resize(samples);
	}

    for (int i = 0; i < bins; i++)
	{
        for (int j = 0; j < samples; j++)
        {
			GetTemperatureReport(i, j, &t);
            (*tempReports)[t.Bin][t.Sample] = t;
        }
	}
}

void GyroTempCalibration::GetTemperatureReport(int binIndex, int sampleIndex, TemperatureReport* tempReport)
{

	// Fill in default values.
	*tempReport = TemperatureReport();	
	tempReport->NumBins = GyroCalibrationNumBins;
	tempReport->Bin = binIndex;
	tempReport->Sample = sampleIndex;
	tempReport->NumSamples = GyroCalibrationNumSamples;
	tempReport->TargetTemperature = binIndex * 5.0 + 15.0;
	
	const GyroCalibrationEntry& entry = GyroCalibration[binIndex][sampleIndex];

	tempReport->Version = entry.Version;
	tempReport->ActualTemperature = entry.ActualTemperature;
	tempReport->Time = entry.Time;
	tempReport->Offset = entry.Offset;
}

void GyroTempCalibration::SetTemperatureReport(const TemperatureReport& tempReport)
{
	GyroCalibrationEntry& entry = GyroCalibration[tempReport.Bin][tempReport.Sample];

	entry.Version = tempReport.Version;
	entry.ActualTemperature = tempReport.ActualTemperature;
	entry.Time = tempReport.Time;
	entry.Offset = tempReport.Offset;
	
	SaveFile();
}	

}  // OVR
