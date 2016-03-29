/************************************************************************************

Filename    :   VrApi_Version.h
Content     :   API version

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrApi_Version_h
#define OVR_VrApi_Version_h

// At some point we will transition to product version 1 
// and reset the major version back to 1 (first product release, version 1.0).
#define VRAPI_PRODUCT_VERSION	1
#define VRAPI_MAJOR_VERSION		0
#define VRAPI_MINOR_VERSION		1
#define VRAPI_PATCH_VERSION		0

// Internal build identifier
#define VRAPI_BUILD_VERSION		0

// Internal build description
#define VRAPI_BUILD_DESCRIPTION ""

// Minimum version of the driver required for this API
#define VRAPI_DRIVER_VERSION	0

#endif	// OVR_VrApi_Version_h
