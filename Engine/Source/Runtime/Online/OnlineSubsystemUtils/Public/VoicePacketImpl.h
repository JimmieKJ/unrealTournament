// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VoiceDataCommon.h"
#include "OnlineSubsystemUtilsPackage.h"

class FUniqueNetId;

#define DEBUG_VOICE_PACKET_ENCODING 0

/** Defines the data involved in a voice packet */
class FVoicePacketImpl : public FVoicePacket
{

PACKAGE_SCOPE:

	/** The unique net id of the talker sending the data */
	TSharedPtr<const FUniqueNetId> Sender;
	/** The data that is to be sent/processed */
	TArray<uint8> Buffer;
	/** The current amount of space used in the buffer for this packet */
	uint16 Length;

public:
	/** Zeros members and validates the assumptions */
	FVoicePacketImpl() :
		Sender(NULL),
		Length(0)
	{
		Buffer.Empty(MAX_VOICE_DATA_SIZE);
		Buffer.AddUninitialized(MAX_VOICE_DATA_SIZE);
	}

	/** Should only be used by TSharedPtr and FVoiceData */
	virtual ~FVoicePacketImpl()
	{
	}

	/**
	 * Copies another packet and inits the ref count
	 *
	 * @param Other packet to copy
	 * @param InRefCount the starting ref count to use
	 */
	FVoicePacketImpl(const FVoicePacketImpl& Other);

	/** Returns the amount of space this packet will consume in a buffer */
	virtual uint16 GetTotalPacketSize() override;

	/** @return the amount of space used by the internal voice buffer */
	virtual uint16 GetBufferSize() override;

	/** @return the sender of this voice packet */
	virtual TSharedPtr<const FUniqueNetId> GetSender() override;

	virtual bool IsReliable() override { return false; }

	/** 
	 * Serialize the voice packet data to a buffer 
	 *
	 * @param Ar buffer to write into
	 */
	virtual void Serialize(class FArchive& Ar) override;
};

/** Holds the current voice packet data state */
struct FVoiceDataImpl
{
	/** Data used by the local talkers before sent */
	FVoicePacketImpl LocalPackets[MAX_SPLITSCREEN_TALKERS];
	/** Holds the set of received packets that need to be processed */
	FVoicePacketList RemotePackets;

	FVoiceDataImpl() {}
	~FVoiceDataImpl() {}
};
