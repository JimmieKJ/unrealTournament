// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBot.h"
#include "UTCustomBot.generated.h"

UCLASS()
class CUSTOMBOT_API AUTCustomBot : public AUTBot
{
	GENERATED_BODY()

public:
	AUTCustomBot(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

	virtual void NotifyPickup(APawn* PickedUpBy, AActor* Pickup, float AudibleRadius) override;
};