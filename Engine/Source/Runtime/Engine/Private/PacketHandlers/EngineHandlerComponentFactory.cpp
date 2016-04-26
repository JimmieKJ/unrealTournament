// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#include "PacketHandlers/EngineHandlerComponentFactory.h"

/**
 * UEngineHandlerComponentFactor
 */
UEngineHandlerComponentFactory::UEngineHandlerComponentFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<HandlerComponent> UEngineHandlerComponentFactory::CreateComponentInstance(FString& Options)
{
	if (Options == TEXT("StatelessConnectHandlerComponent"))
	{
		return MakeShareable(new StatelessConnectHandlerComponent);
	}

	return nullptr;
}
