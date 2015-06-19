/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_LegacyPackets.h
Content     :   Oculus performance capture protocol description.
Created     :   January, 2015
Notes       :   This file contains previous versions of packet structs, which is indended
                for tools to use via some template foo to parse old binary streams.

                The idea being anytime an existing packet struct needs to change, you copy
                it into here first and create a specialization of that old version.

                Then at the beginning of the network stream the host sends version information
                about every packet struct and their sizes, and the client builds a function pointer
                table for each packet type based on the version provided. And if no function
                is found it falls back to a default implementation that just skips it.

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_LEGACYPACKETS_H
#define OVR_CAPTURE_LEGACYPACKETS_H

#include <OVR_Capture_Config.h>
#include <OVR_Capture_Types.h>
#include <OVR_Capture_Packets.h>

namespace OVR
{
namespace Capture
{

// force to 4-byte alignment on all platforms... this *should* cause these structs to ignore -malign-double
// This might cause a slight load/store penalty on uint64/double, but it should be exceedingly minor in the
// grand scheme of things, and likely worth it for bandwidth reduction.
#pragma pack(4)

    // Just as an example... a version=0 ThreadNamePacket
    template<> struct ThreadNamePacket_<0>
    {
        static const PacketIdentifier s_packetID       = Packet_ThreadName;
        static const UInt32           s_version        = 0;
        static const bool             s_hasPayload     = false;
        static const UInt32           s_nameMaxLength  = 16;

        char name[s_nameMaxLength];
    };
    OVR_CAPTURE_STATIC_ASSERT(sizeof(ThreadNamePacket_<0>)==16);


// restore default alignment...
#pragma pack()

} // namespace Capture
} // namespace OVR

#endif
