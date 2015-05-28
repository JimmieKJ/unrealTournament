/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture.cpp
Content     :   Oculus performance capture library
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <OVR_Capture.h>
#include "OVR_Capture_Socket.h"
#include "OVR_Capture_AsyncStream.h"
#include "OVR_Capture_StandardSensors.h"
#include "OVR_Capture_Packets.h"

#if defined(OVR_CAPTURE_DARWIN)
    #include <mach/mach_time.h>
    #include <pthread.h>
#elif defined(OVR_CAPTURE_POSIX)
    #include <pthread.h>
    #include <malloc.h>
#endif

#if defined(OVR_CAPTURE_POSIX)
    #include <pthread.h>
#endif

#if defined(OVR_CAPTURE_HAS_MACH_ABSOLUTE_TIME)
    #include <mach/mach_time.h>
#endif

#if defined(OVR_CAPTURE_USE_CLOCK_GETTIME)
    #include <time.h>
#endif

#if defined(OVR_CAPTURE_HAS_GETTIMEOFDAY)
    #include <sys/time.h>
#endif

#if defined(OVR_CAPTURE_HAS_ZLIB)
    #include <zlib.h>
#endif

#include <memory.h>
#include <new>

#include <stdio.h>

namespace OVR
{
namespace Capture
{

    class Server;

    static const UInt16         g_zeroConfigPort  = ZeroConfigPacket::s_broadcastPort;
    static const UInt16         g_socketPortBegin = 3030;
    static const UInt16         g_socketPortEnd   = 3040;

    static Server              *g_server          = NULL;

    static volatile UInt32      g_initFlags       = 0;
    static volatile UInt32      g_connectionFlags = 0;

    static volatile int         g_labelLock       = 0;

    /***********************************
    * Helper Functions                 *
    ***********************************/

    static void SendLabelPacket(const Label &label)
    {
        if(IsConnected())
        {
            LabelPacket packet;
            packet.labelID = label.GetIdentifier();
            const char *name = label.GetName();
            AsyncStream::Acquire()->WritePacket(packet, name, strlen(name));
        }
    }

    static UInt32 StringHash32(const char *str)
    {
        // Tested on 235,886 words in '/usr/share/dict/words'
    #if defined(OVR_CAPTURE_HAS_ZLIB)
        // Multiple iterations of zlib crc32, which improves entropy significantly
        // 1 Collisions Total with 3 passes... Chance Of Collisions 0.000424%
        // (8 Collisions with a single pass)
        UInt32 h = 0;
        const uInt len = strlen(str);
        for(UInt32 i=0; i<3; i++)
            h = static_cast<UInt32>(::crc32(h, (const Bytef*)str, len));
        return h
    #else
        // 2 Collisions Total with 3 passes... Chance Of Collisions 0.000848%
        // (9 Collisions  with a single pass)
        // So nearly as good as multi pass zlib crc32...
        // Seeded with a bunch of prime numbers...
        const unsigned int a = 54059;
        const unsigned int b = 76963;
        unsigned int       h = 0;
        for(unsigned int i=0; i<3; i++)
        {
            h = (h>>2) | 31;
            for(const char *c=str; *c; c++)
            {
                h = (h * a) ^ ((*c) * b);
            }
        }
        return h;
    #endif
    }

    template<typename PacketType, bool hasPayload> struct PayloadSizer;
    template<typename PacketType> struct PayloadSizer<PacketType, true>
    {
        static UInt32 GetSizeOfPayloadSizeType(void)
        {
            return sizeof(typename PacketType::PayloadSizeType);
        }
    };
    template<typename PacketType> struct PayloadSizer<PacketType, false>
    {
        static UInt32 GetSizeOfPayloadSizeType(void)
        {
            return 0;
        }
    };

    template<typename PacketType> static PacketDescriptorPacket BuildPacketDescriptorPacket(void)
    {
        PacketDescriptorPacket desc = {0};
        desc.packetID              = PacketType::s_packetID;
        desc.version               = PacketType::s_version;
        desc.sizeofPacket          = sizeof(PacketType);
        desc.sizeofPayloadSizeType = PayloadSizer<PacketType, PacketType::s_hasPayload>::GetSizeOfPayloadSizeType();
        return desc;
    }

