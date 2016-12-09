// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"


void SInputKeySelector::Construct( const FArguments& InArgs )
{
	SelectedKey = InArgs._SelectedKey;
	OnKeySelected = InArgs._OnKeySelected;
	Font = InArgs._Font;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	OnIsSelectingKeyChanged = InArgs._OnIsSelectingKeyChanged;
	bAllowModifierKeys = InArgs._AllowModifierKeys;
	bEscapeCancelsSelection = InArgs._EscapeCancelsSelection;

	DefaultFont = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "NormalText" ).Font;

	bIsSelectingKey = false;

	ChildSlot
	[
		SAssignNew(Button, SButton)
		.ButtonStyle(InArgs._ButtonStyle)
		.OnClicked(this, &SInputKeySelector::OnClicked)
		[
			SNew(STextBlock)
			.Text(this, &SInputKeySelector::GetSelectedKeyText)
			.Font(this, &SInputKeySelector::GetFont)
			.Margin(Margin)
			.ColorAndOpacity(this, &SInputKeySelector::GetSlateColorAndOpacity)
			.Justification(ETextJustify::Center)
		]
	];
}

FText SInputKeySelector::GetSelectedKeyText() const
{
	if ( bIsSelectingKey )
	{
		return KeySelectionText;
	}
	else if ( SelectedKey.IsSet() )
	{
		if(SelectedKey.Get().Key.IsValid())
		{
			// If the key in the chord is a modifier key, print it's display name directly since the FInputChord
					// displays these as empty text.
			return SelectedKey.Get().Key.IsModifierKey()
				? SelectedKey.Get().Key.GetDisplayName()
				: SelectedKey.Get().GetInputText();
		}
		return FText();
	}
	return FText();
}

FInputChord SInputKeySelector::GetSelectedKey() const
{
	return SelectedKey.IsSet() ? SelectedKey.Get() : EKeys::Invalid;
}

void SInputKeySelector::SetSelectedKey( TAttribute<FInputChord> InSelectedKey )
{
	if ( SelectedKey.IdenticalTo(InSelectedKey) == false)
	{
		SelectedKey = InSelectedKey;
		OnKeySelected.ExecuteIfBound( SelectedKey.IsSet() ? SelectedKey.Get() : FInputChord( EKeys::Invalid ) );
	}
}

FSlateFontInfo SInputKeySelector::GetFont() const
{
	return Font.IsSet() ? Font.Get() : DefaultFont;
}

void SInputKeySelector::SetFont( TAttribute<FSlateFontInfo> InFont )
{
	Font = InFont;
}

FMargin SInputKeySelector::GetMargin() const
{
	return Margin.Get();
}

void SInputKeySelector::SetMargin( TAttribute<FMargin> InMargin )
{
	Margin = InMargin;
}

FSlateColor SInputKeySelector::GetSlateColorAndOpacity() const
{
	return FSlateColor(GetColorAndOpacity());
}

void SInputKeySelector::SetColorAndOpacity( TAttribute<FLinearColor> InColorAndOpacity )
{
	ColorAndOpacity = InColorAndOpacity;
}

void SInputKeySelector::SetButtonStyle(const FButtonStyle* ButtonStyle )
{
	Button->SetButtonStyle(ButtonStyle);
}

void SInputKeySelector::SetKeySelectionText( FText InKeySelectionText )
{
	KeySelectionText = InKeySelectionText;
}

void SInputKeySelector::SetAllowModifierKeys( bool bInAllowModifierKeys )
{
	bAllowModifierKeys = bInAllowModifierKeys;
}

bool SInputKeySelector::GetIsSelectingKey() const
{
	return bIsSelectingKey;
}

FReply SInputKeySelector::OnClicked()
{
	if ( bIsSelectingKey == false )
	{
		SetIsSelectingKey(true);
		return FReply::Handled()
			.SetUserFocus(SharedThis(this), EFocusCause::SetDirectly);
	}
	return FReply::Unhandled();
}

void SInputKeySelector::SelectKey( FKey Key, bool bShiftDown, bool bControllDown, bool bAltDown, bool bCommandDown )
{
	FInputChord NewSelectedKey = bAllowModifierKeys 
		? FInputChord( Key, bShiftDown, bControllDown, bAltDown, bCommandDown ) 
		: FInputChord( Key );
	if ( SelectedKey.IsBound() == false )
	{
		SelectedKey.Set( NewSelectedKey );
	}
	OnKeySelected.ExecuteIfBound( NewSelectedKey );
}

void SInputKeySelector::SetIsSelectingKey( bool bInIsSelectingKey )
{
	if ( bIsSelectingKey != bInIsSelectingKey )
	{
		bIsSelectingKey = bInIsSelectingKey;
		OnIsSelectingKeyChanged.ExecuteIfBound();
	}
}

FReply SInputKeySelector::OnPreviewKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	// TODO: Add an argument to allow gamepad key selection.
	if ( bIsSelectingKey && InKeyEvent.GetKey().IsGamepadKey() == false)
	{
		// While selecting keys handle all key downs to prevent contained controls from
		// interfering with key selection.
		return FReply::Handled();
	}
	return SWidget::OnPreviewKeyDown( MyGeometry, InKeyEvent );
}

FReply SInputKeySelector::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	// TODO: Add an argument to allow gamepad key selection.
	FKey KeyUp = InKeyEvent.GetKey();
	EModifierKey::Type ModifierKey = EModifierKey::FromBools(
		InKeyEvent.IsControlDown() && KeyUp != EKeys::LeftControl && KeyUp != EKeys::RightControl,
		InKeyEvent.IsAltDown() && KeyUp != EKeys::LeftAlt && KeyUp != EKeys::RightAlt,
		InKeyEvent.IsShiftDown() && KeyUp != EKeys::LeftShift && KeyUp != EKeys::RightShift,
		InKeyEvent.IsCommandDown() && KeyUp != EKeys::LeftCommand && KeyUp != EKeys::RightCommand );

	// Don't allow chords consisting of just modifier keys.
	if ( bIsSelectingKey && KeyUp.IsGamepadKey() == false && ( KeyUp.IsModifierKey() == false || ModifierKey == EModifierKey::None ) )
	{
		SetIsSelectingKey( false );

		if ( KeyUp == EKeys::Escape && bEscapeCancelsSelection )
		{
			return FReply::Handled();
		}

		SelectKey(
			KeyUp,
			ModifierKey == EModifierKey::Shift,
			ModifierKey == EModifierKey::Control,
			ModifierKey == EModifierKey::Alt,
			ModifierKey == EModifierKey::Command);
		return FReply::Handled();
	}
	return SWidget::OnPreviewKeyDown( MyGeometry, InKeyEvent );
}

FReply SInputKeySelector::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( bIsSelectingKey )
	{
		SetIsSelectingKey(false);
		// TODO: Add options for enabling mouse modifiers.
		SelectKey(MouseEvent.GetEffectingButton(), false, false, false, false);
		return FReply::Handled();
	}
	return SWidget::OnPreviewMouseButtonDown( MyGeometry, MouseEvent );
}

void SInputKeySelector::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	if ( bIsSelectingKey )
	{
		SetIsSelectingKey(false);
	}
}
