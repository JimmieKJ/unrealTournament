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

#ifndef PX_PANEL2D_H
#define PX_PANEL2D_H

#include "PxRenderDebug.h"

// This is a helper class to do 2d debug rendering in screenspace.
namespace physx
{
	namespace general_renderdebug4
	{


class PxPanel2D
{
public:
	PxPanel2D(RenderDebug *debugRender,PxU32 pwid,PxU32 phit)
	{
		mRenderDebug = debugRender;
		mWidth		= pwid;
		mHeight		= phit;
		mRecipX		= 1.0f / (pwid/2);
		mRecipY		= 1.0f / (phit/2);
		mRenderDebug->pushRenderState();
		mRenderDebug->addToCurrentState(DebugRenderState::ScreenSpace);
	}

	~PxPanel2D(void)
	{
		mRenderDebug->popRenderState();
	}


	void setFontSize(PxU32 fontSize)
	{
		const PxF32 fontPixel = 0.009f;
		mFontSize = (PxF32)(fontSize*fontPixel);
		mFontOffset = mFontSize*0.3f;
		mRenderDebug->setCurrentTextScale(mFontSize);
	}

	void printText(PxU32 x,PxU32 y,const char *text)
	{
		PxVec3 p;
		getPos(x,y,p);
		p.y-=mFontOffset;
		mRenderDebug->debugText(p,"%s",text);
	}

	void getPos(PxU32 x,PxU32 y,PxVec3 &p)
	{
		p.z = 0;
		p.x = (PxF32)x*mRecipX-1;
		p.y = 1-(PxF32)y*mRecipY;
	}

	void drawLine(PxU32 x1,PxU32 y1,PxU32 x2,PxU32 y2)
	{
		PxVec3 p1,p2;
		getPos(x1,y1,p1);
		getPos(x2,y2,p2);
		mRenderDebug->debugLine(p1,p2);
	}
private:
	RenderDebug	*mRenderDebug;

	PxF32		mFontSize;
	PxF32		mFontOffset;
	PxF32		mRecipX;
	PxF32		mRecipY;
	PxU32		mWidth;
	PxU32		mHeight;
};

	}; // end namespace general_renderdebug4
}; // end namespace physx

#endif
