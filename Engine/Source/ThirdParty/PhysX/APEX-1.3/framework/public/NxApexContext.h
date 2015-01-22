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

#ifndef NX_APEX_CONTEXT_H
#define NX_APEX_CONTEXT_H

/*!
\file
\brief class NxApexContext
*/

#include "NxApexInterface.h"

namespace physx
{
namespace apex
{

class NxApexRenderable;
class NxApexRenderableIterator;

PX_PUSH_PACK_DEFAULT

/**
\brief A container for NxApexActors
*/
class NxApexContext
{
public:
	/**
	\brief Removes all actors from the context and releases them
	*/
	virtual void		              removeAllActors() = 0;

	/**
	\brief Create an iterator for all renderables in this context
	*/
	virtual NxApexRenderableIterator* createRenderableIterator() = 0;

	/**
	\brief Release a renderable iterator

	Equivalent to calling the iterator's release method.
	*/
	virtual void					  releaseRenderableIterator(NxApexRenderableIterator&) = 0;

protected:
	virtual ~NxApexContext() {}
};

/**
\brief Iterate over all renderable NxApexActors in an NxApexContext

An NxApexRenderableIterator is a lock-safe iterator over all renderable
NxApexActors in an NxApexContext.  Actors which are locked are skipped in the initial
pass and deferred till the end.  The returned NxApexRenderable is locked by the
iterator and remains locked until you call getNext().

The NxApexRenderableIterator is also deletion safe.  If an actor is deleted
from the NxApexContext in another thread, the iterator will skip that actor.

An NxApexRenderableIterator should not be held for longer than a single simulation
step.  It should be allocated on demand and released after use.
*/
class NxApexRenderableIterator : public NxApexInterface
{
public:
	/**
	\brief Return the first renderable in an NxApexContext
	*/
	virtual NxApexRenderable* getFirst() = 0;
	/**
	\brief Return the next unlocked renderable in an NxApexContext
	*/
	virtual NxApexRenderable* getNext() = 0;
	/**
	\brief Refresh the renderable actor list for this context

	This function is only necessary if you believe actors have been added or
	deleted since the iterator was created.
	*/
	virtual void			  reset() = 0;
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_APEX_CONTEXT_H
