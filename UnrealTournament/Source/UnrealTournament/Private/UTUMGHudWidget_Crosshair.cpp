// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTUMGHudWidget_Crosshair.h"

UUTUMGHudWidget_Crosshair::UUTUMGHudWidget_Crosshair(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayZOrder = 0.50f;
}

void UUTUMGHudWidget_Crosshair::ApplyCustomizations_Implementation(const FWeaponCustomizationInfo& CustomizationsToApply)
{
	Customizations = CustomizationsToApply;
}

