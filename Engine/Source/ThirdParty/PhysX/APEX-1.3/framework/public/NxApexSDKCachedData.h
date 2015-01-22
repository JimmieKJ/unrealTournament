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

#ifndef NX_APEX_SDKCACHED_DATA_H
#define NX_APEX_SDKCACHED_DATA_H

/*!
\file
\brief classes NxApexModuleCachedData, NxApexSDKCachedData
*/

#include "NxApexSDK.h"

namespace NxParameterized
{
class Interface;
};

namespace physx
{
namespace apex
{


/**
\brief Cached data is stored per-module.
*/
class NxApexModuleCachedData
{
public:
	/**
	 * \brief Retreives the cached data for the asset, if it exists.
	 *
	 * Otherwise returns NULL.
	 */
	virtual ::NxParameterized::Interface*	getCachedDataForAssetAtScale(NxApexAsset& asset, const physx::PxVec3& scale) = 0;

	/**	
	 *	\brief Serializes the cooked data for a single asset into a stream.
	 */
	virtual physx::general_PxIOStream2::PxFileBuf& serializeSingleAsset(NxApexAsset& asset, physx::general_PxIOStream2::PxFileBuf& stream) = 0;

	/**	
	 *	\brief Deserializes the cooked data for a single asset from a stream.
	 */
	virtual physx::general_PxIOStream2::PxFileBuf& deserializeSingleAsset(NxApexAsset& asset, physx::general_PxIOStream2::PxFileBuf& stream) = 0;
};

/**
\brief A method for storing actor data in a scene
*/
class NxApexSDKCachedData
{
public:
	/**
	 * \brief Retreives the scene cached data for the actor, if it exists.
	 *
	 * Otherwise returns NULL.
	 */
	virtual NxApexModuleCachedData*	getCacheForModule(NxAuthObjTypeID moduleID) = 0;

	/**
	 * \brief Save cache configuration to a stream
	 */
	virtual physx::general_PxIOStream2::PxFileBuf&  serialize(physx::general_PxIOStream2::PxFileBuf&) const = 0;

	/**
	 * \brief Load cache configuration from a stream
	 */
	virtual physx::general_PxIOStream2::PxFileBuf&  deserialize(physx::general_PxIOStream2::PxFileBuf&) = 0;

	/**
	 * \brief Clear data
	 */
	virtual void					clear() = 0;

protected:
	NxApexSDKCachedData() {}
	virtual							~NxApexSDKCachedData() {}
};

}
} // end namespace physx::apex


#endif // NX_APEX_SDKCACHED_DATA_H
