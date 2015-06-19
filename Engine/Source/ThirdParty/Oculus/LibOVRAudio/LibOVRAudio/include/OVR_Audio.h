/************************************************************************************
 
PublicHeader:   OVR_Audio.h
Filename    :   OVR_Audio.h
Content     :   Implementation of audio SDK
Created     :   June 28, 2014
Authors     :   Brian Hook
Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.
 
Use of this software is subject to the terms of the Oculus VR, LLC license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.
 
************************************************************************************/
#ifndef OVR_Audio_h
#define OVR_Audio_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OVR_AUDIO_MAJOR_VERSION 0
#define OVR_AUDIO_MINOR_VERSION 10
#define OVR_AUDIO_PATCH_VERSION 0

#ifdef _WIN32
#ifdef OVR_AUDIO_DLL
#define OVRA_EXPORT __declspec( dllexport )
#else
#define OVRA_EXPORT 
#endif
#define FUNC_NAME __FUNCTION__
#elif defined __APPLE__ 
#define OVRA_EXPORT 
#define FUNC_NAME 
#elif defined __linux__
#define OVRA_EXPORT __attribute__((visibility("default")))
#define FUNC_NAME 
#else
#error not implemented
#endif

/// Sound flags 
typedef enum 
{
  ovrAudioSourceFlag_None                          = 0x0000,
                                                       
  ovrAudioSourceFlag_Spatialize2D_HINT_RESERVED    = 0x0001, /// Perform lightweight spatialization only (not implemented)
  ovrAudioSourceFlag_Doppler_RESERVED              = 0x0002, /// Apply Doppler effect (not implemented)
                                                       
  ovrAudioSourceFlag_WideBand_HINT                 = 0x0010, /// Wide band signal (music, voice, noise, etc.)
  ovrAudioSourceFlag_NarrowBand_HINT               = 0x0020, /// Narrow band signal (pure waveforms, e.g sine)
  ovrAudioSourceFlag_BassCompensation              = 0x0040, /// Compensate for drop in bass from HRTF
  ovrAudioSourceFlag_DirectTimeOfArrival           = 0x0080, /// Time of arrival delay for the direct signal

  ovrAudioSourceFlag_DisableResampling_RESERVED    = 0x8000, /// Disable resampling IR to output rate, INTERNAL USE ONLY

} ovrAudioSourceFlag;

typedef enum 
{
  ovrAudioSourceAttenuationMode_None          = 0,

  ovrAudioSourceAttenuationMode_Fixed         = 1,
  ovrAudioSourceAttenuationMode_InverseSquare = 2,

  ovrAudioSourceAttenuationMode_COUNT
  
} ovrAudioSourceAttenuationMode;

/// Spatializer enum used for creating a context
typedef enum
{
    ovrAudioSpatializationProvider_None            = 0,

    ovrAudioSpatializationProvider_OVR_Fast        = 1,
    ovrAudioSpatializationProvider_OVR_HQ          = 2,

    ovrAudioSpatializationProvider_COUNT
} ovrAudioSpatializationProvider;

/// Enable/disable flags
typedef enum
{
    ovrAudioEnable_None                     = 0,

    ovrAudioEnable_Doppler_RESERVED         = 1,   /// global control of Doppler, default: disabled, current unimplemented
    ovrAudioEnable_SimpleRoomModeling       = 2,   /// default: disabled
    ovrAudioEnable_LateReverberation        = 3,   /// default: disabled, but requires simple room modeling enabled

    ovrAudioEnable_RandomizeReverb          = 4,   /// default: enabled

    ovrAudioEnable_COUNT
} ovrAudioEnable;

/// Note: these are not intended for external use!
typedef enum 
{

  ovrAudioHRTFInterpolationMethod_DEPRECATED_Nearest,
  ovrAudioHRTFInterpolationMethod_DEPRECATED_SimpleTimeDomain,
  ovrAudioHRTFInterpolationMethod_DEPRECATED_MinPhaseTimeDomain,
  ovrAudioHRTFInterpolationMethod_PhaseTruncation,
  ovrAudioHRTFInterpolationMethod_PhaseLerp,

  ovrAudioHRTFInterpolationMethod_COUNT

} ovrAudioHRTFInterpolationMethod;

typedef enum
{
  ovrAudioSpatializerStatus_Finished,
  ovrAudioSpatializerStatus_Working,
} ovrAudioSpatializerStatus;

