/************************************************************************************

Filename    :   OVR_CAPI.h
Content     :   C Interface to Oculus sensors and rendering.
Created     :   November 23, 2013
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_CAPI_h
#define OVR_CAPI_h

//-----------------------------------------------------------------------------------
// ***** Simple Math Structures

// 2D integer
typedef struct ovrVector2i_
{
    int x, y;
} ovrVector2i;
typedef struct ovrSizei_
{
    int w, h;
} ovrSizei;
typedef struct ovrRecti_
{
    ovrVector2i Pos;
    ovrSizei    Size;
} ovrRecti;

// 3D
typedef struct ovrQuatf_
{
    float x, y, z, w;  
} ovrQuatf;
typedef struct ovrVector2f_
{
    float x, y;
} ovrVector2f;
typedef struct ovrVector3f_
{
    float x, y, z;
} ovrVector3f;
typedef struct ovrMatrix4f_
{
    float M[4][4];
} ovrMatrix4f;
// Position and orientation together.
typedef struct ovrPosef_
{
    ovrQuatf     Orientation;
    ovrVector3f  Position;    
} ovrPosef;

// Full pose (rigid body) configuration with first and second derivatives.
typedef struct ovrPoseStatef_
{
    ovrPosef     Pose;
    ovrVector3f  AngularVelocity;
    ovrVector3f  LinearVelocity;
    ovrVector3f  AngularAcceleration;
    ovrVector3f  LinearAcceleration;
    double       TimeInSeconds;         // Absolute time of this state sample.
} ovrPoseStatef;

/*
// Field Of View (FOV) in tangent of the angle units.
// As an example, for a standard 90 degree vertical FOV, we would 
// have: { UpTan = tan(90 degrees / 2), DownTan = tan(90 degrees / 2) }.
typedef struct ovrFovPort_
{
    float UpTan;
    float DownTan;
    float LeftTan;
    float RightTan;
} ovrFovPort;
*/


//-----------------------------------------------------------------------------------
// ***** HMD Types

// HMD capability bits reported by device.
//
typedef enum
{
    //ovrHmdCap_Present           = 0x0001,   //  This HMD exists (as opposed to being unplugged).
    //ovrHmdCap_Available         = 0x0002,   //  HMD and is sensor is available for use
											//  (if not owned by another app).    
    ovrHmdCap_Orientation       = 0x0010,   //  Support orientation tracking (IMU).
    ovrHmdCap_YawCorrection     = 0x0020,   //  Supports yaw correction through magnetometer or other means.
    ovrHmdCap_Position          = 0x0040,   //  Supports positional tracking.
} ovrHmdCapBits;



// Handle to HMD; returned by ovrHmd_Create.
typedef struct ovrHmdStruct* ovrHmd;

/*
// Describes the type of positional tracking being done.
typedef enum
{
    ovrPose_None,
    ovrPose_HeadModel,
    ovrPose_Positional
} ovrPoseType;
*/


// Bit flags describing the current status of sensor tracking.
typedef enum
{
    ovrStatus_OrientationTracked    = 0x0001,   // Orientation is currently tracked (connected and in use).
    ovrStatus_PositionTracked       = 0x0002,   // Position is currently tracked (FALSE if out of range).
    ovrStatus_PositionConnected     = 0x0020,   // Position tracking HW is connected.
    ovrStatus_HmdConnected          = 0x0080    // HMD Display is available & connected.
} ovrStatusBits;


// Specifies which eye is being used for rendering.
// This type explicitly does not include a third "NoStereo" option, as such is
// not required for an HMD-centered API.
typedef enum
{
    ovrEye_Left  = 0,
    ovrEye_Right = 1,
    ovrEye_Count = 2
} ovrEyeType;

// State of the sensor at a given absolute time.
typedef struct ovrSensorState_
{
    // Predicted pose configuration at requested absolute time.
    // One can determine the time difference between predicted and actual
    // readings by comparing ovrPoseState.TimeInSeconds.
    ovrPoseStatef  Predicted;
    // Actual recorded pose configuration based on the sensor sample at a 
    // moment closest to the requested time.
    ovrPoseStatef  Recorded;
    // Sensor temperature reading, in degrees Celsius, as sample time.
    float          Temperature;    
    // Sensor status described by ovrStatusBits.
    unsigned       Status;

} ovrSensorState;

