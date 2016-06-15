// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "Color.h"
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
	FVector DrawBarPosition = FVector::ZeroVector;
	DrawBarPosition.X = ScreenPosition.X * Canvas->ClipX;
	DrawBarPosition.Y = ScreenPosition.Y * Canvas->ClipY;
	
	RenderObj_TextureAt(UnlockedPointBackground, DrawBarPosition.X, DrawBarPosition.Y, BarWidth, BarHeight);

	UnlockedPointFillTexture.RenderColor = GetAttackingTeamColor(CapturePoint->TeamNum);
	RenderObj_TextureAt(UnlockedPointFillTexture, DrawBarPosition.X, DrawBarPosition.Y, BarWidth * CapturePoint->CapturePercent, BarHeight);

	for (float LockPercent : CapturePoint->DrainLockSegments)
	{
		const float LockX = BarWidth * LockPercent;
		
		FVector DrawPipPosition = FVector::ZeroVector;
		DrawPipPosition.X = (ScreenPosition.X * Canvas->ClipX) + LockX - (UnlockedPointLockedPip.GetWidth() / 2);
		DrawPipPosition.Y = (ScreenPosition.Y * Canvas->ClipY) + UnlockPipOffset - (UnlockedPointLockedPip.GetHeight() / 2);

		//Colorize already passed lock points, but not previous points
		if (CapturePoint->CapturePercent >= LockPercent)
		{
			UnlockedPointLockedPip.RenderColor = GetAttackingTeamColor(CapturePoint->TeamNum);
		}
		else
		{
			UnlockedPointLockedPip.RenderColor = FColor::White;
		}
		RenderObj_TextureAt(UnlockedPointLockedPip, DrawPipPosition.X, DrawPipPosition.Y, UnlockedPointLockedPip.GetWidth(), UnlockedPointLockedPip.GetHeight());
	}
}

FColor UUTHUDWidget_CapturePointStatus::GetAttackingTeamColor(int ControllingTeamNum)
{
	//attacking team's color. This means the opposite of what the capture point controlling team is
	if (ControllingTeamNum == 0)
	{
		return FColor::Blue;
	}
	else if (ControllingTeamNum == 1)
	{
		return FColor::Red;
	}
	else
	{
		return FColor::Yellow;
	}
}

void UUTHUDWidget_CapturePointStatus::DrawLockedCapturePoint(AUTCTFCapturePoint* CapturePoint)
{
	FVector ScreenPosition = FVector::ZeroVector;
	ScreenPosition.X = (LockedPointStartingScreenPosition.X * Canvas->ClipX) + (AdditionalLockedPointScreenPositionOffset * (NumLockedPoints - 1)).X;
	ScreenPosition.Y = (LockedPointStartingScreenPosition.Y * Canvas->ClipY) + (AdditionalLockedPointScreenPositionOffset * (NumLockedPoints - 1)).Y;

	RenderObj_TextureAt(LockedPointTexture, ScreenPosition.X, ScreenPosition.Y, LockedPointTexture.GetWidth(), LockedPointTexture.GetHeight());
}

void UUTHUDWidget_CapturePointStatus::PostDraw(float RenderedTime)
{
	Super::PostDraw(RenderedTime);
}