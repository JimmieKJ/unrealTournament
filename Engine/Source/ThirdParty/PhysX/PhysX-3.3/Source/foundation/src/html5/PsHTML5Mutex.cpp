/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  


#include "Ps.h"
#include "PsUserAllocated.h"
#include "PsMutex.h"
#include "foundation/PxAssert.h"
#include "PsAtomic.h"
#include "PsThread.h"


// HTML5 currently only supports single threaded environment 
// Stubbed out Mutex implementation. 

namespace physx
{
namespace shdfnd
{

namespace 
{
	struct MutexUnixImpl
	{
	};

	MutexUnixImpl* getMutex(MutexImpl* impl)
	{
		return reinterpret_cast<MutexUnixImpl*>(impl);
	}
}

MutexImpl::MutexImpl() 
{ 
}

MutexImpl::~MutexImpl() 
{ 
}

void MutexImpl::lock()
{
}

bool MutexImpl::trylock()
{
	return true;
}

void MutexImpl::unlock()
{
}

const PxU32& MutexImpl::getSize()
{
	const static PxU32 gSize = sizeof(MutexUnixImpl);
	return gSize;
}

ReadWriteLock::ReadWriteLock()
{
}

ReadWriteLock::~ReadWriteLock()
{
}

void ReadWriteLock::lockReader()
{
}

void ReadWriteLock::lockWriter()
{
}

void ReadWriteLock::unlockReader()
{
}

void ReadWriteLock::unlockWriter()
{
}


} // namespace shdfnd
} // namespace physx

