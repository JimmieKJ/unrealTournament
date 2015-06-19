// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPlayerInput.generated.h"

USTRUCT()
struct FCustomKeyBinding
{
	GENERATED_USTRUCT_BODY()

	FCustomKeyBinding() : KeyName(FName(TEXT(""))), EventType(IE_Pressed), Command(FString("")) {};

	FCustomKeyBinding(FName InKeyName, TEnumAsByte<EInputEvent> InEventType, FString InCommand) : KeyName(InKeyName), EventType(InEventType), Command(InCommand) {};

	UPROPERTY()
	FName KeyName;
	UPROPERTY()
	TEnumAsByte<EInputEvent> EventType;
	UPROPERTY()
	FString Command;
	UPROPERTY()
		FString FriendlyName;
};

UCLASS()
class UNREALTOURNAMENT_API UUTPlayerInput : public UPlayerInput
{
	GENERATED_BODY()

public:
	UUTPlayerInput();

	UPROPERTY(Config)
	TArray<FCustomKeyBinding> CustomBinds;

	/** Binds only valid for spectators */
	UPROPERTY(Config)
	TArray<FCustomKeyBinding> SpectatorBinds;

	UPROPERTY(Config)
	float AccelerationPower;

	UPROPERTY(Config)
	float Acceleration;

	UPROPERTY(Config)
	float AccelerationOffset;

	UPROPERTY(Config)
	float AccelerationMax;
	
	virtual bool ExecuteCustomBind(FKey Key, EInputEvent EventType);
	virtual void UTForceRebuildingKeyMaps(const bool bRestoreDefaults = false); 
};