    /***********************************
    * Server                           *
    ***********************************/

    // Thread/Socket that sits in the background waiting for incoming connections...
    // Not to be confused with the ZeroConfig hosts who advertises our existance, this
    // is the socket that actually accepts the incoming connections and creates the socket.
    class Server : public Thread
    {
        public:
            Server(const char *packageName)
            {
                m_streamSocket = NULL;
                m_listenSocket = NULL;
                m_zeroconfig   = NULL;

                // Find the first open port...
                for(UInt16 port=g_socketPortBegin; port<g_socketPortEnd; port++)
                {
                    SocketAddress listenAddr = SocketAddress::Any(port);
                    m_listenSocket           = Socket::Create(Socket::Type_Stream);
                    if(m_listenSocket && !m_listenSocket->Bind(listenAddr))
                    {
                        m_listenSocket->Release();
                        m_listenSocket = NULL;
                    }
                    if(m_listenSocket && !m_listenSocket->Listen(1))
                    {
                        m_listenSocket->Release();
                        m_listenSocket = NULL;
                    }
                    if(m_listenSocket)
                    {
                        // If we have a valid listen socket, create our zero config server...
                        m_zeroconfig = ZeroConfigHost::Create(g_zeroConfigPort, port, packageName);
                        m_zeroconfig->Start();
                        break;
                    }
                }
            }
            virtual ~Server(void)
            {
                // Signal and wait for quit...
                if(m_listenSocket) m_listenSocket->Shutdown();
                QuitAndWait();

                // Cleanup just in case the thread doesn't...
                if(m_zeroconfig)   m_zeroconfig->Release();
                if(m_streamSocket) m_streamSocket->Release();
                if(m_listenSocket) m_listenSocket->Release();
            }

