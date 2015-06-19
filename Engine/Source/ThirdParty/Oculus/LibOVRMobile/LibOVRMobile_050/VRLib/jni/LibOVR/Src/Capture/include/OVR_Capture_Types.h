/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_Types.h
Content     :   Oculus performance capture library.
Created     :   January, 2015
Notes       :   Pull types from kernel? Although that would lead to OVRMonitor needing
                to depend on Kernel as well.

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_TYPES_H
#define OVR_CAPTURE_TYPES_H

#include <OVR_Capture_Config.h>

namespace OVR
{
namespace Capture
{

    // We choose very specific types for the network protocol...
    typedef unsigned long long UInt64;
    typedef unsigned int       UInt32;
    typedef unsigned short     UInt16;
    typedef unsigned char      UInt8;

    typedef signed long long   Int64;
    typedef signed int         Int32;
    typedef signed short       Int16;
    typedef signed char        Int8;

    OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt64) == 8);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt32) == 4);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt16) == 2);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(UInt8)  == 1);

    OVR_CAPTURE_STATIC_ASSERT(sizeof(Int64)  == 8);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(Int32)  == 4);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(Int16)  == 2);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(Int8)   == 1);

    OVR_CAPTURE_STATIC_ASSERT(sizeof(float)  == 4);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(double) == 8);

    enum CaptureFlag
    {
        Enable_CPU_Zones           = 1<<0,
        Enable_GPU_Zones           = 1<<1,
        Enable_CPU_Clocks          = 1<<2,
        Enable_GPU_Clocks          = 1<<3,
        Enable_Thermal_Sensors     = 1<<4,
        Enable_FrameBuffer_Capture = 1<<5,
        Enable_Logging             = 1<<6,
        Enable_SysTrace            = 1<<7,

        Default_Flags           = Enable_CPU_Zones | Enable_GPU_Zones | Enable_CPU_Clocks | Enable_GPU_Clocks | Enable_Thermal_Sensors | Enable_FrameBuffer_Capture | Enable_Logging | Enable_SysTrace,
        All_Flags               = Enable_CPU_Zones | Enable_GPU_Zones | Enable_CPU_Clocks | Enable_GPU_Clocks | Enable_Thermal_Sensors | Enable_FrameBuffer_Capture | Enable_Logging | Enable_SysTrace,
    };

    enum FrameBufferFormat
    {
        FrameBuffer_RGB_565 = 0,
        FrameBuffer_RGBA_8888,
    };

    enum SensorInterpolator
    {
        Sensor_Interp_Linear = 0,
        Sensor_Interp_Nearest,
    };

    enum SensorUnits
    {
        Sensor_Unit_None = 0,

        // Frequencies...
        Sensor_Unit_Hz,
        Sensor_Unit_KHz,
        Sensor_Unit_MHz,
        Sensor_Unit_GHz,

        // Memory size...
        Sensor_Unit_Byte,
        Sensor_Unit_KByte,
        Sensor_Unit_MByte,
        Sensor_Unit_GByte,

        // Memory Bandwidth...
        Sensor_Unit_Byte_Second,
        Sensor_Unit_KByte_Second,
        Sensor_Unit_MByte_Second,
        Sensor_Unit_GByte_Second,

        // Temperature
        Sensor_Unit_Celsius,
    };

    enum LogPriority
    {
        Log_Info = 0,
        Log_Warning,
        Log_Error,
    };

} // namespace Capture
} // namespace OVR

#endif
