// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTTabButton.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTTabButton::Construct(const FArguments& InArgs)
{
	SButton::Construct( SButton::FArguments()
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
		.ClickMethod(InArgs._ClickMethod)
		.OnClicked(InArgs._OnClicked)
		.TouchMethod(InArgs._TouchMethod)
		.DesiredSizeScale(InArgs._DesiredSizeScale)
		.ContentScale(InArgs._ContentScale)
		.ButtonColorAndOpacity(InArgs._ButtonColorAndOpacity)
		.ForegroundColor(InArgs._ForegroundColor)
		.IsFocusable(InArgs._IsFocusable)
		.PressedSoundOverride(InArgs._PressedSoundOverride)
		.HoveredSoundOverride(InArgs._HoveredSoundOverride)
	);
}

FReply SUTTabButton::Pressed(int32 MouseButtonIndex)
{
	if(IsEnabled())
	{
		bIsPressed = true;
		PlayPressedSound();
		
		if (OnClicked.IsBound())
		{
			OnClicked.Execute();
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();

}

FReply SUTTabButton::Released(int32 MouseButtonIndex, bool bIsUnderCusor)
{
	return bIsUnderCusor ? FReply::Handled() : FReply::Unhandled().ReleaseMouseCapture();
}

FReply SUTTabButton::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	//see if we pressed the Enter (Left Mouse) or Spacebar keys (Right Mouse) or DownArrow (Right Mouse)
	if(InKeyboardEvent.GetKey() == EKeys::Enter || InKeyboardEvent.GetKey() == EKeys::SpaceBar || InKeyboardEvent.GetKey() == EKeys::Down)
	{
		return Pressed(InKeyboardEvent.GetKey() == EKeys::Enter ? 0 : 1);
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}

FReply SUTTabButton::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	//see if we pressed the Enter (Left Mouse) or Spacebar keys (Right Mouse) or DownArrow (Right Mouse)
	if(InKeyboardEvent.GetKey() == EKeys::Enter || InKeyboardEvent.GetKey() == EKeys::SpaceBar || InKeyboardEvent.GetKey() == EKeys::Down)
	{
		return Released(InKeyboardEvent.GetKey() == EKeys::Enter ? 0 : 1, true);
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}


FReply SUTTabButton::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return Pressed(MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? 0 : 1);
}


FReply SUTTabButton::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown( InMyGeometry, InMouseEvent );
}


FReply SUTTabButton::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const bool bIsUnderMouse = MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
	return Released(MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? 0 : 1, bIsUnderMouse);
}

void SUTTabButton::UnPressed()
{
	bIsPressed = false;
}

void SUTTabButton::BePressed()
{
	bIsPressed = true;
}

void SUTTabButton::OnFocusLost( const FFocusEvent& InFocusEvent )
{
}

void SUTTabButton::OnMouseLeave( const FPointerEvent& MouseEvent ) 
{
	// Call parent implementation
	SWidget::OnMouseLeave( MouseEvent );
}


#endif