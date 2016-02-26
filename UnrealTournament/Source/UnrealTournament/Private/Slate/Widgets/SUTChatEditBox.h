// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SUTEditableTextBox.h"
#include "SlateBasics.h"
#include "../SUTStyle.h"

#if !UE_SERVER

class UUTLocalPlayer;
class UNREALTOURNAMENT_API SUTChatEditBox : public SUTEditableTextBox
{
	SLATE_BEGIN_ARGS(SUTChatEditBox)
		: _Style(&SUTStyle::Get().GetWidgetStyle< FEditableTextBoxStyle >("UT.ChatEditBox"))
		, _HintText()
		, _Font()
		, _ForegroundColor()
		, _MinDesiredWidth( 0.0f )
		, _BackgroundColor()		
		, _MaxTextSize(128)
		{}

		/** The styling of the textbox */
		SLATE_STYLE_ARGUMENT( FEditableTextBoxStyle, Style )

		/** Hint text that appears when there is no text in the text box */
		SLATE_ATTRIBUTE( FText, HintText )

		/** Font color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Minimum width that a text block should be */
		SLATE_ATTRIBUTE( float, MinDesiredWidth )

		/** The color of the background/border around the editable text (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, BackgroundColor )

		/** The size of the largest string */
		SLATE_ARGUMENT(int32, MaxTextSize)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> inPlayerOwner);
	void SetChangedDelegate(FOnTextChanged inOnTextChanged);
	void SetCommittedDelegate(FOnTextCommitted inOnTextCommitted);

	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent ) override;

protected:
	virtual FReply InternalOnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

	TSharedPtr<STextBlock> TypeMsg;
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;
	FText InternalTextBuffer;
	int32 MaxTextSize;

	FOnTextChanged ExternalOnTextChanged;
	FOnTextCommitted ExternalOnTextCommitted;

	void EditableTextChanged(const FText& NewText);
	void EditableTextCommited(const FText& NewText, ETextCommit::Type CommitType);

	EVisibility TypedMsgVis() const;

	/** @return Border image for the text box based on the hovered and focused state */
	const FSlateBrush* GetBorderImage() const;


};

#endif