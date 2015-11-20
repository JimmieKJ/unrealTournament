// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

#if WITH_FANCY_TEXT

namespace
{
	/**
     * Helper function to solve some issues with ternary operators inside construction of a widget.
	 */
	TSharedRef< SWidget > AsWidgetRef( const TSharedPtr< SWidget >& InWidget )
	{
		if ( InWidget.IsValid() )
		{
			return InWidget.ToSharedRef();
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
}

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SMultiLineEditableTextBox::Construct( const FArguments& InArgs )
{
	check (InArgs._Style);
	Style = InArgs._Style;

	BorderImageNormal = &InArgs._Style->BackgroundImageNormal;
	BorderImageHovered = &InArgs._Style->BackgroundImageHovered;
	BorderImageFocused = &InArgs._Style->BackgroundImageFocused;
	BorderImageReadOnly = &InArgs._Style->BackgroundImageReadOnly;

	PaddingOverride = InArgs._Padding;
	HScrollBarPaddingOverride = InArgs._HScrollBarPadding;
	VScrollBarPaddingOverride = InArgs._VScrollBarPadding;
	FontOverride = InArgs._Font;
	ForegroundColorOverride = InArgs._ForegroundColor;
	BackgroundColorOverride = InArgs._BackgroundColor;
	ReadOnlyForegroundColorOverride = InArgs._ReadOnlyForegroundColor;

	TAttribute<FMargin> Padding = InArgs._Padding.IsSet() ? InArgs._Padding : InArgs._Style->Padding;
	TAttribute<FMargin> HScrollBarPadding = InArgs._HScrollBarPadding.IsSet() ? InArgs._HScrollBarPadding : InArgs._Style->HScrollBarPadding;
	TAttribute<FMargin> VScrollBarPadding = InArgs._VScrollBarPadding.IsSet() ? InArgs._VScrollBarPadding : InArgs._Style->VScrollBarPadding;
	TAttribute<FSlateFontInfo> Font = InArgs._Font.IsSet() ? InArgs._Font : InArgs._Style->Font;
	TAttribute<FSlateColor> BorderForegroundColor = InArgs._ForegroundColor.IsSet() ? InArgs._ForegroundColor : InArgs._Style->ForegroundColor;
	TAttribute<FSlateColor> BackgroundColor = InArgs._BackgroundColor.IsSet() ? InArgs._BackgroundColor : InArgs._Style->BackgroundColor;
	ReadOnlyForegroundColor = InArgs._ReadOnlyForegroundColor.IsSet() ? InArgs._ReadOnlyForegroundColor : InArgs._Style->ReadOnlyForegroundColor;

	bHasExternalHScrollBar = InArgs._HScrollBar.IsValid();
	HScrollBar = InArgs._HScrollBar;
	if (!HScrollBar.IsValid())
	{
		// Create and use our own scrollbar
		HScrollBar = SNew(SScrollBar)
			.Style(&InArgs._Style->ScrollBarStyle)
			.Orientation(Orient_Horizontal)
			.AlwaysShowScrollbar(InArgs._AlwaysShowScrollbars)
			.Thickness(FVector2D(5.0f, 5.0f));
	}
	
	bHasExternalVScrollBar = InArgs._VScrollBar.IsValid();
	VScrollBar = InArgs._VScrollBar;
	if (!VScrollBar.IsValid())
	{
		// Create and use our own scrollbar
		VScrollBar = SNew(SScrollBar)
			.Style(&InArgs._Style->ScrollBarStyle)
			.Orientation(Orient_Vertical)
			.AlwaysShowScrollbar(InArgs._AlwaysShowScrollbars)
			.Thickness(FVector2D(5.0f, 5.0f));
	}

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SMultiLineEditableTextBox::GetBorderImage )
		.BorderBackgroundColor( BackgroundColor )
		.ForegroundColor( BorderForegroundColor )
		.Padding( Padding )
		[
			SAssignNew( Box, SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillHeight(1)
				[
					SAssignNew( EditableText, SMultiLineEditableText )
					.Text( InArgs._Text )
					.HintText( InArgs._HintText )
					.TextStyle( InArgs._TextStyle )
					.Marshaller( InArgs._Marshaller )
					.Font( Font )
					.IsReadOnly( InArgs._IsReadOnly )
					.OnContextMenuOpening( InArgs._OnContextMenuOpening )
					.OnTextChanged( InArgs._OnTextChanged )
					.OnTextCommitted( InArgs._OnTextCommitted )
					.OnCursorMoved( InArgs._OnCursorMoved )
					.ContextMenuExtender( InArgs._ContextMenuExtender )
					.Justification(InArgs._Justification)
					.RevertTextOnEscape(InArgs._RevertTextOnEscape)
					.SelectAllTextWhenFocused(InArgs._SelectAllTextWhenFocused)
					.ClearTextSelectionOnFocusLoss(InArgs._ClearTextSelectionOnFocusLoss)
					.ClearKeyboardFocusOnCommit(InArgs._ClearKeyboardFocusOnCommit)
					.LineHeightPercentage(InArgs._LineHeightPercentage)
					.Margin(InArgs._Margin)
					.WrapTextAt(InArgs._WrapTextAt)
					.AutoWrapText(InArgs._AutoWrapText)
					.HScrollBar(HScrollBar)
					.VScrollBar(VScrollBar)
					.OnHScrollBarUserScrolled(InArgs._OnHScrollBarUserScrolled)
					.OnVScrollBarUserScrolled(InArgs._OnVScrollBarUserScrolled)
					.OnKeyDownHandler( InArgs._OnKeyDownHandler)
					.ModiferKeyForNewLine(InArgs._ModiferKeyForNewLine)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(HScrollBarPaddingBox, SBox)
					.Padding(HScrollBarPadding)
					[
						AsWidgetRef( HScrollBar )
					]
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(VScrollBarPaddingBox, SBox)
				.Padding(VScrollBarPadding)
				[
					AsWidgetRef( VScrollBar )
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

void SMultiLineEditableTextBox::SetStyle(const FEditableTextBoxStyle* InStyle)
{
	check(InStyle);
	Style = InStyle;

	if (!PaddingOverride.IsSet())
	{
		SetPadding(Style->Padding);
	}

	if (!HScrollBarPaddingOverride.IsSet() && HScrollBarPaddingBox.IsValid())
	{
		HScrollBarPaddingBox->SetPadding(Style->HScrollBarPadding);
	}

	if (!VScrollBarPaddingOverride.IsSet() && VScrollBarPaddingBox.IsValid())
	{
		VScrollBarPaddingBox->SetPadding(Style->VScrollBarPadding);
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

	if (!bHasExternalHScrollBar && HScrollBar.IsValid())
	{
		HScrollBar->SetStyle(&Style->ScrollBarStyle);
	}

	if (!bHasExternalVScrollBar && VScrollBar.IsValid())
	{
		VScrollBar->SetStyle(&Style->ScrollBarStyle);
	}

	BorderImageNormal = &InStyle->BackgroundImageNormal;
	BorderImageHovered = &InStyle->BackgroundImageHovered;
	BorderImageFocused = &InStyle->BackgroundImageFocused;
	BorderImageReadOnly = &InStyle->BackgroundImageReadOnly;
}

void SMultiLineEditableTextBox::SetText( const TAttribute< FText >& InNewText )
{
	EditableText->SetText( InNewText );
}

void SMultiLineEditableTextBox::SetHintText( const TAttribute< FText >& InHintText )
{
	EditableText->SetHintText( InHintText );
}

void SMultiLineEditableTextBox::SetTextBoxForegroundColor(const TAttribute<FSlateColor>& InForegroundColor)
{
	ForegroundColorOverride = InForegroundColor;

	SetForegroundColor(ForegroundColorOverride.IsSet() ? ForegroundColorOverride : Style->ForegroundColor);
}

void SMultiLineEditableTextBox::SetTextBoxBackgroundColor(const TAttribute<FSlateColor>& InBackgroundColor)
{
	BackgroundColorOverride = InBackgroundColor;

	SetBorderBackgroundColor(BackgroundColorOverride.IsSet() ? BackgroundColorOverride : Style->BackgroundColor);
}

void SMultiLineEditableTextBox::SetReadOnlyForegroundColor(const TAttribute<FSlateColor>& InReadOnlyForegroundColor)
{
	ReadOnlyForegroundColorOverride = InReadOnlyForegroundColor;

	ReadOnlyForegroundColor = ReadOnlyForegroundColorOverride.IsSet() ? ReadOnlyForegroundColorOverride : Style->ReadOnlyForegroundColor;
}

void SMultiLineEditableTextBox::SetError( const FText& InError )
{
	SetError( InError.ToString() );
}

void SMultiLineEditableTextBox::SetError( const FString& InError )
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
const FSlateBrush* SMultiLineEditableTextBox::GetBorderImage() const
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

bool SMultiLineEditableTextBox::SupportsKeyboardFocus() const
{
	return StaticCastSharedPtr<SWidget>(EditableText)->SupportsKeyboardFocus();
}

bool SMultiLineEditableTextBox::HasKeyboardFocus() const
{
	// Since keyboard focus is forwarded to our editable text, we will test it instead
	return SBorder::HasKeyboardFocus() || EditableText->HasKeyboardFocus();
}

FReply SMultiLineEditableTextBox::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	FReply Reply = FReply::Handled();

	if ( InFocusEvent.GetCause() != EFocusCause::Cleared )
	{
		// Forward keyboard focus to our editable text widget
		Reply.SetUserFocus(EditableText.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}

FText SMultiLineEditableTextBox::GetSelectedText() const
{
	return EditableText->GetSelectedText();
}

void SMultiLineEditableTextBox::InsertTextAtCursor(const FText& InText)
{
	EditableText->InsertTextAtCursor(InText);
}

void SMultiLineEditableTextBox::InsertTextAtCursor(const FString& InString)
{
	EditableText->InsertTextAtCursor(InString);
}

void SMultiLineEditableTextBox::InsertRunAtCursor(TSharedRef<IRun> InRun)
{
	EditableText->InsertRunAtCursor(InRun);
}

void SMultiLineEditableTextBox::GoTo(const FTextLocation& NewLocation)
{
	EditableText->GoTo(NewLocation);
}

void SMultiLineEditableTextBox::ScrollTo(const FTextLocation& NewLocation)
{
	EditableText->ScrollTo(NewLocation);
}

void SMultiLineEditableTextBox::ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle)
{
	EditableText->ApplyToSelection(InRunInfo, InStyle);
}

TSharedPtr<const IRun> SMultiLineEditableTextBox::GetRunUnderCursor() const
{
	return EditableText->GetRunUnderCursor();
}

const TArray<TSharedRef<const IRun>> SMultiLineEditableTextBox::GetSelectedRuns() const
{
	return EditableText->GetSelectedRuns();
}

TSharedPtr<const SScrollBar> SMultiLineEditableTextBox::GetHScrollBar() const
{
	return EditableText->GetHScrollBar();
}

TSharedPtr<const SScrollBar> SMultiLineEditableTextBox::GetVScrollBar() const
{
	return EditableText->GetVScrollBar();
}

void SMultiLineEditableTextBox::Refresh()
{
	return EditableText->Refresh();
}

void SMultiLineEditableTextBox::SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler)
{
	EditableText->SetOnKeyDownHandler(InOnKeyDownHandler);
}

#endif //WITH_FANCY_TEXT
