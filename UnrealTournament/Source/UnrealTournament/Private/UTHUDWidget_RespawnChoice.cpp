// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_RespawnChoice.h"

UUTHUDWidget_RespawnChoice::UUTHUDWidget_RespawnChoice(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	RespawnChoiceACaptureComponent = ObjectInitializer.CreateDefaultSubobject<USceneCaptureComponent2D>(this, TEXT("RespawnChoiceACaptureComponent"));
	RespawnChoiceACaptureComponent->SetVisibility(false);
	RespawnChoiceACaptureComponent->SetHiddenInGame(false);
	RespawnChoiceBCaptureComponent = ObjectInitializer.CreateDefaultSubobject<USceneCaptureComponent2D>(this, TEXT("RespawnChoiceBCaptureComponent"));
	RespawnChoiceBCaptureComponent->SetVisibility(false);
	RespawnChoiceBCaptureComponent->SetHiddenInGame(false);

	// Not scaling automatically right now
	bScaleByDesignedResolution = false;
}

void UUTHUDWidget_RespawnChoice::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
	
	RespawnChoiceACaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>(this, UTextureRenderTarget2D::StaticClass());
	RespawnChoiceACaptureComponent->TextureTarget->InitCustomFormat(1280, 720, PF_B8G8R8A8, false);
	RespawnChoiceACaptureComponent->TextureTarget->ClearColor = FLinearColor::Black;
	RespawnChoiceACaptureComponent->RegisterComponentWithWorld(Hud->GetWorld());

	RespawnChoiceBCaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>(this, UTextureRenderTarget2D::StaticClass());
	RespawnChoiceBCaptureComponent->TextureTarget->InitCustomFormat(1280, 720, PF_B8G8R8A8, false);
	RespawnChoiceBCaptureComponent->TextureTarget->ClearColor = FLinearColor::Black;
	RespawnChoiceBCaptureComponent->RegisterComponentWithWorld(Hud->GetWorld());
}

