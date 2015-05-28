/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_Socket.h
Content     :   Oculus performance capture library. Support for standard builtin device sensors.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_STANDARDSENSORS_H
#define OVR_CAPTURE_STANDARDSENSORS_H

#include <OVR_Capture.h>
#include "OVR_Capture_Thread.h"

namespace OVR
{
namespace Capture
{

    class StandardSensors : public Thread
    {
        public:
            StandardSensors(void);
            virtual ~StandardSensors(void);

        private:
            virtual void OnThreadExecute(void);

        private:
            
    };

} // namespace Capture
} // namespace OVR

#endif
