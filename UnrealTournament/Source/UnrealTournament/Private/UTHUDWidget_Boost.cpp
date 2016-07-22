// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Boost.h"
#include "UTCarriedObject.h"
#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.h"
#include "UTHUDWidget_QuickStats.h"

UUTHUDWidget_Boost::UUTHUDWidget_Boost(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MI(TEXT("MaterialInstanceConstant'/Game/RestrictedAssets/Proto/UI/HUD/Elements/MI_HudTimer.MI_HudTimer'"));
	HudTimerMI = MI.Object;
	IconScale = 0.7f;

	DesignedResolution = 1080.f;
	Position=FVector2D(8.0f,0.0f) * IconScale;
	Size=FVector2D(101.0f,86.0f) * IconScale;
	ScreenPosition=FVector2D(0.0f, 0.5f);
	Origin=FVector2D(0.0f,0.5f);
}

void UUTHUDWidget_Boost::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
	HudTimerMID = UMaterialInstanceDynamic::Create(HudTimerMI, this);
}

bool UUTHUDWidget_Boost::ShouldDraw_Implementation(bool bShowScores)
{
	if (UTGameState && (UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted() || UTGameState->IsMatchIntermission()))
	{
		bLastUnlocked = false;
		return false;
	}
	if (UTHUDOwner->bShowComsMenu || UTHUDOwner->bShowWeaponWheel)
	{
		bLastUnlocked = false;
		return false;
	}

	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	AUTPlayerState* UTPlayerState = UTC != nullptr ? Cast<AUTPlayerState>(UTC->PlayerState) : nullptr;

	AUTCTFRoundGameState* RoundGameState = GetWorld()->GetGameState<AUTCTFRoundGameState>();
	if (UTPlayerState == nullptr || RoundGameState == nullptr || 
		( (!RoundGameState->IsTeamAbleToEarnPowerup(UTPlayerState->GetTeamNum()) && UTPlayerState->GetRemainingBoosts() <= 0))
	   )
	{
		bLastUnlocked = false;
		return false;
	}


	bool bShouldDraw = (!bShowScores && UTC && UTPlayerState && !UTC->IsDead() &&  UTPlayerState->BoostClass && !UTPlayerState->bOutOfLives);

	// Trigger the animation each spawn
	if (!bShouldDraw)
	{
		bLastUnlocked = false;
	}

	return bShouldDraw;
}


void UUTHUDWidget_Boost::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner && UTHUDOwner->UTPlayerOwner)
	{

		bool bUseQuickStats = !UTHUDOwner->GetQuickStatsHidden();

		Super::Draw_Implementation(DeltaTime);

		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTHUDOwner->UTPlayerOwner->PlayerState);

		if (UTGameState && UTPlayerState)
		{
			AUTCTFRoundGameState* RoundGameState = GetWorld()->GetGameState<AUTCTFRoundGameState>();
			if (RoundGameState != nullptr && UTPlayerState->BoostClass)
			{
				bool bIsUnlocked = UTPlayerState->GetRemainingBoosts() > 0 || RoundGameState->GetKillsNeededForPowerup(UTPlayerState->GetTeamNum()) <= 0;
				if (bIsUnlocked != bLastUnlocked)
				{
					if (bIsUnlocked)
					{
						UnlockAnimTime = 0.0f;
					}
					bLastUnlocked = bIsUnlocked;
				}

				//if (bIsUnlocked && UTPlayerState->GetRemainingBoosts() <= 0) return;

				float Opacity = 1.0f;
				if (bIsUnlocked)
				{
					UnlockAnimTime += DeltaTime;
					if (bUseQuickStats)
					{
						if (UnlockAnimTime >= UNLOCK_ANIM_DURATION) return;
						float Alpha = (UnlockAnimTime / UNLOCK_ANIM_DURATION);
						Opacity = 1.0f - Alpha;

						UUTHUDWidget_QuickStats* QuickStatWidget = Cast<UUTHUDWidget_QuickStats>(UTHUDOwner->FindHudWidgetByClass(UUTHUDWidget_QuickStats::StaticClass()));
						if (bUseQuickStats && QuickStatWidget != nullptr)
						{
							FVector2D DesiredLocation = QuickStatWidget->GetRenderPosition() + QuickStatWidget->GetBoostLocation();

							RenderPosition.X = FMath::InterpEaseIn<float>(RenderPosition.X, DesiredLocation.X, Alpha, 2.0f);
							RenderPosition.Y = FMath::InterpEaseIn<float>(RenderPosition.Y, DesiredLocation.Y, Alpha, 2.0f);
						}
					}
				}

				DrawTexture(UTHUDOwner->HUDAtlas3, 0,0, 101.0f * IconScale, 114.0f * IconScale, 49.0f, 1.0f, 101.0f, 114.0f, Opacity, FLinearColor(0.0f,0.0f,0.0f,0.3f));
				DrawTexture(UTHUDOwner->HUDAtlas3, 0,0, 101.0f * IconScale, 114.0f * IconScale, 49.0f, 121.0f, 101.0f, 114.0f, Opacity, FLinearColor::White);

				AUTInventory* Inv = UTPlayerState->BoostClass->GetDefaultObject<AUTInventory>();
				if (Inv)
				{
					float XScale = (80.0f * IconScale) / Inv->HUDIcon.UL;
					float Height = (80.0f * IconScale) * (Inv->HUDIcon.VL / Inv->HUDIcon.UL);

					DrawTexture(Inv->HUDIcon.Texture, 51.0f * IconScale, 57.0f * IconScale, 80.0f * IconScale, Height, Inv->HUDIcon.U, Inv->HUDIcon.V, Inv->HUDIcon.UL, Inv->HUDIcon.VL, Opacity, FLinearColor::White, FVector2D(0.5f, 0.5f));

					//Show countdown to power up
					if (!bIsUnlocked)
					{
						FText BoostText = FText::Format(NSLOCTEXT("UTHUDWIDGET_Boost","BoostUnlockText","Unlocks in\n{0} kills"), FText::AsNumber(RoundGameState->GetKillsNeededForPowerup(UTPlayerState->GetTeamNum())));
						DrawText(BoostText, 0, Height, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
					}
					else if (!bUseQuickStats)
					{
						FInputActionKeyMapping ActivatePowerupBinding = FindKeyMappingTo("StartActivatePowerup");
						FText BoostText = (ActivatePowerupBinding.Key.GetDisplayName().ToString().Len() < 6) ? ActivatePowerupBinding.Key.GetDisplayName() : FText::FromString(" ");
						DrawText(BoostText, 0, Height, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
					}
				}
			}
		}
	}

}