        private:
            virtual void OnThreadExecute(void)
            {
                SetThreadName("OVR::Capture");
                while(m_listenSocket && !QuitSignaled())
                {
                    // try and accept a new socket connection...
                    SocketAddress streamAddr;
                    m_streamSocket = m_listenSocket->Accept(streamAddr);

                    // If no connection was established, something went totally wrong and we should just abort...
                    if(!m_streamSocket)
                        break;
                    
                    // Before we start sending capture data... first must exchange connection headers...
                    // First attempt to read in the request header from the Client...
                    ConnectionHeaderPacket clientHeader = {0};
                    if(!m_streamSocket->Receive(&clientHeader, sizeof(clientHeader)))
                    {
                        m_streamSocket->Release();
                        m_streamSocket = NULL;
                        continue;
                    }

                    // Load our connection flags...
                    const UInt32 connectionFlags = clientHeader.flags & g_initFlags;
                    
                    // Build and send return header... We *always* send the return header so that if we don't
                    // like something (like version number or feature flags), the client has some hint as to
                    // what we didn't like.
                    ConnectionHeaderPacket serverHeader = {0};
                    serverHeader.size    = sizeof(serverHeader);
                    serverHeader.version = ConnectionHeaderPacket::s_version;
                    serverHeader.flags   = connectionFlags;
                    if(!m_streamSocket->Send(&serverHeader, sizeof(serverHeader)))
                    {
                        m_streamSocket->Release();
                        m_streamSocket = NULL;
                        continue;
                    }

                    // Check version number...
                    if(clientHeader.version != serverHeader.version)
                    {
                        m_streamSocket->Release();
                        m_streamSocket = NULL;
                        continue;
                    }

                    // Check that we have any capture features even turned on...
                    if(!connectionFlags)
                    {
                        m_streamSocket->Release();
                        m_streamSocket = NULL;
                        continue;
                    }

                    // Finally, send our packet descriptors...
                    const PacketDescriptorPacket packetDescs[] =
                    {
                        BuildPacketDescriptorPacket<ThreadNamePacket>(),
                        BuildPacketDescriptorPacket<LabelPacket>(),
                        BuildPacketDescriptorPacket<FramePacket>(),
                        BuildPacketDescriptorPacket<VSyncPacket>(),
                        BuildPacketDescriptorPacket<CPUZoneEnterPacket>(),
                        BuildPacketDescriptorPacket<CPUZoneLeavePacket>(),
                        BuildPacketDescriptorPacket<GPUZoneEnterPacket>(),
                        BuildPacketDescriptorPacket<GPUZoneLeavePacket>(),
                        BuildPacketDescriptorPacket<GPUClockSyncPacket>(),
                        BuildPacketDescriptorPacket<SensorRangePacket>(),
                        BuildPacketDescriptorPacket<SensorPacket>(),
                        BuildPacketDescriptorPacket<FrameBufferPacket>(),
                        BuildPacketDescriptorPacket<LogPacket>(),
                    };
                    const PacketDescriptorHeaderPacket packetDescHeader = { sizeof(packetDescs) / sizeof(packetDescs[0]) };
                    if(!m_streamSocket->Send(&packetDescHeader, sizeof(packetDescHeader)))
                    {
                        m_streamSocket->Release();
                        m_streamSocket = NULL;
                        continue;
                    }
                    if(!m_streamSocket->Send(&packetDescs, sizeof(packetDescs)))
                    {
                        m_streamSocket->Release();
                        m_streamSocket = NULL;
                        continue;
                    }
                    
                    // Connection established!

                    // Signal that we are connected!
                    AtomicExchange(g_connectionFlags, connectionFlags);

                    // Technically any Labels that get initialized on another thread bettween the barrier and loop
                    // will get sent over the network twice, but OVRMonitor will handle that.
                    SpinLock(g_labelLock);
                    for(Label *l=Label::GetHead(); l; l=l->GetNext())
                    {
                        SendLabelPacket(*l);
                    }
                    SpinUnlock(g_labelLock);

                    // Start CPU/GPU/Thermal sensors...
                    StandardSensors stdsensors;
                    if(CheckConnectionFlag(Enable_CPU_Clocks) || CheckConnectionFlag(Enable_GPU_Clocks) || CheckConnectionFlag(Enable_Thermal_Sensors))
                    {
                        stdsensors.Start();
                    }

                    // Spin as long as we are connected flushing data from our data stream...
                    while(!QuitSignaled())
                    {
                        const UInt64 flushBeginTime = GetNanoseconds();
                        if(!AsyncStream::FlushAll(*m_streamSocket))
                        {
                            // Error occured... shutdown the connection.
                            AtomicExchange(g_connectionFlags, (UInt32)0);
                            m_streamSocket->Shutdown();
                            break;
                        }
                        const UInt64 flushEndTime   = GetNanoseconds();
                        const UInt64 flushDeltaTime = flushEndTime - flushBeginTime;
                        const UInt64 sleepTime      = 5000000; // 5ms
                        if(flushDeltaTime < sleepTime)
                        {
                            // Sleep just a bit to keep the thread from killing a core and to let a good chunk of data build up
                            ThreadSleepNanoseconds(sleepTime - flushDeltaTime);
                        }
                    }

                    // TODO: should we call AsyncStream::Shutdown() here???

                    // Close down our sensor thread...
                    stdsensors.QuitAndWait();

                    // Connection was closed at some point, lets clean up our socket...
                    m_streamSocket->Release();
                    m_streamSocket = NULL;

                } // while(m_listenSocket && !QuitSignaled())
            }

        private:
            Socket         *m_streamSocket;
            Socket         *m_listenSocket;
            ZeroConfigHost *m_zeroconfig;
    };

    /***********************************
    * Public API                       *
    ***********************************/


