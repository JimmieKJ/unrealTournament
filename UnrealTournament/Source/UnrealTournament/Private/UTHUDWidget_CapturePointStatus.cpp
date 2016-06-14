// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CapturePointStatus.h"

UUTHUDWidget_CapturePointStatus::UUTHUDWidget_CapturePointStatus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UUTHUDWidget_CapturePointStatus::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
}

void UUTHUDWidget_CapturePointStatus::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);

	for (TActorIterator<AUTCTFCapturePoint> It(UTHUDOwner->GetWorld()); It; ++It)
	{
		AUTCTFCapturePoint* CapPoint = *It;
		if (CapPoint)
		{
			CapturePoints.AddUnique(CapPoint);
		}
	}
}

void UUTHUDWidget_CapturePointStatus::Draw_Implementation(float DeltaTime)
{
	NumUnlockedPoints = 0;
	NumLockedPoints = 0;

	for (AUTCTFCapturePoint* CapturePoint : CapturePoints)
	{
		if (CapturePoint)
		{
			if (CapturePoint->bIsActive)
			{
				NumUnlockedPoints++;
				DrawUnlockedCapturePoint(CapturePoint);
			}
			else
			{
				NumLockedPoints++;
				DrawLockedCapturePoint(CapturePoint);
			}
		}
	}
}

void UUTHUDWidget_CapturePointStatus::DrawUnlockedCapturePoint(AUTCTFCapturePoint* CapturePoint)
{
	FVector ScreenPosition = UnlockedPointStartingScreenPosition + (AdditionalUnlockedPointScreenPositionOffset * NumLockedPoints);
	ScreenPosition.X = ScreenPosition.X * Canvas->ClipX;
	ScreenPosition.Y = ScreenPosition.Y * Canvas->ClipY;
	
	RenderObj_TextureAt(UnlockedPointBackground, ScreenPosition.X, ScreenPosition.Y, BarWidth, BarHeight);
	RenderObj_TextureAt(UnlockedPointFillTexture, ScreenPosition.X, ScreenPosition.Y, BarWidth * CapturePoint->CapturePercent, BarHeight);

	for (float LockPercent : CapturePoint->DrainLockSegments)
	{
		const float LockX = BarWidth * LockPercent;
		
		FVector DrawPipPosition = FVector::ZeroVector;
		DrawPipPosition.X = ScreenPosition.X + LockX;
		DrawPipPosition.Y = ScreenPosition.Y + UnlockPipOffset;

		RenderObj_TextureAt(LockedPointTexture, DrawPipPosition.X, DrawPipPosition.Y, LockedPointTexture.UVs.UL, LockedPointTexture.UVs.VL);
	}
}

void UUTHUDWidget_CapturePointStatus::DrawLockedCapturePoint(AUTCTFCapturePoint* CapturePoint)
{
	FVector ScreenPosition = LockedPointStartingScreenPosition + (AdditionalLockedPointScreenPositionOffset * NumLockedPoints);
	ScreenPosition.X = ScreenPosition.X * Canvas->ClipX;
	ScreenPosition.Y = ScreenPosition.Y * Canvas->ClipY;

	RenderObj_TextureAt(LockedPointTexture, ScreenPosition.X, ScreenPosition.Y, LockedPointTexture.UVs.UL, LockedPointTexture.UVs.VL);
}

void UUTHUDWidget_CapturePointStatus::PostDraw(float RenderedTime)
{
	Super::PostDraw(RenderedTime);
}
