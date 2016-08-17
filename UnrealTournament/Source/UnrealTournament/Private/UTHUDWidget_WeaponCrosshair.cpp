// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCrosshair.h"
#include "UTHUDWidget_WeaponCrosshair.h"

const float MAX_HIT_INDICATOR_TIME = 1.5f;
const float MAX_HIT_MOVEMENT = 100.0f;
const float MAX_HIT_DAMAGE = 150.0f;
const float HIT_STRETCH_TIME=0.15f;
const float FLASH_BLINK_TIME=0.5;

UUTHUDWidget_WeaponCrosshair::UUTHUDWidget_WeaponCrosshair(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.5f, 0.5f);
}

void UUTHUDWidget_WeaponCrosshair::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		bool bDrawDefaultCrosshair = true;
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
		if (UTCharacter != nullptr && UTCharacter->GetWeapon() != nullptr)
		{
			UTCharacter->GetWeapon()->DrawWeaponCrosshair(this, DeltaTime);
		}

		float TimeSinceKill = GetWorld()->GetTimeSeconds() - UTHUDOwner->LastKillTime;
		float SkullDisplayTime = (UTHUDOwner->LastMultiKillCount > 1) ? 1.1f : 0.8f;
		if ((TimeSinceKill < SkullDisplayTime) && (UTHUDOwner->GetDrawHUDKillIconMsg()))
		{
			float SkullSmallTime = (UTHUDOwner->LastMultiKillCount > 1) ? 0.5f : 0.2f;
			float Size = 32.f * (1.f + FMath::Clamp(1.5f*(TimeSinceKill - SkullSmallTime) / SkullDisplayTime, 0.f, 1.f));
			FLinearColor SkullColor = FLinearColor::White;
			float Opacity = 0.7f - 0.6f*(TimeSinceKill - SkullSmallTime)/(SkullDisplayTime - SkullSmallTime);
			int32 NumSkulls = (UTHUDOwner->LastMultiKillCount > 1) ? UTHUDOwner->LastMultiKillCount : 1;
			float StartPos = -0.5f * Size * NumSkulls + 0.5f*Size;
			for (int32 i = 0; i < NumSkulls; i++)
			{
				DrawTexture(UTHUDOwner->HUDAtlas, StartPos, -2.f*Size, Size, Size, 725, 0, 28, 36, Opacity, SkullColor, FVector2D(0.5f, 0.5f));
				StartPos += 1.1f * Size;
			}
		}
		else
		{
			UTHUDOwner->LastMultiKillCount = 0;
		}

		const float TimeSinceGrab = GetWorld()->GetTimeSeconds() - UTHUDOwner->LastFlagGrabTime;
		const float FlagDisplayTime = 2.f;
		if (TimeSinceGrab < FlagDisplayTime)
		{
			AUTPlayerState* PS = UTHUDOwner->GetScorerPlayerState();
			if (PS && PS->CarriedObject)
			{
				float FlagPct = FMath::Max(0.f, TimeSinceGrab - 0.75f*FlagDisplayTime) / FlagDisplayTime;
				float Size = 64.f * (1.f + 5.f*FlagPct);
				FLinearColor FlagColor = FLinearColor::White;
				int32 TeamIndex = PS->CarriedObject->GetTeamNum();
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS != NULL && GS->Teams.IsValidIndex(TeamIndex) && GS->Teams[TeamIndex] != NULL)
				{
					FlagColor = GS->Teams[TeamIndex]->TeamColor;
				}
				float Opacity = 0.8f - 0.8f*FlagPct;
				DrawTexture(UTHUDOwner->HUDAtlas, 0.f, -40.f, Size, Size, 843.f, 87.f, 43.f, 41.f, Opacity, FlagColor, FVector2D(0.5f, 0.5f));
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
			float Opacity = 1.0f - FlashTime / Duration;
			float Height = 16.0f + (128.0f * LastHitMagnitude * Opacity) ;

			for (int32 i = 0; i < 4; i++)
			{
				float RotAngle = 45.0f + 90.f*i;
				FVector2D DrawLocation = CalcRotatedDrawLocation(32.0f, RotAngle);
				DrawTexture(UTHUDOwner->HUDAtlas, DrawLocation.X, DrawLocation.Y, 8.0f, Height, 2.0f, 679.0f, 32.0f, 72.0f, Opacity, FLinearColor::White, FVector2D(0.5f, 1.0f), RotAngle, FVector2D(0.5f, 1.0f));
			}
		}
	}
}