    // Get current time in microseconds...
    UInt64 GetNanoseconds(void)
    {
    #if defined(OVR_CAPTURE_HAS_MACH_ABSOLUTE_TIME)
        // OSX/iOS doesn't have clock_gettime()... but it does have gettimeofday(), or even better mach_absolute_time()
        // which is about 50% faster than gettimeofday() and higher precision!
        // Only 24.5ns per GetNanoseconds() call! But we can do better...
        // It seems that modern Darwin already returns nanoseconds, so numer==denom
        // when we test that assumption it brings us down to 16ns per GetNanoseconds() call!!!
        // Timed on MacBookPro running OSX.
        static mach_timebase_info_data_t info = {0};
        if(!info.denom)
            mach_timebase_info(&info);
        const UInt64 t = mach_absolute_time();
        if(info.numer==info.denom)
            return t;
        return (t * info.numer) / info.denom;

    #elif defined(OVR_CAPTURE_HAS_CLOCK_GETTIME)
        // 23ns per call on i7 Desktop running Ubuntu 64
        // >800ns per call on Galaxy Note 4 running Android 4.3!!!
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        return ((UInt64)tp.tv_sec)*1000000000 + (UInt64)tp.tv_nsec;

    #elif defined(OVR_CAPTURE_HAS_GETTIMEOFDAY)
        // Just here for reference... this timer is only microsecond level of precision, and >2x slower than the mach timer...
        // And on non-mach platforms clock_gettime() is the preferred method...
        // 34ns per call on MacBookPro running OSX...
        // 23ns per call on i7 Desktop running Ubuntu 64
        // >800ns per call on Galaxy Note 4 running Android 4.3!!!
        struct timeval tv;
        gettimeofday(&tv, 0);
        const UInt64 us = ((UInt64)tv.tv_sec)*1000000 + (UInt64)tv.tv_usec;
        return us*1000;

    #else
        #error Unknown Platform!

    #endif
    }

    // Initializes the Capture system... should be called before any other Capture call.
    bool Init(const char *packageName, UInt32 flags)
    {
        OVR_CAPTURE_ASSERT(!g_initFlags && !g_connectionFlags);

        // sanitze input flags;
        flags = flags & All_Flags;

        // If no capture features are enabled... then don't initialize anything!
        if(!flags)
            return false;

        g_initFlags       = flags;
        g_connectionFlags = 0;

        // Initialize the pre-thread stream system...
        AsyncStream::Init();

        OVR_CAPTURE_ASSERT(!g_server);
        g_server = new Server(packageName);
        g_server->Start();

        return true;
    }

    // Closes the capture system... no other Capture calls on *any* thead should be called after this.
    void Shutdown(void)
    {
        if(g_server)
        {
            delete g_server;
            g_server = NULL;
        }
        OVR_CAPTURE_ASSERT(!g_connectionFlags);
        g_initFlags       = 0;
        g_connectionFlags = 0;
    }

    // Indicates that the capture system is currently connected...
    bool IsConnected(void)
    {
        return AtomicGet(g_connectionFlags) != 0;
    }

    // Check to see if (a) a connection is established and (b) that a particular capture feature is enabled on the connection.
    bool CheckConnectionFlag(CaptureFlag feature)
    {
        return (AtomicGet(g_connectionFlags) & feature) != 0;
    }

    // Mark frame boundary... call from only one thread!
    void Frame(void)
    {
        if(IsConnected())
        {
            FramePacket packet;
            packet.timestamp = GetNanoseconds();
            AsyncStream::Acquire()->WritePacket(packet);
        }
    }

    // Mark the start of vsync... this value should be comparable to the same reference point as GetNanoseconds()
    void VSyncTimestamp(UInt64 nanoseconds)
    {
        if(IsConnected())
        {
            VSyncPacket packet;
            packet.timestamp = nanoseconds;
            AsyncStream::Acquire()->WritePacket(packet);
        }
    }

