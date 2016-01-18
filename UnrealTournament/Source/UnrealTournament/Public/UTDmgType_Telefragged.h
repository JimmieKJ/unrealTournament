// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_Telefragged.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_Telefragged : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_Telefragged(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bCausedByWorld = true;
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_Telefragged", "{Player2Name}'s atoms were scattered by {Player1Name}.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_Telefragged", "{Player2Name} telefragged himself.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_Telefragged", "{Player2Name} telefragged herself.");
		GibHealthThreshold = 10000000;
		StatsName = "Telefrag";

		static ConstructorHelpers::FObjectFinder<UTexture> WeaponTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseB.UI_HUD_BaseB'"));
		HUDIcon.Texture = WeaponTexture.Object;

		HUDIcon.U = 599.f;
		HUDIcon.V = 462.f;
		HUDIcon.UL = 121.f;
		HUDIcon.VL = 50.f;
	}
};