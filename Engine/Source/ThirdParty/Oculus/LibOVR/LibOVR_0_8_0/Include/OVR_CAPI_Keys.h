/********************************************************************************//**
\file      OVR_CAPI.h
\brief     Keys for CAPI calls
\copyright Copyright 2014 Oculus VR, LLC All Rights reserved.
************************************************************************************/

#ifndef OVR_CAPI_Keys_h
#define OVR_CAPI_Keys_h

#include "OVR_Version.h"


#define OVR_KEY_USER                        "User"                // string
#define OVR_KEY_NAME                        "Name"                // string
#define OVR_KEY_GENDER                      "Gender"              // string "Male", "Female", or "Unknown"
#define OVR_KEY_PLAYER_HEIGHT               "PlayerHeight"        // float meters
#define OVR_KEY_EYE_HEIGHT                  "EyeHeight"           // float meters
#define OVR_KEY_IPD                         "IPD"                 // float meters
#define OVR_KEY_NECK_TO_EYE_DISTANCE        "NeckEyeDistance"     // float[2] meters
#define OVR_KEY_EYE_RELIEF_DIAL             "EyeReliefDial"       // int in range of 0-10
#define OVR_KEY_EYE_TO_NOSE_DISTANCE        "EyeToNoseDist"       // float[2] meters
#define OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE   "MaxEyeToPlateDist"   // float[2] meters
#define OVR_KEY_EYE_CUP                     "EyeCup"              // char[16] "A", "B", or "C"
#define OVR_KEY_CUSTOM_EYE_RENDER           "CustomEyeRender"     // bool
//Legacy profile value tied to the device and serial
#define OVR_KEY_CAMERA_POSITION_1           "CenteredFromWorld"   // double[7] ovrPosef quat rotation x, y, z, w, translation x, y, z
//New value that now only ties to the device so that swapping headsets retains the offset from the tracker
#define OVR_KEY_CAMERA_POSITION_2			"CenteredFromWorld2"   // double[7] ovrPosef quat rotation x, y, z, w, translation x, y, z
#define OVR_KEY_CAMERA_POSITION OVR_KEY_CAMERA_POSITION_2 


// Default measurements empirically determined at Oculus to make us happy
// The neck model numbers were derived as an average of the male and female averages from ANSUR-88
// NECK_TO_EYE_HORIZONTAL = H22 - H43 = INFRAORBITALE_BACK_OF_HEAD - TRAGION_BACK_OF_HEAD
// NECK_TO_EYE_VERTICAL = H21 - H15 = GONION_TOP_OF_HEAD - ECTOORBITALE_TOP_OF_HEAD
// These were determined to be the best in a small user study, clearly beating out the previous default values
#define OVR_DEFAULT_GENDER                  "Unknown"
#define OVR_DEFAULT_PLAYER_HEIGHT           1.778f
#define OVR_DEFAULT_EYE_HEIGHT              1.675f
#define OVR_DEFAULT_IPD                     0.064f
#define OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL  0.0805f
#define OVR_DEFAULT_NECK_TO_EYE_VERTICAL    0.075f
#define OVR_DEFAULT_EYE_RELIEF_DIAL         3
#define OVR_DEFAULT_CAMERA_POSITION			{0,0,0,1,0,0,0}

#define OVR_PERF_HUD_MODE                   "PerfHudMode"   // allowed values are defined in enum ovrPerfHudMode

#define OVR_LAYER_HUD_MODE "LayerHudMode" // allowed values are defined in enum ovrLayerHudMode
#define OVR_LAYER_HUD_CURRENT_LAYER "LayerHudCurrentLayer" // The layer to show 
#define OVR_LAYER_HUD_SHOW_ALL_LAYERS "LayerHudShowAll" // Hide other layers when the hud is enabled

#define OVR_DEBUG_HUD_STEREO_MODE               "DebugHudStereoMode"                // allowed values are defined in enum ovrDebugHudStereoMode
#define OVR_DEBUG_HUD_STEREO_GUIDE_INFO_ENABLE  "DebugHudStereoGuideInfoEnable"     // bool
#define OVR_DEBUG_HUD_STEREO_GUIDE_SIZE         "DebugHudStereoGuideSize2f"         // float[2]
#define OVR_DEBUG_HUD_STEREO_GUIDE_POSITION     "DebugHudStereoGuidePosition3f"     // float[3]
#define OVR_DEBUG_HUD_STEREO_GUIDE_YAWPITCHROLL "DebugHudStereoGuideYawPitchRoll3f" // float[3]
#define OVR_DEBUG_HUD_STEREO_GUIDE_COLOR        "DebugHudStereoGuideColor4f"        // float[4]



#endif // OVR_CAPI_Keys_h