    // Upload the framebuffer for the current frame... should be called once a frame!
    void FrameBuffer(UInt64 timestamp, FrameBufferFormat format, UInt32 width, UInt32 height, const void *buffer)
    {
        if(CheckConnectionFlag(Enable_FrameBuffer_Capture))
        {
            UInt32 pixelSize = 0;
            switch(format)
            {
                case FrameBuffer_RGB_565:   pixelSize=2; break;
                case FrameBuffer_RGBA_8888: pixelSize=4; break;
            }
            OVR_CAPTURE_ASSERT(pixelSize);
            const UInt32 payloadSize = pixelSize * width * height;
            FrameBufferPacket packet;
            packet.format     = format;
            packet.width      = width;
            packet.height     = height;
            packet.timestamp  = timestamp;
            // TODO: we should probably just send framebuffer packets directly over the network rather than
            //       caching them due to their size and to reduce latency.
            AsyncStream::Acquire()->WritePacket(packet, buffer, payloadSize);
        }
    }

    // Misc application message logging...
    void Logf(LogPriority priority, const char *format, ...)
    {
        if(CheckConnectionFlag(Enable_Logging))
        {
            va_list args;
            va_start(args, format);
            Logv(priority, format, args);
            va_end(args);
        }
    }

    void Logv(LogPriority priority, const char *format, va_list args)
    {
        if(CheckConnectionFlag(Enable_Logging))
        {
            LogPacket packet;
            packet.timestamp = GetNanoseconds();
            packet.priority  = priority;
            const size_t bufferMaxSize = 512;
            char buffer[bufferMaxSize];
            const int bufferSize = vsnprintf(buffer, bufferMaxSize, format, args);
            if(bufferSize > 0)
            {
                AsyncStream::Acquire()->WritePacket(packet, buffer, (UInt32)bufferSize);
            }
        }
    }

    // Mark a CPU profiled region.... Begin(); DoSomething(); End();
    // Nesting is allowed. And every Begin() should ALWAYS have a matching End()!!!
    void EnterCPUZone(const Label &label)
    {
        if(CheckConnectionFlag(Enable_CPU_Zones))
        {
            CPUZoneEnterPacket packet;
            packet.labelID   = label.GetIdentifier();
            packet.timestamp = GetNanoseconds();
            AsyncStream::Acquire()->WritePacket(packet);
        }
    }

    void LeaveCPUZone(void)
    {
        if(CheckConnectionFlag(Enable_CPU_Zones))
        {
            CPUZoneLeavePacket packet;
            packet.timestamp = GetNanoseconds();
            AsyncStream::Acquire()->WritePacket(packet);
        }
    }

    // Set sensor range of values.
    void SensorSetRange(const Label &label, float minValue, float maxValue, SensorInterpolator interpolator, SensorUnits units)
    {
        if(IsConnected())
        {
            SensorRangePacket packet;
            packet.labelID      = label.GetIdentifier();
            packet.interpolator = interpolator;
            packet.units        = units;
            packet.minValue     = minValue;
            packet.maxValue     = maxValue;
            AsyncStream::Acquire()->WritePacket(packet);
        }
    }

    // Set the absolute value of a sensor, may be called at any frequency.
    void SensorSetValue(const Label &label, float value)
    {
        if(IsConnected())
        {
            SensorPacket packet;
            packet.labelID   = label.GetIdentifier();
            packet.timestamp = GetNanoseconds();
            packet.value     = value;
            AsyncStream::Acquire()->WritePacket(packet);
        }
    }

    // Add a value to a counter... the absolute value gets reset to zero every time Frame() is called.
    void CounterAddValue(const Label &label, float value)
    {
    }

    /***********************************
    * Label                            *
    ***********************************/

    Label  *Label::s_head      = NULL;

    // Use this constructor if Label is a global variable (not local static).
    Label::Label(const char *name)
    {
        Init(name);
    }

    bool Label::ConditionalInit(const char *name)
    {
        SpinLock(g_labelLock);
        if(!m_name) Init(name);
        SpinUnlock(g_labelLock);
        return true;
    }

    void Label::Init(const char *name)
    {
        m_next       = s_head;
        s_head       = this;
        m_identifier = StringHash32(name);
        m_name       = name;
        SendLabelPacket(*this);
    }

    Label *Label::GetHead(void)
    {
        Label *head = s_head;
        return head;
    }

    Label *Label::GetNext(void) const
    {
        return m_next;
    }

} // namespace Capture
} // namespace OVR
