/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_Packets.h
Content     :   Oculus performance capture protocol description.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

// These structures/enums/types are the raw components of the network protocol used by OVR::Capture...
// For internal and tooling use only... normal applications will never need to use these directly.

#ifndef OVR_CAPTURE_PACKETS_H
#define OVR_CAPTURE_PACKETS_H

#include <OVR_Capture_Config.h>
#include <OVR_Capture_Types.h>

namespace OVR
{
namespace Capture
{

// force to 4-byte alignment on all platforms... this *should* cause these structs to ignore -malign-double
// This might cause a slight load/store penalty on uint64/double, but it should be exceedingly minor in the
// grand scheme of things, and likely worth it for bandwidth reduction.
#pragma pack(4)

    enum PacketIdentifier
    {
        Packet_ThreadName = 1,

        Packet_Label,

        Packet_Frame,
        Packet_VSync,

        Packet_CPU_Zone_Enter,
        Packet_CPU_Zone_Leave,

        Packet_GPU_Zone_Enter,
        Packet_GPU_Zone_Leave,

        Packet_GPU_Clock_Sync,

        Packet_Sensor_Set,
        Packet_Sensor_Range,

        Packet_FrameBuffer,

        Packet_Log,
    };

    struct ZeroConfigPacket
    {
        static const UInt64 s_magicNumber   = 0x540b4992be74a388ull;
        static const UInt16 s_broadcastPort = 2020;
        static const UInt32 s_nameMaxLength = 64;

        UInt64 magicNumber;
        UInt32 tcpPort;
        char   packageName[s_nameMaxLength];
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(ZeroConfigPacket)==8+4+64);



    struct ConnectionHeaderPacket
    {
        static const UInt32 s_version = 1;

        UInt32 size;    // sizeof(ConnectionHeaderPacket)
        UInt32 version; // ConnectionHeaderPacket::s_version
        UInt32 flags;   // bits from CaptureFlag
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(ConnectionHeaderPacket)==4+4+4);



    struct PacketDescriptorHeaderPacket
    {
        UInt32 numPacketTypes; // the number of PacketDescriptorPacket we are about to send
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(PacketDescriptorHeaderPacket)==4);

    struct PacketDescriptorPacket
    {
        UInt32 packetID;              // value from PacketIdentifier
        UInt32 version;               // PacketType::s_version
        UInt32 sizeofPacket;          // sizeof(PacketType)
        UInt32 sizeofPayloadSizeType; // sizeof(PacketType::PayloadSizeType)
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(PacketDescriptorPacket)==4+4+4+4);

    struct StreamHeaderPacket
    {
        UInt32 threadID;
        UInt32 streamSize;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(StreamHeaderPacket)==4+4);




    struct PacketHeader
    {
        UInt8 packetID; // value from PacketIdentifier
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(PacketHeader)==1);




    template<UInt32 packet_version=1>
    struct ThreadNamePacket_
    {
        typedef UInt8 PayloadSizeType;

        static const PacketIdentifier s_packetID       = Packet_ThreadName;
        static const UInt32           s_version        = packet_version;
        static const bool             s_hasPayload     = true;

        UInt8 dummy; // C++ doesn't allow for zero-sized structs. So we burn a byte in this one special case.
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(ThreadNamePacket_<>)==1);
    typedef ThreadNamePacket_<> ThreadNamePacket; // Current Version

    template<UInt32 packet_version=1>
    struct LabelPacket_
    {
        typedef UInt8 PayloadSizeType;

        static const PacketIdentifier s_packetID       = Packet_Label;
        static const UInt32           s_version        = packet_version;
        static const bool             s_hasPayload     = true;

        UInt32 labelID;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(LabelPacket_<>)==4);
    typedef LabelPacket_<> LabelPacket; // Current Version

    template<UInt32 packet_version=1>
    struct FramePacket_
    {
        static const PacketIdentifier s_packetID   = Packet_Frame;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(FramePacket_<>)==8);
    typedef FramePacket_<> FramePacket; // Current Version

