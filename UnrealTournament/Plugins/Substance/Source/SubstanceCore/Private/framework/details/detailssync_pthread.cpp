//! @file detailssync_pthread.cpp
//! @brief Substance Rendering synchonization API PThread implementation.
//! @author Christophe Soum - Allegorithmic
//! @date 20121029
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"

#include "detailssync.h"

#ifdef AIR_USE_PTHREAD_SYNC

#include <errno.h>


Substance::Details::Sync::thread::thread() :
	mParameters(NULL)
{
}


Substance::Details::Sync::thread::~thread()
{
	release();
}


void Substance::Details::Sync::thread::release()
{
	if (mParameters)
	{
		join();
		delete[] mParameters;
		mParameters = NULL;
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
	const_cast<thread&>(t).mThread = 0;
	const_cast<thread&>(t).mParameters = NULL;
	return *this;
}


void Substance::Details::Sync::thread::start(
	SyncThreadStart startRoutine)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_attr_setstacksize(&attr,  30 * PTHREAD_STACK_MIN);

	pthread_create(&mThread,&attr,startRoutine,mParameters);
}


void Substance::Details::Sync::thread::join()
{
	if (mParameters!=NULL)
	{
		pthread_join(mThread,NULL);
	}
}


bool Substance::Details::Sync::thread::joinable()
{
	return mParameters!=NULL;
}


Substance::Details::Sync::mutex::mutex()
{
	pthread_mutex_init(&mMutex,NULL);
}


Substance::Details::Sync::mutex::~mutex()
{
	pthread_mutex_destroy(&mMutex);
}


void Substance::Details::Sync::mutex::lock()
{
	pthread_mutex_lock(&mMutex);
}


bool Substance::Details::Sync::mutex::try_lock()
{
	return pthread_mutex_trylock(&mMutex)!=EBUSY;
}


void Substance::Details::Sync::mutex::unlock()
{
	pthread_mutex_unlock(&mMutex);
}


Substance::Details::Sync::condition_variable::condition_variable()
{
	pthread_cond_init(&mCond,NULL);
}


Substance::Details::Sync::condition_variable::~condition_variable()
{
	pthread_cond_destroy(&mCond);
}


void Substance::Details::Sync::condition_variable::notify_one()
{
	pthread_cond_signal(&mCond);
}


void Substance::Details::Sync::condition_variable::wait(unique_lock& l)
{
	pthread_cond_wait(&mCond,&l.mMutex.mMutex);
}

#endif  // ifdef AIR_USE_PTHREAD_SYNC
