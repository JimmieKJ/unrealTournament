// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPlaceablePowerup.h"
#include "UTDroppedPickup.h"
#include "UTHUDWidget_Powerups.h"
#include "UnrealNetwork.h"

AUTPlaceablePowerup::AUTPlaceablePowerup(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bDestroySpawnedPowerups = false;
	bDestroyWhenOutOfBoosts = true;
}

void AUTPlaceablePowerup::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	SpawnPowerup();
}

void AUTPlaceablePowerup::Destroyed()
{
	if (bDestroySpawnedPowerups)
	{
		for (int index = 0; index < SpawnedPowerups.Num(); ++index)
		{
			if (SpawnedPowerups[index].IsValid())
			{
				SpawnedPowerups[index]->Destroy();
			}
		}
	}

	Super::Destroyed();
}

void AUTPlaceablePowerup::DrawInventoryHUD_Implementation(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size)
{
	//Only overwrite paused behavior
	if (!bTimerPaused)
	{
		Super::DrawInventoryHUD_Implementation(Widget, Pos, Size);
	}
	else
	{
		//Display icon (but no timer text like default) if our placeable is paused.
		if (HUDIcon.Texture != NULL)
		{
			UFont* MediumFont = Widget->UTHUDOwner->MediumFont;
			float Xl, YL;
			Widget->GetCanvas()->StrLen(MediumFont, FString(TEXT("000")), Xl, YL);

			// icon left aligned, time remaining right aligned
			Widget->DrawTexture(HUDIcon.Texture, Xl * -1, Pos.Y, Size.X, Size.Y, HUDIcon.U, HUDIcon.V, HUDIcon.UL, HUDIcon.VL, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
		}
	}
}

FText AUTPlaceablePowerup::GetHUDText() const
{
	if (!bTimerPaused)
	{
		return Super::GetHUDText();
	}
	else
	{
		if (UTOwner && UTOwner->PlayerState)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTOwner->PlayerState);
			if (UTPS)
			{
				return FText::AsNumber(UTPS->GetRemainingBoosts());
			}
		}

		return FText::GetEmpty();
	}
}

int32 AUTPlaceablePowerup::GetHUDValue() const
{
	if (!bTimerPaused)
	{
		return Super::GetHUDValue();
	}
	else
	{
		if (UTOwner && UTOwner->PlayerState)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTOwner->PlayerState);
			if (UTPS)
			{
				return static_cast<int32>(UTPS->GetRemainingBoosts());
			}
		}

		return 0;
	}
}

// Allows inventory items to decide if a widget should be allowed to render them.
bool AUTPlaceablePowerup::HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget)
{
	return (Cast<UUTHUDWidget_Powerups>(TargetWidget) != NULL);
}

void AUTPlaceablePowerup::SpawnPowerup()
{
	if (PowerupToSpawn)
	{
		const FRotator SpawnRotation = (UTOwner->GetActorTransform().GetRotation()).Rotator() + (SpawnOffset.GetRotation().Rotator());
		const FVector RotatedOffset = UTOwner->GetActorTransform().GetRotation().RotateVector(SpawnOffset.GetLocation());
		const FVector SpawnLocation = UTOwner->GetActorTransform().GetLocation() + RotatedOffset;
		
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Instigator = UTOwner;
		Params.bNoFail = true;
		
		AActor* NewlySpawnedActor = GetWorld()->SpawnActor<AActor>(PowerupToSpawn, SpawnLocation, SpawnRotation, Params);
		SpawnedPowerups.Add(NewlySpawnedActor);

		//Force replicate to on since the actor we are spawning might not normally be replicated
		NewlySpawnedActor->SetReplicates(true);

		if (bAttachToOwner)
		{
			FAttachmentTransformRules AttachmentRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
			AttachmentRules.bWeldSimulatedBodies = true;			
			NewlySpawnedActor->AttachToActor(UTOwner, AttachmentRules, NAME_None);
		}
	}

	//Remove itself if we are out of boosts
	if (UTOwner && UTOwner->PlayerState)
	{
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTOwner->PlayerState);
		if (UTPS)
		{
			if (bDestroyWhenOutOfBoosts && (UTPS->GetRemainingBoosts() <= 0))
			{
				Destroy();
			}
		}
	}
}