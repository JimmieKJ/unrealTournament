/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_Socket.cpp
Content     :   Oculus performance capture library. Support for standard builtin device sensors.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_StandardSensors.h"
#include "OVR_Capture_FileIO.h"

#include <stdio.h> // sprintf

namespace OVR
{
namespace Capture
{

    struct CpuSensorDesc
    {
        Label       label;
        const char *onlinePath;
        const char *freqPath;
        const char *maxFreqPath;
    };
    static const CpuSensorDesc g_cpuDescs[] =
    {
        { Label("CPU0 Clocks"), "/sys/devices/system/cpu/cpu0/online", "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq" },
        { Label("CPU1 Clocks"), "/sys/devices/system/cpu/cpu1/online", "/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq", "/sys/devices/system/cpu/cpu1/cpufreq/cpuinfo_max_freq" },
        { Label("CPU2 Clocks"), "/sys/devices/system/cpu/cpu2/online", "/sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq", "/sys/devices/system/cpu/cpu2/cpufreq/cpuinfo_max_freq" },
        { Label("CPU3 Clocks"), "/sys/devices/system/cpu/cpu3/online", "/sys/devices/system/cpu/cpu3/cpufreq/scaling_cur_freq", "/sys/devices/system/cpu/cpu3/cpufreq/cpuinfo_max_freq" },
    };
    static const unsigned int g_maxCpus = sizeof(g_cpuDescs) / sizeof(g_cpuDescs[0]);

    static const Label g_gpuLabel("GPU Clocks");
    static const Label g_memLabel("Memory Bandwidth");

    struct ThermalSensorDesc
    {
        Label label;
        bool  initialized;
        char  name[64];
    };

    StandardSensors::StandardSensors(void)
    {
    }

    StandardSensors::~StandardSensors(void)
    {
    }

