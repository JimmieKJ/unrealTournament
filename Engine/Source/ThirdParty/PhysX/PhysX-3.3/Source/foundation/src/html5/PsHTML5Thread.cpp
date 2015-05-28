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
#include "PsFoundation.h"
#include "PsAtomic.h"
#include "PsThread.h"
#include "foundation/PxAssert.h"
#include <map>


namespace physx
{
namespace shdfnd
{

namespace 
{

	typedef enum
	{
		_PxThreadNotStarted,
		_PxThreadStarted,
		_PxThreadStopped
	} PxThreadState;

	class _ThreadImpl
	{};

	_ThreadImpl* getThread(ThreadImpl* impl)
	{
		return reinterpret_cast<_ThreadImpl*>(impl);
	}
	
	static void setTid(_ThreadImpl& threadImpl)
	{
	}
	
	void *PxThreadStart(void *arg)
	{
		return NULL;
	}
}

static const PxU32 gSize = sizeof(_ThreadImpl);
const PxU32& ThreadImpl::getSize()  { return gSize; }


ThreadImpl::Id ThreadImpl::getId()
{
	return 0;
}

ThreadImpl::ThreadImpl()
{
	shdfnd::getFoundation().error(PxErrorCode::eABORT, __FILE__, __LINE__, " PhysX HTML5: Thread implementation does not exist.  ");
}

ThreadImpl::ThreadImpl(ThreadImpl::ExecuteFn fn, void *arg)
{
	shdfnd::getFoundation().error(PxErrorCode::eABORT, __FILE__, __LINE__, " PhysX HTML5: Thread implementation does not exist. ");
}

ThreadImpl::~ThreadImpl()
{
}


void ThreadImpl::start(PxU32 stackSize, Runnable* runnable)
{
	shdfnd::getFoundation().error(PxErrorCode::eABORT, __FILE__, __LINE__, " PhysX HTML5: Thread implementation does not exist. ");
}


void ThreadImpl::signalQuit()
{
}

bool ThreadImpl::waitForQuit()
{
	return true; 
}


bool ThreadImpl::quitIsSignalled()
{
	return true; 
}

void ThreadImpl::quit()
{
}

void ThreadImpl::kill()
{
}

void ThreadImpl::sleep(PxU32 ms)
{
}

void ThreadImpl::yield()
{
}

PxU32 ThreadImpl::setAffinityMask(PxU32 mask)
{
	return PxU32(0);
}

void ThreadImpl::setName(const char *name)
{
}

static ThreadPriority::Enum convertPriorityFromLinux(PxU32 inPrio, int policy)
{
	return ThreadPriority::eNORMAL;
}

static int convertPriorityToLinux(ThreadPriority::Enum inPrio, int policy)
{
	return 19;
}

void ThreadImpl::setPriority(ThreadPriority::Enum val)
{
}

ThreadPriority::Enum ThreadImpl::getPriority(Id pthread)
{
	return ThreadPriority::eNORMAL;
}

PxU32 ThreadImpl::getNbPhysicalCores()
{
	return 1; 
}

std::map< PxU32, void* > TLSsdata; 
PxU32 Key; 

PxU32 TlsAlloc()
{ 
	TLSsdata[Key++] = 0;
	return Key; 
}

void TlsFree(PxU32 index)
{
	TLSsdata.erase( index );
}

void* TlsGet(PxU32 index)
{
	return (void*)TLSsdata[index];
}

PxU32 TlsSet(PxU32 index, void *value)
{
	TLSsdata[ index ] = value;
	return 1;
}

// DM: On Linux x86-32, without implementation-specific restrictions
// the default stack size for a new thread should be 2 megabytes (kernel.org).
// NOTE: take care of this value on other architecutres!
PxU32 ThreadImpl::getDefaultStackSize() 
{
	return 1 << 21;
}

} // namespace shdfnd
} // namespace physx

