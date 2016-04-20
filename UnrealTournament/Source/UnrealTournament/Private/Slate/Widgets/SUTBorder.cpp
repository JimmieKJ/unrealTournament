// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTBorder.h"

#if !UE_SERVER

void SUTBorder::Construct(const FArguments& InArgs)
{
	bIsAnimating = false;
	OnAnimEnd = InArgs._OnAnimEnd;
	SBorder::Construct( SBorder::FArguments()
		.Content()
		[
			InArgs._Content.Widget
		]

		.HAlign( InArgs._HAlign )
		.VAlign( InArgs._VAlign )
		.Padding( InArgs._Padding )
		.OnMouseButtonDown(InArgs._OnMouseButtonDown)
		.OnMouseButtonUp(InArgs._OnMouseButtonUp)
		.OnMouseMove(InArgs._OnMouseMove)
		.OnMouseDoubleClick(InArgs._OnMouseDoubleClick)
		.BorderImage( InArgs._BorderImage )
		.ContentScale( InArgs._ContentScale )
		.DesiredSizeScale( InArgs._DesiredSizeScale )
		.ColorAndOpacity( InArgs._ColorAndOpacity )
		.BorderBackgroundColor( InArgs._BorderBackgroundColor)
		.ForegroundColor( InArgs._ForegroundColor )
		.ShowEffectWhenDisabled( InArgs._ShowEffectWhenDisabled )

	);
}

void SUTBorder::Animate(FVector2D StartTransform, FVector2D EndTransform, float StartOpacity, float EndOpacity, float Duration)
{
	FVector2D StartingTransform = StartTransform;
	// We are currently animating, so use our current values.
	if (bIsAnimating && AnimTransforms.Num() == 2 && AnimDuration > 0)
	{
		FVector2D NewStartTransform = FMath::InterpEaseOut<FVector2D>(AnimTransforms[0], AnimTransforms[1], (AnimTimer / AnimDuration),2.0);
	}

	float StartingOpacity = StartOpacity;
	if (bIsAnimating && AnimOpacity.Num() == 2 && AnimDuration > 0)
	{
		StartingOpacity = FMath::InterpEaseOut<float>(AnimOpacity[0],AnimOpacity[1], (AnimTimer / AnimDuration),2.0);
	}
	
	if (bIsAnimating)
	{
		Duration = Duration - (AnimDuration - AnimTimer);
	}

	bIsAnimating = true;
	AnimTransforms.Empty();
	AnimTransforms.Add(StartingTransform);
	AnimTransforms.Add(EndTransform);
	AnimOpacity.Empty();
	AnimOpacity.Add(StartingOpacity);
	AnimOpacity.Add(EndOpacity);

	AnimTimer = 0.0f;
	AnimDuration = Duration;

	// Initialize
	UpdateAnim(0.0f);

}

void SUTBorder::UpdateAnim(float DeltaTime)
{
	if (bIsAnimating)
	{
		if (AnimDuration > 0.0f)
		{
			AnimTimer += DeltaTime;
			float Alpha = FMath::Clamp<float>(AnimTimer / AnimDuration, 0.0f, 1.0f);

			FVector2D FinalTransform = FMath::InterpEaseOut<FVector2D>(AnimTransforms[0], AnimTransforms[1], Alpha,2.0);
			float FinalOpacity = FMath::InterpEaseOut<float>(AnimOpacity[0],AnimOpacity[1], Alpha,2.0);

			SetRenderTransform(FinalTransform);
			SetColorAndOpacity(FLinearColor(1.0,1.0,1.0,FinalOpacity));
			SetBorderBackgroundColor(FLinearColor(1.0,1.0,1.0,FinalOpacity));
		}

		if (AnimDuration == 0 || AnimTimer >= AnimDuration)
		{
			bIsAnimating = false;
			OnAnimEnd.ExecuteIfBound();
		}
	}
}


void SUTBorder::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UpdateAnim(InDeltaTime);
}



#endif