    void StandardSensors::OnThreadExecute(void)
    {
        // Pre-load what files we can... reduces open/close overhead (which is significant)

        // Setup CPU Clocks Support...
        FileHandle cpuOnlineFiles[g_maxCpus];
        FileHandle cpuFreqFiles[g_maxCpus];
        for(unsigned int i=0; i<g_maxCpus; i++)
        {
            cpuOnlineFiles[i] = cpuFreqFiles[i] = NullFileHandle;
        }
        if(CheckConnectionFlag(Enable_CPU_Zones))
        {
            for(unsigned int i=0; i<g_maxCpus; i++)
            {
                const CpuSensorDesc &desc = g_cpuDescs[i];
                cpuOnlineFiles[i] = OpenFile(desc.onlinePath);
                cpuFreqFiles[i]   = NullFileHandle;
                if(cpuOnlineFiles[i] != NullFileHandle)
                {
                    const int maxFreq = ReadIntFile(desc.maxFreqPath);
                    SensorSetRange(desc.label, 0, (float)maxFreq, Sensor_Interp_Nearest, Sensor_Unit_KHz);
                }
            }
        }

        // Setup GPU Clocks Support...
        FileHandle gpuFreqFile = NullFileHandle;
        if(CheckConnectionFlag(Enable_GPU_Clocks))
        {
            gpuFreqFile = OpenFile("/sys/class/kgsl/kgsl-3d0/gpuclk");
        }
        if(gpuFreqFile != NullFileHandle)
        {
            const int maxFreq = ReadIntFile("/sys/class/kgsl/kgsl-3d0/max_gpuclk");
            SensorSetRange(g_gpuLabel, 0, (float)maxFreq, Sensor_Interp_Nearest, Sensor_Unit_Hz);
        }

        // Setup Memory Clocks Support...
        FileHandle memFreqFile = NullFileHandle;
        //memFreqFile = OpenFile("/sys/class/devfreq/0.qcom,cpubw/cur_freq");
        if(memFreqFile != NullFileHandle)
        {
            const int maxFreq = ReadIntFile("/sys/class/devfreq/0.qcom,cpubw/max_freq");
            SensorSetRange(g_memLabel, 0, (float)maxFreq, Sensor_Interp_Nearest, Sensor_Unit_MByte_Second);
        }

        // Setup thermal sensors...
        static const unsigned int maxThermalSensors = 20;
        static ThermalSensorDesc  thermalDescs[maxThermalSensors];
        FileHandle                thermalFiles[maxThermalSensors];
        for(unsigned int i=0; i<maxThermalSensors; i++)
        {
            thermalFiles[i] = NullFileHandle;
        }
        if(CheckConnectionFlag(Enable_Thermal_Sensors))
        {
            for(unsigned int i=0; i<maxThermalSensors; i++)
            {
                ThermalSensorDesc &desc = thermalDescs[i];

                char typePath[64] = {0};
                char tempPath[64] = {0};
                char modePath[64] = {0};
                sprintf(typePath, "/sys/devices/virtual/thermal/thermal_zone%d/type", i);
                sprintf(tempPath, "/sys/devices/virtual/thermal/thermal_zone%d/temp", i);
                sprintf(modePath, "/sys/devices/virtual/thermal/thermal_zone%d/mode", i);

                // If either of these files don't exist, then we got to the end of the thermal zone list...
                if(!CheckFileExists(typePath) || !CheckFileExists(tempPath))
                    break;

                // check to see if the zone is disabled... its okay if there is no mode file...
                char mode[16] = {0};
                if(ReadFileLine(modePath, mode, sizeof(mode))>0 && !strcmp(mode, "disabled"))
                    continue;

                if(!desc.initialized)
                {
                    // Read the sensor name in...
                    ReadFileLine(typePath, desc.name, sizeof(desc.name));

                    // Initialize the Label...
                    desc.initialized = desc.label.ConditionalInit(desc.name);
                }

                // Finally... open the file.
                thermalFiles[i] = OpenFile(tempPath);
        
                if(thermalFiles[i] != NullFileHandle)
                {
                    // by default 0 to 100 degrees...
                    SensorSetRange(desc.label, 0, 100, Sensor_Interp_Linear);
                }
            }
        }

        // For clocks, we store the last value and only send updates when it changes since we
        // use blocking chart rendering.
        int lastCpuFreq[g_maxCpus] = {0};
        int lastGpuFreq            = 0;
        int lastMemValue           = 0;

        unsigned int sampleCount = 0;

        while(!QuitSignaled() && IsConnected())
        {
            // Sample CPU Frequencies...
            for(unsigned int i=0; i<g_maxCpus; i++)
            {
                // If the 'online' file can't be found, then we just assume this CPU doesn't even exist
                if(cpuOnlineFiles[i] == NullFileHandle)
                    continue;

                const CpuSensorDesc &desc   = g_cpuDescs[i];
                const bool           online = ReadIntFile(desc.onlinePath);
                if(online && cpuFreqFiles[i]==NullFileHandle)
                {
                    // Open the frequency file if we are online and its not already open...
                    cpuFreqFiles[i] = OpenFile(desc.freqPath);
                }
                else if(!online && cpuFreqFiles[i]!=NullFileHandle)
                {
                    // close the frequency file if we are no longer online
                    CloseFile(cpuFreqFiles[i]);
                    cpuFreqFiles[i] = NullFileHandle;
                }
                const int freq = cpuFreqFiles[i]==NullFileHandle ? 0 : ReadIntFile(cpuFreqFiles[i]);
                if(freq != lastCpuFreq[i])
                {
                    // Convert from KHz to Hz
                    SensorSetValue(desc.label, (float)freq);
                    lastCpuFreq[i] = freq;
                }
                ThreadYield();
            }

            // Sample GPU Frequency...
            if(gpuFreqFile != NullFileHandle)
            {
                const int freq = ReadIntFile(gpuFreqFile);
                if(freq != lastGpuFreq)
                {
                    SensorSetValue(g_gpuLabel, (float)freq);
                    lastGpuFreq = freq;
                }
            }

            // Sample Memory Bandwidth
            if(memFreqFile != NullFileHandle)
            {
                const int value = ReadIntFile(memFreqFile);
                if(value != lastMemValue)
                {
                    SensorSetValue(g_memLabel, (float)value);
                    lastMemValue = value;
                }
            }

            // Sample thermal sensors...
            if((sampleCount&15) == 0) // sample temperature at a much lower frequency as clocks... thermals don't change that fast.
            {
                for(unsigned int i=0; i<maxThermalSensors; i++)
                {
                    FileHandle file = thermalFiles[i];
                    if(file != NullFileHandle)
                    {
                        SensorSetValue(thermalDescs[i].label, (float)ReadIntFile(file));
                    }
                }
                ThreadYield();
            }

            // Sleep 5ms between samples...
            ThreadSleepMicroseconds(5000);
            sampleCount++;
        }

        // Close down cached file handles...
        for(unsigned int i=0; i<g_maxCpus; i++)
        {
            if(cpuOnlineFiles[i] != NullFileHandle) CloseFile(cpuOnlineFiles[i]);
            if(cpuFreqFiles[i]   != NullFileHandle) CloseFile(cpuFreqFiles[i]);
        }
        if(gpuFreqFile != NullFileHandle) CloseFile(gpuFreqFile);
        if(memFreqFile != NullFileHandle) CloseFile(memFreqFile);
        for(unsigned int i=0; i<maxThermalSensors; i++)
        {
            if(thermalFiles[i] != NullFileHandle) CloseFile(thermalFiles[i]);
        }
    }

} // namespace Capture
} // namespace OVR

