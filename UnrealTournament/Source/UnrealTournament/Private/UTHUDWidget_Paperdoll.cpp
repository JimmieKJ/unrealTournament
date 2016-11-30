// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTProfileSettings.h"
#include "UTHUDWidget_WeaponBar.h"
#include "UTJumpBoots.h"
#include "UTFlagRunGameState.h"
#include "UTRallyPoint.h"
#include "UTArmor.h"

const int32 ALTERNATE_X_OFFSET = -64;
const float ANIMATION_TIME = 0.45f;


UUTHUDWidget_Paperdoll::UUTHUDWidget_Paperdoll(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080.0f;
	Position=FVector2D(0.0f, -8.0f);
	Size=FVector2D(224.0f,46.0f);
	ScreenPosition=FVector2D(0.5f, 1.0f);
	Origin=FVector2D(0.5f,1.0f);
	bAnimating = false;
	DrawOffset = FVector2D(0.0f, 0.0f);

	RallyAnimTimers.Add(RALLY_ANIMATION_TIME * 0.25);
	RallyAnimTimers.Add(RALLY_ANIMATION_TIME * 0.5);
	RallyAnimTimers.Add(RALLY_ANIMATION_TIME * 0.75);
}

void UUTHUDWidget_Paperdoll::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	LastHealth = 100;
	LastArmor = 0;
	ArmorFlashTimer = 0.0f;
	HealthFlashTimer = 0.0f;
}

bool UUTHUDWidget_Paperdoll::ShouldDraw_Implementation(bool bShowScores)
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	bool bIsHidden = false;
	if (UTHUDOwner)
	{
		bIsHidden = !UTHUDOwner->GetQuickStatsHidden();
	}

	return ( !bIsHidden && (GS == NULL || !GS->HasMatchEnded()) && Super::ShouldDraw_Implementation(bShowScores) );
}

void UUTHUDWidget_Paperdoll::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	UUTHUDWidget_WeaponBar* WB = Cast<UUTHUDWidget_WeaponBar>(InUTHUDOwner->FindHudWidgetByClass(UUTHUDWidget_WeaponBar::StaticClass()));
	ScreenPosition.Y = 1.0f;
	if (WB)
	{
		UUTProfileSettings* PlayerProfile = InUTHUDOwner->UTPlayerOwner->GetProfileSettings();
		bool bVerticalWeaponBar = PlayerProfile ? PlayerProfile->bVerticalWeaponBar : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bVerticalWeaponBar;
		if (!bVerticalWeaponBar)
		{
			ScreenPosition.Y = 0.925f;	
		}
	}

	AUTGameState* UTGS = InUTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (UTGS)
	{
		HealthBackground.bUseTeamColors = false;	//UTGameState->bTeamGame;	
		ArmorBackground.bUseTeamColors = false;		//UTGameState->bTeamGame;	
	}

	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);
}


