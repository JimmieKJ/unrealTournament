// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Boost.h"
#include "UTCarriedObject.h"
#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.h"
#include "UTHUDWidget_QuickStats.h"

const float ANIM_DURATION = 1.75f;
const float ANIM_PING_TIME = 2.5f;

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
	LastAnimTime = 0.0f;
}

void UUTHUDWidget_Boost::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
	HudTimerMID = UMaterialInstanceDynamic::Create(HudTimerMI, this);
}

bool UUTHUDWidget_Boost::ShouldDraw_Implementation(bool bShowScores)
{
	if (UTHUDOwner == nullptr || UTHUDOwner->UTPlayerOwner == nullptr || (UTGameState && (UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted() || UTGameState->IsMatchIntermission())))
	{
		return false;
	}

	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	AUTPlayerState* UTPlayerState = UTC != nullptr ? Cast<AUTPlayerState>(UTC->PlayerState) : nullptr;

	AUTCTFRoundGameState* RoundGameState = GetWorld()->GetGameState<AUTCTFRoundGameState>();
	if (UTPlayerState == nullptr || RoundGameState == nullptr || 
		( (!RoundGameState->IsTeamAbleToEarnPowerup(UTPlayerState->GetTeamNum()) && UTPlayerState->GetRemainingBoosts() <= 0))
	   )
	{
		return false;
	}

	return (!bShowScores && UTC && UTPlayerState && !UTC->IsDead() &&  UTPlayerState->BoostClass && !UTPlayerState->bOutOfLives);
}

void UUTHUDWidget_Boost::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	if (InUTHUDOwner && InUTHUDOwner->UTPlayerOwner)
	{
		bool bIsUnlocked = false;
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(InUTHUDOwner->UTPlayerOwner->PlayerState);
		if (UTGameState && UTPlayerState && UTPlayerState->BoostClass)
		{
			AUTCTFRoundGameState* RoundGameState = GetWorld()->GetGameState<AUTCTFRoundGameState>();
			if (RoundGameState != nullptr && UTPlayerState->BoostClass)
			{
				bIsUnlocked = UTPlayerState->GetRemainingBoosts() > 0 || RoundGameState->GetKillsNeededForPowerup(UTPlayerState->GetTeamNum()) <= 0;

				if (bIsUnlocked)
				{
					BoostText.Text = InUTHUDOwner->BoostLabel;
				}
				else
				{
					BoostText.Text = FText::Format(NSLOCTEXT("UTHUDWIDGET_Boost","BoostUnlockText","{0}  x "), FText::AsNumber(RoundGameState->GetKillsNeededForPowerup(UTPlayerState->GetTeamNum())));
				}

			}

			AUTInventory* Inv = UTPlayerState->BoostClass->GetDefaultObject<AUTInventory>();
			if (Inv)
			{
				BoostIcon.Size.Y = 76.0f ;
				BoostIcon.Size.X = 76.0f * (Inv->HUDIcon.UL / Inv->HUDIcon.VL);
				BoostIcon.Atlas = Inv->HUDIcon.Texture;
				BoostIcon.UVs = FTextureUVs(Inv->HUDIcon.U, Inv->HUDIcon.V, Inv->HUDIcon.UL, Inv->HUDIcon.VL);
			}
		}

		Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);

		// Align the labels.

		float center = Background.Size.X * 0.5;
		float TextSize = BoostText.GetSize().X;
		float TotalSize = TextSize + (bIsUnlocked ? 0 : BoostSkull.Size.X);

		BoostSkull.bHidden = bIsUnlocked;

		BoostText.Position.X = center - (TotalSize * 0.5);
		BoostSkull.Position.X = BoostText.Position.X + TextSize;

		if (bIsUnlocked)
		{
			if (UTHUDOwner->GetQuickInfoHidden() && GetWorld()->GetTimeSeconds() - LastAnimTime > ANIM_PING_TIME)
			{
				bAnimating = true;
				AnimTime = 0.0f;
				LastAnimTime = GetWorld()->GetTimeSeconds();
			}
		}

		if (bAnimating)
		{
			AnimTime += DeltaTime;
			float Perc = AnimTime < ANIM_DURATION ? AnimTime / ANIM_DURATION : 1.0f;
			float AnimScale = 2.0f - UUTHUDWidget::BounceEaseOut(0.0f, 1.0f, Perc, 7.0f);
			BoostIcon.RenderScale = AnimScale;
			BoostText.TextScale = AnimScale;
			bAnimating = AnimTime < ANIM_DURATION;
		}
		else
		{
			BoostIcon.RenderScale = 1.0f;
			BoostText.TextScale = 1.0f;
		}
	}
	else
	{
		Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);
	}
}

