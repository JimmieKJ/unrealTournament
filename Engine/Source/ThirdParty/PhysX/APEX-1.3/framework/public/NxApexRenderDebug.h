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

#ifndef NX_APEX_RENDER_DEBUG_H
#define NX_APEX_RENDER_DEBUG_H

/*!
\file
\brief debug rendering classes and structures
*/

#include "NxApexDefs.h"
#include "PxRenderDebug.h" // include the header file containing the base class
#include "NxApexInterface.h"
#include "NxApexRenderable.h"

#if NX_SDK_VERSION_MAJOR == 2
class NxDebugRenderable;
#endif // NX_SDK_VERSION_MAJOR == 2
#if NX_SDK_VERSION_MAJOR == 3
#include "common/PxRenderBuffer.h"
#endif


namespace physx
{

namespace apex
{
class NxUserRenderer;
class NxUserRenderResourceManager;

PX_PUSH_PACK_DEFAULT

#if NX_SDK_VERSION_MAJOR == 3

// This is a helper class implementation of PxRenderBuffer that holds the counts
// and pointers for renderable data.  Does not own the memory, simply used to transfer
// the state.  The append method is not supported.
class PhysXRenderBuffer : public PxRenderBuffer
{
public:
	virtual PxU32 getNbPoints() const 
	{
		return mNbPoints;
	}

	virtual const PxDebugPoint* getPoints() const
	{
		return mPoints;
	}

	virtual PxU32 getNbLines() const
	{
		return mNbLines;
	}

	virtual const PxDebugLine* getLines() const 
	{
		return mLines;
	}

	virtual PxU32 getNbTriangles() const 
	{
		return mNbTriangles;
	}

	virtual const PxDebugTriangle* getTriangles() const 
	{
		return mTriangles;
	}

	virtual PxU32 getNbTexts() const 
	{
		return mNbTexts;
	}

	virtual const PxDebugText* getTexts() const 
	{
		return mTexts;
	}

	virtual void append(const PxRenderBuffer& other)
	{
		PX_UNUSED(other);
		PX_ALWAYS_ASSERT(); // this method not implemented!
	}

	virtual void clear() 
	{
		mNbPoints = 0;
		mPoints = NULL;
		mNbLines = 0;
		mLines = NULL;
		mNbTriangles = 0;
		mTriangles = NULL;
		mNbTexts = 0;
		mTexts = NULL;
	}

	PxU32			mNbPoints;
	PxDebugPoint	*mPoints;
	PxU32			mNbLines;
	PxDebugLine		*mLines;
	PxU32			mNbTriangles;
	PxDebugTriangle	*mTriangles;
	PxU32			mNbTexts;
	PxDebugText		*mTexts;
};
#endif

/**
\brief wrapper for NxDebugRenderable
 */
class NxApexRenderDebug : public NxApexInterface, public NxApexRenderable, public physx::RenderDebug
{
public:
	/**
	\brief Method to support rendering to a legacy PhysX SDK NxDebugRenderable object instead
	of to the APEX Render Resources API (i.e.: NxApexRenderable).

	This method is used to enable or disable the use of a legacy NxDebugRenderable.  When enabled,
	use the getDebugRenderable() method to get a legacy NxDebugRenerable object that will contain
	all the debug output.
	*/
	virtual void	setUseDebugRenderable(bool state) = 0;


#if NX_SDK_VERSION_MAJOR == 2
	/**
	\brief Method to support rendering to a legacy PhysX SDK NxDebugRenderable object instead
	of to the APEX Render Resources API (i.e.: NxApexRenderable).

	When enabled with a call to setUseDebugRenderable(true), this method will return a legacy
	NxDebugRenderable object that contains all of the output of the NxApexRenderDebug class.
	*/
	virtual void	getDebugRenderable(NxDebugRenderable& renderable) = 0;

	/**
	\brief Method to support rendering from an existing legacy PhysX SDK NxDebugRenderable object.

	The contents of the legacy NxDebugRenderable is added to the current contents of the
	NxApexRenderDebug object, and is output through the APEX Render Resources API.
	*/
	virtual void	addDebugRenderable(const NxDebugRenderable& renderable) = 0;
#elif NX_SDK_VERSION_MAJOR == 3

	/**
	\brief Method to support rendering to a legacy PhysX SDK PxRenderBuffer object instead
	of to the APEX Render Resources API (i.e.: NxApexRenderable).

	When enabled with a call to setUseDebugRenderable(true), this method will return a legacy
	PxRenderBuffer object that contains all of the output of the NxApexRenderDebug class.
	*/
	virtual void	getRenderBuffer(PhysXRenderBuffer& renderable) = 0;

	/**
	\brief Method to support rendering from an existing PhysX SDK PxRenderBuffer object.

	The contents of the PxRenderBuffer is added to the current contents of the
	NxApexRenderDebug object, and is output through the APEX Render Resources API.
	*/
	virtual void	addDebugRenderable(const physx::PxRenderBuffer& renderBuffer) = 0;

#endif //NX_SDK_VERSION_MAJOR == 2

protected:

	virtual ~NxApexRenderDebug(void) { };

};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_APEX_RENDER_DEBUG_H
