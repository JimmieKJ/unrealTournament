// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.

#ifndef NX_APEX_DEFS_H
#define NX_APEX_DEFS_H

/*!
\file
\brief Version identifiers and other macro definitions

	This file is intended to be usable without picking up the entire
	public APEX API, so it explicitly does not include NxApex.h
*/

#include "NxPhysXSDKVersion.h"

/*!
 \def NX_APEX_SDK_VERSION
 \brief APEX Framework API version

 Used for making sure you are linking to the same version of the SDK files
 that you have included.  Should be incremented with every API change.

 \def NX_APEX_SDK_RELEASE
 \brief APEX SDK Release version

 Used for conditionally compiling user code based on the APEX SDK release version.

 \def DYNAMIC_CAST
 \brief Determines use of dynamic_cast<> by APEX modules

 \def APEX_USE_GRB
 \brief Determines use of GPU Rigid Bodies by APEX modules

 \def APEX_USE_PARTICLES
 \brief Determines use of particle-related APEX modules

 \def NX_APEX_DEFAULT_NO_INTEROP_IMPLEMENTATION
 \brief Provide API stubs with no CUDA interop support

 Use this to add default implementations of interop-related interfaces for NxUserRenderer.
*/

#include "foundation/PxPreprocessor.h"

#define NX_APEX_SDK_VERSION 1
#define NX_APEX_SDK_RELEASE 0x01030100

#if defined(PX_WINDOWS)
	// CUDA does not currently have a VS2013 supported build.
	#if _MSC_VER >= 1800
		#define APEX_CUDA_SUPPORT 0
	#else
		#define APEX_CUDA_SUPPORT 1
	#endif
#endif


#if USE_RTTI
#define DYNAMIC_CAST(type) dynamic_cast<type>
#else
#define DYNAMIC_CAST(type) static_cast<type>
#endif

#if NX_SDK_VERSION_NUMBER >= 281 && defined(PX_WINDOWS) && NX_SDK_VERSION_MAJOR >= 2
#define APEX_USE_GRB 1
#else
#define APEX_USE_GRB 0
#endif

#if defined(PX_WINDOWS)
#define APEX_USE_PARTICLES 1
#else
#define APEX_USE_PARTICLES 0
#endif

#define NX_APEX_DEFAULT_NO_INTEROP_IMPLEMENTATION 1

#endif // NX_APEX_DEFS_H
