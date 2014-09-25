// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPlayerInput.generated.h"

USTRUCT()
struct FCustomKeyBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName KeyName;
	UPROPERTY()
	TEnumAsByte<EInputEvent> EventType;
	UPROPERTY()
	FString Command;
};

UCLASS()
class UUTPlayerInput : public UPlayerInput
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Config)
	TArray<FCustomKeyBinding> CustomBinds;

	virtual bool ExecuteCustomBind(FKey Key, EInputEvent EventType);
	virtual void UTForceRebuildingKeyMaps(const bool bRestoreDefaults = false); 
};