// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
	
	RespawnChoiceACaptureComponent->TextureTarget = ConstructObject<UTextureRenderTarget2D>(UTextureRenderTarget2D::StaticClass(), this);
	RespawnChoiceACaptureComponent->TextureTarget->InitCustomFormat(1280, 720, PF_B8G8R8A8, false);
	RespawnChoiceACaptureComponent->TextureTarget->ClearColor = FLinearColor::Black;
	RespawnChoiceACaptureComponent->RegisterComponentWithWorld(Hud->GetWorld());

	RespawnChoiceBCaptureComponent->TextureTarget = ConstructObject<UTextureRenderTarget2D>(UTextureRenderTarget2D::StaticClass(), this);
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
			RespawnChoiceBCaptureComponent->SetVisibility(true);

			bHasValidRespawnCapture = true;
			LastRespawnCaptureTime = UTHUDOwner->GetWorld()->RealTimeSeconds;
		}
		else if (UTHUDOwner->GetWorld()->RealTimeSeconds - LastRespawnCaptureTime > 0.2f && UTPS->RespawnTime <= 0.25f)
		{
			RespawnChoiceACaptureComponent->SetVisibility(false);
			RespawnChoiceBCaptureComponent->SetVisibility(false);

			FVector2D Size;
			Size.X = (Canvas->SizeY / 720.0f) * RespawnChoiceACaptureComponent->TextureTarget->SizeX * 0.4f;
			Size.Y = (Canvas->SizeY / 720.0f) * RespawnChoiceACaptureComponent->TextureTarget->SizeY * 0.4f;

			FVector2D PositionA(Canvas->SizeX / 2.0f - Size.X - Canvas->SizeX * 0.05f, Canvas->SizeY * .25f);
			FVector2D PositionB(Canvas->SizeX / 2.0f + Canvas->SizeX * 0.05f, Canvas->SizeY * .25f);

			Canvas->K2_DrawTexture(RespawnChoiceACaptureComponent->TextureTarget, PositionA, Size, FVector2D(0, 0), FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
			Canvas->K2_DrawTexture(RespawnChoiceBCaptureComponent->TextureTarget, PositionB, Size, FVector2D(0, 0), FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
						
			FText ChoiceA = NSLOCTEXT("UTHUDWidth_RespawnChoice", "FIRE", "FIRE");
			DrawText(ChoiceA, PositionA.X + Size.X / 2.0f, PositionA.Y + Size.Y, UTHUDOwner->GetFontFromSizeIndex(2), 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Top);

			FText ChoiceB = NSLOCTEXT("UTHUDWidth_RespawnChoice", "ALT-FIRE", "ALT-FIRE");
			DrawText(ChoiceB, PositionB.X + Size.X / 2.0f, PositionB.Y + Size.Y, UTHUDOwner->GetFontFromSizeIndex(2), 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Top);
		}
	}
	else
	{
		bHasValidRespawnCapture = false;
	}

	Super::Draw_Implementation(DeltaTime);
}

bool UUTHUDWidget_RespawnChoice::ShouldDraw_Implementation(bool bShowScores)
{
	return !bShowScores;
}