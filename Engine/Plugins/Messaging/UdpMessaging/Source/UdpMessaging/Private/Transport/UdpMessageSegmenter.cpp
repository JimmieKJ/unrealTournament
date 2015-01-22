// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UdpMessagingPrivatePCH.h"


/* FUdpMessageSegmenter structors
 *****************************************************************************/

FUdpMessageSegmenter::~FUdpMessageSegmenter()
{
	if (MessageReader != nullptr)
	{
		delete MessageReader;
	}
}


/* FUdpMessageSegmenter interface
 *****************************************************************************/

int64 FUdpMessageSegmenter::GetMessageSize() const
{
	if (MessageReader == nullptr)
	{
		return 0;
	}

	return MessageReader->TotalSize();
}


bool FUdpMessageSegmenter::GetNextPendingSegment(TArray<uint8>& OutData, uint16& OutSegment) const
{
	if (MessageReader == nullptr)
	{
		return false;
	}

	for (TConstSetBitIterator<> It(PendingSegments); It; ++It)
	{
		OutSegment = It.GetIndex();

		uint32 SegmentOffset = OutSegment * SegmentSize;
		int32 ActualSegmentSize = MessageReader->TotalSize() - SegmentOffset;

		if (ActualSegmentSize > SegmentSize)
		{
			ActualSegmentSize = SegmentSize;
		}

		OutData.Reset(ActualSegmentSize);
		OutData.AddUninitialized(ActualSegmentSize);

		MessageReader->Seek(SegmentOffset);
		MessageReader->Serialize(OutData.GetData(), ActualSegmentSize);

		//FMemory::Memcpy(OutData.GetTypedData(), Message->GetTypedData() + SegmentOffset, ActualSegmentSize);

		return true;
	}

	return false;
}


void FUdpMessageSegmenter::Initialize()
{
	if (MessageReader != nullptr)
	{
		return;
	}

	if (SerializedMessage->GetState() == EUdpSerializedMessageState::Complete)
	{
		MessageReader = SerializedMessage->CreateReader();
		PendingSegmentsCount = (MessageReader->TotalSize() + SegmentSize - 1) / SegmentSize;
		PendingSegments.Init(true, PendingSegmentsCount);
	}
}


void FUdpMessageSegmenter::MarkAsSent(uint16 Segment)
{
	if (Segment < PendingSegments.Num())
	{
		PendingSegments[Segment] = false;
		--PendingSegmentsCount;
	}
}


void FUdpMessageSegmenter::MarkForRetransmission(const TArray<uint16>& Segments)
{
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		uint16 Segment = Segments[Index];

		if (Segment < PendingSegments.Num())
		{
			PendingSegments[Segment] = true;
		}
	}
}
