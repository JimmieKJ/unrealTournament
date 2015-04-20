// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTButton.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTButton::Construct(const FArguments& InArgs)
{
	OnButtonClick = InArgs._UTOnButtonClicked;
	bIsToggleButton = InArgs._IsToggleButton;
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

FReply SUTButton::Pressed(int32 MouseButtonIndex)
{
	if(IsEnabled())
	{
		bIsPressed = true;
		
		PlayPressedSound();
		
		if( ClickMethod == EButtonClickMethod::MouseDown )
		{
			//get the reply from the execute function

			if (OnButtonClick.IsBound())
			{
				OnButtonClick.Execute(MouseButtonIndex);
			}
			return FReply::Handled();
		}
		else
		{
			//we need to capture the mouse for MouseUp events
			return FReply::Handled().CaptureMouse( AsShared() );
		}
	}
	return FReply::Unhandled();

}

FReply SUTButton::Released(int32 MouseButtonIndex, bool bIsUnderCusor)
{
	
	if ( IsEnabled() )
	{
		if (!bIsToggleButton)
		{
			bIsPressed = false;
		}

		if( ClickMethod != EButtonClickMethod::MouseDown )
		{
			if( bIsUnderCusor )
			{
				const bool bTriggerForMouseEvent = ( ClickMethod == EButtonClickMethod::MouseUp || HasMouseCapture() );

				if( HasMouseCapture() && OnButtonClick.IsBound() )
				{
					OnButtonClick.Execute(MouseButtonIndex);
					return FReply::Handled().ReleaseMouseCapture();
				}
			}
		}

	}

	return FReply::Unhandled().ReleaseMouseCapture();
}

FReply SUTButton::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	//see if we pressed the Enter (Left Mouse) or Spacebar keys (Right Mouse) or DownArrow (Right Mouse)
	if(InKeyboardEvent.GetKey() == EKeys::Enter || InKeyboardEvent.GetKey() == EKeys::SpaceBar || InKeyboardEvent.GetKey() == EKeys::Down)
	{
		return Pressed(InKeyboardEvent.GetKey() == EKeys::Enter ? 0 : 1);
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}

FReply SUTButton::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	//see if we pressed the Enter (Left Mouse) or Spacebar keys (Right Mouse) or DownArrow (Right Mouse)
	if(InKeyboardEvent.GetKey() == EKeys::Enter || InKeyboardEvent.GetKey() == EKeys::SpaceBar || InKeyboardEvent.GetKey() == EKeys::Down)
	{
		return Released(InKeyboardEvent.GetKey() == EKeys::Enter ? 0 : 1, true);
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}


FReply SUTButton::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return Pressed(MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? 0 : 1);
}


FReply SUTButton::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown( InMyGeometry, InMouseEvent );
}


FReply SUTButton::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const bool bIsUnderMouse = MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
	return Released(MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? 0 : 1, bIsUnderMouse);
}

void SUTButton::UnPressed()
{
	bIsPressed = false;
}

void SUTButton::BePressed()
{
	bIsPressed = true;
}


void SUTButton::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	if (!bIsToggleButton) SButton::OnFocusLost(InFocusEvent);
}

void SUTButton::OnMouseLeave( const FPointerEvent& MouseEvent ) 
{
	if (bIsToggleButton)
	{
		// Call parent implementation
		SWidget::OnMouseLeave( MouseEvent );
	}
	else
	{
		SButton::OnMouseLeave( MouseEvent );
	}
}



#endif