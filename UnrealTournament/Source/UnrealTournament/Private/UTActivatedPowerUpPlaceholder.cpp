#include "UnrealTournament.h"

#include "UTActivatedPowerupPlaceholder.h"
#include "UTHUDWidget_Powerups.h"

AUTActivatedPowerUpPlaceholder::AUTActivatedPowerUpPlaceholder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAlwaysDropOnDeath = false;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
}

void AUTActivatedPowerUpPlaceholder::BeginPlay()
{
	bHasOverlayBeenAddedAlready = false;

	Super::BeginPlay();
}

void AUTActivatedPowerUpPlaceholder::AddOverlayMaterials_Implementation(AUTGameState* GS) const
{
	GS->AddOverlayEffect(OverlayEffect, OverlayEffect1P);
}

void AUTActivatedPowerUpPlaceholder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetUTOwner())
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetUTOwner()->GetController());
		if (UTPC)
		{
			AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTPC->PlayerState);
			if (UTPlayerState)
			{
				//we aren't full, but we are in progress. Scale the UI to match how long we've held
				if ((UTPC->PowerUpButtonHoldPercentage > SMALL_NUMBER) && (UTPlayerState->RemainingBoosts > 0))
				{
					if (!bHasOverlayBeenAddedAlready)
					{
						GetUTOwner()->SetWeaponOverlayEffect(OverlayEffect, true);

						bHasOverlayBeenAddedAlready = true;
					}
				}
				//No longer being held, remove overlays
				else
				{
					if (bHasOverlayBeenAddedAlready)
					{
						GetUTOwner()->SetWeaponOverlayEffect(OverlayEffect, false);

						bHasOverlayBeenAddedAlready = false;
					}
				}
			}
		}
	}
}

bool AUTActivatedPowerUpPlaceholder::HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget)
{
	return false;
}