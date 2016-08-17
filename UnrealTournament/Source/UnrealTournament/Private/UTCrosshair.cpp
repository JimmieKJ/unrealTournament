// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCrosshair.h"

UUTCrosshair::UUTCrosshair(const class FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	CrosshairName = NSLOCTEXT("UUTCrosshair", "NoName", "No Name");
	OffsetAdjust = FVector2D(0.f, 0.f);
}

void UUTCrosshair::ActivateCrosshair_Implementation(AUTHUD* TargetHUD, const FWeaponCustomizationInfo& CustomizationsToApply)
{
	if (!UMGClassname.IsEmpty() && TargetHUD != nullptr)
	{
		if (!ActiveUMG.IsValid())			
		{
			ActiveUMG = Cast<UUTUMGHudWidget_Crosshair>(TargetHUD->ActivateUMGHudWidget(UMGClassname).Get());
		}
		else
		{
			TargetHUD->ActivateActualUMGHudWidget(ActiveUMG);
		}

		if (ActiveUMG.IsValid())
		{
			ActiveUMG->ApplyCustomizations(CustomizationsToApply);
		}
	}
}

void UUTCrosshair::DeactivateCrosshair_Implementation(AUTHUD* TargetHUD)
{
	if (ActiveUMG.IsValid())
	{
		TargetHUD->DeactivateActualUMGHudWidget(ActiveUMG);
	}
}

void UUTCrosshair::NativeDrawCrosshair(AUTHUD* TargetHUD, UCanvas* Canvas, AUTWeapon* Weapon, float DeltaTime, const FWeaponCustomizationInfo& CustomizationsToApply)
{
	if (!ActiveUMG.IsValid())
	{
		DrawCrosshair(TargetHUD, Canvas, Weapon, DeltaTime, CustomizationsToApply);
	}
}

void UUTCrosshair::DrawCrosshair_Implementation(AUTHUD* TargetHUD, UCanvas* Canvas, AUTWeapon* Weapon, float DeltaTime, const FWeaponCustomizationInfo& CustomizationsToApply)
{
	float HUDCrosshairScale = (TargetHUD == nullptr ? 1.0f : TargetHUD->GetCrosshairScale());

	float X = (Canvas->SizeX * 0.5f) - (CrosshairIcon.UL * CustomizationsToApply.CrosshairScaleOverride * HUDCrosshairScale * 0.5f) - 1.f + OffsetAdjust.X;
	float Y = (Canvas->SizeY * 0.5f) - (CrosshairIcon.VL * CustomizationsToApply.CrosshairScaleOverride * HUDCrosshairScale * 0.5f) - 1.f + OffsetAdjust.Y;

	Canvas->DrawColor = CustomizationsToApply.CrosshairColorOverride.ToFColor(false);
	Canvas->DrawIcon(CrosshairIcon, X, Y, CustomizationsToApply.CrosshairScaleOverride * HUDCrosshairScale);
}

