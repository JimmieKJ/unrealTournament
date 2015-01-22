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

#ifndef NX_APEX_RENDERABLE_H
#define NX_APEX_RENDERABLE_H

/*!
\file
\brief class NxApexRenderable
*/

#include "NxApexRenderDataProvider.h"

namespace physx
{
namespace apex
{

class NxUserRenderer;

PX_PUSH_PACK_DEFAULT

/**
\brief Base class of any actor that can be rendered
 */
class NxApexRenderable : public NxApexRenderDataProvider
{
public:
	/**
	When called, this method will use the NxUserRenderer interface to render itself (if visible, etc)
	by calling renderer.renderResource( NxApexRenderContext& ) as many times as necessary.   See locking
	semantics for NxApexRenderDataProvider::lockRenderResources().
	*/
	virtual void dispatchRenderResources(NxUserRenderer& renderer) = 0;

	/**
	Returns AABB covering rendered data.  The actor's world bounds is updated each frame
	during NxApexScene::fetchResults().  This function does not require the NxApexRenderable actor to be locked.
	*/
	virtual physx::PxBounds3 getBounds() const = 0;
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif
