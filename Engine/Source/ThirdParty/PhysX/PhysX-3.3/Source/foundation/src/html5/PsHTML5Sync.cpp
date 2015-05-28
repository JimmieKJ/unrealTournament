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
#include "PsSync.h"

#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

namespace physx
{
namespace shdfnd
{
	
	namespace 
	{
		class _SyncImpl
		{};
	
		_SyncImpl* getSync(SyncImpl* impl)
		{
			return reinterpret_cast<_SyncImpl*>(impl);
		}
	}

	static const PxU32 gSize = sizeof(_SyncImpl);
	const PxU32& SyncImpl::getSize()  { return gSize; }


	SyncImpl::SyncImpl()
	{
	}

	SyncImpl::~SyncImpl()
	{
	}

	void SyncImpl::reset()
	{
	}

	void SyncImpl::set()
	{
	}

	bool SyncImpl::wait(PxU32 ms)
	{
		return true;
	}

} // namespace shdfnd
} // namespace physx

