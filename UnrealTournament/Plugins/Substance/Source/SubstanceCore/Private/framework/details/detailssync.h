//! @file detailssync.h
//! @brief Substance Rendering synchronization API definitions.
//! @author Christophe Soum - Allegorithmic
//! @date 20110701
//! @copyright Allegorithmic. All rights reserved.
//!
//! Use C++11 sync if available (on MSVC compilers, set AIR_USE_CPP11_SYNC
//! on GCC when available), otherwise use Win32 thread or Pthread,
//! use boost::thread as fallback.
//!
//! By default Win32 implementation uses Condition Variable that are not
//! supported on WindowsXP. To force support of this OS, use 
//!	AIR_USE_WIN32_SEMAPHORE define (not recommended, may introduce 
//! synchronization issues).

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSSYNC_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSSYNC_H

#include "SubstanceCoreTypedefs.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#if PLATFORM_XBOXONE
#include "XboxOneAllowPlatformTypes.h"
#endif

// If not already defined, select thread impl.
#if !defined(AIR_USE_WIN32_SYNC) && \
		!defined(AIR_USE_PTHREAD_SYNC) && \
		!defined(AIR_USE_BOOST_SYNC) && \
		!defined(AIR_USE_CPP11_SYNC)

	#if defined(__has_include)
		// Detect C++11 sync if __has_include is available (Clang)
		#if __has_include(<thread>) && \
				__has_include(<mutex>) && \
				__has_include(<condition_variable>)
			#define AIR_HAS_STD_SYNC_HEADERS 1
		#endif
	#endif
	#if defined(_MSC_VER)
		#if _MSC_VER>=1700               // At least MSVC 2012
			#define AIR_USE_CPP11_SYNC 1
		#else
			#define AIR_USE_WIN32_SYNC 1
		#endif
	#elif defined(AIR_HAS_STD_SYNC_HEADERS)
		#define AIR_USE_CPP11_SYNC 1
	#elif defined(__GNUC__)
		#define AIR_USE_PTHREAD_SYNC 1
	#else
		#define AIR_USE_BOOST_SYNC 1
	#endif
#endif


#if defined(AIR_USE_WIN32_SYNC)

	// Win32 threads implementation
	#ifdef _XBOX
		#include <xtl.h>
		#define AIR_SYNC_THREADQUALIFIER DWORD __stdcall
		#define AIR_USE_WIN32_SEMAPHORE      // Use semaphore instead CV: safe 
	#else // ifdef _XBOX
		#ifdef _DURANGO
			#include <xdk.h>
			#define AIR_SYNC_THREADQUALIFIER DWORD __stdcall
		#else // ifdef _DURANGO
			#include <windows.h>
			#define AIR_SYNC_THREADQUALIFIER unsigned long WINAPI
		#endif // ifdef _DURANGO
	#endif // ifdef _XBOX

	//! @brief Thread start routine signature 
	typedef LPTHREAD_START_ROUTINE SyncThreadStart;
	typedef HANDLE SyncThread;      //!< Thread type

	#if defined(AIR_USE_WIN32_SEMAPHORE)
		typedef HANDLE SyncMutex;       //!< Mutex type
		typedef HANDLE SyncCond;        //!< Conditional var. type
	#else //if defined(AIR_USE_WIN32_SEMAPHORE)
		typedef CRITICAL_SECTION SyncMutex;    //!< Mutex type
		typedef CONDITION_VARIABLE SyncCond;    //!< Conditional var. type
	#endif //if defined(AIR_USE_WIN32_SEMAPHORE)

#elif defined(AIR_USE_PTHREAD_SYNC)

	// PThread implementation
	#include <pthread.h>
	
	#define AIR_SYNC_THREADQUALIFIER void*

	typedef pthread_mutex_t SyncMutex; //!< Mutex type
	typedef pthread_t SyncThread;      //!< Thread type
	typedef pthread_cond_t SyncCond;   //!< Conditional var. type

	//! @brief Thread start routine signature 
	typedef void *(*SyncThreadStart)(void *);
	
#endif 


#ifndef AIR_USE_BOOST_SYNC

// PTHREAD & WIN32 specific implementation

#include <utility>


namespace Substance
{
namespace Details
{
namespace Sync
{


template <class F,class A1>
static AIR_SYNC_THREADQUALIFIER threadStartArg1(void* ptr)
{
	std::pair<F,A1> p = *(std::pair<F,A1>*)ptr;
	p.first(p.second);
	return 0;
}

template <class F>
static AIR_SYNC_THREADQUALIFIER threadStartVoid(void* ptr)
{
	F f = *(F*)ptr;
	f();
	return 0;	
}


//! @brief See boost::thread for documentation
class thread
{
public:
	thread();
	~thread();
	thread(const thread&);

	template <class F,class A1>
	thread(F f,A1 a1)
	{
		std::pair<F,A1> p(f,a1);
		mParameters = new char[sizeof(p)];
		memcpy(mParameters,&p,sizeof(p)); 
		start(threadStartArg1<F,A1>);
	}
	