/// Headphone models used for correction/deconvolution
typedef enum
{
    ovrAudioHeadphones_CV1         = 0,   // Correct for default headphones on CV1
    ovrAudioHeadphones_Custom      = 1,   // Correct using custom IR
    
    ovrAudioHeadphones_COUNT
} ovrAudioHeadphones;

/// Performance counter enumerant used for querying profiling information
typedef enum
{
    ovrAudioPerformanceCounter_Spatialization            = 0,    
    ovrAudioPerformanceCounter_HeadphoneCorrection       = 1,

    ovrAudioPerformanceCounter_COUNT
} ovrAudioPerformanceCounter;

/// Opaque type definitions for audio source and context
typedef struct ovrAudioSource_   ovrAudioSource;
typedef struct ovrAudioContext_ *ovrAudioContext;

//  ovrAudio_Initialize
//  
//  Load the OVR audio library.  Call this first before any other ovrAudio 
//  functions!  It returns 1 on success, 0 on failure.
//
int     ovrAudio_Initialize( void );

/// ovrAudio_Shutdown
//  Shut down all of ovrAudio.  Be sure to destroy any existing contexts 
//  first!
void    ovrAudio_Shutdown( void );

/// ovrAudio_GetVersion
//
//  Return library's built version information. Can be called any time.  
//  Returns build string.
const char *ovrAudio_GetVersion( int *Major, int *Minor, int *Patch );

/// Returns last error for audio state. Returns null for no error.
/// String is valid until next call to ovrAudio_GetLastError or audio is shutdown.
OVRA_EXPORT const char *ovrAudio_GetLastError( ovrAudioContext Context );

/// ovrAudio_AllocSamples
/// ovrAudio_FreeSamples
//  Helper functions that allocate and/or free 16-byte aligned sample data
//  sufficient for passing to the spatialization API.
OVRA_EXPORT float *     ovrAudio_AllocSamples( int NumSamples );
OVRA_EXPORT void        ovrAudio_FreeSamples( float *Samples );

/// ovrAudio_GetTransformFromPose
//
//  Utility to get transform from pose 
//  Pose - pointer to pose
//  Vx   - pointer to store orientation vector (X)
//  Vy   - pointer to store orientation vector (Y)
//  Vz   - pointer to store orientation vector (Z)
//  Pos  - position
typedef struct ovrPosef_ ovrPosef;
OVRA_EXPORT int     ovrAudio_GetTransformFromPose( const ovrPosef *Pose, 
                                              float *Vx, float *Vy, float *Vz,
                                              float *Pos );

/// ovrAudio_CreateContext
// 
//  Creates a context, must be called before all other audio code.
// 
//  pContext      - pointer to context.  *pContext must be NULL!
//  pConfig       - properly filled out ovrAudioContextConfig structa
//
//  
typedef struct _ovrAudioContextConfig
{
  uint32_t   acc_Size;              // set to size of the struct
  uint32_t   acc_Provider;          // should be one of ovrAudioSpatializationProvider
  uint32_t   acc_MaxNumSources;     // maximum number of audio sources to support
  uint32_t   acc_SampleRate;        // sample rate
  uint32_t   acc_BufferLength;      // size of mono input buffers that will be passed to spatializer
} ovrAudioContextConfiguration;

OVRA_EXPORT int ovrAudio_CreateContext( ovrAudioContext *pContext,
                                        const ovrAudioContextConfiguration *pConfig );

/// ovrAudio_DestroyContext
//
//  Call after context is no longer used.
OVRA_EXPORT void ovrAudio_DestroyContext( ovrAudioContext Context );

/// ovrAudio_Enable
//
//  Enable/disable options in the audio context.
OVRA_EXPORT void ovrAudio_Enable( ovrAudioContext Context, 
                                  ovrAudioEnable What, 
                                  int Enable );

/// ovrAudio_SetHRTFInterpolationMethod
//
// Choose interpolation method.
OVRA_EXPORT void ovrAudio_SetHRTFInterpolationMethod( ovrAudioContext Context, 
                                                      ovrAudioHRTFInterpolationMethod InterpolationMethod );

/// ovrAudio_SetSimpleBoxRoomParameters
//
// These parameters are used for reverberation/early reflections if 
// ovrAudioEnable_SimpleRoomModeling is enabled.
//
// Width/Height/Depth - default is 10/10/10m
// Ref* - reflection coefficient.  0.0 = full absorptive, 1.0 = fully reflective, but
//        we cap at 0.95.  Default is 0.25.
//
typedef struct _ovrAudioBoxRoomParameters
{
   uint32_t       brp_Size;
   float          brp_ReflectLeft, brp_ReflectRight;
   float          brp_ReflectUp, brp_ReflectDown;
   float          brp_ReflectBehind, brp_ReflectFront;
   float          brp_Width, brp_Height, brp_Depth;
} ovrAudioBoxRoomParameters;

