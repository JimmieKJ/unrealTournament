// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDmg_SniperHeadshot.generated.h"

UCLASS(CustomConstructor)
class UUTDmg_SniperHeadshot : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	TSubclassOf<ULocalMessage> MessageClass;

	UUTDmg_SniperHeadshot(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		static ConstructorHelpers::FObjectFinder<UClass> MessageContentClass(TEXT("Class'/Game/RestrictedAssets/Blueprints/HeadShotMessage.HeadShotMessage_C'"));

		MessageClass = MessageContentClass.Object;
	}

	virtual void ScoreKill_Implementation(AUTPlayerState* KillerState, AUTPlayerState* VictimState, APawn* KilledPawn) const
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(KillerState->GetOwner());
		if (PC != NULL)
		{
			PC->ClientReceiveLocalizedMessage(MessageClass);
		}
	}
};