// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
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
	LastHitTime = -100;
	LastHitMagnitude = 0.0f;
	bFlashing = false;
}

void UUTHUDWidget_WeaponCrosshair::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
		if (UTCharacter)
		{
			if (UTCharacter->GetWeapon() != NULL)
			{
				UTCharacter->GetWeapon()->DrawWeaponCrosshair(this, DeltaTime);
			}
			else
			{
				// fall back crosshair
				UTexture2D* CrosshairTexture = UTHUDOwner->DefaultCrosshairTex;
				if (CrosshairTexture != NULL)
				{
					float W = CrosshairTexture->GetSurfaceWidth();
					float H = CrosshairTexture->GetSurfaceHeight();
					float CrosshairScale = UTHUDOwner->GetCrosshairScale();

					DrawTexture(CrosshairTexture, 0, 0, W * CrosshairScale, H * CrosshairScale, 0.0, 0.0, 16, 16, 1.0, UTHUDOwner->GetCrosshairColor(FLinearColor::White), FVector2D(0.5f, 0.5f));
				}
			}
		}
		const float TimeSinceKill = GetWorld()->GetTimeSeconds() - UTHUDOwner->LastKillTime;
		const float SkullDisplayTime = 0.8f;
		if ((TimeSinceKill < SkullDisplayTime) && (UTHUDOwner->GetDrawHUDKillIconMsg()))
		{
			float Size = 32.f * (1.f + FMath::Min(1.5f*(TimeSinceKill - 0.2f) / SkullDisplayTime, 1.f));
			FLinearColor SkullColor = FLinearColor::White;
			float Opacity = 0.7f - 0.6f*TimeSinceKill/SkullDisplayTime;
			DrawTexture(UTHUDOwner->HUDAtlas, 0, -2.f*Size, Size, Size, 725, 0, 28, 36, Opacity, SkullColor, FVector2D(0.5f, 0.5f));
		}

		const float TimeSinceGrab = GetWorld()->GetTimeSeconds() - UTHUDOwner->LastFlagGrabTime;
		const float FlagDisplayTime = 1.f;
		if (TimeSinceGrab < FlagDisplayTime)
		{
			AUTPlayerState* PS = UTHUDOwner->GetScorerPlayerState();
			if (PS && PS->CarriedObject)
			{
				float FlagPct = FMath::Max(0.f, TimeSinceGrab - 0.5f*FlagDisplayTime) / FlagDisplayTime;
				float Size = 64.f * (1.f + 5.f*FlagPct);
				FLinearColor FlagColor = FLinearColor::White;
				int32 TeamIndex = PS->CarriedObject->GetTeamNum();
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS != NULL && GS->Teams.IsValidIndex(TeamIndex) && GS->Teams[TeamIndex] != NULL)
				{
					FlagColor = GS->Teams[TeamIndex]->TeamColor;
				}
				float Opacity = 0.8f - 0.8f*FlagPct;
				DrawTexture(UTHUDOwner->HUDAtlas, 0.f, -48.f, Size, Size, 843.f, 87.f, 43.f, 41.f, Opacity, FlagColor, FVector2D(0.5f, 0.5f));
			}
		}
	}

	if (UTHUDOwner)
	{
		// Display hit/kill indicators...		
		if (UTHUDOwner->LastConfirmedHitTime != LastHitTime)
		{
			// We have store it here since the amount of time and distance the indicator moves is dependent on the size of the hit and if there was a kill.
			LastHitTime = UTHUDOwner->LastConfirmedHitTime;
			LastHitMagnitude = FMath::Clamp<float>(float(UTHUDOwner->LastConfirmedHitDamage) / MAX_HIT_DAMAGE, 0.25f, 1.0f);
			bFlashing = true;
			FlashTime = 0.0f;
		}

		float TimeSinceLastKill = GetWorld()->GetTimeSeconds() - UTHUDOwner->LastConfirmedHitTime;
		if (bFlashing)
		{
			float Duration = FMath::Clamp<float>(FLASH_BLINK_TIME * LastHitMagnitude, 0.1, 1.0);
			float Perc = FlashTime / Duration;
			float Opacity = 1.0f - Perc;
			float BackOpacity = Opacity;

			float Height = 48.0f + (72.0f * LastHitMagnitude * Opacity) ;

			UE_LOG(UT,Log,TEXT("Here %f %f %f %f"), Height, LastHitMagnitude, Opacity, Duration);

			FVector2D DrawLocation;
			DrawLocation = CalcRotatedDrawLocation(32.0f, 45.0f);
			DrawTexture(UTHUDOwner->HUDAtlas, DrawLocation.X, DrawLocation.Y, 16.0f, Height, 2.0f, 679.0f, 32.0f, 72.0f, Opacity, FLinearColor::White, FVector2D(0.5f, 1.0f), 45.0f, FVector2D(0.5f, 1.0f));

			DrawLocation = CalcRotatedDrawLocation(32.0f, 135.0f);
			DrawTexture(UTHUDOwner->HUDAtlas, DrawLocation.X, DrawLocation.Y, 16.0f, Height, 2.0f, 679.0f, 32.0f, 72.0f, Opacity, FLinearColor::White, FVector2D(0.5f, 1.0f), 135.0f, FVector2D(0.5f, 1.0f));

			DrawLocation = CalcRotatedDrawLocation(32.0f, 225.0f);
			DrawTexture(UTHUDOwner->HUDAtlas, DrawLocation.X, DrawLocation.Y, 16.0f, Height, 2.0f, 679.0f, 32.0f, 72.0f, Opacity, FLinearColor::White, FVector2D(0.5f, 1.0f), 225.0f, FVector2D(0.5f, 1.0f));

			DrawLocation = CalcRotatedDrawLocation(32.0f, 315.0f);
			DrawTexture(UTHUDOwner->HUDAtlas, DrawLocation.X, DrawLocation.Y, 16.0f, Height, 2.0f, 679.0f, 32.0f, 72.0f, Opacity, FLinearColor::White, FVector2D(0.5f, 1.0f), 315.0f, FVector2D(0.5f, 1.0f));

			FlashTime += DeltaTime;
			if (FlashTime >= Duration)
			{
				FlashTime = 0;
				bFlashing = false;
			}
		}
	}
}


