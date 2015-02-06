// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InputBindingEditorPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SGestureEditBox"


/* SGestureEditBox interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SGestureEditBox::Construct( const FArguments& InArgs, TSharedPtr<FGestureTreeItem> InputCommand )
{
	BorderImageNormal = FEditorStyle::GetBrush( "EditableTextBox.Background.Normal" );
	BorderImageHovered = FEditorStyle::GetBrush( "EditableTextBox.Background.Hovered" );
	BorderImageFocused = FEditorStyle::GetBrush( "EditableTextBox.Background.Focused" );

	static const FName InvertedForegroundName("InvertedForeground");

	ChildSlot
	[
		SAssignNew( ConflictPopup, SMenuAnchor )
		.Placement(MenuPlacement_ComboBox)
		.OnGetMenuContent( this, &SGestureEditBox::OnGetContentForConflictPopup )
		.OnMenuOpenChanged(this, &SGestureEditBox::OnConflictPopupOpenChanged)
		[
			SNew( SBox )
			.WidthOverride( 200.0f )
			[
				SNew( SBorder )
				.VAlign(VAlign_Center)
				.Padding( FMargin( 4.0f, 2.0f ) )
				.BorderImage( this, &SGestureEditBox::GetBorderImage )
				.ForegroundColor( FEditorStyle::GetSlateColor(InvertedForegroundName) )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					.VAlign(VAlign_Center)
					[
						SAssignNew( GestureEditor, SGestureEditor, InputCommand )
						.OnEditBoxLostFocus( this, &SGestureEditBox::OnGestureEditorLostFocus )
						.OnGestureChanged( this, &SGestureEditBox::OnGestureChanged )
						.OnEditingStarted( this, &SGestureEditBox::OnGestureEditingStarted )
						.OnEditingStopped( this, &SGestureEditBox::OnGestureEditingStopped )
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						// Remove binding button
						SNew(SButton)
						.Visibility( this, &SGestureEditBox::GetGestureRemoveButtonVisibility )
						.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
						.ContentPadding(0)
						.OnClicked( this, &SGestureEditBox::OnGestureRemoveButtonClicked )
						.ForegroundColor( FSlateColor::UseForeground() )
						.IsFocusable(false)
						.ToolTipText(LOCTEXT("GestureEditButtonRemove_ToolTip", "Remove this binding") )
						[
							SNew( SBox )
							[
								SNew( SImage )
								.Image( FEditorStyle::GetBrush( "Symbols.X" ) )
								.ColorAndOpacity( FLinearColor(.7f,0,0,.75f) )
							]
						]
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


const FSlateBrush* SGestureEditBox::GetBorderImage() const
{
	if ( GestureEditor->HasKeyboardFocus() )
	{
		return BorderImageFocused;
	}
	else
	{
		if ( GestureEditor->IsHovered() )
		{
			return BorderImageHovered;
		}
		else
		{
			return BorderImageNormal;
		}
	}
}


FText SGestureEditBox::GetNotificationMessage() const
{
	return GestureEditor->GetNotificationText();
}


void SGestureEditBox::OnGestureEditorLostFocus()
{
	if( (!GestureAcceptButton.IsValid() || GestureAcceptButton->HasMouseCapture() == false) && !GestureEditor->IsTyping() )
	{
		if( GestureEditor->IsEditing() && GestureEditor->IsEditedGestureValid() && !GestureEditor->HasConflict() )
		{
			GestureEditor->CommitNewGesture();
		}

		GestureEditor->StopEditing();
	}
}


void SGestureEditBox::OnGestureEditingStarted()
{
	ConflictPopup->SetIsOpen(false);
}


void SGestureEditBox::OnGestureEditingStopped()
{
	if( GestureEditor->IsEditedGestureValid() && !GestureEditor->HasConflict() )
	{
		GestureEditor->CommitNewGesture();
	}
}


void SGestureEditBox::OnGestureChanged()
{
	if( GestureEditor->HasConflict() )
	{
		ConflictPopup->SetIsOpen(true, true);
	}
	else
	{
		ConflictPopup->SetIsOpen(false);

		if (GestureEditor->IsEditedGestureValid())
		{
			GestureEditor->CommitNewGesture();
			GestureEditor->StopEditing();
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
		}
	}
}

EVisibility SGestureEditBox::GetGestureRemoveButtonVisibility() const
{
	if( !GestureEditor->IsEditing() && GestureEditor->IsActiveGestureValid() )
	{
		// Gesture should display a button.  What the button does is defined by the two cases above
		return EVisibility::Visible;
	}
	else
	{
		// Nothing to remove, but still take up the space
		return EVisibility::Hidden;
	}
}


FReply SGestureEditBox::OnGestureRemoveButtonClicked()
{
	if( !GestureEditor->IsEditing() )
	{
		GestureEditor->RemoveActiveGesture();
	}

	return FReply::Handled();
}


FReply SGestureEditBox::OnAcceptNewGestureButtonClicked()
{
	if( GestureEditor->IsEditing() )
	{
		GestureEditor->CommitNewGesture();
		GestureEditor->StopEditing();
		
	}

	ConflictPopup->SetIsOpen(false);

	return FReply::Handled();
}


TSharedRef<SWidget> SGestureEditBox::OnGetContentForConflictPopup()
{
	return SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("NotificationList.ItemBackground")  )
		[
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 2.0f, 0.0f )
				.AutoHeight()
				[
					SNew( STextBlock )
						.WrapTextAt(200.0f)
						.ColorAndOpacity( FLinearColor( .75f, 0.0f, 0.0f, 1.0f ) )
						.Text( this, &SGestureEditBox::GetNotificationMessage )
						.Visibility( this, &SGestureEditBox::GetNotificationVisibility )
				]

			+ SVerticalBox::Slot()
				.Padding( 2.0f )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.AutoHeight()
				[
					SAssignNew( GestureAcceptButton, SButton )
						.ContentPadding(1)
						.ToolTipText( LOCTEXT("GestureAcceptButton_ToolTip", "Accept this new binding") )
						.OnClicked( this, &SGestureEditBox::OnAcceptNewGestureButtonClicked )
						[
							SNew( SHorizontalBox )

							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew( SImage )
										.Image( FEditorStyle::GetBrush( "Symbols.Check" ) )
										.ColorAndOpacity( FLinearColor(0,.7f,0,.75f) )
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2.0f,0.0f)
								[
									SNew( STextBlock )
										.Text( LOCTEXT("GestureAcceptButtonText_Override", "Override") )
								]
						]
				]
		];
}

void SGestureEditBox::OnConflictPopupOpenChanged(bool bIsOpen)
{
	if(!bIsOpen)
	{
		GestureEditor->StopEditing();
	}
}

EVisibility SGestureEditBox::GetNotificationVisibility() const
{
	return !GestureEditor->GetNotificationText().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply SGestureEditBox::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent )
{
	// This is a passthrough if the gesture edit box gets a mouse click in the button area and the button isnt visible. 
	// we should focus the lower level editing widget in this case
	if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		if( !GestureEditor->IsEditing() )
		{
			GestureEditor->StartEditing();
		}
		return FReply::Handled().SetUserFocus(GestureEditor.ToSharedRef(), EFocusCause::Mouse);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
