// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a re-sequencer for messages received over the UDP transport.
 */
class FUdpMessageResequencer
{
public:

	/** Default constructor. */
	FUdpMessageResequencer() { }

	/**
	 * Creates and initializes a new message resequencer.
	 */
	FUdpMessageResequencer(uint16 InWindowSize)
		: NextSequence(1)
		, WindowSize(InWindowSize)
	{ }

public:

	/**
	 * Gets the next expected sequence number.
	 *
	 * @return Next sequence.
	 */
	uint64 GetNextSequence() const
	{
		return NextSequence;
	}

	/**
	 * Extracts the next available message in the sequence.
	 *
	 * @return true if a message was returned, false otherwise.
	 */
	bool Pop(FUdpReassembledMessagePtr& OutMessage)
	{
		if (MessageHeap.HeapTop()->GetSequence() == NextSequence)
		{
			MessageHeap.HeapPop(OutMessage, FSequenceComparer());

			NextSequence++;

			return true;
		}

		return false;
	}

	/**
	 * Resequences the specified message.
	 *
	 * @param Message The message to resequence.
	 * @return true if the message is in sequence, false otherwise.
	 */
	bool Resequence(const FUdpReassembledMessagePtr& Message)
	{
		MessageHeap.HeapPush(Message, FSequenceComparer());

		return (Message->GetSequence() == NextSequence);
	}

	/** Resets the re-sequencer. */
	void Reset()
	{
		MessageHeap.Reset();

		NextSequence = 1;
	}

private:

	/** Helper for ordering messages by their sequence numbers. */
	struct FSequenceComparer
	{
		bool operator()(const FUdpReassembledMessagePtr& A, const FUdpReassembledMessagePtr& B) const
		{
			return A->GetSequence() < B->GetSequence();
		}
	};

private:

	/** Holds the next expected sequence number. */
	uint64 NextSequence;

	/** Holds the highest received sequence number. */
	uint64 HighestReceivedSequence;

	/** Holds the messages that need to be resequenced. */
	TArray<FUdpReassembledMessagePtr> MessageHeap;

	/** Holds the maximum resequence window size. */
	uint16 WindowSize;
};
