// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SCheckBox::Construct( const SCheckBox::FArguments& InArgs )
{
	check(InArgs._Style != nullptr);
	Style = InArgs._Style;

	UncheckedImage = InArgs._UncheckedImage;
	UncheckedHoveredImage = InArgs._UncheckedHoveredImage;
	UncheckedPressedImage = InArgs._UncheckedPressedImage;

	CheckedImage = InArgs._CheckedImage;
	CheckedHoveredImage = InArgs._CheckedHoveredImage;
	CheckedPressedImage = InArgs._CheckedPressedImage;

	UndeterminedImage = InArgs._UndeterminedImage;
	UndeterminedHoveredImage = InArgs._UndeterminedHoveredImage;
	UndeterminedPressedImage = InArgs._UndeterminedPressedImage;
	
	TAttribute<FMargin> Padding = InArgs._Padding.IsSet() ? InArgs._Padding : InArgs._Style->Padding;
	TAttribute<FSlateColor> ForegroundColor = InArgs._ForegroundColor.IsSet() ? InArgs._ForegroundColor : InArgs._Style->ForegroundColor;
	TAttribute<FSlateColor> BorderBackgroundColor = InArgs._BorderBackgroundColor.IsSet() ? InArgs._BorderBackgroundColor : InArgs._Style->BorderBackgroundColor;
	ESlateCheckBoxType::Type CheckBoxType = InArgs._Type.IsSet() ? InArgs._Type.GetValue() : InArgs._Style->CheckBoxType.GetValue();

	bIsPressed = false;
	bIsFocusable = InArgs._IsFocusable;

	if( CheckBoxType == ESlateCheckBoxType::CheckBox )
	{
		// Check boxes use a separate check button to the side of the user's content (often, a text label or icon.)
		this->ChildSlot
		[
			SNew(SHorizontalBox)
			// Make sure we aren't trying to compute the desired size when the check box is collapsed
			.Visibility(InArgs._Visibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign( VAlign_Center )
			.HAlign( HAlign_Center )
			[
				SNew(SImage)
				.Image( this, &SCheckBox::OnGetCheckImage )
				.ColorAndOpacity( ForegroundColor )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( Padding )
			.VAlign( VAlign_Center )
			[
				SAssignNew(ContentContainer, SBorder)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.Padding(0.0f)
				[
					InArgs._Content.Widget
				]
			]
		];
	}
	else if( ensure( CheckBoxType == ESlateCheckBoxType::ToggleButton ) )
	{

		// Toggle buttons have a visual appearance that is similar to a Slate button
		this->ChildSlot
		[
			SAssignNew( ContentContainer, SBorder )
			// Make sure we aren't trying to compute the desired size when the check box is collapsed
			.Visibility(InArgs._Visibility)
			// Bind the border background to our method that gets a slate brush for the current state of the control
			.BorderImage( this, &SCheckBox::OnGetCheckImage )
			// Let the user decide the padding amount
			.Padding( Padding )
			// Set the user's incoming content as the content of our border
			.ForegroundColor( ForegroundColor )
			// Set the color of the border image
			.BorderBackgroundColor( BorderBackgroundColor )
			// Content alignment
			.HAlign( InArgs._HAlign )
			[
				InArgs._Content.Widget
			]
		];
	}

	IsCheckboxChecked = InArgs._IsChecked;
	OnCheckStateChanged = InArgs._OnCheckStateChanged;

	ClickMethod = InArgs._ClickMethod.Get();

	OnGetMenuContent = InArgs._OnGetMenuContent;

	HoveredSound = InArgs._HoveredSoundOverride.Get(InArgs._Style->HoveredSlateSound);
	CheckedSound = InArgs._CheckedSoundOverride.Get(InArgs._Style->CheckedSlateSound);
	UncheckedSound = InArgs._UncheckedSoundOverride.Get(InArgs._Style->UncheckedSlateSound);
}

/**
 * See SWidget::SupportsKeyboardFocus().
 *
 * @return  True if this widget can take keyboard focus
 */
bool SCheckBox::SupportsKeyboardFocus() const
{
	// Buttons are focusable by default
	return bIsFocusable;
}

FReply SCheckBox::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( InKeyEvent.GetKey() == EKeys::SpaceBar )
	{
		ToggleCheckedState();
		const TAttribute<ECheckBoxState>& State = IsCheckboxChecked.Get();
		if(State == ECheckBoxState::Checked)
		{
			PlayCheckedSound();
		}
		else if(State == ECheckBoxState::Unchecked)
		{
			PlayUncheckedSound();
		}
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

/**
 * See SWidget::OnMouseButtonDown.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SCheckBox::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = true;

		if( ClickMethod == EButtonClickMethod::MouseDown )
		{
			ToggleCheckedState();
			const TAttribute<ECheckBoxState>& State = IsCheckboxChecked.Get();
			if(State == ECheckBoxState::Checked)
			{
				PlayCheckedSound();
			}
			else if(State == ECheckBoxState::Unchecked)
			{
				PlayUncheckedSound();
			}

			// Set focus to this button, but don't capture the mouse
			return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
		}
		else
		{
			// Capture the mouse, and also set focus to this button
			return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
		}
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OnGetMenuContent.IsBound() )
	{
		FSlateApplication::Get().PushMenu(
			AsShared(),
			OnGetMenuContent.Execute(),
			MouseEvent.GetScreenSpacePosition(),
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
			);

		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

/**
 * See SWidget::OnMouseButtonDoubleClick.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SCheckBox::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown( InMyGeometry, InMouseEvent );
}

/**
 * See SWidget::OnMouseButtonUp.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SCheckBox::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = false;

		if( ClickMethod == EButtonClickMethod::MouseDown )
		{
			// NOTE: If we're configured to click on mouse-down, then we never capture the mouse thus
			//       may never receive an OnMouseButtonUp() call.  We make sure that our bIsPressed
			//       state is reset by overriding OnMouseLeave().
		}
		else
		{
			const bool IsUnderMouse = MyGeometry.IsUnderLocation( MouseEvent.GetScreenSpacePosition() );
			if ( IsUnderMouse )
			{
				// If we were asked to allow the button to be clicked on mouse up, regardless of whether the user
				// pressed the button down first, then we'll allow the click to proceed without an active capture
				if( ClickMethod == EButtonClickMethod::MouseUp || HasMouseCapture() )
				{
					ToggleCheckedState();
					const TAttribute<ECheckBoxState>& State = IsCheckboxChecked.Get();
					if(State == ECheckBoxState::Checked)
					{
						PlayCheckedSound();
					}
					else if(State == ECheckBoxState::Unchecked)
					{
						PlayUncheckedSound();
					}
				}
			}
		}

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SCheckBox::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	PlayHoverSound();
	SCompoundWidget::OnMouseEnter( MyGeometry, MouseEvent );
}


void SCheckBox::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	// Call parent implementation
	SWidget::OnMouseLeave( MouseEvent );

	// If we're setup to click on mouse-down, then we never capture the mouse and may not receive a
	// mouse up event, so we need to make sure our pressed state is reset properly here
	if( ClickMethod == EButtonClickMethod::MouseDown )
	{
		bIsPressed = false;
	}
}


/**
 * Gets the check image to display for the current state of the check box
 * @return	The name of the image to display
 */
const FSlateBrush* SCheckBox::OnGetCheckImage() const
{
	ECheckBoxState State = IsCheckboxChecked.Get();

	const FSlateBrush* ImageToUse;
	switch( State )
	{
		case ECheckBoxState::Unchecked:
			ImageToUse = IsPressed() ? GetUncheckedPressedImage() : ( IsHovered() ? GetUncheckedHoveredImage() : GetUncheckedImage() );
			break;
	
		case ECheckBoxState::Checked:
			ImageToUse = IsPressed() ? GetCheckedPressedImage() : ( IsHovered() ? GetCheckedHoveredImage() : GetCheckedImage() );
			break;
	
		default:
		case ECheckBoxState::Undetermined:
			ImageToUse = IsPressed() ? GetUndeterminedPressedImage() : ( IsHovered() ? GetUndeterminedHoveredImage() : GetUndeterminedImage() );
			break;
	}

	return ImageToUse;
}


ECheckBoxState SCheckBox::GetCheckedState() const
{
	return IsCheckboxChecked.Get();
}

/**
 * Toggles the checked state for this check box, fire events as needed
 */
void SCheckBox::ToggleCheckedState()
{
	const TAttribute<ECheckBoxState>& State = IsCheckboxChecked.Get();

	// If the current check box state is checked OR undetermined we set the check box to checked.
	if( State == ECheckBoxState::Checked || State == ECheckBoxState::Undetermined )
	{
		if ( !IsCheckboxChecked.IsBound() )
		{
			// When we are not bound, just toggle the current state.
			IsCheckboxChecked.Set( ECheckBoxState::Unchecked );
		}

		// The state of the check box changed.  Execute the delegate to notify users
		OnCheckStateChanged.ExecuteIfBound( ECheckBoxState::Unchecked );
	}
	else if( State == ECheckBoxState::Unchecked )
	{
		if ( !IsCheckboxChecked.IsBound() )
		{
			// When we are not bound, just toggle the current state.
			IsCheckboxChecked.Set( ECheckBoxState::Checked );
		}

		// The state of the check box changed.  Execute the delegate to notify users
		OnCheckStateChanged.ExecuteIfBound( ECheckBoxState::Checked );
	}
}

void SCheckBox::SetIsChecked(TAttribute<ECheckBoxState> InIsChecked)
{
	IsCheckboxChecked = InIsChecked;
}

void SCheckBox::PlayCheckedSound() const
{
	FSlateApplication::Get().PlaySound( CheckedSound );
}

void SCheckBox::PlayUncheckedSound() const
{
	FSlateApplication::Get().PlaySound( UncheckedSound );
}

void SCheckBox::PlayHoverSound() const
{
	FSlateApplication::Get().PlaySound( HoveredSound );
}

void SCheckBox::SetContent(const TSharedRef< SWidget >& InContent)
{
	ContentContainer->SetContent(InContent);
}

void SCheckBox::SetStyle(const FCheckBoxStyle* InStyle)
{
	Style = InStyle;
}

void SCheckBox::SetUncheckedImage(const FSlateBrush* Brush)
{
	UncheckedImage = Brush;
}

void SCheckBox::SetUncheckedHoveredImage(const FSlateBrush* Brush)
{
	UncheckedHoveredImage = Brush;
}

void SCheckBox::SetUncheckedPressedImage(const FSlateBrush* Brush)
{
	UncheckedPressedImage = Brush;
}

void SCheckBox::SetCheckedImage(const FSlateBrush* Brush)
{
	CheckedImage = Brush;
}

void SCheckBox::SetCheckedHoveredImage(const FSlateBrush* Brush)
{
	CheckedHoveredImage = Brush;
}

void SCheckBox::SetCheckedPressedImage(const FSlateBrush* Brush)
{
	CheckedPressedImage = Brush;
}

void SCheckBox::SetUndeterminedImage(const FSlateBrush* Brush)
{
	UndeterminedImage = Brush;
}

void SCheckBox::SetUndeterminedHoveredImage(const FSlateBrush* Brush)
{
	UndeterminedHoveredImage = Brush;
}

void SCheckBox::SetUndeterminedPressedImage(const FSlateBrush* Brush)
{
	UndeterminedPressedImage = Brush;
}

const FSlateBrush* SCheckBox::GetUncheckedImage() const
{
	return UncheckedImage ? UncheckedImage : &Style->UncheckedImage;
}

const FSlateBrush* SCheckBox::GetUncheckedHoveredImage() const
{
	return UncheckedHoveredImage ? UncheckedHoveredImage : &Style->UncheckedHoveredImage;
}

const FSlateBrush* SCheckBox::GetUncheckedPressedImage() const
{
	return UncheckedPressedImage ? UncheckedPressedImage : &Style->UncheckedPressedImage;
}

const FSlateBrush* SCheckBox::GetCheckedImage() const
{
	return CheckedImage ? CheckedImage : &Style->CheckedImage;
}

const FSlateBrush* SCheckBox::GetCheckedHoveredImage() const
{
	return CheckedHoveredImage ? CheckedHoveredImage : &Style->CheckedHoveredImage;
}

const FSlateBrush* SCheckBox::GetCheckedPressedImage() const
{
	return CheckedPressedImage ? CheckedPressedImage : &Style->CheckedPressedImage;
}

const FSlateBrush* SCheckBox::GetUndeterminedImage() const
{
	return UndeterminedImage ? UndeterminedImage : &Style->UndeterminedImage;
}

const FSlateBrush* SCheckBox::GetUndeterminedHoveredImage() const
{
	return UndeterminedHoveredImage ? UndeterminedHoveredImage : &Style->UndeterminedHoveredImage;
}

const FSlateBrush* SCheckBox::GetUndeterminedPressedImage() const
{
	return UndeterminedPressedImage ? UndeterminedPressedImage : &Style->UndeterminedPressedImage;
}
