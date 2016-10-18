// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintRuntimeSettings.generated.h"

UENUM()
enum class EBlueprintWarningBehavior : uint8
{
	Warn,
	Error,
	Suppress,
};

USTRUCT()
struct FBlueprintWarningSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName WarningIdentifier;

	UPROPERTY()
	FText WarningDescription;

	UPROPERTY()
	EBlueprintWarningBehavior WarningBehavior;
};

/**
* Implements the settings for the BlueprintRuntime module
*/
UCLASS(config = Engine, defaultconfig)
class BLUEPRINTRUNTIME_API UBlueprintRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, config, Category = Warnings)
	TArray<FBlueprintWarningSettings> WarningSettings;
};
