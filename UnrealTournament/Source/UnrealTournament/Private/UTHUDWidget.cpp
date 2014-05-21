// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

UUTHUDWidget::UUTHUDWidget(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
}

bool UUTHUDWidget::IsHidden()
{
	return bHidden;
}

void UUTHUDWidget::SetHidden(bool bIsHidden)
{
	bHidden = bIsHidden;
}

void UUTHUDWidget::PreDraw(AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	UTHUDOwner = InUTHUDOwner;
	Canvas = InCanvas;
	CanvasCenter = InCanvasCenter;

	RenderPosition.X = Canvas->ClipX * Position.X;
	RenderPosition.Y = Canvas->ClipY * Position.Y;
}

void UUTHUDWidget::Draw(float DeltaTime)
{
}

void UUTHUDWidget::PostDraw()
{
	UTHUDOwner = NULL;
	Canvas = NULL;
}



void UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, FLinearColor Color, ETextHorzPos::Type TextHorzPosition, ETextVertPos::Type TextVertPosition)
{
	FVector2D RenderPos = FVector2D(RenderPosition.X + X,RenderPosition.Y + Y);
	// Handle Justification
	if (TextHorzPosition != ETextHorzPos::Left || TextVertPosition != ETextVertPos::Top )
	{
		int32 XL, YL;
		Font->GetStringHeightAndWidth(Text.ToString(), YL, XL);

		if (TextHorzPosition != ETextHorzPos::Left)
		{
			RenderPos.X -= TextHorzPosition == ETextHorzPos::Right ? XL : XL * 0.5f;
		}

		if (TextVertPosition != ETextVertPos::Top)
		{
			RenderPos.Y -= TextVertPosition == ETextVertPos::Bottom ? XL : XL * 0.5f;
		}
	}
	FCanvasTextItem TextItem(RenderPos, Text, Font, Color);
	Canvas->DrawItem(TextItem);
}
