// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacterContent.h"

#include "UTDmg_SniperHeadshot.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmg_SniperHeadshot : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmg_SniperHeadshot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		GibHealthThreshold = -10000000;
		GibDamageThreshold = 1000000;
	}

	virtual void PlayDeathEffects_Implementation(AUTCharacter* DyingPawn) const override
	{
		FGibSlotInfo GibInfo;
		if (DyingPawn->CharacterData != NULL)
		{
			for (const FGibSlotInfo& DefaultGib : DyingPawn->CharacterData.GetDefaultObject()->Gibs)
			{
				if (GibInfo.GibType == NULL)
				{
					// default if head bone not found
					GibInfo = DefaultGib;
				}
				else if (DefaultGib.BoneName == DyingPawn->HeadBone)
				{
					GibInfo = DefaultGib;
					break;
				}
			}
		}
		if (GibInfo.GibType != NULL)
		{
			DyingPawn->SpawnGib(GibInfo);
		}
		DyingPawn->SetHeadScale(0.0f);
	}
};