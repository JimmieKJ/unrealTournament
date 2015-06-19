// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UdpMessagingPrivatePCH.h"
#include "JsonStructDeserializerBackend.h"
#include "StructDeserializer.h"


/* FUdpDeserializedMessage interface
 *****************************************************************************/

bool FUdpDeserializedMessage::Deserialize( const FUdpReassembledMessageRef& ReassembledMessage )
{
	// Note that some complex values are deserialized manually here, so that we
	// can sanity check their values. @see FUdpSerializeMessageTask::DoTask()

	FMemoryReader MessageReader(ReassembledMessage->GetData());
	MessageReader.ArMaxSerializeSize = NAME_SIZE;

	// message type info
	{
		FName MessageType;
		MessageReader << MessageType;

		// @todo gmp: cache message types for faster lookup
		TypeInfo = FindObjectSafe<UScriptStruct>(ANY_PACKAGE, *MessageType.ToString());

		if (!TypeInfo.IsValid(false, true))
		{
			return false;
		}
	}

	// sender address
	{
		MessageReader << Sender;
	}

	// recipient addresses
	{
		int32 NumRecipients = 0;
		MessageReader << NumRecipients;

		if ((NumRecipients < 0) || (NumRecipients > UDP_MESSAGING_MAX_RECIPIENTS))
		{
			return false;
		}

		Recipients.Empty(NumRecipients);

		while (0 < NumRecipients--)
		{
			MessageReader << *::new(Recipients) FMessageAddress;
		}
	}

	// message scope
	{
		MessageReader << Scope;

		if (static_cast<uint8>(Scope.GetValue()) > static_cast<uint8>(EMessageScope::All))
		{
			return false;
		}
	}

	// time sent & expiration
	{
		MessageReader << TimeSent;
		MessageReader << Expiration;
	}

	// annotations
	{
		int32 NumAnnotations = 0;
		MessageReader << NumAnnotations;

		if (NumAnnotations > UDP_MESSAGING_MAX_ANNOTATIONS)
		{
			return false;
		}

		while (0 < NumAnnotations--)
		{
			FName Key;
			FString Value;

			MessageReader << Key;
			MessageReader << Value;

			Annotations.Add(Key, Value);
		}
	}

	// create message body
	MessageData = FMemory::Malloc(TypeInfo->PropertiesSize);
	TypeInfo->InitializeStruct(MessageData);

	// deserialize message body
	FJsonStructDeserializerBackend Backend(MessageReader);

	return FStructDeserializer::Deserialize(MessageData, *TypeInfo, Backend);
}


/* IMessageContext interface
 *****************************************************************************/

const TMap<FName, FString>& FUdpDeserializedMessage::GetAnnotations() const
{
	return Annotations;
}


IMessageAttachmentPtr FUdpDeserializedMessage::GetAttachment() const
{
	return Attachment;
}


const FDateTime& FUdpDeserializedMessage::GetExpiration() const
{
	return Expiration;
}


const void* FUdpDeserializedMessage::GetMessage() const
{
	return MessageData;
}


const TWeakObjectPtr<UScriptStruct>& FUdpDeserializedMessage::GetMessageTypeInfo() const
{
	return TypeInfo;
}


IMessageContextPtr FUdpDeserializedMessage::GetOriginalContext() const
{
	return nullptr;
}


const TArray<FMessageAddress>& FUdpDeserializedMessage::GetRecipients() const
{
	return Recipients;
}


EMessageScope FUdpDeserializedMessage::GetScope() const
{
	return Scope;
}


const FMessageAddress& FUdpDeserializedMessage::GetSender() const
{
	return Sender;
}


ENamedThreads::Type FUdpDeserializedMessage::GetSenderThread() const
{
	return ENamedThreads::AnyThread;
}


const FDateTime& FUdpDeserializedMessage::GetTimeForwarded() const
{
	return TimeSent;
}


const FDateTime& FUdpDeserializedMessage::GetTimeSent() const
{
	return TimeSent;
}
