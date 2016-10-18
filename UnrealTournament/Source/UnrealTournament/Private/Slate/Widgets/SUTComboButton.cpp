// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"

#include "SUTComboButton.h"

#if !UE_SERVER

void SUTComboButton::Construct(const FArguments& InArgs)
{
	// A lot of this code is copied directly from SComboButton.

	// Work out which values we should use based on whether we were given an override, or should use the style's version
	const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &InArgs._ComboButtonStyle->ButtonStyle;

	MenuBorderBrush = &InArgs._ComboButtonStyle->MenuBorderBrush;
	MenuBorderPadding = InArgs._ComboButtonStyle->MenuBorderPadding;

	MenuButtonStyle = InArgs._MenuButtonStyle;
	MenuButtonTextStyle = InArgs._MenuButtonTextStyle;

	OnButtonSubMenuSelect = InArgs._OnButtonSubMenuSelect;
	OnSubmenuShown = InArgs._OnSubmenuShown;
	OnClicked = InArgs._OnClicked;

	ContentWidgetPtr = InArgs._MenuContent.Widget;

	bIsFocusable = InArgs._IsFocusable;
	bRightClickOpensMenu = InArgs._bRightClickOpensMenu;
	bShowDownArrow = InArgs._HasDownArrow;

	ContentHAlign = InArgs._ContentHAlign;

	// Build the actual button space.https://forums.unrealtournament.com/attachment.php?attachmentid=10183&d=1415937070
	TSharedPtr<SHorizontalBox> HBox;

	SMenuAnchor::Construct( SMenuAnchor::FArguments()
		.Placement( InArgs._MenuPlacement )
		[
			SAssignNew( MyButton, SUTButton )
			.ContentPadding( FMargin( 1, 0 ) )
			.ButtonStyle( OurButtonStyle )
			.TextStyle(InArgs._TextStyle)
			.Text(InArgs._Text)
			.ClickMethod( EButtonClickMethod::MouseDown )
			.UTOnButtonClicked( this, &SUTComboButton::UTOnButtonClicked )
			.ContentPadding( InArgs._ContentPadding )
			.IsFocusable( InArgs._IsFocusable )
			.IsToggleButton(true)
			[
				// Button and down arrow on the right
				// +-------------------+---+
				// | Button Content    | v |
				// +-------------------+---+
				SAssignNew( HBox, SHorizontalBox )
				+ SHorizontalBox::Slot()
				.Expose( ButtonContentSlot )
				.FillWidth( 1 )
				.HAlign( InArgs._HAlign )
				.VAlign( InArgs._VAlign )
				[
					InArgs._ButtonContent.Widget
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				.Padding( InArgs._HasDownArrow ? 2 : 0 )
				[
					SNew( SImage )
					.Visibility( InArgs._HasDownArrow ? EVisibility::Visible : EVisibility::Collapsed )
					.Image( &InArgs._ComboButtonStyle->DownArrowImage )
					// Inherit tinting from parent
					. ColorAndOpacity( FSlateColor::UseForeground() )
				]
			]
		]
	);


	// Build the menu box that will hold the content.
	SAssignNew(MenuBox, SScrollBox);

	// Handle the default menu items

	if (!InArgs._DefaultMenuItems.IsEmpty() && InArgs._DefaultMenuItems != TEXT(""))
	{
		TArray<FString> MenuItems;
		InArgs._DefaultMenuItems.ParseIntoArray(MenuItems, TEXT(","), true);
		for (int32 i=0; i < MenuItems.Num(); i++)
		{
			if (MenuItems[i] == TEXT("-"))
			{
				// Add a spacer
				SubMenuItems.Add(FUTComboSubMenuItem());
			}
			else
			{
				// Add an item
				SubMenuItems.Add(FUTComboSubMenuItem(FText::FromString(*MenuItems[i]), FOnClicked::CreateSP(this, &SUTComboButton::SimpleSubMenuButtonClicked, i)));
			}
		}
	}

	if (SubMenuItems.Num() > 0)
	{
		RebuildMenuContent();
	}
}

void SUTComboButton::AddSpacer(bool bUpdate)
{
	SubMenuItems.Add(FUTComboSubMenuItem());
	if (bUpdate)
	{
		RebuildMenuContent();
	}
}


void SUTComboButton::AddSubMenuItem(const FText& NewItem, FOnClicked OnClickDelegate, bool bUpdate)
{
	SubMenuItems.Add(FUTComboSubMenuItem(NewItem, OnClickDelegate));
	if (bUpdate)
	{
		RebuildMenuContent();
	}
}

void SUTComboButton::ClearSubMenuItems()
{
	SubMenuItems.Empty();
	RebuildMenuContent();
}

void SUTComboButton::RebuildMenuContent()
{
	if (MenuBox.IsValid())
	{
		MenuBox->ClearChildren();
		for (int32 i=0;i<SubMenuItems.Num();i++)
		{

			if (SubMenuItems[i].bSpacer)
			{
				MenuBox->AddSlot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.Padding(FMargin(10.0f, 6.0f, 10.0f, 6.0f))
				[
					SNew(SBox)
					.HeightOverride(3)
					[
						SNew(SImage )
						.Image(SUTStyle::Get().GetBrush("UT.ContextMenu.Item.Spacer"))
					]
				];
			}
			else
			{
				MenuBox->AddSlot()
				.HAlign(ContentHAlign)
				[
					SNew(SUTButton)
					.ButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(SubMenuItems[i].MenuCaption)
					.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
					.OnClicked(this, &SUTComboButton::InternalSubMenuButtonClickedHandler, i)
				];
			}
		}

		SetMenus(MenuBox.ToSharedRef());

	}
}

FReply SUTComboButton::InternalSubMenuButtonClickedHandler(int32 MenuItemIndex)
{
	// First, close the submenu
	SetIsOpen(false);

	// Now verify the index
	if ( MenuItemIndex >= 0 && MenuItemIndex < SubMenuItems.Num())
	{
		if (SubMenuItems[MenuItemIndex].OnClickedDelegate.IsBound())
		{
			SubMenuItems[MenuItemIndex].OnClickedDelegate.Execute();
		}
	}

	return FReply::Handled();
}


FReply SUTComboButton::SimpleSubMenuButtonClicked(int32 MenuItemIndex)
{
	SetIsOpen(false);

	if (OnButtonSubMenuSelect.IsBound())
	{
		OnButtonSubMenuSelect.Execute(MenuItemIndex, SharedThis(this));
	}
	return FReply::Handled();
}

void SUTComboButton::OnMenuDismissed()
{
	SetIsOpen(false, false);
	SMenuAnchor::OnMenuDismissed();
}

void SUTComboButton::SetIsOpen( bool InIsOpen, const bool bFocusMenu, const int32 FocusUserIndex )
{
	SMenuAnchor::SetIsOpen(InIsOpen, bFocusMenu, FocusUserIndex);
	if (MyButton.IsValid())
	{
		if (InIsOpen)
		{
			MyButton->BePressed();
		}
		else
		{
			MyButton->UnPressed();
		}
	}
}


FReply SUTComboButton::UTOnButtonClicked(int32 ButtonIndex)
{
	if (!bRightClickOpensMenu || ButtonIndex > 0)
	{
		bDismissedThisTick = false;
		TSharedPtr<SWidget> Content = nullptr;

		if (ShouldOpenDueToClick())
		{
			// Button was clicked; show the popup.
			// Do nothing if clicking on the button also dismissed the menu, because we will end up doing the same thing twice.
			this->SetIsOpen( true, bIsFocusable );

			// If the menu is open, execute the related delegate.
			if( OnSubmenuShown.IsBound() )
			{
				OnSubmenuShown.Execute();
			}

			// Focusing any newly-created widgets must occur after they have been added to the UI root.
			FReply ButtonClickedReply = FReply::Handled();
	
			if (bIsFocusable)
			{
				TSharedPtr<SWidget> WidgetToFocus = WidgetToFocusPtr.Pin();
				if (!WidgetToFocus.IsValid())
				{
					// no explicitly focused widget, try to focus the content
					WidgetToFocus = Content;
				}

				if (!WidgetToFocus.IsValid())
				{
					// no content, so try to focus the original widget set on construction
					WidgetToFocus = ContentWidgetPtr.Pin();
				}

				if (WidgetToFocus.IsValid())
				{
					return FReply::Handled().SetUserFocus(WidgetToFocus.ToSharedRef(), EFocusCause::SetDirectly);
				}
			}
		}
		else
		{
			OnMenuDismissed();
			MyButton->UnPressed();
		}
	}

	else
	{
		if (OnClicked.IsBound())
		{
			return OnClicked.Execute();
		}
	}

	return FReply::Handled();

}

void SUTComboButton::SetMenus( const TSharedRef< SWidget >& InContent )
{
	MenuContent = 
		SNew(SBorder)
		.BorderImage(MenuBorderBrush)
		.Padding(MenuBorderPadding)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.MaxHeight(700)
			[
				InContent
			]
		];
}

void SUTComboButton::SetButtonStyle(const FButtonStyle* NewButtonStyle)
{
	if (MyButton.IsValid())
	{
		MyButton->SetButtonStyle(NewButtonStyle);
	}
}

void SUTComboButton::UnPressed()
{
	if (MyButton.IsValid()) MyButton->UnPressed();
}

void SUTComboButton::BePressed()
{
	if (MyButton.IsValid()) MyButton->BePressed();
}

FReply SUTComboButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

int32 SUTComboButton::GetSubMenuItemCount()
{
	return SubMenuItems.Num();
}

#endif