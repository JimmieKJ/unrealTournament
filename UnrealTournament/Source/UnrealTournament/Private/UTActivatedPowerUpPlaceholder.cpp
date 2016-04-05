#include "UnrealTournament.h"

#include "UTActivatedPowerupPlaceholder.h"
#include "UTHUDWidget_Powerups.h"

AUTActivatedPowerUpPlaceholder::AUTActivatedPowerUpPlaceholder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAlwaysDropOnDeath = false;

	HUDText = FText::GetEmpty();

	InitialFlashScaleOverride = 1.10f;
	FlashTimerOverride = 1.0f;
	InitialFlashTime = 1.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
}

void AUTActivatedPowerUpPlaceholder::BeginPlay()
{
	OriginalHUDIconUL = HUDIcon.UL;
	bHasOverlayBeenAddedAlready = false;
	
	UpdateHUDText();

	Super::BeginPlay();
}

FText AUTActivatedPowerUpPlaceholder::GetHUDText() const
{
	return HUDText;
}

void AUTActivatedPowerUpPlaceholder::UpdateHUDText()
{
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	if (InputSettings)
	{
		for (int32 inputIndex = 0; inputIndex < InputSettings->ActionMappings.Num(); ++inputIndex)
		{
			FInputActionKeyMapping& Action = InputSettings->ActionMappings[inputIndex];
			if (Action.ActionName == "StartActivatePowerup")
			{
				HUDText = Action.Key.GetDisplayName();
			}
		}
	}
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
				//Flash if we are fully held
				if ((UTPC->PowerUpButtonHoldPercentage >= 1.0f) && (UTPlayerState->RemainingBoosts > 0))
				{
					FlashTimer = FlashTimerOverride;
					InitialFlashTime = InitialFlashTimeOverride;
					InitialFlashScale = InitialFlashScaleOverride;
				}
				//we aren't full, but we are in progress. Scale the UI to match how long we've held
				else if ((UTPC->PowerUpButtonHoldPercentage > SMALL_NUMBER) && (UTPlayerState->RemainingBoosts > 0))
				{
					HUDIcon.UL = OriginalHUDIconUL * UTPC->PowerUpButtonHoldPercentage;

					if (!bHasOverlayBeenAddedAlready)
					{
						GetUTOwner()->SetWeaponOverlayEffect(OverlayEffect, true);

						bHasOverlayBeenAddedAlready = true;
					}
				}
				//No longer being held, remove overlays
				else
				{
					HUDIcon.UL = OriginalHUDIconUL;

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
	int RemainingBoosts = 0;
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetUTOwner()->GetController());
	if (UTPC)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTPC->PlayerState);
		if (UTPlayerState)
		{
			RemainingBoosts = UTPlayerState->RemainingBoosts;
		}
	}

	return ((Cast<UUTHUDWidget_Powerups>(TargetWidget) != NULL) && (RemainingBoosts > 0));
}