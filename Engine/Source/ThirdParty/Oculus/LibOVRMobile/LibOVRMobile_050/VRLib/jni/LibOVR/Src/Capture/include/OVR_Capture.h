/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture.h
Content     :   Oculus performance capture library
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_H
#define OVR_CAPTURE_H

#include <OVR_Capture_Config.h>
#include <OVR_Capture_Types.h>

#include <stdarg.h> // for va_list


// Thread-Safe creation of local static label...
#define OVR_CAPTURE_CREATE_LABEL(name)                                                 \
    static OVR::Capture::Label _ovrcap_label_##name;                                   \
    static bool                _ovrcap_label_initialized_##name = false;               \
    if(!_ovrcap_label_initialized_##name)                                              \
        _ovrcap_label_initialized_##name = _ovrcap_label_##name.ConditionalInit(#name);

// Quick drop in performance zone.
#define OVR_CAPTURE_CPU_ZONE(name)                                          \
    OVR_CAPTURE_CREATE_LABEL(name);                                         \
    OVR::Capture::CPUScope _ovrcap_cpuscope_##name(_ovrcap_label_##name);

#define OVR_CAPTURE_SENSOR_SET(name, value)                                 \
    if(OVR::Capture::IsConnected())                                         \
    {                                                                       \
        OVR_CAPTURE_CREATE_LABEL(name);                                     \
        OVR::Capture::SensorSetValue(_ovrcap_label_##name, value);          \
    }

#define OVR_CAPTURE_COUNTER_ADD(name, value)                                \
    if(OVR::Capture::IsConnected())                                         \
    {                                                                       \
        OVR_CAPTURE_CREATE_LABEL(name);                                     \
        OVR::Capture::CounterAddValue(_ovrcap_label_##name, value);         \
    }

namespace OVR
{
namespace Capture
{

    class Label;
    class CPUScope;

    // Get current time in nanoseconds...
    UInt64 GetNanoseconds(void);

    // Initializes the Capture system... should be called before any other Capture call.
    bool Init(const char *packageName, UInt32 flags=Default_Flags);

    // Closes the capture system... no other Capture calls on *any* thead should be called after this.
    void Shutdown(void);

    // Indicates that the capture system is currently connected...
    bool IsConnected(void);

    // Check to see if (a) a connection is established and (b) that a particular capture feature is enabled on the connection.
    bool CheckConnectionFlag(CaptureFlag feature);

    // Mark frame boundary... call from only one thread!
    void Frame(void);

    // Mark the start of vsync... this value should be comparable to the same reference point as GetNanoseconds()
    void VSyncTimestamp(UInt64 nanoseconds);

    // Upload the framebuffer for the current frame... should be called once a frame!
    void FrameBuffer(UInt64 nanoseconds, FrameBufferFormat format, UInt32 width, UInt32 height, const void *buf);

    // Misc application message logging...
    void Logf(LogPriority priority, const char *format, ...);
    void Logv(LogPriority priority, const char *format, va_list args);

    // Mark a CPU profiled region.... Begin(); DoSomething(); End();
    // Nesting is allowed. And every Begin() should ALWAYS have a matching End()!!!
    void EnterCPUZone(const Label &label);
    void LeaveCPUZone(void);

    // Set sensor range of values.
    void SensorSetRange(const Label &label, float minValue, float maxValue, SensorInterpolator interpolator=Sensor_Interp_Linear, SensorUnits units=Sensor_Unit_None);

    // Set the absolute value of a sensor, may be called at any frequency.
    void SensorSetValue(const Label &label, float value);

    // Add a value to a counter... the absolute value gets reset to zero every time Frame() is called.
    void CounterAddValue(const Label &label, float value);

    class Label
    {
        friend class Server;
        public:
            // we assume we are already initialized to zero... C++ spec guarantees this for global/static memory.
            // use this function if Label is a local static variable. and initialize using conditionalInit()
            inline Label(void) {}

            // Use this constructor if Label is a global variable (not local static).
            Label(const char *name);

            bool ConditionalInit(const char *name);

            inline UInt32 GetIdentifier(void) const
            {
                return m_identifier;
            }

            inline const char  *GetName(void) const
            {
                return m_name;
            }

        private:
            void Init(const char *name);
            static Label *GetHead(void);
            Label *GetNext(void) const;

        private:
            static Label  *s_head;
            Label         *m_next;
            UInt32         m_identifier;
            const char    *m_name;
    };

    class CPUScope
    {
        public:
            inline CPUScope(const Label &label) :
                m_isReady(CheckConnectionFlag(Enable_CPU_Zones))
            {
                if(m_isReady) EnterCPUZone(label);
            }
            inline ~CPUScope(void)
            {
                if(m_isReady) LeaveCPUZone();
            }
        private:
            const bool m_isReady;
    };


} // namespace Capture
} // namespace OVR

#endif
