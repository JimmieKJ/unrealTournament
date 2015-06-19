// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SEditableTextBox::Construct( const FArguments& InArgs )
{
	check (InArgs._Style);
	SetStyle(InArgs._Style);

	TAttribute<FMargin> Padding = InArgs._Padding.IsSet() ? InArgs._Padding : InArgs._Style->Padding;
	TAttribute<FSlateFontInfo> Font = InArgs._Font.IsSet() ? InArgs._Font : InArgs._Style->Font;
	TAttribute<FSlateColor> ForegroundColor = InArgs._ForegroundColor.IsSet() ? InArgs._ForegroundColor : InArgs._Style->ForegroundColor;
	TAttribute<FSlateColor> BackgroundColor = InArgs._BackgroundColor.IsSet() ? InArgs._BackgroundColor : InArgs._Style->BackgroundColor;
	ReadOnlyForegroundColor = InArgs._ReadOnlyForegroundColor.IsSet() ? InArgs._ReadOnlyForegroundColor : InArgs._Style->ReadOnlyForegroundColor;

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SEditableTextBox::GetBorderImage )
		.BorderBackgroundColor( BackgroundColor )
		.ForegroundColor( ForegroundColor )
		.Padding( 0 )
		[
			SAssignNew( Box, SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			[
				SNew( SBox )
				.Padding( Padding )
				[
					SAssignNew( EditableText, SEditableText )
					.Text( InArgs._Text )
					.HintText( InArgs._HintText )
					.Font( Font )
					.IsReadOnly( InArgs._IsReadOnly )
					.IsPassword( InArgs._IsPassword )
					.IsCaretMovedWhenGainFocus( InArgs._IsCaretMovedWhenGainFocus )
					.SelectAllTextWhenFocused( InArgs._SelectAllTextWhenFocused )
					.RevertTextOnEscape( InArgs._RevertTextOnEscape )
					.ClearKeyboardFocusOnCommit( InArgs._ClearKeyboardFocusOnCommit )
					.OnTextChanged( InArgs._OnTextChanged )
					.OnTextCommitted( InArgs._OnTextCommitted )
					.MinDesiredWidth( InArgs._MinDesiredWidth )
					.SelectAllTextOnCommit( InArgs._SelectAllTextOnCommit )
				]
			]
		]
	);

	ErrorReporting = InArgs._ErrorReporting;
	if ( ErrorReporting.IsValid() )
	{
		Box->AddSlot()
		.AutoWidth()
		.Padding(3,0)
		[
			ErrorReporting->AsWidget()
		];
	}

}


void SEditableTextBox::SetStyle(const FEditableTextBoxStyle* InStyle)
{
	Style = InStyle;

	if ( Style == nullptr )
	{
		FArguments Defaults;
		Style = Defaults._Style;
	}

	BorderImageNormal = &Style->BackgroundImageNormal;
	BorderImageHovered = &Style->BackgroundImageHovered;
	BorderImageFocused = &Style->BackgroundImageFocused;
	BorderImageReadOnly = &Style->BackgroundImageReadOnly;
}


void SEditableTextBox::SetText( const TAttribute< FText >& InNewText )
{
	EditableText->SetText( InNewText );
}


void SEditableTextBox::SetError( const FText& InError )
{
	SetError( InError.ToString() );
}


void SEditableTextBox::SetError( const FString& InError )
{
	const bool bHaveError = !InError.IsEmpty();

	if ( !ErrorReporting.IsValid() )
	{
		// No error reporting was specified; make a default one
		TSharedPtr<SPopupErrorText> ErrorTextWidget;
		Box->AddSlot()
		.AutoWidth()
		.Padding(3,0)
		[
			SAssignNew( ErrorTextWidget, SPopupErrorText )
		];
		ErrorReporting = ErrorTextWidget;
	}

	ErrorReporting->SetError( InError );
}


/** @return Border image for the text box based on the hovered and focused state */
const FSlateBrush* SEditableTextBox::GetBorderImage() const
{
	if ( EditableText->GetIsReadOnly() )
	{
		return BorderImageReadOnly;
	}
	else if ( EditableText->HasKeyboardFocus() )
	{
		return BorderImageFocused;
	}
	else
	{
		if ( EditableText->IsHovered() )
		{
			return BorderImageHovered;
		}
		else
		{
			return BorderImageNormal;
		}
	}
}


bool SEditableTextBox::SupportsKeyboardFocus() const
{
	return StaticCastSharedPtr<SWidget>(EditableText)->SupportsKeyboardFocus();
}


bool SEditableTextBox::HasKeyboardFocus() const
{
	// Since keyboard focus is forwarded to our editable text, we will test it instead
	return SBorder::HasKeyboardFocus() || EditableText->HasKeyboardFocus();
}


FReply SEditableTextBox::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	FReply Reply = FReply::Handled();

	if ( InFocusEvent.GetCause() != EFocusCause::Cleared )
	{
		// Forward keyboard focus to our editable text widget
		Reply.SetUserFocus(EditableText.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}


FReply SEditableTextBox::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	FKey Key = InKeyEvent.GetKey();

	if( Key == EKeys::Escape && EditableText->HasKeyboardFocus() )
	{
		// Clear selection
		EditableText->ClearSelection();
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Cleared);
	}

	return FReply::Unhandled();
}


void SEditableTextBox::SetHintText(const TAttribute< FText >& InHintText)
{
	EditableText->SetHintText(InHintText);
}


void SEditableTextBox::SetIsReadOnly(TAttribute< bool > InIsReadOnly)
{
	EditableText->SetIsReadOnly(InIsReadOnly);
}


void SEditableTextBox::SetIsPassword(TAttribute< bool > InIsPassword)
{
	EditableText->SetIsPassword(InIsPassword);
}
