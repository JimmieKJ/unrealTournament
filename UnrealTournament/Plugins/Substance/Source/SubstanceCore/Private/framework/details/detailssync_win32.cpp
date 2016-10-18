//! @file detailssync_win32.cpp
//! @brief Substance Rendering synchronization API Win32 implementation.
//! @author Christophe Soum - Allegorithmic
//! @date 20110701
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"

#include "framework/details/detailssync.h"

#ifdef AIR_USE_WIN32_SYNC

Substance::Details::Sync::thread::thread() :
	mThread(NULL),
	mParameters(NULL)
{
}


Substance::Details::Sync::thread::~thread()
{
	release();
}


void Substance::Details::Sync::thread::release()
{
	if (mThread)
	{
		join();
		delete[] mParameters;
		CloseHandle(mThread);
		mThread = NULL;
	}
}


Substance::Details::Sync::thread::thread(const thread& t)
{
	*this = t;
}


Substance::Details::Sync::thread& 
Substance::Details::Sync::thread::operator=(const thread& t)
{
	release();
	mThread = t.mThread;
	mParameters = t.mParameters;
	const_cast<thread&>(t).mThread = NULL;
	const_cast<thread&>(t).mParameters = NULL;
	return *this;
}


void Substance::Details::Sync::thread::start(
	LPTHREAD_START_ROUTINE startRoutine)
{
	mThread = CreateThread(
		NULL,
		0,
		startRoutine,
		mParameters,
		0,
		NULL);
}


void Substance::Details::Sync::thread::join()
{
	if (mThread!=NULL)
	{
		WaitForSingleObject(mThread,INFINITE);
	}
}


bool Substance::Details::Sync::thread::joinable()
{
	return mThread!=NULL;
}



#if defined(AIR_USE_WIN32_SEMAPHORE)

Substance::Details::Sync::mutex::mutex() :
	mMutex(CreateMutex(NULL,0,NULL))
{
}


Substance::Details::Sync::mutex::~mutex()
{
	CloseHandle(mMutex);
}


void Substance::Details::Sync::mutex::lock()
{
	WaitForSingleObject(mMutex,INFINITE);
}


bool Substance::Details::Sync::mutex::try_lock()
{
	return WaitForSingleObject(mMutex,0)!=WAIT_TIMEOUT;
}


void Substance::Details::Sync::mutex::unlock()
{
	ReleaseMutex(mMutex);
}


Substance::Details::Sync::condition_variable::condition_variable() :
	mCond(CreateSemaphore(NULL,0,1,NULL))
{
}


Substance::Details::Sync::condition_variable::~condition_variable()
{
	CloseHandle(mCond);
}


void Substance::Details::Sync::condition_variable::notify_one()
{
	ReleaseSemaphore(mCond,1,NULL);
}


void Substance::Details::Sync::condition_variable::wait(unique_lock& l)
{
	SignalObjectAndWait(l.mMutex.mMutex,mCond,INFINITE,FALSE);
}


#else //if defined(AIR_USE_WIN32_SEMAPHORE)

Substance::Details::Sync::mutex::mutex()
{
	InitializeCriticalSection(&mMutex);
}


Substance::Details::Sync::mutex::~mutex()
{
	DeleteCriticalSection(&mMutex);
}


void Substance::Details::Sync::mutex::lock()
{
	EnterCriticalSection(&mMutex);
}


bool Substance::Details::Sync::mutex::try_lock()
{
	return TryEnterCriticalSection(&mMutex)!=0;
}


void Substance::Details::Sync::mutex::unlock()
{
	LeaveCriticalSection(&mMutex);
}


Substance::Details::Sync::condition_variable::condition_variable()
{
	InitializeConditionVariable(&mCond);
}


Substance::Details::Sync::condition_variable::~condition_variable()
{
	// do nothing
}


void Substance::Details::Sync::condition_variable::notify_one()
{
	WakeConditionVariable(&mCond);
}


void Substance::Details::Sync::condition_variable::wait(unique_lock& l)
{
	SleepConditionVariableCS(&mCond,&l.mMutex.mMutex,INFINITE);
}

#endif //if defined(AIR_USE_WIN32_SEMAPHORE)

#endif  // ifdef AIR_USE_WIN32_SYNC
