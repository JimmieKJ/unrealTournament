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
#ifndef NX_APEX_READ_WRITE_LOCK_H

#define NX_APEX_READ_WRITE_LOCK_H

#include "foundation/PxPreprocessor.h"
#include "NxApexDefs.h"

#if NX_SDK_VERSION_MAJOR == 3

#include "PxScene.h"

namespace physx
{
	namespace apex
	{

class ScopedPhysX3LockRead
{
public:
	ScopedPhysX3LockRead(PxScene *scene,const char *fileName,int lineno) : mScene(scene)
	{
		if ( mScene )
		{
			mScene->lockRead(fileName,lineno);
		}
	}
	~ScopedPhysX3LockRead()
	{
		if ( mScene )
		{
			mScene->unlockRead();
		}
	}
private:
	PxScene* mScene;
};

class ScopedPhysX3LockWrite
{
public:
	ScopedPhysX3LockWrite(PxScene *scene,const char *fileName,int lineno) : mScene(scene)
	{
		if ( mScene ) 
		{
			mScene->lockWrite(fileName,lineno);
		}
	}
	~ScopedPhysX3LockWrite()
	{
		if ( mScene )
		{
			mScene->unlockWrite();
		}
	}
private:
	PxScene* mScene;
};

}; // end apx namespace
}; // end physx namespace


#if defined(_DEBUG) || defined(PX_CHECKED)
#define SCOPED_PHYSX3_LOCK_WRITE(x) physx::apex::ScopedPhysX3LockWrite _wlock(x,__FILE__,__LINE__);
#else
#define SCOPED_PHYSX3_LOCK_WRITE(x) physx::apex::ScopedPhysX3LockWrite _wlock(x,"",__LINE__);
#endif

#if defined(_DEBUG) || defined(PX_CHECKED)
#define SCOPED_PHYSX3_LOCK_READ(x) physx::apex::ScopedPhysX3LockRead _rlock(x,__FILE__,__LINE__);
#else
#define SCOPED_PHYSX3_LOCK_READ(x) physx::apex::ScopedPhysX3LockRead _rlock(x,"",__LINE__);
#endif

#elif NX_SDK_VERSION_MAJOR == 2
#define SCOPED_PHYSX3_LOCK_WRITE(x) ;
#define SCOPED_PHYSX3_LOCK_READ(x) ;
#endif

#endif
