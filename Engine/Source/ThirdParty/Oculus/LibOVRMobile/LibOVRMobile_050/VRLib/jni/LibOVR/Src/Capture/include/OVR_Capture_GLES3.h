/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_GLES3.h
Content     :   Oculus performance capture library. OpenGL ES 3 interfaces.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_GLES3_H
#define OVR_CAPTURE_GLES3_H

#include <OVR_Capture.h>

#if defined(OVR_CAPTURE_HAS_GLES3)

// Quick drop in performance zone.
#define OVR_CAPTURE_GPU_ZONE(name)                                          \
    OVR_CAPTURE_CREATE_LABEL(GL##name);                                     \
    OVR::Capture::GPUScopeGLES3 _ovrcap_gpuscope_##name(_ovrcap_label_##name);

namespace OVR
{
namespace Capture
{

    // Captures the frame buffer from a Texture Object from an OpenGL ES 3.0 Context.
    // Must be called from a thread with a valid GLES3 context!
    void FrameBufferGLES3(unsigned int textureID);

    void EnterGPUZoneGLES3(const Label &label);
    void LeaveGPUZoneGLES3(void);

    class GPUScopeGLES3
    {
        public:
            inline GPUScopeGLES3(const Label &label) :
                m_isReady(CheckConnectionFlag(Enable_GPU_Zones))
            {
                if(m_isReady) EnterGPUZoneGLES3(label);
            }
            inline ~GPUScopeGLES3(void)
            {
                if(m_isReady) LeaveGPUZoneGLES3();
            }
        private:
            const bool m_isReady;
    };

} // namespace Capture
} // namespace OVR

#endif // #if defined(OVR_CAPTURE_HAS_GLES3)

#endif
