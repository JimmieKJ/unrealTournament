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

#ifndef PX_PROCESS_RENDER_DEBUG_H
#define PX_PROCESS_RENDER_DEBUG_H

#include "PxRenderDebug.h"

namespace physx
{
namespace general_renderdebug4
{

#define MAX_LINE_VERTEX 2048
#define MAX_SOLID_VERTEX 2048

PX_PUSH_PACK_DEFAULT


class PxProcessRenderDebug
{
public:
	enum DisplayType
	{
		SCREEN_SPACE,
		WORLD_SPACE,
		WORLD_SPACE_NOZ,
		DT_LAST
	};

	virtual void processRenderDebug(const DebugPrimitive **dplist,
									PxU32 pcount,
									RenderDebugInterface *iface,
									DisplayType type) = 0;

	virtual void flush(RenderDebugInterface *iface,DisplayType type) = 0;

	virtual void flushFrame(RenderDebugInterface *iface) = 0;

	virtual void release(void) = 0;

	virtual void setViewMatrix(const physx::PxMat44 &view) 
	{
		PX_UNUSED(view);
	}

protected:
	virtual ~PxProcessRenderDebug(void) { };

};


PxProcessRenderDebug * createProcessRenderDebug(void);


PX_POP_PACK

}; // end of namespace
using namespace general_renderdebug4;
}; // end of namespace

#endif