void UUTHUDWidget_Paperdoll::Draw_Implementation(float DeltaTime)
{
	Opacity = 1.0f;
	float FlagOpacity = 1.0f;

	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	AUTPlayerState* PS = UTC ? Cast<AUTPlayerState>(UTC->PlayerState) : NULL;
	UUTHUDWidget_Paperdoll* DefObj = GetClass()->GetDefaultObject<UUTHUDWidget_Paperdoll>();

	AUTFlagRunGameState* GameState = UTHUDOwner->GetWorld()->GetGameState<AUTFlagRunGameState>();

	bool bPlayerCanRally = UTHUDOwner->UTPlayerOwner->CanPerformRally();
	bool bShowTimer = !bPlayerCanRally && PS && PS->Team && GameState && GameState->bAttackersCanRally && ((PS->Team->TeamIndex == 0) == GameState->bRedToCap) && UTC && UTC->bCanRally && (PS->RemainingRallyDelay > 0);
	if (GameState && GameState->CurrentRallyPoint)
	{
		bShowTimer = bShowTimer || (GameState && !GameState->bAttackersCanRally && PS && PS->Team && ((PS->Team->TeamIndex == 0) == GameState->bRedToCap));
	}

	if (UTC != NULL && !UTC->IsDead())
	{
		PlayerArmor = UTC->GetArmorAmount();

		ShieldOverlay.bHidden = PlayerArmor <= 100;
		ArmorText.Text = FText::AsNumber(PlayerArmor);
		if (PlayerArmor != LastArmor)
		{
			ArmorText.RenderColor = (PlayerArmor > LastArmor) ? ArmorPositiveFlashColor : ArmorNegativeFlashColor;
			ArmorFlashTimer = ArmorFlashTime;		
			LastArmor = PlayerArmor;
			ArmorText.TextScale = 2.f;
		}
		else if (ArmorFlashTimer > 0.f)
		{
			ArmorFlashTimer = ArmorFlashTimer - DeltaTime;
			if (ArmorFlashTimer < 0.5f*ArmorFlashTime)
			{
				ArmorText.RenderColor = FMath::CInterpTo(ArmorText.RenderColor, DefObj->ArmorText.RenderColor, DeltaTime, (1.f / (ArmorFlashTime > 0.f ? 2.f*ArmorFlashTime : 1.f)));
			}
			ArmorText.TextScale = 1.f + ArmorFlashTimer / ArmorFlashTime;
		}
		else
		{
			ArmorText.RenderColor = DefObj->ArmorText.RenderColor; 
			ArmorText.TextScale = 1.f;
		}

		HealthText.Text = FText::AsNumber(UTC->Health);
		if (UTC->Health != LastHealth)
		{
			HealthText.RenderColor = (UTC->Health > LastHealth) ? HealthPositiveFlashColor : HealthNegativeFlashColor;
			HealthFlashTimer = HealthFlashTime;		
			LastHealth = UTC->Health;
			HealthText.TextScale = 2.f;

		}
		else if (HealthFlashTimer > 0.f)
		{
			HealthFlashTimer = HealthFlashTimer - DeltaTime;
			if (HealthFlashTimer < 0.5f*HealthFlashTime)
			{
				HealthText.RenderColor = FMath::CInterpTo(HealthText.RenderColor, DefObj->HealthText.RenderColor, DeltaTime, (1.f / (HealthFlashTime > 0.f ? 2.f*HealthFlashTime : 1.f)));
			}
			HealthText.TextScale = 1.f + HealthFlashTimer / HealthFlashTime;
		}
		else
		{
			HealthText.RenderColor = DefObj->HealthText.RenderColor;
			HealthText.TextScale = 1.f;
		}

		int32 DesiredXOffset = 0;

		bShowFlagInfo = PS && PS->CarriedObject;
		if (bShowFlagInfo || bPlayerCanRally || bShowTimer)
		{
			if ( UTHUDOwner->GetQuickInfoHidden() )
			{
				// We have the flag.. make room for it.
				DesiredXOffset = -64;		
			}
		}

		if (DrawOffset.X != DesiredXOffset && !bAnimating)
		{
			bAnimating = true;
			DrawOffsetTransitionTime = ANIMATION_TIME;
		}

		DrawOffsetTransitionTime -= DeltaTime;
		if (DrawOffsetTransitionTime > 0.0f && UTHUDOwner->GetQuickInfoHidden())
		{
			FlagOpacity = 1.0f - (DrawOffsetTransitionTime / ANIMATION_TIME);
			DrawOffset.X = FMath::InterpEaseInOut<float>(DesiredXOffset != 0.0f ? 0.0f : ALTERNATE_X_OFFSET, DesiredXOffset, FlagOpacity, 2.0f);
		}
		else
		{
			DrawOffset.X = DesiredXOffset;
			bAnimating = false;
		}
	}

	// Draw the Health...
	RenderObj_Texture(HealthBackground, DrawOffset); 
	RenderObj_Texture(HealthIcon, DrawOffset); 
	RenderObj_Text(HealthText, DrawOffset); 

	// Draw the Armor...
	RenderObj_Texture(ArmorBackground, DrawOffset * -1.f); 
	RenderObj_Texture(ShieldOverlay, DrawOffset * -1.f); 
	RenderObj_Texture(ArmorIcon, DrawOffset * -1.f); 
	RenderObj_Text(ArmorText, DrawOffset * -1.f); 

	FlagText.Text = FText::GetEmpty();
	if (UTHUDOwner->GetQuickInfoHidden() && (bPlayerCanRally || bShowFlagInfo || bShowTimer))
	{
		FlagIcon.Position.Y = bPlayerCanRally ? -16.f : 0.f;
		Opacity = FlagOpacity;		
		RenderScale *= FlagOpacity;

		RenderObj_Texture(FlagBackground);
		if (bPlayerCanRally)
		{
			DrawRallyIcon(DeltaTime, GameState ? GameState->CurrentRallyPoint : nullptr);		
		}

		if (bPlayerCanRally || bShowFlagInfo)
		{
			FlagIcon.RenderScale = 1.25f + (0.75f * FMath::Abs<float>(FMath::Sin(GetWorld()->GetTimeSeconds() * 3.f)));
		}
		else
		{
			FlagIcon.RenderScale = 1.25f;
		}

		FlagIcon.UVs = FlagHolderIconUVs;
		FlagIcon.bUseTeamColors = false;
		FlagIcon.RenderOpacity = 1.0f;
		FLinearColor TeamColor = (PS && PS->Team && PS->Team->TeamIndex == 1) ? FLinearColor::Blue : FLinearColor::Red;
		FlagIcon.RenderColor = (bPlayerCanRally|| bShowTimer) ? FLinearColor::Yellow : TeamColor;
		RenderObj_Texture(FlagIcon);

		if (bPlayerCanRally)
		{
			FlagText.Text = UTHUDOwner->RallyLabel;
			RenderObj_Text(FlagText);
		}
		else if (bShowTimer)
		{
			int32 RemainingTime = PS->CarriedObject && GameState && GameState->CurrentRallyPoint
				? 1 + GameState->CurrentRallyPoint->ReplicatedCountdown
				: 1 + FMath::Max(int32(PS->RemainingRallyDelay), (GameState && GameState->CurrentRallyPoint) ? GameState->CurrentRallyPoint->ReplicatedCountdown : 0);
			FlagText.Text = FText::AsNumber(RemainingTime);
			RenderObj_Text(FlagText);
		}

		if (PS && PS->CarriedObject && PS->CarriedObject->bCurrentlyPinged)
		{
			if (PS->CarriedObject->HoldingPawn && PS->CarriedObject->HoldingPawn->bIsInCombat)
			{
				RenderObj_Texture(CombatIcon);
			}
			else
			{
				RenderObj_Texture(DetectedIcon);
			}
		}
	}
}