// For now.
// TBD: Decide if this becomes a part of HMDDesc
typedef struct ovrSensorDesc_
{
    // HID Vendor and ProductId of the device.
    short   VendorId;
    short   ProductId;
    // Sensor (and display) serial number.
    char    SerialNumber[20];

    // From DeviceInfo
    unsigned	Version;
} ovrSensorDesc;


// -----------------------------------------------------------------------------------
// ***** API Interfaces

// Basic steps to use the API:
//
// Setup:
//  1. ovr_Initialize();
//  2. ovrHmd hmd = ovrHmd_Create(0);
//  3. ovrHmd_StartSensor(hmd, ovrHmdCap_Orientation|ovrHmdCap_YawCorrection, 0);
//
// Game Loop:
//  6. Call ovrHmd_PredictedSensorState for get rendering orientation for each eye.
//
// Shutdown:
//  9. ovrHmd_Destroy(hmd)
//  10. ovr_Shutdown()
//


// Library init/shutdown, must be called around all other OVR code.
// No other functions calls are allowed before ovr_Initialize succeeds or after ovr_Shutdown.
bool        ovr_Initialize();
void        ovr_Shutdown();

// Detects or re-detects HMDs and reports the total number detected.
// Users can get information about each HMD by calling ovrHmd_Create with an index.
int         ovrHmd_Detect();

// Creates a handle to an HMD and optionally fills in data about it.
// Index can [0 .. ovrHmd_Detect()-1]; index mappings can cange after each ovrHmd_Detect call.
// If not null, returned handle must be freed with ovrHmd_Destroy.
ovrHmd      ovrHmd_Create(int index);
void        ovrHmd_Destroy(ovrHmd hmd);


// Returns last error for HMD state. Returns null for no error.
// String is valid until next call or GetLastError or HMD is destroyed.
// Pass null hmd to get global error (for create, etc).
const char* ovrHmd_GetLastError(ovrHmd hmd);


//-------------------------------------------------------------------------------------
// ***** Sensor Interface

// Starts sensor sampling, enabling specified capabilities, described by ovrHmdCapBits.
//  - supportedCaps specifies support that is requested. The function will succeed even if,
//    if these caps are not available (i.e. sensor or camera is unplugged). Support will
//    automatically be enabled if such device is plugged in later. Software should check
//    ovrSensorState.Status for real-time status.
// - requiredCaps specify sensor capabilities required at the time of the call. If they
//   are not available, the function will fail. Pass 0 if only specifying SupportedCaps.
bool        ovrHmd_StartSensor(ovrHmd hmd, unsigned supportedSensorCaps,
										   unsigned requiredSensorCaps);
// Stops sensor sampling, shutting down internal resources.
void        ovrHmd_StopSensor(ovrHmd hmd);
// Resets sensor orientation.
void        ovrHmd_ResetSensor(ovrHmd hmd);
// Recenters the orientation on the yaw axis.
void        ovrHmd_RecenterYaw(ovrHmd hmd);

// Returns sensor state reading based on the specified absolute system time.
// Pass absTime value of 0.0 to request the most recent sensor reading; in this case
// both PredictedPose and SamplePose will have the same value.
// ovrHmd_GetEyePredictedSensorState relies on this internally.
// This may also be used for more refined timing of FrontBuffer rendering logic, etc.
ovrSensorState ovrHmd_GetSensorState(ovrHmd hmd, double absTime, bool allowSensorCreate);





// Returns information about a sensor.
// Only valid after SensorStart.
bool         ovrHmd_GetSensorDesc(ovrHmd hmd, ovrSensorDesc* descOut);

//-------------------------------------------------------------------------------------
// ***** Stateless math setup functions

// Returns global, absolute high-resolution time in seconds. This is the same
// value as used in sensor messages.
double      ovr_GetTimeInSeconds();

// -----------------------------------------------------------------------------------
// ***** Latency Test interface

// Does latency test processing and returns 'TRUE' if specified rgb color should
// be used to clear the screen.
bool         ovrHmd_ProcessLatencyTest(ovrHmd hmd, unsigned char rgbColorOut[3]);

// Returns non-null string once with latency test result, when it is available.
// Buffer is valid until next call.
const char*  ovrHmd_GetLatencyTestResult(ovrHmd hmd);

// -----------------------------------------------------------------------------------
// ***** Mobile Additions

int          ovr_GetDeviceManagerThreadTid();
void         ovr_SuspendDeviceManagerThread();
void         ovr_ResumeDeviceManagerThread();

#endif	// OVR_CAPI_h
