// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendsToolTip.h"

void SFriendsToolTip::Construct(const FArguments& InArgs)
{
	SToolTip::Construct(SToolTip::FArguments()
		.BorderImage(FStyleDefaults::GetNoBrush())
		.TextMargin(0)
		[
			SNew(SBorder)
			.BorderImage(InArgs._BackgroundImage)
			.Padding(FMargin(8.0f))
			[
				SNew(STextBlock)
				.Text(InArgs._DisplayText)
				.Font(InArgs._Font)
				.ColorAndOpacity(InArgs._TextColor)
			]
		]
	);
}

int32 SFriendsToolTip::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	OutDrawElements.QueueDeferredPainting(FSlateWindowElementList::FDeferredPaint(ChildSlot.GetWidget(), Args, AllottedGeometry, MyClippingRect, InWidgetStyle, bParentEnabled));

	return LayerId;
}
