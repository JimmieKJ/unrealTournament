// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

	PaddingOverride = InArgs._Padding;
	FontOverride = InArgs._Font;
	ForegroundColorOverride = InArgs._ForegroundColor;
	BackgroundColorOverride = InArgs._BackgroundColor;
	ReadOnlyForegroundColorOverride = InArgs._ReadOnlyForegroundColor;

	TAttribute<FMargin> Padding = PaddingOverride.IsSet() ? PaddingOverride : InArgs._Style->Padding;
	TAttribute<FSlateFontInfo> Font = FontOverride.IsSet() ? FontOverride : InArgs._Style->Font;
	TAttribute<FSlateColor> BorderForegroundColor = ForegroundColorOverride.IsSet() ? ForegroundColorOverride : InArgs._Style->ForegroundColor;
	TAttribute<FSlateColor> BackgroundColor = BackgroundColorOverride.IsSet() ? BackgroundColorOverride : InArgs._Style->BackgroundColor;
	ReadOnlyForegroundColor = ReadOnlyForegroundColorOverride.IsSet() ? ReadOnlyForegroundColorOverride : InArgs._Style->ReadOnlyForegroundColor;

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SEditableTextBox::GetBorderImage )
		.BorderBackgroundColor( BackgroundColor )
		.ForegroundColor( BorderForegroundColor )
		.Padding( 0 )
		[
			SAssignNew( Box, SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			[
				SAssignNew(PaddingBox, SBox)
				.Padding( Padding )
				.VAlign(VAlign_Center)
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
					.AllowContextMenu( InArgs._AllowContextMenu )
					.OnContextMenuOpening( InArgs._OnContextMenuOpening )
					.OnTextChanged( InArgs._OnTextChanged )
					.OnTextCommitted( InArgs._OnTextCommitted )
					.MinDesiredWidth( InArgs._MinDesiredWidth )
					.SelectAllTextOnCommit( InArgs._SelectAllTextOnCommit )
					.OnKeyDownHandler( InArgs._OnKeyDownHandler )
					.VirtualKeyboardType( InArgs._VirtualKeyboardType )
					.TextShapingMethod( InArgs._TextShapingMethod )
					.TextFlowDirection( InArgs._TextFlowDirection )
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

	check(Style);

	if (!PaddingOverride.IsSet() && PaddingBox.IsValid())
	{
		PaddingBox->SetPadding(Style->Padding);
	}

	if (!FontOverride.IsSet() && EditableText.IsValid())
	{
		EditableText->SetFont(Style->Font);
	}

	if (!ForegroundColorOverride.IsSet())
	{
		SetForegroundColor(Style->ForegroundColor);
	}

	if (!BackgroundColorOverride.IsSet())
	{
		SetBorderBackgroundColor(Style->BackgroundColor);
	}

	if (!ReadOnlyForegroundColorOverride.IsSet())
	{
		ReadOnlyForegroundColor = Style->ReadOnlyForegroundColor;
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


void SEditableTextBox::SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler)
{
	EditableText->SetOnKeyDownHandler(InOnKeyDownHandler);
}


void SEditableTextBox::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	EditableText->SetTextShapingMethod(InTextShapingMethod);
}


void SEditableTextBox::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	EditableText->SetTextFlowDirection(InTextFlowDirection);
}


bool SEditableTextBox::AnyTextSelected() const
{
	return EditableText->AnyTextSelected();
}


void SEditableTextBox::SelectAllText()
{
	EditableText->SelectAllText();
}


void SEditableTextBox::ClearSelection()
{
	EditableText->ClearSelection();
}


FText SEditableTextBox::GetSelectedText() const
{
	return EditableText->GetSelectedText();
}

bool SEditableTextBox::HasError() const
{
	return ErrorReporting.IsValid() && ErrorReporting->HasError();
}

const FSlateBrush* SEditableTextBox::GetBorderImage() const
{
	if ( EditableText->IsTextReadOnly() )
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


FReply SEditableTextBox::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape && EditableText->HasKeyboardFocus())
	{
		// Clear focus
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


void SEditableTextBox::SetFont(const TAttribute<FSlateFontInfo>& InFont)
{
	FontOverride = InFont;

	EditableText->SetFont(FontOverride.IsSet() ? FontOverride : Style->Font);
}


void SEditableTextBox::SetTextBoxForegroundColor(const TAttribute<FSlateColor>& InForegroundColor)
{
	ForegroundColorOverride = InForegroundColor;

	SetForegroundColor(ForegroundColorOverride.IsSet() ? ForegroundColorOverride : Style->ForegroundColor);
}

void SEditableTextBox::SetTextBoxBackgroundColor(const TAttribute<FSlateColor>& InBackgroundColor)
{
	BackgroundColorOverride = InBackgroundColor;

	SetBorderBackgroundColor(BackgroundColorOverride.IsSet() ? BackgroundColorOverride : Style->BackgroundColor);
}


void SEditableTextBox::SetReadOnlyForegroundColor(const TAttribute<FSlateColor>& InReadOnlyForegroundColor)
{
	ReadOnlyForegroundColorOverride = InReadOnlyForegroundColor;

	ReadOnlyForegroundColor = ReadOnlyForegroundColorOverride.IsSet() ? ReadOnlyForegroundColorOverride : Style->ReadOnlyForegroundColor;
}


void SEditableTextBox::SetMinimumDesiredWidth(const TAttribute<float>& InMinimumDesiredWidth)
{
	EditableText->SetMinDesiredWidth(InMinimumDesiredWidth);
}


void SEditableTextBox::SetIsCaretMovedWhenGainFocus(const TAttribute<bool>& InIsCaretMovedWhenGainFocus)
{
	EditableText->SetIsCaretMovedWhenGainFocus(InIsCaretMovedWhenGainFocus);
}


void SEditableTextBox::SetSelectAllTextWhenFocused(const TAttribute<bool>& InSelectAllTextWhenFocused)
{
	EditableText->SetSelectAllTextWhenFocused(InSelectAllTextWhenFocused);
}


void SEditableTextBox::SetRevertTextOnEscape(const TAttribute<bool>& InRevertTextOnEscape)
{
	EditableText->SetRevertTextOnEscape(InRevertTextOnEscape);
}


void SEditableTextBox::SetClearKeyboardFocusOnCommit(const TAttribute<bool>& InClearKeyboardFocusOnCommit)
{
	EditableText->SetClearKeyboardFocusOnCommit(InClearKeyboardFocusOnCommit);
}


void SEditableTextBox::SetSelectAllTextOnCommit(const TAttribute<bool>& InSelectAllTextOnCommit)
{
	EditableText->SetSelectAllTextOnCommit(InSelectAllTextOnCommit);
}

void SEditableTextBox::SetAllowContextMenu(TAttribute<bool> InAllowContextMenu)
{
	EditableText->SetAllowContextMenu(InAllowContextMenu);
}
