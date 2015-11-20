// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemSteamPrivatePCH.h"
#include "VoicePacketSteam.h"
#include "OnlineSubsystemSteamTypes.h"

/**
 * Copies another packet and inits the ref count
 *
 * @param InRefCount the starting ref count to use
 */
FVoicePacketSteam::FVoicePacketSteam(const FVoicePacketSteam& Other) :
	FVoicePacket(Other)
{
	Sender = Other.Sender;
	Length = Other.Length;

	// Copy the contents of the voice packet
	Buffer.Empty(Other.Length);
	Buffer.AddUninitialized(Other.Length);
	FMemory::Memcpy(Buffer.GetData(), Other.Buffer.GetData(), Other.Length);
}

/** Returns the amount of space this packet will consume in a buffer */
uint16 FVoicePacketSteam::GetTotalPacketSize()
{
	return Sender->GetSize() + sizeof(Length) + Length;
}

/** @return the amount of space used by the internal voice buffer */
uint16 FVoicePacketSteam::GetBufferSize()
{
	return Length;
}

/** @return the sender of this voice packet */
TSharedPtr<const FUniqueNetId> FVoicePacketSteam::GetSender()
{
	return Sender;
}

/** 
 * Serialize the voice packet data to a buffer 
 *
 * @param Ar buffer to write into
 */
void FVoicePacketSteam::Serialize(class FArchive& Ar)
{
	// Make sure not to overflow the buffer by reading an invalid amount
	if (Ar.IsLoading())
	{
		uint64 SenderUID;
		Ar << SenderUID;
		Sender = MakeShareable(new FUniqueNetIdSteam(SenderUID));
		Ar << Length;
		// Verify the packet is a valid size
		if (Length <= MAX_VOICE_DATA_SIZE)
		{
			Buffer.Empty(Length);
			Buffer.AddUninitialized(Length);
			Ar.Serialize(Buffer.GetData(), Length);
		}
		else
		{
			Length = 0;
		}
	}
	else
	{
		Ar << *(uint64*)Sender->GetBytes();
		Ar << Length;

		// Always safe to save the data as the voice code prevents overwrites
		Ar.Serialize(Buffer.GetData(), Length);
	}
}



