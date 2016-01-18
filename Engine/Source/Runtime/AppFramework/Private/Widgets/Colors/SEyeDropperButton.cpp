// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AppFrameworkPrivatePCH.h"
#include "SEyeDropperButton.h"

#define LOCTEXT_NAMESPACE "EyeDroppperButton"

void SEyeDropperButton::Construct(const FArguments& InArgs)
{
	OnValueChanged = InArgs._OnValueChanged;
	OnBegin = InArgs._OnBegin;
	OnComplete = InArgs._OnComplete;
	DisplayGamma = InArgs._DisplayGamma;

	// This is a button containing an dropper image and text that tells the user to hit Esc.
	// Their visibility and colors are changed according to whether dropper mode is active or not.
	SButton::Construct(
		SButton::FArguments()
		.ContentPadding(1.0f)
		.OnClicked(this, &SEyeDropperButton::OnClicked)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.Padding(FMargin(1.0f, 0.0f))
			[
				SNew(SImage)
				.Image(FCoreStyle::Get().GetBrush("ColorPicker.EyeDropper"))
				.ToolTipText(LOCTEXT("EyeDropperButton_ToolTip", "Activates the eye-dropper for selecting a colored pixel from any window."))
				.ColorAndOpacity(this, &SEyeDropperButton::GetDropperImageColor)
			]

			+ SOverlay::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EscapeCue", "Esc"))
				.ToolTipText(LOCTEXT("EyeDropperEscapeCue_ToolTip", "Hit Escape key to stop the eye dropper"))
				.Visibility(this, &SEyeDropperButton::GetEscapeTextVisibility)
			]
		]
	);
}

EActiveTimerReturnType SEyeDropperButton::ActiveTick(double InCurrentTime, float InDeltaTime)
{
	if ( bWasClickActivated )
	{
		if ( HasMouseCapture() && bWasLeft && !bWasReEntered )
		{
			FVector2D CurrentCursorPosition = FSlateApplication::Get().GetCursorPos();
			if ( CurrentCursorPosition != LastCursorPosition )
			{
				// In dropper mode and outside the button - sample the pixel color and push it to the client
				// Convert the display gamma into a ratio of gamma from the default gamma.
				float Gamma = DisplayGamma.Get(2.2f) / 2.2f;
				FLinearColor ScreenColor = FPlatformMisc::GetScreenPixelColor(CurrentCursorPosition, Gamma);
				OnValueChanged.ExecuteIfBound(ScreenColor);
			}

			LastCursorPosition = CurrentCursorPosition;
		}

		return EActiveTimerReturnType::Continue;
	}
	
	LastCursorPosition.Reset();
	return EActiveTimerReturnType::Stop;
}

FReply SEyeDropperButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Clicking ANY mouse button when the dropper isn't active resets the active dropper states ready to activate
	if ( !HasMouseCapture() )
	{
		bWasClickActivated = false;
		bWasLeft = false;
		bWasReEntered = false;
	}

	return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SEyeDropperButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// If a mouse click is completing and the dropper is active ALWAYS deactivate
	bool bDeactivating = false;
	if ( bWasClickActivated )
	{
		bDeactivating = true;
	}

	// bWasClicked is reset here because if it is set during SButton::OnMouseButtonUp()
	// then the button was 'clicked' according to the usual rules. We might want to capture the
	// mouse when the button is clicked but can't do it in the Clicked callback.
	bWasClicked = false;
	FReply Reply = SButton::OnMouseButtonUp(MyGeometry, MouseEvent);

	// Switching dropper mode off?
	if ( bDeactivating )
	{
		// These state flags clear dropper mode
		bWasClickActivated = false;
		bWasLeft = false;
		bWasReEntered = false;

		Reply.ReleaseMouseCapture();

		OnComplete.ExecuteIfBound();
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bWasClicked )
	{
		// A normal LMB mouse click on the button occurred
		// Set the initial dropper mode state and capture the mouse
		bWasClickActivated = true;
		bWasLeft = false;
		bWasReEntered = false;

		OnBegin.ExecuteIfBound();

		Reply.CaptureMouse(this->AsShared());

		RegisterActiveTimer(0, FWidgetActiveTimerDelegate::CreateSP(this, &SEyeDropperButton::ActiveTick));
	}
	bWasClicked = false;

	return Reply;
}

FReply SEyeDropperButton::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// If the mouse is captured and bWasClickActivated is set then we are in dropper mode
	if ( HasMouseCapture() && bWasClickActivated )
	{
		if ( IsMouseOver(MyGeometry, MouseEvent) )
		{
			if ( bWasLeft )
			{
				// Mouse is over the button having left it once
				bWasReEntered = true;
			}
		}
		else
		{
			// Mouse is outside the button
			bWasLeft = true;
			bWasReEntered = false;
		}
	}

	return SButton::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SEyeDropperButton::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape key when in dropper mode
	if ( InKeyEvent.GetKey() == EKeys::Escape &&
		HasMouseCapture() &&
		bWasClickActivated )
	{
		// Clear the dropper mode states
		bWasClickActivated = false;
		bWasLeft = false;
		bWasReEntered = false;

		// This is needed to switch the dropper cursor off immediately so the user can see the Esc key worked
		FSlateApplication::Get().QueryCursor();

		FReply ReleaseReply = FReply::Handled().ReleaseMouseCapture();

		OnComplete.ExecuteIfBound();

		return ReleaseReply;
	}

	return FReply::Unhandled();
}

FCursorReply SEyeDropperButton::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	// Cursor is changed to the dropper when dropper mode is active and the states are correct
	if ( HasMouseCapture() &&
		bWasClickActivated && bWasLeft && !bWasReEntered )
	{
		return FCursorReply::Cursor(EMouseCursor::EyeDropper);
	}

	return SButton::OnCursorQuery(MyGeometry, CursorEvent);
}

FReply SEyeDropperButton::OnClicked()
{
	// Log clicks so that OnMouseButtonUp() can post process clicks
	bWasClicked = true;
	return FReply::Handled();
}

EVisibility SEyeDropperButton::GetEscapeTextVisibility() const
{
	// Show the Esc key message in the button when dropper mode is active
	if ( HasMouseCapture() && bWasClickActivated )
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

FSlateColor SEyeDropperButton::GetDropperImageColor() const
{
	// Make the dropper image in the button pale when dropper mode is active
	if ( HasMouseCapture() && bWasClickActivated )
	{
		return FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
	}
	return FSlateColor::UseForeground();
}

bool SEyeDropperButton::IsMouseOver(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	return MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
}


#undef LOCTEXT_NAMESPACE
