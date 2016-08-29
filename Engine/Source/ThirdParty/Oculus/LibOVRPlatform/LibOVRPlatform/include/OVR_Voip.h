// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_VOIP_H
#define OVR_VOIP_H

#include "OVR_Platform_Defs.h"
#include "OVR_Types.h"
#include "OVR_VoipSampleRate.h"

/// Accepts a VoIP connection from a given user.
OVRP_PUBLIC_FUNCTION(void) ovr_Voip_Accept(ovrID userID);

/// Returns the size of the internal ringbuffer used by the voip system in
/// elements. This size is the maximum number of elements that can ever be
/// returned by ovr_Voip_GetPCMData.
OVRP_PUBLIC_FUNCTION(size_t) ovr_Voip_GetOutputBufferMaxSize();

/// Gets all available samples of voice data for the specified peer and copies
/// it into outputBuffer. The voip system will generate data at roughly the
/// rate of 480 samples per 10ms. This function should be called every frame
/// with 50ms (2400 elements) of buffer size to account for frame rate
/// variations. The data format is 16 bit fixed point 48khz mono.
OVRP_PUBLIC_FUNCTION(size_t) ovr_Voip_GetPCM(ovrID senderID, int16_t *outputBuffer, size_t outputBufferNumElements);

/// Gets all available samples of voice data for the specified peer and copies
/// it into outputBuffer. The voip system will generate data at roughly the
/// rate of 480 samples per 10ms. This function should be called every frame
/// with 50ms (2400 elements) of buffer size to account for frame rate
/// variations. The data format is 32 bit floating point 48khz mono.
OVRP_PUBLIC_FUNCTION(size_t) ovr_Voip_GetPCMFloat(ovrID senderID, float *outputBuffer, size_t outputBufferNumElements);

/// Returns the current number of audio samples available to read for the
/// specified user. This function is inherently racy; it's possible that more
/// data can be added between a call to this function and a subsequent call to
/// ovr_Voip_GetPCM.
OVRP_PUBLIC_FUNCTION(size_t) ovr_Voip_GetPCMSize(ovrID senderID);

/// Sets the output sample rate. Audio data will be resampled as it is placed
/// into the internal ringbuffer.
OVRP_PUBLIC_FUNCTION(void) ovr_Voip_SetOutputSampleRate(ovrVoipSampleRate rate);

/// Attempts to establish a VoIP session with the specified user. Message of
/// type ovrMessage_Notification_Voip_StateChange will be posted when the
/// session gets established.
OVRP_PUBLIC_FUNCTION(void) ovr_Voip_Start(ovrID userID);

/// Terminates a VoIP session with the specified user. Note that a muting
/// functionality should be used to temporarily stop sending audio; restarting
/// a VoIP session after tearing it down may be an expensive operation.
OVRP_PUBLIC_FUNCTION(void) ovr_Voip_Stop(ovrID userID);


#endif
