// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


static TAutoConsoleVariable<float> StaticToolTipWrapWidth(
	TEXT( "Slate.ToolTipWrapWidth" ),
	1000.0f,
	TEXT( "Width of Slate tool-tips before we wrap the tool-tip text" ) );


float SToolTip::GetToolTipWrapWidth()
{
	return StaticToolTipWrapWidth.GetValueOnGameThread();
}


void SToolTip::Construct( const FArguments& InArgs )
{
	TextContent = InArgs._Text;
	bIsInteractive = InArgs._IsInteractive;
	Font = InArgs._Font;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	TextMargin = InArgs._TextMargin;
	BorderImage = InArgs._BorderImage;
	
	SetContentWidget(InArgs._Content.Widget);
}


void SToolTip::SetContentWidget(const TSharedRef<SWidget>& InContentWidget)
{
	if (InContentWidget != SNullWidget::NullWidget)
	{
		// Widget content argument takes precedence over the text content.
		WidgetContent = InContentWidget;
	}

	ToolTipContent = (!WidgetContent.IsValid())
		? StaticCastSharedRef<SWidget>(
		SNew(STextBlock)
		.Text(TextContent)
		.Font(Font)
		.ColorAndOpacity(ColorAndOpacity)
		.WrapTextAt_Static(&SToolTip::GetToolTipWrapWidth)
		)
		: InContentWidget;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(BorderImage)
		.Padding(TextMargin)
		[
			ToolTipContent.ToSharedRef()
		]
	];
}


bool SToolTip::IsEmpty() const
{
	return !WidgetContent.IsValid() && TextContent.Get().IsEmpty();
}


bool SToolTip::IsInteractive() const
{
	return bIsInteractive.Get();
}
