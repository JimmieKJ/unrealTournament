// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunPvEHUD.h"

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
}