OVRA_EXPORT int ovrAudio_SetSimpleBoxRoomParameters( ovrAudioContext Context, 
                                                     const ovrAudioBoxRoomParameters *Parameters );

//    
/// ovrAudio_SetListenerPoseStatef
//
// If this is not set then the listener is always assumed to be facing into
// the screen (0,0,-1) at location (0,0,0) and that all spatialized sounds
// are in listener-relative coordinates.
typedef struct ovrPoseStatef_ ovrPoseStatef;
OVRA_EXPORT int ovrAudio_SetListenerPoseStatef( ovrAudioContext Context, 
                                                const ovrPoseStatef *PoseState );

/// ovrAudio_ResetAudioSource
//
//  Sound - index of the sound to reset (0..NumSources-1).  Use this if you change
//          change parameters or sound to remove reverb tails, etc.
OVRA_EXPORT int ovrAudio_ResetAudioSource( ovrAudioContext Context, int Sound );

/// ovrAudio_SetAudioSourcePos
//
//  Set position of audio source.  Use "OVR" coordinate system (same as pose).
//
OVRA_EXPORT int ovrAudio_SetAudioSourcePos( ovrAudioContext Context, int Sound, float X, float Y, float Z );

/// ovrAudio_SetAudioSourceRange
//
//  Set min/max range of audio source.
//
OVRA_EXPORT int ovrAudio_SetAudioSourceRange( ovrAudioContext Context, int Sound, float RangeMin, float RangeMax );

/// ovrAudio_SetAudioSourceFlags
//
//  Set flags for an audio source.
//
OVRA_EXPORT int ovrAudio_SetAudioSourceFlags( ovrAudioContext Context, int Sound, unsigned int Flags );

/// ovrAudio_SetAudioSourceAttenuationMode
//
//  Sound      - which audio source to modify
//  Mode       - None, Fixed, or Inverse Square
//  FixedScale - if the Mode is Fixed, then this Value is used to scale volume
OVRA_EXPORT int ovrAudio_SetAudioSourceAttenuationMode( ovrAudioContext Context, 
                                                        int Sound, 
                                                        ovrAudioSourceAttenuationMode Mode, 
                                                        float FixedScale );

/// ovrAudio_SpatializeMonoSourceInterleaved
//  Take a mono audio source and spatialize it.  Generates interleaved output.
OVRA_EXPORT int ovrAudio_SpatializeMonoSourceInterleaved( ovrAudioContext Context, int Sound, float *Dst, const float *Src );

/// ovrAudio_SpatializeMonoSourceLR
//  Take a mono audio source and spatialize it.  Generates stereo output (two channels).
OVRA_EXPORT int ovrAudio_SpatializeMonoSourceLR( ovrAudioContext Context, int Sound, float *DstLeft, float *DstRight, const float *Src );

/// Drain the tail of the sound, which can be longer than the sound itself due to
//  reflections and reverb.
//
//  Status will be an ovrAudioSpatializerStatus
OVRA_EXPORT int ovrAudio_FinishTailInterleaved( ovrAudioContext Context, int Sound, float *Dst, int *Status );
OVRA_EXPORT int ovrAudio_FinishTailLR( ovrAudioContext Context, int Sound, float *DstLeft, float *DstRight, int *Status );

//
// Headphone correction
//
// NOTE: These are currently unimplemented!
//
OVRA_EXPORT int ovrAudio_SetHeadphoneModel( ovrAudioContext Context, 
                                            ovrAudioHeadphones Model,
                                            const float *ImpulseResponse, int NumSamples );
OVRA_EXPORT int ovrAudio_HeadphoneCorrection( ovrAudioContext Context, 
                                              float *OutLeft, float *OutRight,
                                              const float *InLeft, const float *InRight,
                                              int NumSamples );

//
// Profiling interface
//
OVRA_EXPORT int ovrAudio_GetPerformanceCounter( ovrAudioContext Context, ovrAudioPerformanceCounter Counter, int64_t *Count, double *TimeMicroSeconds );
OVRA_EXPORT int ovrAudio_ResetPerformanceCounter( ovrAudioContext Context, ovrAudioPerformanceCounter Counter );

#ifdef __cplusplus
}
#endif

#endif // OVR_Audio_h
