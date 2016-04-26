// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PacketHandler.h"

#include "HandlerComponentFactory.generated.h"

/**
 * A UObject alternative for loading HandlerComponents without strict module dependency
 */
UCLASS()
class PACKETHANDLER_API UHandlerComponentFactory : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options)
		PURE_VIRTUAL(UHandlerComponentFactory::CreateComponentInstance, return nullptr;);
};
