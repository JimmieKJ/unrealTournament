// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
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

	// Build the actual button space.https://forums.unrealtournament.com/attachment.php?attachmentid=10183&d=1415937070
	TSharedPtr<SHorizontalBox> HBox;

	SMenuAnchor::Construct( SMenuAnchor::FArguments()
		.Placement( InArgs._MenuPlacement )
		[
			SAssignNew( MyButton, SUTButton )
			.ContentPadding( FMargin( 1, 0 ) )
			.ButtonStyle( OurButtonStyle )
			.ClickMethod( EButtonClickMethod::MouseDown )
			.UTOnButtonClicked( this, &SUTComboButton::UTOnButtonClicked )
			.ContentPadding( InArgs._ContentPadding )
			.IsFocusable( InArgs._IsFocusable )
			.IsToggleButton(InArgs._IsToggleButton)
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

	// Handle the default menu items

	if (!InArgs._DefaultMenuItems.IsEmpty() && InArgs._DefaultMenuItems != TEXT(""))
	{
		TArray<FString> MenuItems;
		InArgs._DefaultMenuItems.ParseIntoArray(&MenuItems, TEXT(","), true);
		for (int32 i=0; i < MenuItems.Num(); i++)
		{
			SubMenuItems.Add(FText::FromString(*MenuItems[i]));
		}
	}
	else
	{
		SubMenuItems.Add(FText::GetEmpty());
	}

	if (SubMenuItems.Num() > 0)
	{
		RebuildSubMenu();
	}
}

void SUTComboButton::AddSubMenuItem(const FText& NewItem)
{
	SubMenuItems.Add(NewItem);
	RebuildSubMenu();
}
void SUTComboButton::ClearSubMenuItems()
{
	SubMenuItems.Empty();
	RebuildSubMenu();
}

void SUTComboButton::RebuildSubMenu()
{
	TSharedPtr<SVerticalBox> MenuBox;
	SAssignNew(MenuBox, SVerticalBox);

	for (int32 i=0;i<SubMenuItems.Num();i++)
	{
		MenuBox->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(MenuButtonStyle)
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(SubMenuItems[i])
			.TextStyle(MenuButtonTextStyle)
			.OnClicked(this, &SUTComboButton::SubMenuButtonClicked, i)
		];
	}

	SetMenus(MenuBox.ToSharedRef());
}

FReply SUTComboButton::SubMenuButtonClicked(int32 ButtonIndex)
{
	if (OnButtonSubMenuSelect.IsBound())
	{
		OnButtonSubMenuSelect.Execute(ButtonIndex, SharedThis(this));
	}

	SetIsOpen(false);
	return FReply::Handled();
}

void SUTComboButton::UTOnButtonClicked(int32 ButtonIndex)
{

	if (!bRightClickOpensMenu || ButtonIndex > 0)
	{
		bDismissedThisTick = false;
		TSharedPtr<SWidget> Content = nullptr;

		// Button was clicked; show the popup.
		// Do nothing if clicking on the button also dismissed the menu, because we will end up doing the same thing twice.
		this->SetIsOpen( ShouldOpenDueToClick(), bIsFocusable );

		// If the menu is open, execute the related delegate.
		if( IsOpen() && OnSubmenuShown.IsBound() )
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
				ButtonClickedReply.SetUserFocus(WidgetToFocus.ToSharedRef(), EFocusCause::SetDirectly);
			}
		}
	}

	else
	{
		if (OnClicked.IsBound())
		{
			OnClicked.Execute();
		}
	}

}

void SUTComboButton::SetMenus( const TSharedRef< SWidget >& InContent )
{
	MenuContent = 
		SNew(SBorder)
		.BorderImage(MenuBorderBrush)
		.Padding(MenuBorderPadding)
		[
			InContent
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


#endif