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
	if (HUDIcon.Texture != NULL)
	{
		FText Text = GetHUDText();

		UFont* MediumFont = Widget->UTHUDOwner->MediumFont;
		float Xl, YL;
		Widget->GetCanvas()->StrLen(MediumFont, FString(TEXT("000")), Xl, YL);

		// Draws the Timer first 
		Widget->DrawText(Text, 0, Pos.Y + (Size.Y * 0.5f) , Widget->UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);

		// icon left aligned, time remaining right aligned
		Widget->DrawTexture(HUDIcon.Texture, Xl * -1, Pos.Y, Size.X, Size.Y, HUDIcon.U, HUDIcon.V, HUDIcon.UL, HUDIcon.VL,1.0, FLinearColor::White, FVector2D(1.0,0.0));
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