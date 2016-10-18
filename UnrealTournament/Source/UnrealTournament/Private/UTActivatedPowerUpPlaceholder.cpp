#include "UnrealTournament.h"

#include "UTActivatedPowerUpPlaceholder.h"
#include "UTHUDWidget_Powerups.h"

AUTActivatedPowerUpPlaceholder::AUTActivatedPowerUpPlaceholder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAlwaysDropOnDeath = false;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	InitialLifeSpan = 0.75f;
}

void AUTActivatedPowerUpPlaceholder::AddOverlayMaterials_Implementation(AUTGameState* GS) const
{
	GS->AddOverlayEffect(OverlayEffect, OverlayEffect1P);
}

void AUTActivatedPowerUpPlaceholder::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);
	if (GetUTOwner())
	{
		GetUTOwner()->SetWeaponOverlayEffect(OverlayEffect, true);
	}
}

void AUTActivatedPowerUpPlaceholder::Removed()
{
	if (GetUTOwner())
	{
		GetUTOwner()->SetWeaponOverlayEffect(OverlayEffect, false);
	}
	Super::Removed();
}

bool AUTActivatedPowerUpPlaceholder::HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget)
{
	return false;
}