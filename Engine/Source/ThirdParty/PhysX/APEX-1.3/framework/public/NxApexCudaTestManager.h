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

#ifndef NX_APEX_CUDA_TEST_MANAGER_H
#define NX_APEX_CUDA_TEST_MANAGER_H

/*!
\file
\brief classes NxApexCudaTestManager
*/

#include <NxApexDefs.h>
#include <foundation/PxSimpleTypes.h>

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/** 
 */

 /**
\brief A class that set information on kernels should be tested and give directive for creation ApexCudaTestContext
*/
class NxApexCudaTestManager
{
public:
	 /**
	\brief Set the write path
	*/
	virtual void setWritePath(const char* path) = 0;

	 /**
	\brief Set write for function
	*/
	virtual void setWriteForFunction(const char* functionName, const char* moduleName) = 0;

	 /**
	\brief Set the maximum number of frames
	*/
	virtual void setMaxSamples(physx::PxU32 maxFrames) = 0;

	 /**
	\brief Set the frames
	*/
	virtual void setFrames(physx::PxU32 numFrames, const PxU32* frames) = 0;
	
	 /**
	\brief Set the frame period
	*/
	virtual void setFramePeriod(physx::PxU32 period) = 0;

	/**
	\brief Set maximal count of kernel calls per frame 
	*/
	virtual void setCallPerFrameMaxCount(PxU32 cpfMaxCount) = 0;

	/**
	\brief Set flag for writing kernel context for kernel that return not successful error code
	*/
	virtual void setWriteForNotSuccessfulKernel(bool flag) = 0;

	 /**
	\brief Run a kernel
	*/
	virtual bool runKernel(const char* path) = 0;
};

PX_POP_PACK

}
}

#endif // NX_APEX_CUDA_TEST_MANAGER_H

