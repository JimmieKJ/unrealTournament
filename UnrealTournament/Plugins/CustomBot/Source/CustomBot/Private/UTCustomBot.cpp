// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomBotPrivatePCH.h"
#include "UTBotConfig.h"
#include "UTCustomPathFollowingComponent.h"
#include "UTCustomBot.h"

AUTCustomBot::AUTCustomBot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTCustomPathFollowingComponent>(TEXT("PathFollowingComponent")))
{

}

void AUTCustomBot::Tick(float DeltaTime)
{
	APawn* MyPawn = GetPawn();
	if (MyPawn == nullptr)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS == nullptr || PS->RespawnTime <= 0.0f)
			{
				World->GetAuthGameMode()->RestartPlayer(this);
			}
			MyPawn = GetPawn();
		}
	}

	// skip UTBot'a tick - it contains all the regular logic we want to override
	AAIController::Tick(DeltaTime);
}

void AUTCustomBot::NotifyPickup(APawn* PickedUpBy, AActor* Pickup, float AudibleRadius)
{

}

