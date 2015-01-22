// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LIVEEDITORLISTENSERVER_DEFAULT_PORT 4159

namespace nLiveEditorListenServer
{
	enum ELiveEditorMessageType
	{
		CLASSDEFAULT_OBJECT_PROPERTY
	};

	struct FClassDefaultPropertyPayload
	{
		FString ClassName;
		FString PropertyName;
		FString PropertyValue;

		FORCEINLINE friend FArchive& operator<<(FArchive& Ar,FClassDefaultPropertyPayload& Payload)
		{
			return Ar << Payload.ClassName << Payload.PropertyName << Payload.PropertyValue;
		}
	};

	struct FNetworkMessage
	{
		TEnumAsByte<ELiveEditorMessageType> Type;
		FClassDefaultPropertyPayload Payload;

		FORCEINLINE friend FArchive& operator<<(FArchive& Ar,FNetworkMessage& NetworkMessage)
		{
			return Ar << NetworkMessage.Type << NetworkMessage.Payload;
		}
	};
}
