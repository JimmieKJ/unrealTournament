// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "SDisappearingBar.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// SDisappearingBar

void SDisappearingBar::Construct(const FArguments& InArgs)
{
	FadeCurve = FCurveSequence(0, 0.25f);
	//FadeCurve.Play();

	bDisappear = false;

	ColorAndOpacity = TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateSP(this, &SDisappearingBar::GetFadeColorAndOpacity));

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void SDisappearingBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if ( AllottedGeometry.IsUnderLocation(FSlateApplication::Get().GetCursorPos()) )
	{
		if ( !bDisappear )
		{
			FadeCurve.Play();
		}

		bDisappear = true;
	}
	else
	{
		if ( bDisappear )
		{
			FadeCurve.PlayReverse();
		}

		bDisappear = false;
	}
}

FLinearColor SDisappearingBar::GetFadeColorAndOpacity() const
{
	return FLinearColor(1, 1, 1, 1 - FadeCurve.GetLerp());
}

#undef LOCTEXT_NAMESPACE
