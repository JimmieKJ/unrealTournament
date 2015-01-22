// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformRunnableThread.h: Apple platform threading functions
==============================================================================================*/

#pragma once

#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"

/**
* Apple implementation of the Process OS functions
**/
class FRunnableThreadApple : public FRunnableThreadPThread
{
public:
    FRunnableThreadApple()
    :   Pool(NULL)
    {
    }
    
private:
	/**
	 * Allows a platform subclass to setup anything needed on the thread before running the Run function
	 */
	virtual void PreRun()
	{
		//@todo - zombie - This and the following build somehow. It makes no sense to me. -astarkey
		Pool = FPlatformMisc::CreateAutoreleasePool();
		pthread_setname_np(TCHAR_TO_ANSI(*ThreadName));
	}

	/**
	 * Allows a platform subclass to teardown anything needed on the thread after running the Run function
	 */
	virtual void PostRun()
	{
		FPlatformMisc::ReleaseAutoreleasePool(Pool);
	}
    
	/**
	 * Allows platforms to adjust stack size
	 */
	virtual uint32 AdjustStackSize(uint32 InStackSize)
	{
		InStackSize = FRunnableThreadPThread::AdjustStackSize(InStackSize);
        
		// If it's set, make sure it's at least 128 KB or stack allocations (e.g. in Logf) may fail
		if (InStackSize && InStackSize < 128 * 1024)
		{
			InStackSize = 128 * 1024;
		}
        
		return InStackSize;
	}
    
    void*      Pool;
};