    template<UInt32 packet_version=1>
    struct VSyncPacket_
    {
        static const PacketIdentifier s_packetID   = Packet_VSync;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(VSyncPacket_<>)==8);
    typedef VSyncPacket_<> VSyncPacket; // Current Version

    template<UInt32 packet_version=1>
    struct CPUZoneEnterPacket_
    {
        static const PacketIdentifier s_packetID   = Packet_CPU_Zone_Enter;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt32 labelID;
        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(CPUZoneEnterPacket_<>)==4+8);
    typedef CPUZoneEnterPacket_<> CPUZoneEnterPacket; // Current Version

    template<UInt32 packet_version=1>
    struct CPUZoneLeavePacket_
    {
        static const PacketIdentifier s_packetID   = Packet_CPU_Zone_Leave;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(CPUZoneLeavePacket_<>)==8);
    typedef CPUZoneLeavePacket_<> CPUZoneLeavePacket; // Current Version

    template<UInt32 packet_version=1>
    struct GPUZoneEnterPacket_
    {
        static const PacketIdentifier s_packetID   = Packet_GPU_Zone_Enter;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt32 labelID;
        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(GPUZoneEnterPacket_<>)==4+8);
    typedef GPUZoneEnterPacket_<> GPUZoneEnterPacket; // Current Version

    template<UInt32 packet_version=1>
    struct GPUZoneLeavePacket_
    {
        static const PacketIdentifier s_packetID   = Packet_GPU_Zone_Leave;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(GPUZoneLeavePacket_<>)==8);
    typedef GPUZoneLeavePacket_<> GPUZoneLeavePacket; // Current Version

    template<UInt32 packet_version=1>
    struct GPUClockSyncPacket_
    {
        static const PacketIdentifier s_packetID   = Packet_GPU_Clock_Sync;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt64 timestampCPU;
        UInt64 timestampGPU;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(GPUClockSyncPacket_<>)==8+8);
    typedef GPUClockSyncPacket_<> GPUClockSyncPacket; // Current Version

    template<UInt32 packet_version=1>
    struct SensorRangePacket_
    {
        static const PacketIdentifier s_packetID   = Packet_Sensor_Range;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt32 labelID;
        UInt16 interpolator;  // SensorInterpolator
        UInt16 units;         // SensorUnits
        float  minValue;
        float  maxValue;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(SensorRangePacket_<>)==4+2+2+4+4);
    typedef SensorRangePacket_<> SensorRangePacket; // Current Version

    template<UInt32 packet_version=1>
    struct SensorPacket_
    {
        static const PacketIdentifier s_packetID   = Packet_Sensor_Set;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = false;

        UInt32 labelID;
        float  value;
        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(SensorPacket_<>)==4+4+8);
    typedef SensorPacket_<> SensorPacket; // Current Version

    template<UInt32 packet_version=1>
    struct FrameBufferPacket_
    {
        typedef UInt32 PayloadSizeType;

        static const PacketIdentifier s_packetID   = Packet_FrameBuffer;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = true;

        UInt32 format;
        UInt32 width;
        UInt32 height;
        UInt64 timestamp;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(FrameBufferPacket_<>::PayloadSizeType)==4);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(FrameBufferPacket_<>)==4+4+4+8);
    typedef FrameBufferPacket_<> FrameBufferPacket; // Current Version

    template<UInt32 packet_version=1>
    struct LogPacket_
    {
        typedef UInt16 PayloadSizeType;

        static const PacketIdentifier s_packetID   = Packet_Log;
        static const UInt32           s_version    = packet_version;
        static const bool             s_hasPayload = true;

        UInt64 timestamp;
        UInt32 priority;
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(LogPacket_<>::PayloadSizeType)==2);
    OVR_CAPTURE_STATIC_ASSERT(sizeof(LogPacket_<>)==8+4);
    typedef LogPacket_<> LogPacket; // Current Version

// restore default alignment...
#pragma pack()

} // namespace Capture
} // namespace OVR

#endif
