// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Holds a deserialized message.
 */
class FUdpDeserializedMessage
	: public IMessageContext
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InAttachment An optional message attachment.
	 */
	FUdpDeserializedMessage( const IMessageAttachmentPtr& InAttachment )
		: Attachment(InAttachment)
	{ }

public:

	/**
	 * Deserializes the given reassembled message.
	 *
	 * @param ReassembledMessage The reassembled message to deserialize.
	 * @return true on success, false otherwise.
	 */
	bool Deserialize( const FUdpReassembledMessageRef& ReassembledMessage );

public:

	// IMessageContext interface

	virtual const TMap<FName, FString>& GetAnnotations() const override;
	virtual IMessageAttachmentPtr GetAttachment() const override;
	virtual const FDateTime& GetExpiration() const override;
	virtual const void* GetMessage() const override;
	virtual const TWeakObjectPtr<UScriptStruct>& GetMessageTypeInfo() const override;
	virtual IMessageContextPtr GetOriginalContext() const override;
	virtual const TArray<FMessageAddress>& GetRecipients() const override;
	virtual EMessageScope GetScope() const override;
	virtual const FMessageAddress& GetSender() const override;
	virtual ENamedThreads::Type GetSenderThread() const override;
	virtual const FDateTime& GetTimeForwarded() const override;
	virtual const FDateTime& GetTimeSent() const override;

private:

	/** Holds the optional message annotations. */
	TMap<FName, FString> Annotations;

	/** Holds a pointer to attached binary data. */
	IMessageAttachmentPtr Attachment;

	/** Holds the expiration time. */
	FDateTime Expiration;

	/** Holds the message. */
	void* MessageData;

	/** Holds the message recipients. */
	TArray<FMessageAddress> Recipients;

	/** Holds the message's scope. */
	TEnumAsByte<EMessageScope> Scope;

	/** Holds the sender's identifier. */
	FMessageAddress Sender;

	/** Holds the time at which the message was sent. */
	FDateTime TimeSent;

	/** Holds the message's type information. */
	TWeakObjectPtr<UScriptStruct> TypeInfo;
};


/** Type definition for shared pointers to instances of FUdpDeserializedMessage. */
typedef TSharedPtr<FUdpDeserializedMessage, ESPMode::ThreadSafe> FUdpDeserializedMessagePtr;

/** Type definition for shared references to instances of FUdpDeserializedMessage. */
typedef TSharedRef<FUdpDeserializedMessage, ESPMode::ThreadSafe> FUdpDeserializedMessageRef;