void UUTHUDWidget_RespawnChoice::Draw_Implementation(float DeltaTime)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);
	if (UTPS != nullptr && UTPS->RespawnChoiceA && UTPS->RespawnChoiceB)
	{
		if (!bHasValidRespawnCapture)
		{
			RespawnChoiceACaptureComponent->SetWorldLocationAndRotation(UTPS->RespawnChoiceA->GetActorLocation(), UTPS->RespawnChoiceA->GetActorRotation());
			RespawnChoiceBCaptureComponent->SetWorldLocationAndRotation(UTPS->RespawnChoiceB->GetActorLocation(), UTPS->RespawnChoiceB->GetActorRotation());
			RespawnChoiceACaptureComponent->SetVisibility(true);
			RespawnChoiceACaptureComponent->UpdateContent();
			RespawnChoiceBCaptureComponent->SetVisibility(true);
			RespawnChoiceBCaptureComponent->UpdateContent();

			bHasValidRespawnCapture = true;
			LastRespawnCaptureTime = UTHUDOwner->GetWorld()->RealTimeSeconds;
		}
		else if (UTHUDOwner->GetWorld()->RealTimeSeconds - LastRespawnCaptureTime > 0.2f)
		{
			RespawnChoiceACaptureComponent->SetVisibility(false);
			RespawnChoiceBCaptureComponent->SetVisibility(false);

			if (UTPS->RespawnTime <= 0.25f)
			{
				FVector2D DrawSize;
				DrawSize.X = (Canvas->SizeY / 720.0f) * RespawnChoiceACaptureComponent->TextureTarget->SizeX * 0.4f;
				DrawSize.Y = (Canvas->SizeY / 720.0f) * RespawnChoiceACaptureComponent->TextureTarget->SizeY * 0.4f;

				FVector2D PositionA(Canvas->SizeX / 2.0f - DrawSize.X - Canvas->SizeX * 0.05f, Canvas->SizeY * .25f);
				FVector2D PositionB(Canvas->SizeX / 2.0f + Canvas->SizeX * 0.05f, Canvas->SizeY * .25f);

				// Draw the Backgrounds
				FLinearColor ChoiceColor = UTPS->bChosePrimaryRespawnChoice ? FLinearColor::Yellow : FLinearColor::White;
				DrawTexture(UTHUDOwner->ScoreboardAtlas, PositionA.X - 0.05f*DrawSize.X, PositionA.Y - 0.15f*DrawSize.Y, 1.1f*DrawSize.X, 4, 4, 2, 124, 8, 1.0, ChoiceColor);
				DrawTexture(UTHUDOwner->ScoreboardAtlas, PositionA.X - 0.05f*DrawSize.X, PositionA.Y - 0.15f*DrawSize.Y + 4, 1.1f*DrawSize.X, 1.3f*DrawSize.Y - 8, 4, 10, 124, 112, 1.0, ChoiceColor);
				DrawTexture(UTHUDOwner->ScoreboardAtlas, PositionA.X - 0.05f*DrawSize.X, PositionA.Y + 1.15f*DrawSize.Y - 4, 1.1f*DrawSize.X, 4, 4, 122, 124, 8, 1.0, ChoiceColor);

				ChoiceColor = UTPS->bChosePrimaryRespawnChoice ? FLinearColor::White : FLinearColor::Yellow;
				DrawTexture(UTHUDOwner->ScoreboardAtlas, PositionB.X - 0.05f*DrawSize.X, PositionB.Y - 0.15f*DrawSize.Y, 1.1f*DrawSize.X, 4, 4, 2, 124, 8, 1.0, ChoiceColor);
				DrawTexture(UTHUDOwner->ScoreboardAtlas, PositionB.X - 0.05f*DrawSize.X, PositionB.Y - 0.15f*DrawSize.Y + 4, 1.1f*DrawSize.X, 1.3f*DrawSize.Y - 8, 4, 10, 124, 112, 1.0, ChoiceColor);
				DrawTexture(UTHUDOwner->ScoreboardAtlas, PositionB.X - 0.05f*DrawSize.X, PositionB.Y + 1.15f*DrawSize.Y - 4, 1.1f*DrawSize.X, 4, 4, 122, 124, 8, 1.0, ChoiceColor);

				Canvas->K2_DrawTexture(RespawnChoiceACaptureComponent->TextureTarget, PositionA, DrawSize, FVector2D(0, 0), FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
				Canvas->K2_DrawTexture(RespawnChoiceBCaptureComponent->TextureTarget, PositionB, DrawSize, FVector2D(0, 0), FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
						
				FText ChoiceA = NSLOCTEXT("UTHUDWidth_RespawnChoice", "FIRE", "FIRE to select this spawn");
				DrawText(ChoiceA, PositionA.X + DrawSize.X / 2.0f, PositionA.Y + DrawSize.Y, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Top);

				FText ChoiceB = NSLOCTEXT("UTHUDWidth_RespawnChoice", "ALT-FIRE", "ALT-FIRE to select this spawn");
				DrawText(ChoiceB, PositionB.X + DrawSize.X / 2.0f, PositionB.Y + DrawSize.Y, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Top);
			}
		}
	}
	else
	{
		RespawnChoiceACaptureComponent->SetVisibility(false);
		RespawnChoiceBCaptureComponent->SetVisibility(false);
		bHasValidRespawnCapture = false;
	}
	Super::Draw_Implementation(DeltaTime);
}

bool UUTHUDWidget_RespawnChoice::ShouldDraw_Implementation(bool bShowScores)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);
	if (!bShowScores && UTPS != nullptr && !UTPS->bOnlySpectator && UTPS->RespawnChoiceA && UTPS->RespawnChoiceB && UTGameState && (UTGameState->IsMatchInProgress() || UTGameState->IsMatchInCountdown() || UTGameState->IsMatchIntermission()))
	{
		return true;
	}
	if (bHasValidRespawnCapture)
	{
		RespawnChoiceACaptureComponent->SetVisibility(false);
		RespawnChoiceBCaptureComponent->SetVisibility(false);
		bHasValidRespawnCapture = false;
	}
	return false;
}