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

#ifndef NX_RESOURCE_CALLBACK_H
#define NX_RESOURCE_CALLBACK_H

/*!
\file
\brief class NxResourceCallback
*/

#include "NxApexUsingNamespace.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/**
\brief User defined callback for resource management

The user may implement a subclass of this abstract class and provide an instance to the
ApexSDK descriptor.  These callbacks can only be triggered directly by ApexSDK API calls,
so they do not need to be re-entrant or thread safe.
*/
class NxResourceCallback
{
public:
	virtual ~NxResourceCallback() {}

	/**
	\brief Request a resource from the user

	Will be called by the ApexSDK if a named resource is required but has not yet been provided.
	The resource pointer is returned directly, NxResourceProvider::setResource() should not be called.
	This function will be called at most once per named resource, unless an intermediate call to
	releaseResource() has been made.
	
	\note If this call results in the application calling NxApexSDK::createAsset, the name given 
		  to the asset must match the input name parameter in this method.
	*/
	virtual void* requestResource(const char* nameSpace, const char* name) = 0;

	/**
	\brief Request the user to release a resource

	Will be called by the ApexSDK when all internal references to a named resource have been released.
	If this named resource is required again in the future, a new call to requestResource() will be made.
	*/
	virtual void  releaseResource(const char* nameSpace, const char* name, void* resource) = 0;
};

PX_POP_PACK

}
} // namespace physx::apex

#endif // NX_RESOURCE_CALLBACK_H
