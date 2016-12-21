// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunPvEHUD.h"
#include "UTFlagRunPvEGameState.h"

AUTFlagRunPvEHUD::AUTFlagRunPvEHUD(const FObjectInitializer& OI)
	: Super(OI)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TimerMat(TEXT("/Game/RestrictedAssets/Proto/UI/HUD/Elements/CircularTimer.CircularTimer"));
	ChargeIndicatorMat = TimerMat.Object;
}

void AUTFlagRunPvEHUD::BeginPlay()
{
	Super::BeginPlay();

	BoostWheel = Cast<UUTRadialMenu_BoostPowerup>(AddHudWidget(UUTRadialMenu_BoostPowerup::StaticClass()));
	RadialMenus.Add(BoostWheel);

	ChargeIndicatorMID = UMaterialInstanceDynamic::Create(ChargeIndicatorMat, this);
}

void AUTFlagRunPvEHUD::ToggleBoostWheel(bool bShow)
{
	bShowBoostWheel = bShow;

	AUTCharacter* UTCharacter = UTPlayerOwner ? UTPlayerOwner->GetUTCharacter() : nullptr;
	if (bShow && UTCharacter && !UTCharacter->IsDead())
	{
		BoostWheel->BecomeInteractive();
	}
	else
	{
		BoostWheel->BecomeNonInteractive();
	}
}

static FName NAME_PercentComplete(TEXT("PercentComplete"));

void AUTFlagRunPvEHUD::DrawHUD()
{
	Super::DrawHUD();

	if (UTPlayerOwner != nullptr && UTPlayerOwner->GetUTCharacter() != nullptr && UTPlayerOwner->UTPlayerState != nullptr && !ScoreboardIsUp())
	{
		if (ChargeIndicatorMID != nullptr)
		{
			ChargeIndicatorMID->SetScalarParameterValue(NAME_PercentComplete, UTPlayerOwner->UTPlayerState->GetRemainingBoosts() > 0 ? 1.0f : UTPlayerOwner->UTPlayerState->BoostRechargePct);
		}
		const float Size = FMath::Min<float>(Canvas->ClipX * 0.1f, Canvas->ClipY * 0.1f);
		{
			FCanvasTileItem TileItem(FVector2D(Canvas->ClipX * 0.8f, Canvas->ClipY * 0.8f), ChargeIndicatorMID->GetRenderProxy(0), FVector2D(Size, Size), FVector2D(1.0f, 0.0f), FVector2D(0.0f, 1.0f));
			TileItem.Rotation.Yaw = -90.0f;
			TileItem.PivotPoint = FVector2D(0.5f, 0.5f);
			TileItem.SetColor(FLinearColor(1.0f, 1.0f, 0.0f, 1.0f));
			Canvas->DrawItem(TileItem);
		}
		if (UTPlayerOwner->UTPlayerState->GetRemainingBoosts() > 0)
		{
			const float ScaleAdd = FMath::InterpEaseOut<float>(0.0f, 1.0f, GetWorld()->TimeSeconds - FMath::TruncToFloat(GetWorld()->TimeSeconds), 2.0f);
			FCanvasTileItem TileItem(FVector2D(Canvas->ClipX * 0.8f - Size * ScaleAdd * 0.5f, Canvas->ClipY * 0.8f - Size * ScaleAdd * 0.5f), ChargeIndicatorMID->GetRenderProxy(0), FVector2D(Size * (1.0f + ScaleAdd), Size * (1.0f + ScaleAdd)), FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
			TileItem.SetColor(FLinearColor(1.0f, 1.0f, 0.0f, 0.7f - 0.65f * ScaleAdd));
			Canvas->DrawItem(TileItem);
		}
		TArray<FString> Keys;
		UTPlayerOwner->ResolveKeybind(TEXT("ActivateSpecial"), Keys);
		if (Keys.Num() > 0)
		{
			float XL, YL;
			Canvas->TextSize(SmallFont, Keys[0], XL, YL);
			Canvas->DrawColor = FColor::White;
			Canvas->DrawText(SmallFont, Keys[0], Canvas->ClipX * 0.8f + (Size - XL) * 0.5f, Canvas->ClipY * 0.8f - YL * 1.1f);
		}
	}

	if (UTPlayerOwner != nullptr && UTPlayerOwner->PlayerState != nullptr && !UTPlayerOwner->PlayerState->bOnlySpectator && !ScoreboardIsUp())
	{
		// draw extra life display
		AUTFlagRunPvEGameState* GS = GetWorld()->GetGameState<AUTFlagRunPvEGameState>();
		if (GS != nullptr)
		{
			Canvas->DrawColor = FColor::White;
			FText Txt = FText::Format(NSLOCTEXT("UnrealTournmanet", "KillsToLives", "{0} more kills for extra life"), FText::AsNumber(GS->KillsUntilExtraLife));
			float XL, YL;
			Canvas->TextSize(SmallFont, Txt.ToString(), XL, YL);
			Canvas->DrawText(SmallFont, Txt, FMath::TruncToFloat(Canvas->ClipX * 0.95f - XL), FMath::TruncToFloat(Canvas->ClipY * 0.15f));
		}
	}
}

void AUTFlagRunPvEHUD::GetPlayerListForIcons(TArray<AUTPlayerState*>& SortedPlayers)
{
	AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(GetWorld()->GetGameState());
	AUTPlayerState* HUDPS = GetScorerPlayerState();
	for (APlayerState* PS : GS->PlayerArray)
	{
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
		// only display elite monsters on HUD
		if (UTPS != NULL && UTPS->Team != NULL && !UTPS->bOnlySpectator && !UTPS->bIsInactive && (UTPS->GetTeamNum() != 0 || UTPS->RemainingLives > 0))
		{
			UTPS->SelectionOrder = (UTPS == HUDPS) ? -1 : UTPS->SpectatingIDTeam;
			SortedPlayers.Add(UTPS);
		}
	}
	SortedPlayers.Sort([](AUTPlayerState& A, AUTPlayerState& B) { return A.SelectionOrder > B.SelectionOrder; });
}