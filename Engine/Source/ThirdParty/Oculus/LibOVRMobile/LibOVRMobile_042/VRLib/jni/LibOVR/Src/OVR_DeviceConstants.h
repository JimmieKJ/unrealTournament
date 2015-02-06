/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_DeviceConstants.h
Content     :   Device constants
Created     :   February 5, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_DeviceConstants_h
#define OVR_DeviceConstants_h

namespace OVR {


//-------------------------------------------------------------------------------------
// Different device types supported by OVR; this type is reported by DeviceBase::GetType.
// 
enum DeviceType
{
    Device_None             = 0,
    Device_Manager          = 1,
    Device_HMD              = 2,
    Device_Sensor           = 3,
    Device_LatencyTester    = 4,
    Device_BootLoader       = 5,
    Device_All              = 0xFF // Set for enumeration only, to enumerate all device types.
};



//-------------------------------------------------------------------------------------
// Different lens distortion types supported by devices.
// 
enum DistortionEqnType
{
    Distortion_No_Override  = -1,    
	// These two are leagcy and deprecated.
    Distortion_Poly4        = 0,    // scale = (K0 + K1*r^2 + K2*r^4 + K3*r^6)
    Distortion_RecipPoly4   = 1,    // scale = 1/(K0 + K1*r^2 + K2*r^4 + K3*r^6)

    // CatmullRom10 is the preferred distortion format.
    Distortion_CatmullRom10 = 2,    // scale = Catmull-Rom spline through points (1.0, K[1]...K[9])
    Distortion_CatmullRom20 = 3,    // scale = Catmull-Rom spline through points (1.0, K[1]...K[20])

    Distortion_Experimental = 4,	// JDC

    Distortion_LAST                 // For ease of enumeration.
};


//-------------------------------------------------------------------------------------
// HMD types.
//
enum HmdTypeEnum
{
    HmdType_None,

    HmdType_DKProto,            // First duct-tape model, never sold.
    HmdType_DK1,                // DevKit1 - on sale to developers.
    HmdType_DKHDProto,          // DKHD - shown at various shows, never sold.
    HmdType_DKHDProto585,       // DKHD, 5.85-inch panel, never sold.
    HmdType_DKHDProto566Mi,     // DKHD, 5.66-inch panel, never sold.

    // Reminder - this header file is public - codenames only!

    HmdType_Unknown,            // Used for unnamed HW lab experiments.

    HmdType_LAST
};


//-------------------------------------------------------------------------------------
// HMD shutter types.
//
enum HmdShutterTypeEnum
{
    HmdShutter_Global,
    HmdShutter_RollingTopToBottom,
    HmdShutter_RollingLeftToRight,
    // TODO:
    // color-sequential e.g. LCOS?
    // low-persistence?
    // alternate eyes?
    // alternate columns?
    // outside-in?

    HmdShutter_LAST
};



//-------------------------------------------------------------------------------------
// For headsets that use eye cups
//
enum EyeCupType
{
    // Public lenses
    EyeCup_BlackA = 0,
    EyeCup_BlackB = 1,
    EyeCup_BlackC = 2,

    // Internal R&D codenames.
    // Reminder - this header file is public - codenames only!
    EyeCup_OrangeA,
    EyeCup_RedA,
    EyeCup_BlueA,
    EyeCup_DelilahA,
    EyeCup_JamesA,
    EyeCup_SunMandalaA,

    EyeCup_LAST
};


} // namespace OVR

#endif
