/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_AsyncStream.h
Content     :   Oculus performance capture library. Interface for async data streaming.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_ASYNCSTREAM_H
#define OVR_CAPTURE_ASYNCSTREAM_H

#include <OVR_Capture.h>
#include <OVR_Capture_Packets.h>
#include "OVR_Capture_Thread.h"

namespace OVR
{
namespace Capture
{

    class Socket;

    class AsyncStream
    {
        friend class AsyncStreamCleanup;
        public:
            // Initialize the per-thread stream system... MUST be called before being connected!
            static void         Init(void);

            // Acquire a per-thread stream for the current thread...
            static AsyncStream *Acquire(void);

            // Flush all existing thread streams... returns false on socket error
            static bool         FlushAll(Socket &s);

        public:
            // Flushes all available packets over the network... returns false on socket error
            bool Flush(Socket &s);

            // Do any required cleanup when the connection is closed.
            void OnClose(void);

            template<typename PacketType>
            inline void WritePacket(const PacketType &packet)
            {
                OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == false);

                // initialize the header...
                PacketHeader header;
                header.packetID = PacketType::s_packetID;

                // Calculate total packet size...
                const UInt32 totalSize = sizeof(header) + sizeof(packet);

                // acquire the next available space in the cache we can write to...
                UInt8 *ptr = BeginWrite(totalSize);

                // Failed to acquire write permissions... probably because we disconnected... abort!
                if(!ptr)
                    return;

                // Write the header into cache...
                memcpy(ptr, &header, sizeof(header));
                ptr += sizeof(header);

                // Write the packet into cache...
                memcpy(ptr, &packet, sizeof(packet));
                ptr += sizeof(packet);

                // We are finished writing... release the lock...
                EndWrite();
            }

            template<typename PacketType>
            inline void WritePacket(const PacketType &packet, const void *payload, typename PacketType::PayloadSizeType payloadSize)
            {
                OVR_CAPTURE_STATIC_ASSERT(PacketType::s_hasPayload == true);

                // initialize the header...
                PacketHeader header;
                header.packetID = PacketType::s_packetID;

                // Calculate total packet size...
                const UInt32 totalSize = sizeof(header) + sizeof(packet) + sizeof(payloadSize) + payloadSize;

                // acquire the next available space in the cache we can write to...
                UInt8 *ptr = BeginWrite(totalSize);

                // Failed to acquire write permissions... probably because we disconnected... abort!
                if(!ptr)
                    return;

                // Write the header into cache...
                memcpy(ptr, &header, sizeof(header));
                ptr += sizeof(header);

                // Write the packet into cache...
                memcpy(ptr, &packet, sizeof(packet));
                ptr += sizeof(packet);

                // Write Payload Header...
                memcpy(ptr, &payloadSize, sizeof(payloadSize));
                ptr += sizeof(payloadSize);

                // Finally write the payload...
                if(payloadSize > 0)
                {
                    memcpy(ptr, payload, payloadSize);
                    ptr += payloadSize;
                }

                // We are finished writing... release the lock...
                EndWrite();
            }

        private:
            inline UInt8 *BeginWrite(UInt32 writeSize)
            {
                // Sanity check... this shouldn't happen, but might if someone sends a giant frame buffer...
                OVR_CAPTURE_ASSERT(writeSize < s_bufferSize);

                // acquire the next available space in the cache we can write to...
                UInt8 *ptr = NULL;
                while(true)
                {
                    SpinLock(m_bufferLock);
                    if(m_cacheTail + writeSize <= m_cacheEnd)
                    {
                        // Increment the cache pointer to give us exclusive access to a region we can write to...
                        ptr          = m_cacheTail;
                        m_cacheTail += writeSize;
                        break;
                    }
                    else
                    {
                        // While we are still locked, and the buffer is full... close the gate so that the WaitForOpen
                        // call below blocks. We need to close it while we are still locked so that we don't close it
                        // right after the socket thread does a buffer swap.
                        m_gate.Close();
                    }
                    // we should only get here if the buffer isn't big enough for this packet
                    // in which case we have two choices.... realloc() the buffer to be big enough
                    // or spin until the socket thread flushes it.
                    // Currently we choose to spin, but we must make sure we yield() in the event that
                    // we are on a SCHED_FIFO priority thread (e.g. timewarp) so that the scheduler doesn't
                    // freak out.
                    SpinUnlock(m_bufferLock);
                    // Outside of the spinlock, use a "real" synchronization object to wait for the socket thread, this
                    // should guarantee that SCHED_FIFO threads actually really do yield unlikely simply callingsched_yield().
                    m_gate.WaitForOpen();
                    // And if the connection closed while we waited... we need to just abort and return immediately.
                    if(!IsConnected())
                        return NULL;
                }
                return ptr;
            }

            inline void EndWrite(void)
            {
                // We are finished writing... release the lock...
                SpinUnlock(m_bufferLock);
            }

        private:
            AsyncStream(void);
            ~AsyncStream(void);

            void SendThreadName(void);

        private:
            static void ThreadDetach(void *arg);

        private:
            // 1MB cache... which is hopefully bigger than any single packet.
            // a 128*128 565 FrameBuffer is 32KB + sizeof(header)
            static const UInt32 s_bufferSize = 1024*1024;

            static AsyncStream *s_head;
            AsyncStream        *m_next;

            volatile Int32      m_bufferLock;

            ThreadGate m_gate;

            // The threat that created this stream via Acquire()
            UInt32 m_threadID;

            // The buffer we are currently filling...
            UInt8 *m_cacheBegin;
            UInt8 *m_cacheTail;
            UInt8 *m_cacheEnd;

            // The buffer we are currently flushing over the network...
            UInt8 *m_flushBegin;
            UInt8 *m_flushTail;
            UInt8 *m_flushEnd;
    };

} // namespace Capture
} // namespace OVR

#endif