	template <class F>
	explicit thread(F f)
	{
		mParameters = new char[sizeof(f)];
		memcpy(mParameters,&f,sizeof(f));
		start(threadStartVoid<F>);
	}
	
	thread& operator=(const thread&);

	void join();

	bool joinable();

protected:
	SyncThread mThread;
	char* mParameters;
	
	void start(SyncThreadStart startRoutine);
	void release();
};


//! @brief See boost::thread for documentation
class mutex
{
public:
	mutex();
	~mutex();

	void lock();
	bool try_lock();
	void unlock();
protected:
	SyncMutex mMutex;
	friend class condition_variable;
private:
	mutex(const mutex&);
	const mutex& operator=(const mutex&);
};


//! @brief See std|boost::unique_lock for documentation
class unique_lock
{
public:
	unique_lock(mutex& m) : mMutex(m) { mMutex.lock(); }
	~unique_lock() { mMutex.unlock(); }
protected:
	mutex &mMutex;
	friend class condition_variable;
private:
	unique_lock(const unique_lock&);
	const unique_lock& operator=(const unique_lock&);
};


//! @brief See boost::thread for documentation
class condition_variable
{
public:
	condition_variable();
	~condition_variable();

	void notify_one();

		void wait(unique_lock&);
	
protected:
	SyncCond mCond;
};


} // namespace Sync
} // namespace Details
} // namespace Substance


#else // ifndef ALG_USE_BOOST_SYNC


#include <boost/thread.hpp>

namespace Substance
{
namespace Details
{
namespace Sync
{

	// Use boost::thread
	typedef boost::mutex mutex;
	typedef boost::thread thread;
	typedef boost::condition_variable condition_variable;

} // namespace Sync
} // namespace Details
} // namespace Substance


#endif // ifndef ALG_USE_BOOST_SYNC


// Interlocked (atomics) functions defines

#if defined(ANDROID)
	#include <sched.h>
#elif defined(_XBOX)
	#include <Xtl.h>
#elif defined(__INTEL_COMPILER) || defined(_MSC_VER)
	extern "C" long _InterlockedExchange(long volatile *, long);
	extern "C" long _InterlockedExchangeAdd(long volatile *, long);
	#pragma intrinsic(_InterlockedExchange,_InterlockedExchangeAdd)
	#if defined(_M_IA64) || defined (_M_X64)
		extern "C" __int64 _InterlockedExchange64(__int64 volatile *,__int64);
		extern "C" __int64 _InterlockedExchangeAdd64(__int64 volatile *,__int64);
		#pragma intrinsic(_InterlockedExchange64,_InterlockedExchangeAdd64)
	#endif
#elif defined(SN_TARGET_PS3)
	#include <cell/atomic.h>
#elif defined(SN_TARGET_PSP2)
	#include <sce_atomic.h>
#elif defined(__APPLE__)
	#include <libkern/OSAtomic.h>
#endif


namespace Substance
{
namespace Details
{
namespace Sync
{
	//! @brief Interlocked pointer exchange
	//! @param dest Pointer to the destination value. 
	//! @param val The new value
	//! @return Return the original value
	inline void* interlockedSwapPointer(void*volatile* dest, void* val)
	{
		#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
			#if defined(_M_IA64) || defined (_M_X64)
				return (void*)_InterlockedExchange64(
					(volatile long long *)dest,(__int64)val);
			#else
				return (void*)_InterlockedExchange(
					(volatile long *)dest,(long)val);
			#endif
		#elif defined(__GNUC__)
			#if defined(__APPLE__)
				#if defined(__LP64__)
					unsigned long long oldValue = 0;
					do 
					{ 
						oldValue = (unsigned long long)*dest;
					}
					while (OSAtomicCompareAndSwap64(
						(unsigned long long)oldValue,
						(unsigned long long)val,
						(long long*)dest) == false);
					return (void*)oldValue;
				#else
					unsigned int oldValue = 0;
					do 
					{ 
						oldValue = (unsigned int)*dest; 
					}
					while (OSAtomicCompareAndSwap32(
						(unsigned int)oldValue,
						(unsigned int)val,
						(int*)dest) == false);
					return (void*)oldValue;
				#endif
			#else
				#ifdef __LP64__
					return (void*)__sync_lock_test_and_set(
						(volatile unsigned long long*)dest,
						(unsigned long long)val);
				#else
					return (void*)__sync_lock_test_and_set(
						(volatile unsigned int*)dest,
						(unsigned int)val);	
				#endif
			#endif
		#elif defined(SN_TARGET_PSP2)
			return (void*)sceAtomicExchange32((volatile int*)dest,(int)val);
		#endif
	}
	
	
} // namespace Sync
} // namespace Details
} // namespace Substance

#if PLATFORM_WINDOWS
#   include "HideWindowsPlatformTypes.h"
#endif


#if PLATFORM_XBOXONE
#   include "XboxOneHidePlatformTypes.h"
#endif

#endif // _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSSYNC_H
