/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_AsyncStream.cpp
Content     :   Oculus performance capture library. Interface for async data streaming.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_AsyncStream.h"
#include "OVR_Capture_Socket.h"
#include "OVR_Capture_FileIO.h"

#include <stdio.h>

namespace OVR
{
namespace Capture
{

    static ThreadLocalKey     g_tlskey   = NullThreadLocalKey;
    static CriticalSection    g_listlock;

    template<typename T>
    inline void Swap(T &a, T &b)
    {
        T temp = a;
        a      = b;
        b      = temp;
    }


    AsyncStream *AsyncStream::s_head = NULL;


    // Initialize the per-thread stream system... MUST be called before being connected!
    void AsyncStream::Init(void)
    {
        OVR_CAPTURE_ASSERT(g_tlskey == NullThreadLocalKey);
        g_tlskey = CreateThreadLocalKey(ThreadDetach);
    }

    // Acquire a per-thread stream for the current thread...
    AsyncStream *AsyncStream::Acquire(void)
    {
        AsyncStream *stream = (AsyncStream*)GetThreadLocalValue(g_tlskey);
        if(!stream)
        {
            stream = new AsyncStream();
            SetThreadLocalValue(g_tlskey, stream);
        }
        return stream;
    }

    // Flush all existing thread streams... returns false on socket error
    bool AsyncStream::FlushAll(Socket &s)
    {
        OVR_CAPTURE_CPU_ZONE(AsyncStream_FlushAll);
        bool okay = true;
        g_listlock.Lock();
        for(AsyncStream *curr=s_head; curr; curr=curr->m_next)
        {
            okay = curr->Flush(s);
            if(!okay) break;
        }
        g_listlock.Unlock();
        return okay;
    }


    // Flushes all available packets over the network... returns number of bytes sent
    bool AsyncStream::Flush(Socket &s)
    {
        OVR_CAPTURE_CPU_ZONE(AsyncStream_Flush);

        bool okay = true;

        // Take ownership of any pending data...
        SpinLock(m_bufferLock);
        Swap(m_cacheBegin, m_flushBegin);
        Swap(m_cacheTail,  m_flushTail);
        Swap(m_cacheEnd,   m_flushEnd);
        SpinUnlock(m_bufferLock);

        // Signal that we just swapped in a new buffer... wake up any threads that were waiting on us to flush.
        m_gate.Open();

        if(m_flushTail > m_flushBegin)
        {
            const size_t sendSize = (size_t)(m_flushTail-m_flushBegin);

            // first send stream header...
            StreamHeaderPacket streamheader;
            streamheader.threadID   = m_threadID;
            streamheader.streamSize = sendSize;
            okay = s.Send(&streamheader, sizeof(streamheader));

            // This send payload...
            okay = okay && s.Send(m_flushBegin, sendSize);
            m_flushTail = m_flushBegin;
        }

        OVR_CAPTURE_ASSERT(m_flushBegin == m_flushTail); // should be empty at this point...

        return okay;
    }

    // Do any required cleanup when the connection is closed.
    void AsyncStream::OnClose(void)
    {
        // Make sure we leave the gate open to prevent any threads from getting hung...
        m_gate.Open();
    }

    AsyncStream::AsyncStream(void)
    {
        m_bufferLock  = 0;

    #if defined(OVR_CAPTURE_POSIX)
        OVR_CAPTURE_STATIC_ASSERT(sizeof(pid_t) <= sizeof(UInt32));
        union
        {
            UInt32 i;
            pid_t  t;
        };
        i = 0;
        t = gettid();
        m_threadID = i;
    #elif defined(OVR_CAPTURE_WINDOWS)
        m_threadID = static_cast<UInt32>(GetCurrentThreadId());
    #else
        #error UNKNOWN PLATFORM!
    #endif

        m_cacheBegin  = (UInt8*)malloc(s_bufferSize);
        m_cacheTail   = m_cacheBegin;
        m_cacheEnd    = m_cacheBegin + s_bufferSize;

        m_flushBegin  = (UInt8*)malloc(s_bufferSize);
        m_flushTail   = m_flushBegin;
        m_flushEnd    = m_flushBegin + s_bufferSize;

        // Make sure we are open by default... we don't close until we fill the buffer...
        m_gate.Open();

        // when we are finally initialized... add ourselves to the linked list...
        g_listlock.Lock();
        m_next = s_head;
        s_head = this;
        g_listlock.Unlock();

        // Try and acquire thread name...
        SendThreadName();
    }

    AsyncStream::~AsyncStream(void)
    {
        // Before we destroy ourselves... must remove ourselves from the linked list...
        // TODO: currently we don't wait for the server thread to flush any remaining data,
        //       so in theory when a thread is destoryed it is possible to lose some data in
        //       the outbound stream!!!!!
        g_listlock.Lock();
        if(s_head == this)
        {
            // We our the head element... simply replace the head with the next one...
            s_head = m_next;
        }
        else
        {
            // Search for the element before us and set its next to our next...
            for(AsyncStream *curr=s_head; curr; curr=curr->m_next)
            {
                if(curr->m_next == this)
                {
                    curr->m_next = m_next;
                    break;
                }
            }
        }
        g_listlock.Unlock();

        if(m_cacheBegin) free(m_cacheBegin);
        if(m_flushBegin) free(m_flushBegin);
    }

    void AsyncStream::SendThreadName(void)
    {
        ThreadNamePacket packet = {0};
        char name[64] = {0};
    #if defined(OVR_CAPTURE_ANDROID)
        char commpath[64] = {0};
        sprintf(commpath, "/proc/%d/task/%d/comm", getpid(), gettid());
        ReadFileLine(commpath, name, sizeof(name));
    #elif defined(OVR_CAPTURE_DARWIN)
        pthread_getname_np(pthread_self(), name, sizeof(name));
    #endif
        if(name[0])
        {
            WritePacket(packet, name, strlen(name));
        }
    }

    void AsyncStream::ThreadDetach(void *arg)
    {
        AsyncStream *stream = (AsyncStream*)arg;
        // TODO: we should probably just remove a reference count or something instead here
        //       and cleanup after the next Flush()...
        delete stream;
    }

} // namespace Capture
} // namespace OVR
