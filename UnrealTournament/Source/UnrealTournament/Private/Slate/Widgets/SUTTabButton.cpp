// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTTabButton.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTTabButton::Construct(const FArguments& InArgs)
{
	SUTButton::Construct( SUTButton::FArguments()
		.Content()
		[
			InArgs._Content.Widget
		]
		.ButtonStyle(InArgs._ButtonStyle)
		.TextStyle(InArgs._TextStyle)
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.ContentPadding(InArgs._ContentPadding)
		.Text(InArgs._Text)
		.UTOnButtonClicked(InArgs._UTOnButtonClicked)
		.ClickMethod(InArgs._ClickMethod)
		.TouchMethod(InArgs._TouchMethod)
		.DesiredSizeScale(InArgs._DesiredSizeScale)
		.ContentScale(InArgs._ContentScale)
		.ButtonColorAndOpacity(InArgs._ButtonColorAndOpacity)
		.ForegroundColor(InArgs._ForegroundColor)
		.TextNormalColor(InArgs._TextNormalColor)
		.TextHoverColor(InArgs._TextHoverColor)
		.TextFocusColor(InArgs._TextFocusColor)
		.TextPressedColor(InArgs._TextPressedColor)
		.TextDisabledColor(InArgs._TextDisabledColor)
		.IsFocusable(InArgs._IsFocusable)
		.PressedSoundOverride(InArgs._PressedSoundOverride)
		.HoveredSoundOverride(InArgs._HoveredSoundOverride)
		.IsToggleButton(InArgs._IsToggleButton)
		.WidgetTag(InArgs._WidgetTag)
		.UTOnMouseOver(InArgs._UTOnMouseOver)
		.OnClicked(InArgs._OnClicked)
		.CaptionHAlign(InArgs._CaptionHAlign)
	);
}



#endif