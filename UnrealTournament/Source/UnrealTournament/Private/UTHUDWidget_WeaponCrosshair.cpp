// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCrosshair.h"
#include "UTHUDWidget_WeaponCrosshair.h"

const float MAX_HIT_INDICATOR_TIME = 1.5f;
const float MAX_HIT_MOVEMENT = 100.0f;
const float MAX_HIT_DAMAGE = 125.0f;
const float HIT_STRETCH_TIME=0.15f;
const float FLASH_BLINK_TIME=0.5;

UUTHUDWidget_WeaponCrosshair::UUTHUDWidget_WeaponCrosshair(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.5f, 0.5f);
	LastWeapon = nullptr;
}

bool UUTHUDWidget_WeaponCrosshair::ShouldDraw_Implementation(bool bShowScores)
{
	if (Super::ShouldDraw_Implementation(bShowScores))
	{
		// skip drawing if spectating in third person
		if (UTHUDOwner && UTPlayerOwner && (UTHUDOwner->UTPlayerOwner->GetViewTarget() != UTHUDOwner->UTPlayerOwner->GetPawn()) && UTHUDOwner->UTPlayerOwner->IsBehindView())
		{
			return false;
		}
		return true;
	}
	return false;
}

void UUTHUDWidget_WeaponCrosshair::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		bool bDrawDefaultCrosshair = true;
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
		if (UTCharacter != nullptr && UTCharacter->GetWeapon() != nullptr)
		{
			AUTWeapon* Weapon = UTCharacter->GetWeapon();
			if (Weapon != LastWeapon)
			{
				FWeaponCustomizationInfo CrosshairCustomizationInfo;
				if (LastWeapon != nullptr)
				{
					UUTCrosshair* LastCrosshair = UTHUDOwner->GetCrosshairForWeapon(LastWeapon->WeaponCustomizationTag, CrosshairCustomizationInfo);
					if (LastCrosshair != nullptr)
					{
						LastCrosshair->DeactivateCrosshair(UTHUDOwner);
					}
				}

				if (Weapon != nullptr)
				{
					UUTCrosshair* NewCrosshair = UTHUDOwner->GetCrosshairForWeapon(Weapon->WeaponCustomizationTag, CrosshairCustomizationInfo);
					if (NewCrosshair != nullptr)
					{
						NewCrosshair->ActivateCrosshair(UTHUDOwner, CrosshairCustomizationInfo, Weapon);
					}
				}

				LastWeapon = Weapon;
			}

			UTCharacter->GetWeapon()->DrawWeaponCrosshair(this, DeltaTime);
		}
		UTHUDOwner->DrawKillSkulls();

		const float TimeSinceGrab = GetWorld()->GetTimeSeconds() - UTHUDOwner->LastFlagGrabTime;
		const float FlagDisplayTime = 3.f;
		if (TimeSinceGrab < FlagDisplayTime)
		{
			AUTPlayerState* PS = UTHUDOwner->GetScorerPlayerState();
			if (PS && PS->CarriedObject)
			{
				float FlagPct = FMath::Max(0.f, TimeSinceGrab - 0.75f*FlagDisplayTime) / FlagDisplayTime;
				float DrawSize = 64.f * (1.f + 5.f*FlagPct);
				FLinearColor FlagColor = FLinearColor::White;
				int32 TeamIndex = PS->CarriedObject->GetTeamNum();
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS != NULL && GS->Teams.IsValidIndex(TeamIndex) && GS->Teams[TeamIndex] != NULL)
				{
					FlagColor = GS->Teams[TeamIndex]->TeamColor;
				}
				float DrawOpacity = 0.8f - 0.8f*FlagPct;
				DrawTexture(UTHUDOwner->HUDAtlas, 0.f, -40.f, DrawSize, DrawSize, 843.f, 87.f, 43.f, 41.f, DrawOpacity, FlagColor, FVector2D(0.5f, 0.5f));
			}
		}
	}

	if (UTHUDOwner && UTHUDOwner->UTPlayerOwner && (UTHUDOwner->UTPlayerOwner->GetProfileSettings() == nullptr || !UTHUDOwner->UTPlayerOwner->GetProfileSettings()->bHideDamageIndicators) )
	{
		// Display hit/kill indicators...		
		float LastHitMagnitude = FMath::Clamp<float>(float(UTHUDOwner->LastConfirmedHitDamage) / MAX_HIT_DAMAGE, 0.0f, 1.0f);
		float Duration = FMath::Clamp<float>(FLASH_BLINK_TIME * LastHitMagnitude, 0.2f, 1.f);
		float FlashTime = GetWorld()->GetTimeSeconds() - UTHUDOwner->LastConfirmedHitTime;
		if (FlashTime < Duration)
		{
			float DrawOpacity = 1.0f - FlashTime / Duration;
			float Height = 8.0f + (150.0f * LastHitMagnitude * DrawOpacity) ;

			for (int32 i = 0; i < 4; i++)
			{
				float RotAngle = 45.0f + 90.f*i;
				FVector2D DrawLocation = CalcRotatedDrawLocation(32.0f, RotAngle);
				DrawTexture(UTHUDOwner->HUDAtlas, DrawLocation.X, DrawLocation.Y, 8.0f, Height, 2.0f, 679.0f, 32.0f, 72.0f, DrawOpacity, FLinearColor::White, FVector2D(0.5f, 1.0f), RotAngle, FVector2D(0.5f, 1.0f));
			}
		}
	}
}
