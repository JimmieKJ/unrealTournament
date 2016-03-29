// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	LinuxPlatformThreading.h: Linux platform threading functions
==============================================================================================*/

#pragma once

#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"

/**
* Linux implementation of the Process OS functions
**/
class FRunnableThreadLinux : public FRunnableThreadPThread
{
	struct EConstants
	{
		enum
		{
			LinuxThreadNameLimit = 15,			// the limit for thread name is just 15 chars :( http://man7.org/linux/man-pages/man3/pthread_setname_np.3.html
			CrashHandlerStackSize = SIGSTKSZ + 128 * 1024	// should be at least SIGSTKSIZE, plus 128K because we do logging in crash handler
		};
	};

	/** Each thread needs a separate stack for the signal handler, so possible stack overflows in the thread are handled */
	void* ThreadCrashHandlingStack;

public:

	/** Separate stack for the signal handler (so possible stack overflows don't go unnoticed), shared between threads. */
	static char MainThreadSignalHandlerStack[EConstants::CrashHandlerStackSize];

	FRunnableThreadLinux()
		:	FRunnableThreadPThread()
		,	ThreadCrashHandlingStack(nullptr)
	{
	}

private:

	/**
	 * Allows a platform subclass to setup anything needed on the thread before running the Run function
	 */
	virtual void PreRun()
	{
		FString SizeLimitedThreadName = ThreadName;

		if (SizeLimitedThreadName.Len() > EConstants::LinuxThreadNameLimit)
		{
			// first, attempt to cut out common and meaningless substrings
			SizeLimitedThreadName = SizeLimitedThreadName.Replace(TEXT("Thread"), TEXT(""));
			SizeLimitedThreadName = SizeLimitedThreadName.Replace(TEXT("Runnable"), TEXT(""));

			// if still larger
			if (SizeLimitedThreadName.Len() > EConstants::LinuxThreadNameLimit)
			{
				FString Temp = SizeLimitedThreadName;

				// cut out the middle and replace with a substitute
				const TCHAR Dash[] = TEXT("-");
				const int32 DashLen = ARRAY_COUNT(Dash) - 1;
				int NumToLeave = (EConstants::LinuxThreadNameLimit - DashLen) / 2;

				SizeLimitedThreadName = Temp.Left(EConstants::LinuxThreadNameLimit - (NumToLeave + DashLen));
				SizeLimitedThreadName += Dash;
				SizeLimitedThreadName += Temp.Right(NumToLeave);

				check(SizeLimitedThreadName.Len() <= EConstants::LinuxThreadNameLimit);
			}
		}

		int ErrCode = pthread_setname_np(Thread, TCHAR_TO_ANSI(*SizeLimitedThreadName));
		if (ErrCode != 0)
		{
			UE_LOG(LogHAL, Warning, TEXT("pthread_setname_np(, '%s') failed with error %d (%s)."), *ThreadName, ErrCode, ANSI_TO_TCHAR(strerror(ErrCode)));
		}

		// set the alternate stack for handling crashes due to stack overflow
		ThreadCrashHandlingStack = FMemory::Malloc(EConstants::CrashHandlerStackSize);
		if (ThreadCrashHandlingStack)
		{
			stack_t SignalHandlerStack;
			FMemory::Memzero(SignalHandlerStack);
			SignalHandlerStack.ss_sp = ThreadCrashHandlingStack;
			SignalHandlerStack.ss_size = EConstants::CrashHandlerStackSize;

			if (sigaltstack(&SignalHandlerStack, nullptr) < 0)
			{
				int ErrNo = errno;
				UE_LOG(LogLinux, Warning, TEXT("Unable to set alternate stack for crash handler, sigaltstack() failed with errno=%d (%s)"),
					ErrNo,
					UTF8_TO_TCHAR(strerror(ErrNo))
					);

				// free the memory immediately
				FMemory::Free(ThreadCrashHandlingStack);
				ThreadCrashHandlingStack = nullptr;
			}
		}
	}

	virtual void PostRun()
	{
		if (ThreadCrashHandlingStack)
		{
			FMemory::Free(ThreadCrashHandlingStack);
			ThreadCrashHandlingStack = nullptr;
		}
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
};
