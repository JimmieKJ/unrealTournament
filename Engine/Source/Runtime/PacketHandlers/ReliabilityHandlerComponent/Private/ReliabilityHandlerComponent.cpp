// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "ReliabilityHandlerComponent.h"

IMPLEMENT_MODULE(FReliabilityHandlerComponentModuleInterface, ReliabilityHandlerComponent);

// RELIABILITY
ReliabilityHandlerComponent::ReliabilityHandlerComponent()
: LocalPacketID(1)
, LocalPacketIDACKED(0)
, RemotePacketID(0)
, RemotePacketIDACKED(0)
, ResendResolutionTime(0.5f)
{
}

void ReliabilityHandlerComponent::Initialize()
{
	SetActive(true);
	Initialized();
	State = Handler::Component::State::Initialized;
}

bool ReliabilityHandlerComponent::IsValid() const
{
	return true;
}

void ReliabilityHandlerComponent::Outgoing(FBitWriter& Packet)
{
	switch (State)
	{
		case Handler::Component::State::Initialized:
		{
			FBitWriter Local;
			Local.AllowAppend(true);
			Local.SetAllowResize(true);

			if (IsActive() && IsValid())
			{
				bool PacketHasData = Packet.GetNumBytes() > 0;

				Local.Serialize(&PacketHasData, sizeof(bool));
				Local.SerializeIntPacked(RemotePacketID);

				// Save packet for reliability
				if (PacketHasData)
				{
					Local.SerializeIntPacked(LocalPacketID);
					Local.Serialize(Packet.GetData(), Packet.GetNumBytes());
					++LocalPacketID;

					BufferedPacket* ReliablePacket = new BufferedPacket;

					uint8* Data = new uint8[Packet.GetNumBytes()];
					memcpy(Data, Packet.GetData(), Packet.GetNumBytes());
					ReliablePacket->Data = Data;
					ReliablePacket->Id = LocalPacketID;
					ReliablePacket->CountBits = Packet.GetNumBits();
					ReliablePacket->ResendTime = Handler->Time + ResendResolutionTime;
					BufferedPackets.Enqueue(ReliablePacket);
				}

				Packet = Local;
				break;
			}
		}
		default:
		{
			break;
		}
	}
}

void ReliabilityHandlerComponent::Incoming(FBitReader& Packet)
{
	switch (State)
	{
		case Handler::Component::State::Initialized:
		{
			if (IsActive() && IsValid())
			{
				bool PacketHasData = false;
				Packet.Serialize(&PacketHasData, sizeof(bool));

				// ACK
				uint32 IncomingLocalPacketIDACK;
				Packet.SerializeIntPacked(IncomingLocalPacketIDACK);

				// Invalid ACK
				if (IncomingLocalPacketIDACK > LocalPacketID)
				{
					Packet.Seek(Packet.GetNumBytes());
					break;
				}

				// Set latest ACK
				LocalPacketIDACKED = IncomingLocalPacketIDACK;

				if (PacketHasData)
				{
					// Remote ID
					uint32 IncomingRemotePacketID;
					Packet.SerializeIntPacked(IncomingRemotePacketID);

					// Out of sequence or duplicate packet, seek to end
					// meaning no more bytes will be read from the packet
					if (RemotePacketID + 1 != IncomingRemotePacketID)
					{
						Packet.Seek(Packet.GetNumBytes());
						return;
					}

					// Set latest ID
					RemotePacketID = IncomingRemotePacketID;
				}

				// Remove header from reader
				FBitReader Copy(Packet.GetData() + (Packet.GetNumBytes() - Packet.GetBytesLeft()), Packet.GetBitsLeft());
				Packet = Copy;
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void ReliabilityHandlerComponent::Tick(float DeltaTime)
{
	float CurrentTime = Handler->Time;

	// Resend UNACKED packets
	while (BufferedPackets.IsEmpty() == false)
	{
		BufferedPacket* Packet;
		BufferedPackets.Peek(Packet);

		// Resend
		if (Packet->ResendTime > CurrentTime)
		{
			BufferedPackets.Dequeue(Packet);

			// Id has not been ACKED
			if (Packet->Id < LocalPacketIDACKED)
			{
				Handler->QueuePacketForSending(Packet);
			}
			// ID has been ACKED
			else
			{
				delete Packet;
			}
		}
		// No packets to resend this frame
		else
		{
			break;
		}
	}

	// Need to send ACK, queue a packet
	if (IsActive() && RemotePacketID > RemotePacketIDACKED)
	{
		Handler->QueuePacketForSending(new BufferedPacket);
	}
}

void ReliabilityHandlerComponent::QueuePacketForResending(uint8* Packet, int32 CountBits)
{
	BufferedPackets.Enqueue(new BufferedPacket(Packet, CountBits, Handler->Time + ResendResolutionTime, LocalPacketID));
}

// MODULE INTERFACE
TSharedPtr<HandlerComponent> FReliabilityHandlerComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new ReliabilityHandlerComponent);
}