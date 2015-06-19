/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_Thread.cpp
Content     :   Misc Threading functionality....
Created     :   January, 2015
Notes       :   Will be replaced by OVR Kernel versions soon!

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_Thread.h"

namespace OVR
{
namespace Capture
{


    ThreadLocalKey CreateThreadLocalKey(void (*destructor)(void*))
    {
    #if defined(OVR_CAPTURE_POSIX)
        ThreadLocalKey key = {0};
        pthread_key_create(&key, destructor);
        return key;
    #else
        #error Unknown Platform!
    #endif
    }

    void DestroyThreadLocalKey(ThreadLocalKey key)
    {
    #if defined(OVR_CAPTURE_POSIX)
        pthread_key_delete(key);
    #else
        #error Unknown Platform!
    #endif
    }

    void SetThreadLocalValue(ThreadLocalKey key, void *value)
    {
    #if defined(OVR_CAPTURE_POSIX)
        pthread_setspecific(key, value);
    #else
        #error Unknown Platform!
    #endif
    }

    void *GetThreadLocalValue(ThreadLocalKey key)
    {
    #if defined(OVR_CAPTURE_POSIX)
        return pthread_getspecific(key);
    #else
        #error Unknown Platform!
    #endif
    }

    void SetThreadName(const char *name)
    {
    #if defined(OVR_CAPTURE_DARWIN)
        pthread_setname_np(name);
    #elif defined(OVR_CAPTURE_ANDROID)
        pthread_setname_np(pthread_self(), name);
    #else
        #error Unknown Platform!
    #endif
    }


    CriticalSection::CriticalSection(void)
    {
    #if defined(OVR_CAPTURE_WINDOWS)
        InitializeCriticalSection(&m_cs);
    #elif defined(OVR_CAPTURE_POSIX)
        pthread_mutex_init(&m_mutex, NULL);
    #endif
    }

    CriticalSection::~CriticalSection(void)
    {
        #if defined(OVR_CAPTURE_WINDOWS)
            DeleteCriticalSection(&m_cs);
        #elif defined(OVR_CAPTURE_POSIX)
            pthread_mutex_destroy(&m_mutex);
        #endif
    }

    void CriticalSection::Lock(void)
    {
    #if defined(OVR_CAPTURE_WINDOWS)
        EnterCriticalSection(&m_cs);
    #elif defined(OVR_CAPTURE_POSIX)
        pthread_mutex_lock(&m_mutex);
    #endif
    }

    bool CriticalSection::TryLock(void)
    {
    #if defined(OVR_CAPTURE_WINDOWS)
        return TryEnterCriticalSection(&m_cs) ? true : false;
    #elif defined(OVR_CAPTURE_POSIX)
        return pthread_mutex_trylock(&m_mutex)==0;
    #endif
    }

    void CriticalSection::Unlock(void)
    {
    #if defined(OVR_CAPTURE_WINDOWS)
        LeaveCriticalSection(&m_cs);
    #elif defined(OVR_CAPTURE_POSIX)
        pthread_mutex_unlock(&m_mutex);
    #endif
    }



    ThreadGate::ThreadGate(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init( &m_cond,  NULL);
    #endif
        Open();
    }

    ThreadGate::~ThreadGate(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy( &m_cond);
    #endif
    }

    void ThreadGate::Open(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        pthread_mutex_lock(&m_mutex);
        m_open = true;
        pthread_cond_broadcast(&m_cond);
        pthread_mutex_unlock(&m_mutex);
    #endif
    }

    void ThreadGate::WaitForOpen(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        pthread_mutex_lock(&m_mutex);
        while(!m_open)
            pthread_cond_wait(&m_cond, &m_mutex);
        pthread_mutex_unlock(&m_mutex);
    #endif
    }

    void ThreadGate::Close(void)
    {
    #if defined(OVR_CAPTURE_POSIX)
        pthread_mutex_lock(&m_mutex);
        m_open = false;
        pthread_mutex_unlock(&m_mutex);
    #endif
    }




    Thread::Thread(void)
    {
        m_thread       = 0;
        m_quitSignaled = false;
    }

    Thread::~Thread(void)
    {
        QuitAndWait();
    }

    void Thread::Start(void)
    {
        OVR_CAPTURE_ASSERT(!m_thread && !m_quitSignaled);
    #if defined(OVR_CAPTURE_POSIX)
        pthread_create(&m_thread, 0, ThreadEntry, this);
    #else
        #error Unknown Platform!
    #endif
    }

    void Thread::QuitAndWait(void)
    {
        if(m_thread)
        {
            AtomicExchange(m_quitSignaled, true);
        #if defined(OVR_CAPTURE_POSIX)
            pthread_join(m_thread, 0);
        #else
            #error Unknown Platform!
        #endif
            m_thread       = 0;
            m_quitSignaled = false;
        }
    }

    bool Thread::QuitSignaled(void) const
    {
        return AtomicGet(m_quitSignaled);
    }

#if defined(OVR_CAPTURE_POSIX)
    void *Thread::ThreadEntry(void *arg)
    {
        Thread *thread = (Thread*)arg;
        thread->OnThreadExecute();
        return NULL;
    }
#endif

} // namespace Capture
} // namespace OVR
