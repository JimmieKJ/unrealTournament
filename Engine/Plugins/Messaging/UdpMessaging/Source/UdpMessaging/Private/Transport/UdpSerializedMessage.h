// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Enumerates possibly states of a serialized message.
 *
 * @see FUdpSerializedMessage
 */
enum class EUdpSerializedMessageState
{
	/** The message data is complete. */
	Complete,

	/** The message data is incomplete. */
	Incomplete,

	/** The message data is invalid. */
	Invalid
};


/**
 * Holds serialized message data.
 */
class FUdpSerializedMessage
	: public FMemoryWriter
{
public:

	/** Default constructor. */
	FUdpSerializedMessage()
		: FMemoryWriter(Data, true)
		, State(EUdpSerializedMessageState::Incomplete)
	{ }

public:

	/**
	 * Creates an archive reader to the data.
	 *
	 * The caller is responsible for deleting the returned object.
	 *
	 * @return An archive reader.
	 */
	FArchive* CreateReader()
	{
		return new FMemoryReader(Data, true);
	}

	/**
	 * Gets the state of the message data.
	 *
	 * @return Message data state.
	 * @see OnStateChanged
	 */
	EUdpSerializedMessageState GetState() const
	{
		return State;
	}

	/**
	 * Returns a delegate that is executed when the message data's state changed.
	 *
	 * @return The delegate.
	 * @see GetState
	 */
	FSimpleDelegate& OnStateChanged()
	{
		return StateChangedDelegate;
	}

	/**
	 * Updates the state of this message data.
	 *
	 * @param InState The state to set.
	 */
	void UpdateState( EUdpSerializedMessageState InState )
	{
		State = InState;
		StateChangedDelegate.ExecuteIfBound();
	}

private:

	/** Holds the data. */
	TArray<uint8> Data;

	/** Holds the message data state. */
	EUdpSerializedMessageState State;

private:

	/** Holds a delegate that is invoked when the data's state changed. */
	FSimpleDelegate StateChangedDelegate;
};


/** Type definition for shared pointers to instances of FUdpSerializedMessage. */
typedef TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> FUdpSerializedMessagePtr;

/** Type definition for shared references to instances of FUdpSerializedMessage. */
typedef TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe> FUdpSerializedMessageRef;
