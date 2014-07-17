// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTimedPowerup.h"
#include "UnrealNetwork.h"

AUTTimedPowerup::AUTTimedPowerup(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	TimeRemaining = 30.0f;
	RespawnTime = 90.0f;
	bAlwaysDropOnDeath = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
}

void AUTTimedPowerup::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	ClientSetTimeRemaining(TimeRemaining);
	if (OverlayMaterial != NULL)
	{
		NewOwner->SetWeaponOverlay(OverlayMaterial, true);
	}
	GetWorld()->GetTimerManager().SetTimer(this, &AUTTimedPowerup::PlayFadingSound, FMath::Max<float>(0.1f, TimeRemaining - 3.0f), false);
}
void AUTTimedPowerup::Removed()
{
	if (OverlayMaterial != NULL)
	{
		GetUTOwner()->SetWeaponOverlay(OverlayMaterial, false);
	}
	GetWorld()->GetTimerManager().ClearTimer(this, &AUTTimedPowerup::PlayFadingSound);
	Super::Removed();
}

void AUTTimedPowerup::PlayFadingSound()
{
	// reset timer if time got added
	if (TimeRemaining > 3.0)
	{
		GetWorld()->GetTimerManager().SetTimer(this, &AUTTimedPowerup::PlayFadingSound, TimeRemaining - 3.0f, false);
	}
	else
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), PowerupFadingSound, GetUTOwner());
		GetWorld()->GetTimerManager().SetTimer(this, &AUTTimedPowerup::PlayFadingSound, 0.75f, false);
	}
}

void AUTTimedPowerup::TimeExpired_Implementation()
{
	if (Role == ROLE_Authority)
	{
		if (GetUTOwner() != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), PowerupOverSound, GetUTOwner());
		}

		Destroy();
	}
}

bool AUTTimedPowerup::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	if (ContainedInv != NULL)
	{
		TimeRemaining += Cast<AUTTimedPowerup>(ContainedInv)->TimeRemaining;
	}
	else
	{
		TimeRemaining += GetClass()->GetDefaultObject<AUTTimedPowerup>()->TimeRemaining;
	}
	ClientSetTimeRemaining(TimeRemaining);
	return true;
}

void AUTTimedPowerup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetUTOwner() != NULL && TimeRemaining > 0.0f)
	{
		TimeRemaining -= DeltaTime;
		if (TimeRemaining <= 0.0f)
		{
			TimeExpired();
		}
	}
}

void AUTTimedPowerup::ClientSetTimeRemaining_Implementation(float InTimeRemaining)
{
	TimeRemaining = InTimeRemaining;
}

void AUTTimedPowerup::DrawInventoryHUD_Implementation(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size)
{
	if (HUDIcon.Texture != NULL)
	{

		FText Text = FText::AsNumber(FMath::CeilToInt(TimeRemaining));

		UFont* MediumFont = Widget->UTHUDOwner->MediumFont;
		float Xl, YL;
		Widget->GetCanvas()->StrLen(MediumFont, FString(TEXT("000")), Xl, YL);

		// Draws the Timer first 
		Widget->DrawText(Text, 0, Pos.Y + (Size.Y * 0.5f) , Widget->UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);

		// icon left aligned, time remaining right aligned
		Widget->DrawTexture(HUDIcon.Texture, Xl * -1, Pos.Y, Size.X, Size.Y, HUDIcon.U, HUDIcon.V, HUDIcon.UL, HUDIcon.VL,1.0, FLinearColor::White, FVector2D(1.0,0.0));

	}
}