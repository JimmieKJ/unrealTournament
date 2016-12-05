// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTArsenalBoost.h"
#include "UTCTFGameState.h"

#include "UTRecallBoost.generated.h"

UCLASS()
class AUTRecallBoost : public AUTArsenalBoost
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AUTReplicatedEmitter> DestinationEffect;

	virtual bool HandleGivenTo_Implementation(AUTCharacter* NewOwner) override
	{
		Super::HandleGivenTo_Implementation(NewOwner);

		AUTCTFGameState* GS = NewOwner->GetWorld()->GetGameState<AUTCTFGameState>();
		if (GS != nullptr)
		{
			AUTCTFFlagBase* Base = GS->GetFlagBase(NewOwner->GetTeamNum());
			if (Base != nullptr && NewOwner->TeleportTo(Base->GetActorLocation() + FVector(0.0f, 0.0f, NewOwner->GetSimpleCollisionHalfHeight()), Base->GetActorRotation()))
			{
				if (DestinationEffect != NULL)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Instigator = NewOwner;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.Owner = NewOwner;
					NewOwner->GetWorld()->SpawnActor<AUTReplicatedEmitter>(DestinationEffect, NewOwner->GetActorLocation(), NewOwner->GetActorRotation(), SpawnParams);
				}
			}
		}

		return true;
	}
};