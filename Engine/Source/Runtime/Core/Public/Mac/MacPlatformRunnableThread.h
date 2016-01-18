// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	MacPlatformThreading.h: Mac platform threading functions
==============================================================================================*/

#pragma once

#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"

/**
* Mac implementation of the Process OS functions
**/
class FRunnableThreadMac : public FRunnableThreadPThread
{
public:
    FRunnableThreadMac() : FRunnableThreadPThread()
    {
    }
    
private:
	/**
	 * Allows a platform subclass to setup anything needed on the thread before running the Run function
	 */
	virtual void PreRun() override
	{
		pthread_setname_np(TCHAR_TO_ANSI(*ThreadName));
	}

	virtual int GetDefaultStackSize() override
	{
		// default is 512 KB, we need more
		return 4 * 1024 * 1024;
	}

	/**
	 * Allows platforms to adjust stack size
	 */
	virtual uint32 AdjustStackSize(uint32 InStackSize) override
	{
		if (InStackSize < GetDefaultStackSize())
		{
			InStackSize = GetDefaultStackSize();
		}
		return InStackSize;
	}
};
