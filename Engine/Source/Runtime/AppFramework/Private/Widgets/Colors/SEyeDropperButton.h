// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "EyeDroppperButton"


/**
 * Class for placing a color picker eye-dropper button.
 * A self contained until that only needs client code to set the display gamma and listen
 * for the OnValueChanged events. It toggles the dropper when clicked.
 * When active it captures the mouse, shows a dropper cursor and samples the pixel color constantly.
 * It is stopped normally by hitting the ESC key.
 */
class SEyeDropperButton : public SButton
{
public:
	DECLARE_DELEGATE(FOnDropperComplete)

	SLATE_BEGIN_ARGS( SEyeDropperButton )
		: _OnValueChanged()
		, _OnBegin()
		, _OnComplete()
		, _DisplayGamma()
		{}

		/** Invoked when a new value is selected by the dropper */
		SLATE_EVENT(FOnLinearColorValueChanged, OnValueChanged)

		/** Invoked when the dropper goes from inactive to active */
		SLATE_EVENT(FSimpleDelegate, OnBegin)

		/** Invoked when the dropper goes from active to inactive */
		SLATE_EVENT(FOnDropperComplete, OnComplete)

		/** Sets the display Gamma setting - used to correct colors sampled from the screen */
		SLATE_ATTRIBUTE(float, DisplayGamma)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs )
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

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if (bWasClickActivated && HasMouseCapture() && bWasLeft && !bWasReEntered)
		{
			FVector2D CurrentCursorPosition = FSlateApplication::Get().GetCursorPos();
			if (CurrentCursorPosition != LastCursorPosition)
			{
				// In dropper mode and outside the button - sample the pixel color and push it to the client
				FLinearColor ScreenColor = FPlatformMisc::GetScreenPixelColor(CurrentCursorPosition, false);
				OnValueChanged.ExecuteIfBound(ScreenColor);
			}

			LastCursorPosition = CurrentCursorPosition;
		}
		else
		{
			LastCursorPosition.Reset();
		}

		SButton::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}


	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		// Clicking ANY mouse button when the dropper isn't active resets the active dropper states ready to activate
		if (!HasMouseCapture())
		{
			bWasClickActivated = false;
			bWasLeft = false;
			bWasReEntered = false;
		}

		return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
	}

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		// If a mouse click is completing and the dropper is active ALWAYS deactivate
		bool bDeactivating = false;
		if (bWasClickActivated)
		{
			bDeactivating = true;
		}

		// bWasClicked is reset here because if it is set during SButton::OnMouseButtonUp()
		// then the button was 'clicked' according to the usual rules. We might want to capture the
		// mouse when the button is clicked but can't do it in the Clicked callback.
		bWasClicked = false;
		FReply Reply = SButton::OnMouseButtonUp(MyGeometry, MouseEvent);

		// Switching dropper mode off?
		if (bDeactivating)
		{
			// These state flags clear dropper mode
			bWasClickActivated = false;
			bWasLeft = false;
			bWasReEntered = false;

			Reply.ReleaseMouseCapture();

			OnComplete.ExecuteIfBound();
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bWasClicked)
		{
			// A normal LMB mouse click on the button occurred
			// Set the initial dropper mode state and capture the mouse
			bWasClickActivated = true;
			bWasLeft = false;
			bWasReEntered = false;

			OnBegin.ExecuteIfBound();

			Reply.CaptureMouse(this->AsShared());
		}
		bWasClicked = false;

		return Reply;
	}

	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		// If the mouse is captured and bWasClickActivated is set then we are in dropper mode
		if (HasMouseCapture() && bWasClickActivated)
		{
			if (IsMouseOver(MyGeometry, MouseEvent))
			{
				if (bWasLeft)
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

		return SButton::OnMouseMove( MyGeometry, MouseEvent );
	}

	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		// Escape key when in dropper mode
		if (InKeyEvent.GetKey() == EKeys::Escape &&
			HasMouseCapture() &&
			bWasClickActivated)
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

	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override
	{
		// Cursor is changed to the dropper when dropper mode is active and the states are correct
		if (HasMouseCapture() &&
			bWasClickActivated && bWasLeft && !bWasReEntered )
		{
			return FCursorReply::Cursor( EMouseCursor::EyeDropper );
		}

		return SButton::OnCursorQuery( MyGeometry, CursorEvent );
	}

	FReply OnClicked()
	{
		// Log clicks so that OnMouseButtonUp() can post process clicks
		bWasClicked = true;
		return FReply::Handled();
	}

private:

	EVisibility GetEscapeTextVisibility() const
	{
		// Show the Esc key message in the button when dropper mode is active
		if (HasMouseCapture() && bWasClickActivated)
		{
			return EVisibility::Visible;
		}
		return EVisibility::Hidden;
	}

	FSlateColor GetDropperImageColor() const
	{
		// Make the dropper image in the button pale when dropper mode is active
		if (HasMouseCapture() && bWasClickActivated)
		{
			return FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
		}
		return FSlateColor::UseForeground();
	}

	bool IsMouseOver(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
	{ 
		return MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
	}

	/** Invoked when a new value is selected by the dropper */
	FOnLinearColorValueChanged OnValueChanged;

	/** Invoked when the dropper goes from inactive to active */
	FSimpleDelegate OnBegin;

	/** Invoked when the dropper goes from active to inactive - can be used to commit colors by the owning picker */
	FOnDropperComplete OnComplete;

	/** Sets the display Gamma setting - used to correct colors sampled from the screen */
	TAttribute<float> DisplayGamma;

	/** Previous Tick's cursor position */
	TOptional<FVector2D> LastCursorPosition;

	// Dropper states
	bool bWasClicked;
	bool bWasClickActivated;
	bool bWasLeft;
	bool bWasReEntered;
};


#undef LOCTEXT_NAMESPACE
