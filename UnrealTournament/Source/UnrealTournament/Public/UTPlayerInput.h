// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPlayerInput.generated.h"


UCLASS()
class UNREALTOURNAMENT_API UUTPlayerInput : public UPlayerInput
{
	GENERATED_BODY()

public:
	UUTPlayerInput();

	virtual void PostInitProperties() override;

	UPROPERTY(Config)
	TArray<FCustomKeyBinding> CustomBinds;

	UPROPERTY(Config)
	TArray<FCustomKeyBinding> LocalBinds;

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