void UUTHUDWidget_Paperdoll::DrawRallyIcon(float DeltaTime, AUTRallyPoint* RallyPoint)
{
	FlagIcon.UVs = RallyIconUVs;
	FlagIcon.RenderScale = 1.0f;
	FlagIcon.bUseTeamColors = true;
	if (RallyPoint && (RallyPoint->RallyTimeRemaining > 0.f))
	{
		FLinearColor RealColor = FlagBackground.RenderColor;
		FlagBackground.RenderColor = FLinearColor::Gray;
		float RealWidth = FlagBackground.Size.X;
		FlagBackground.Size.X *= 0.1f*RallyPoint->RallyTimeRemaining;
		float RealUVUL = FlagBackground.UVs.UL;
		FlagBackground.UVs.UL *= 0.1f*RallyPoint->RallyTimeRemaining;
		float RealPosX = FlagBackground.Position.X;
		FlagBackground.Position.X -= 0.5f*(RealWidth - FlagBackground.Size.X);
		RenderObj_Texture(FlagBackground);
		FlagBackground.RenderColor = RealColor;
		FlagBackground.Size.X = RealWidth;
		FlagBackground.UVs.UL = RealUVUL;
		FlagBackground.Position.X = RealPosX;

	}
	for (int32 i = 0; i < RallyAnimTimers.Num(); i++)
	{

		RallyAnimTimers[i] += DeltaTime;
		if (RallyAnimTimers[i] > RALLY_ANIMATION_TIME) RallyAnimTimers[i] = 0;
		float DrawPosition = (RallyAnimTimers[i] / RALLY_ANIMATION_TIME);
	
		float XPos = FMath::InterpEaseOut<float>(64.0f, 0.0f, DrawPosition, 2.0f);
		FlagIcon.RenderOpacity = FMath::InterpEaseOut<float>(1.0f, 0.0f, DrawPosition, 2.0f);
		RenderObj_Texture(FlagIcon,FVector2D(XPos, 0.0f));
		RenderObj_Texture(FlagIcon,FVector2D(XPos * -1, 0.0f));
	}
}