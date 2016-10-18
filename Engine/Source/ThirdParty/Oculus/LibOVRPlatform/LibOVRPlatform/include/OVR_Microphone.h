// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_MICROPHONE_H
#define OVR_MICROPHONE_H

#include "OVR_Platform_Defs.h"
#include <stddef.h>

typedef struct ovrMicrophone *ovrMicrophoneHandle;

OVRP_PUBLIC_FUNCTION(size_t) ovr_Microphone_ReadData(const ovrMicrophoneHandle obj, float *outputBuffer, size_t outputBufferSize);
OVRP_PUBLIC_FUNCTION(void)   ovr_Microphone_Start(const ovrMicrophoneHandle obj);
OVRP_PUBLIC_FUNCTION(void)   ovr_Microphone_Stop(const ovrMicrophoneHandle obj);

#endif
