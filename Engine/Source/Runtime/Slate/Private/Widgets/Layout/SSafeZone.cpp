// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SSafeZone.h"


void SSafeZone::Construct(const FArguments& InArgs)
{
	SBox::Construct(SBox::FArguments()
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(this, &SSafeZone::GetSafeZonePadding)
		[
			InArgs._Content.Widget
		]
	);

	// @todo: should we have a function that just returns the safe area size?
	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics(Metrics);

	if (InArgs._IsTitleSafe)
	{
		SafeMargin = FMargin(Metrics.TitleSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y);
	}
	else
	{
		SafeMargin = FMargin(Metrics.ActionSafePaddingSize.X, Metrics.ActionSafePaddingSize.Y);
	}

	Padding = InArgs._Padding;
}

FMargin SSafeZone::GetSafeZonePadding() const
{
	// return either the TitleSafe or the ActionSafe size, added to the user padding
	return Padding.Get() + SafeMargin;
}
