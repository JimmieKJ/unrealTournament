/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_Thread.h
Content     :   Misc Threading functionality....
Created     :   January, 2015
Notes       :   Will be replaced by OVR Kernel versions soon! But first we really
                need to add support for joinable threads in OVR Kernel.
                OVR::Capture::Thread replace with OVR::Thread
                OVR::Capture::ThreadGate replace with OVR::Event
                OVR::Capture::CriticalSection replace with OVR::Mutex
                OVR::Capture::Atomic* replace with OVR_Atomic.h
                Is there a replacement for SpinLock?

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_THREAD_H
#define OVR_CAPTURE_THREAD_H

#include <OVR_Capture.h>
#include <OVR_Capture_Types.h>

#include <string.h> // memcpy

#if defined(OVR_CAPTURE_POSIX)
    #include <pthread.h>
    #include <sched.h>
    #include <unistd.h>
#endif

namespace OVR
{
namespace Capture
{

#if defined(OVR_CAPTURE_POSIX)
    typedef pthread_key_t ThreadLocalKey;
    static const ThreadLocalKey NullThreadLocalKey = 0;
#endif

    ThreadLocalKey CreateThreadLocalKey(void (*destructor)(void*));
    void           DestroyThreadLocalKey(ThreadLocalKey key);
    void           SetThreadLocalValue(ThreadLocalKey key, void *value);
    void          *GetThreadLocalValue(ThreadLocalKey key);

    void           SetThreadName(const char *name);

    inline void MemoryBarrier(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        __sync_synchronize();
    #else
        #error Unknown platform!
    #endif
    }

    template<typename T>
    inline T AtomicExchange(volatile T &destination, T exchange)
    {
    #if defined(OVR_CAPTURE_POSIX)
        return __sync_lock_test_and_set(&destination, exchange);
    #else
        #error Unknown platform!
    #endif
    }

    template<typename T>
    inline T AtomicAdd(volatile T &destination, T x)
    {
    #if defined(OVR_CAPTURE_POSIX)
        return __sync_fetch_and_add(&destination, x);
    #else
        #error Unknown platform!
    #endif
    }

    template<typename T>
    inline T AtomicSub(volatile T &destination, T x)
    {
    #if defined(OVR_CAPTURE_POSIX)
        return __sync_fetch_and_sub(&destination, x);
    #else
        #error Unknown platform!
    #endif
    }

    template<typename T>
    inline T AtomicGet(volatile T &x)
    {
        MemoryBarrier();
        return x;
    }

    inline void ThreadYield(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        sched_yield();
    #else
        #error Unknown Platform!
    #endif
    }

    inline void ThreadSleepNanoseconds(UInt32 nanoseconds)
    {
    #if defined(OVR_CAPTURE_POSIX)
        // Because in our case when we sleep we really want to try and 
        // sleep the maximum amount of time. So we use nanosleep and loop
        // until we sleep the full amount of time.
        struct timespec req = {0,0};
        req.tv_nsec = nanoseconds;
        while(nanosleep(&req, &req) != 0)
            continue;
    #else
        #error Unknown Platform!
    #endif
    }

    inline void ThreadSleepMicroseconds(UInt32 microseconds)
    {
        ThreadSleepNanoseconds(microseconds * 1000);
    }

    inline bool SpinTryLock(volatile int &atomic)
    {
        return AtomicExchange(atomic, 1) ? false : true;
    }

    inline void SpinLock(volatile int &atomic)
    {
        // Do a quick test immediately because by definition the spin lock should
        // be extremely low contention.
        if(SpinTryLock(atomic))
            return;

        // Try without yielding for the first few cycles...
        for(int i=0; i<50; i++)
            if(SpinTryLock(atomic))
                return;

        // TODO: we should measure how often we end up going down the yield path...
        //       a histogram of how many iterations it takes to succeed would be perfect.

        // If we are taking a long time, start yielding...
        while(true)
        {
            ThreadYield();
            for(int i=0; i<10; i++)
                if(SpinTryLock(atomic))
                    return;
        }
    }

    inline void SpinUnlock(volatile int &atomic)
    {
        MemoryBarrier();
        atomic = 0;
    }

    class CriticalSection
    {
        public:
            CriticalSection(void);
            ~CriticalSection(void);
            void Lock(void);
            bool TryLock(void);
            void Unlock(void);
        private:
        #if defined(OVR_CAPTURE_WINDOWS)
            CRITICAL_SECTION  m_cs;
        #elif defined(OVR_CAPTURE_POSIX)
            pthread_mutex_t   m_mutex;
        #endif
    };

    class ThreadGate
    {
        public:
            ThreadGate(void);
            ~ThreadGate(void);

            void Open(void);
            void WaitForOpen(void);
            void Close(void);

        private:
        #if defined(OVR_CAPTURE_POSIX)
            pthread_mutex_t m_mutex;
            pthread_cond_t  m_cond;
            bool            m_open;
        #endif
    };

    class Thread
    {
        public:
            Thread(void);
            virtual ~Thread(void);

            void Start(void);
            void QuitAndWait(void);

        protected:
            bool QuitSignaled(void) const;

        private:
            virtual void OnThreadExecute(void) = 0;

        #if defined(OVR_CAPTURE_POSIX)
            static void *ThreadEntry(void *arg);
        #endif

        private:
        #if defined(OVR_CAPTURE_POSIX)
            pthread_t m_thread;
        #endif
            bool      m_quitSignaled;
    };

} // namespace Capture
} // namespace OVR

#endif
