// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SEditableLabel.h"


/* SEditableLabel interface
 *****************************************************************************/

void SEditableLabel::Construct(const FArguments& InArgs)
{
	CanEditAttribute = InArgs._CanEdit;
	OnTextChanged = InArgs._OnTextChanged;
	TextAttribute = InArgs._Text;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(TextBlock, STextBlock)
					.ColorAndOpacity(InArgs._ColorAndOpacity)
					.Font(InArgs._Font)
					.HighlightColor(InArgs._HighlightColor)
					.HighlightShape(InArgs._HighlightShape)
					.HighlightText(InArgs._HighlightText)
					.MinDesiredWidth(InArgs._MinDesiredWidth)
					.ShadowColorAndOpacity(InArgs._ShadowColorAndOpacity)
					.ShadowOffset(InArgs._ShadowOffset)
					.TextStyle(InArgs._TextStyle)
					.Text(InArgs._Text)
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center) 
			[
				SAssignNew(EditableText, SEditableText)
					.ClearKeyboardFocusOnCommit(true)
					.ColorAndOpacity(InArgs._ColorAndOpacity)
					.Font(InArgs._Font)
					.MinDesiredWidth(InArgs._MinDesiredWidth)
					.OnTextCommitted(this, &SEditableLabel::HandleEditableTextTextCommitted)
					.RevertTextOnEscape(true)
					.SelectAllTextOnCommit(false)
					.SelectAllTextWhenFocused(true)
					.Style(InArgs._EditableTextStyle)
					.Text(InArgs._Text)
					.VirtualKeyboardType(EKeyboardType::Keyboard_Number)
					.Visibility(EVisibility::Collapsed)
			]

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
					.Image(FCoreStyle::Get().GetBrush(TEXT("EditableLabel.EditIcon")))
					.Visibility(this, &SEditableLabel::HandleIconVisibility)
			]
	];
}


void SEditableLabel::EnterTextMode()
{
	if (!CanEditAttribute.Get())
	{
		return;
	}

	TextBlock->SetVisibility(EVisibility::Collapsed);
	EditableText->SetVisibility(EVisibility::Visible);
}
	

void SEditableLabel::ExitTextMode()
{
	TextBlock->SetVisibility(EVisibility::Visible);
	EditableText->SetVisibility(EVisibility::Collapsed);
}


/* SWidget interface
 *****************************************************************************/

bool SEditableLabel::HasKeyboardFocus() const
{
	// this label is considered focused when we are typing it text.
	return SCompoundWidget::HasKeyboardFocus() ||
		(EditableText.IsValid() && EditableText->HasKeyboardFocus());
}


FReply SEditableLabel::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	if ((InFocusEvent.GetCause() == EFocusCause::Navigation) ||
		(InFocusEvent.GetCause() == EFocusCause::SetDirectly))
	{
		EnterTextMode();
		return FReply::Handled().SetUserFocus(EditableText.ToSharedRef(), InFocusEvent.GetCause());
	}

	return FReply::Unhandled();
}


FReply SEditableLabel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape)
	{
		ExitTextMode();
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Navigation);
	}

	if (Key == EKeys::Enter)
	{
		EnterTextMode();
		return FReply::Handled().SetUserFocus(EditableText.ToSharedRef(), EFocusCause::Navigation);
	}

	return FReply::Unhandled();
}


FReply SEditableLabel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// @todo Slate: implement some mechanism to not enter edit mode right away on mouse up
	}

	return FReply::Unhandled();
}


FReply SEditableLabel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		EnterTextMode();

		return FReply::Handled();			
	}

	return FReply::Unhandled();
}


bool SEditableLabel::SupportsKeyboardFocus() const
{
	return true;
}


/* SEditableLabel callbacks
 *****************************************************************************/

void SEditableLabel::HandleEditableTextTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	ExitTextMode();
	OnTextChanged.ExecuteIfBound(NewText);
}


EVisibility SEditableLabel::HandleIconVisibility() const
{
	return (IsHovered() && !EditableText->HasKeyboardFocus() && CanEditAttribute.Get())
		? EVisibility::Visible
		: EVisibility::Collapsed;
}
