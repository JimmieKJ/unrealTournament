/************************************************************************************

Filename    :   VrApi_Config.h
Content     :   VrApi preprocessor settings
Created     :   April 23, 2015
Authors     :   James Dolan

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrApi_Config_h
#define OVR_VrApi_Config_h

#if defined( _MSC_VER ) || defined( __ICL )

#if defined( OVR_VRAPI_ENABLE_EXPORT )
    #define OVR_VRAPI_EXPORT  __declspec(dllexport)
#else
    #define OVR_VRAPI_EXPORT
#endif

#define OVR_VRAPI_DEPRECATED __declspec(deprecated)

#else

#if defined( OVR_VRAPI_ENABLE_EXPORT )
    #define OVR_VRAPI_EXPORT __attribute__((__visibility__("default")))
#else
    #define OVR_VRAPI_EXPORT 
#endif

#define OVR_VRAPI_DEPRECATED __attribute__ ((deprecated))

#endif

#endif	// !OVR_VrApi_Config_h
