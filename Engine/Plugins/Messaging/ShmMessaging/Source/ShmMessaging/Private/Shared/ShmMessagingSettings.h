// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShmMessagingSettings.generated.h"


UCLASS(config=Engine)
class UShmMessagingSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Whether the UDP transport channel is enabled. */
	UPROPERTY(config, EditAnywhere, Category=Transport)
	bool EnableTransport;
};
