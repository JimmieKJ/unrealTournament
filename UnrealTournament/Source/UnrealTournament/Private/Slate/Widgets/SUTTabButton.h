// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SUTButton.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTTabButton : public SUTButton
{
	SLATE_BEGIN_ARGS(SUTTabButton)
		: _Content()
		, _ButtonStyle( &FCoreStyle::Get().GetWidgetStyle< FButtonStyle >( "Button" ) )
		, _TextStyle( &FCoreStyle::Get().GetWidgetStyle< FTextBlockStyle >("NormalText") )
		, _HAlign( HAlign_Fill )
		, _VAlign( VAlign_Fill )
		, _ContentPadding(FMargin(4.0, 2.0))
		, _Text()
		, _ClickMethod( EButtonClickMethod::DownAndUp )
		, _TouchMethod( EButtonTouchMethod::DownAndUp )
		, _DesiredSizeScale( FVector2D(1,1) )
		, _ContentScale( FVector2D(1,1) )
		, _ButtonColorAndOpacity(FLinearColor::White)
		, _ForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _TextNormalColor(SUTStyle::GetSlateColor("TabNormalTextColor"))
		, _TextHoverColor(SUTStyle::GetSlateColor("TabHoverTextColor"))
		, _TextFocusColor(SUTStyle::GetSlateColor("TabFocusTextColor"))
		, _TextPressedColor(SUTStyle::GetSlateColor("TabPressedTextColor"))
		, _TextDisabledColor(SUTStyle::GetSlateColor("TabDisabledTextColor"))
		, _IsFocusable( true )
		, _IsToggleButton(false)
		, _WidgetTag(0)
		, _CaptionHAlign( HAlign_Left )

		{}

		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT( FArguments, Content )

		/** The visual style of the button */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )

		/** The text style of the button */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )

		/** Horizontal alignment */
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )

		/** Vertical alignment */
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )
	
		/** Spacing between button's border and the content. */
		SLATE_ATTRIBUTE( FMargin, ContentPadding )
	
		/** The text to display in this button, if no custom content is specified */
		SLATE_ATTRIBUTE(FText, Text)
	
		/** Called when the button is clicked */
		SLATE_EVENT( FUTButtonClick, UTOnButtonClicked )

		/** Sets the rules to use for determining whether the button was clicked.  This is an advanced setting and generally should be left as the default. */
		SLATE_ARGUMENT( EButtonClickMethod::Type, ClickMethod )

		/** How should the button be clicked with touch events? */
		SLATE_ARGUMENT( EButtonTouchMethod::Type, TouchMethod )

		SLATE_ATTRIBUTE( FVector2D, DesiredSizeScale )

		SLATE_ATTRIBUTE( FVector2D, ContentScale )

		SLATE_ATTRIBUTE( FSlateColor, ButtonColorAndOpacity )

		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )

		SLATE_ATTRIBUTE( FSlateColor, TextNormalColor)

		SLATE_ATTRIBUTE( FSlateColor, TextHoverColor )

		SLATE_ATTRIBUTE( FSlateColor, TextFocusColor )

		SLATE_ATTRIBUTE( FSlateColor, TextPressedColor )

		SLATE_ATTRIBUTE( FSlateColor, TextDisabledColor )


		/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
		SLATE_ARGUMENT( bool, IsFocusable )

		/** The sound to play when the button is pressed */
		SLATE_ARGUMENT( TOptional<FSlateSound>, PressedSoundOverride )

		/** The sound to play when the button is hovered */
		SLATE_ARGUMENT( TOptional<FSlateSound>, HoveredSoundOverride )

		/** Determines if this is a toggle button or not. */
		SLATE_ARGUMENT( bool, IsToggleButton )

		SLATE_ARGUMENT(int32, WidgetTag)

		SLATE_EVENT( FUTMouseOver, UTOnMouseOver)

		SLATE_EVENT( FOnClicked, OnClicked )

		/** Horizontal alignment */
		SLATE_ARGUMENT( EHorizontalAlignment, CaptionHAlign )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

};

#endif