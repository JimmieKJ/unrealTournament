// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUTMenuAnchor.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"

#if !UE_SERVER

void SUTMenuAnchor::Construct(const FArguments& InArgs)
{
	SearchTag = InArgs._SearchTag;

	MenuButtonStyle = InArgs._MenuButtonStyle;
	MenuButtonTextStyle = InArgs._MenuButtonTextStyle;

	OnSubMenuSelect = InArgs._OnSubMenuSelect;
	OnSubmenuShown = InArgs._OnSubmenuShown;
	OnGetSubMenuOptions = InArgs._OnGetSubMenuOptions;

	MenuBorderPadding = InArgs._MenuBorderPadding;

	ContentWidgetPtr = InArgs._MenuContent.Widget;

	TSharedPtr<SHorizontalBox> HBox;
	SMenuAnchor::Construct( SMenuAnchor::FArguments()
		.Placement( InArgs._MenuPlacement )
		[
			SNew( SUTButton )
			.ButtonStyle( SUTStyle::Get(),"UT.ClearButton")
			.ClickMethod( EButtonClickMethod::MouseDown )
			.UTOnButtonClicked( this, &SUTMenuAnchor::UTOnButtonClicked )
			.OnClicked(InArgs._OnClicked)
			.ContentPadding( InArgs._ContentPadding )
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.Expose( ButtonContentSlot )
				.FillWidth( 1 )
				.HAlign( InArgs._HAlign )
				.VAlign( InArgs._VAlign )
				[
					InArgs._ButtonContent.Widget
				]
			]
		]
	);

	// Handle the default menu items

	SubMenuItems = InArgs._DefaultSubMenuItems;

	if (SubMenuItems.Num() > 0)
	{
		RebuildSubMenu();
	}
}

void SUTMenuAnchor::RebuildSubMenu()
{
	TSharedPtr<SVerticalBox> MenuBox;
	SAssignNew(MenuBox, SVerticalBox);

	for (int32 i=0;i<SubMenuItems.Num();i++)
	{
		if (SubMenuItems[i].bSpacer)
		{
			MenuBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(&MenuButtonStyle->Normal)
				]
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0)
					.Padding(10.0,5.0f,10.0,5.0)
					[
						SNew(SBox).HeightOverride(4)					
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
						]
					]
				]
			];
		}
		else
		{
			MenuBox->AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(MenuButtonStyle)
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(SubMenuItems[i].OptionText)
				.TextStyle(MenuButtonTextStyle)
				.OnClicked(this, &SUTMenuAnchor::SubMenuButtonClicked, SubMenuItems[i].OptionTag)
			];

		}
	}

	MenuContent = 
		SNew(SBorder)
		.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
		.Padding(MenuBorderPadding)
		[
			MenuBox.ToSharedRef()
		];
}

FReply SUTMenuAnchor::SubMenuButtonClicked(FName Tag)
{
	if (OnSubMenuSelect.IsBound())
	{
		OnSubMenuSelect.Execute(Tag);
	}

	SetIsOpen(false);
	return FReply::Handled();
}

void SUTMenuAnchor::UTOnButtonClicked(int32 ButtonIndex)
{

	if (ButtonIndex > 0)
	{
		// Rebuild the submenu if the delegate is et
		if (OnGetSubMenuOptions.IsBound())
		{
			SubMenuItems.Empty();
			OnGetSubMenuOptions.Execute(SearchTag, SubMenuItems);
			RebuildSubMenu();
		}

		TSharedPtr<SWidget> Content = nullptr;

		// Button was clicked; show the popup.
		// Do nothing if clicking on the button also dismissed the menu, because we will end up doing the same thing twice.
		SetIsOpen( true, true);

		// If the menu is open, execute the related delegate.
		if( IsOpen() && OnSubmenuShown.IsBound() )
		{
			OnSubmenuShown.Execute();
		}

		// Focusing any newly-created widgets must occur after they have been added to the UI root.
		FReply ButtonClickedReply = FReply::Handled();
	
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



#endif