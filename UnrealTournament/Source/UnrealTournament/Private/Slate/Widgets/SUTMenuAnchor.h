// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SUTButton.h"


#if !UE_SERVER

DECLARE_DELEGATE_OneParam( FUTSubMenuSelect, FName );
DECLARE_DELEGATE(FMenuShown);

struct FMenuOptionData
{
	FText OptionText;
	FName OptionTag;
	bool bSpacer;

	FMenuOptionData()
	{
		OptionText = FText::GetEmpty();
		OptionTag = NAME_None;
		bSpacer = true;
	}

	FMenuOptionData(FText inOptionText, FName inOptionTag)
	: OptionText(inOptionText)
	, OptionTag(inOptionTag)
	{
		bSpacer = false;
	}
};

DECLARE_DELEGATE_TwoParams(FUTGetSubMenuOptions, FString, TArray<FMenuOptionData>&);

class UNREALTOURNAMENT_API SUTMenuAnchor : public SMenuAnchor
{
	SLATE_BEGIN_ARGS(SUTMenuAnchor)
		: _MenuButtonStyle(nullptr)
		, _MenuButtonTextStyle(nullptr)
		, _ButtonContent()
		, _MenuContent()
		, _MenuPlacement(MenuPlacement_ComboBox)
		, _ContentPadding(FMargin(5))
		, _MenuBorderPadding(FMargin(2.0,2.0,2.0,2.0))
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Center)
		, _SearchTag(TEXT(""))
		{}

		// Styles

		/** Style of the submenu buttons */
		SLATE_STYLE_ARGUMENT( FButtonStyle, MenuButtonStyle)

		/** Style of the Submenu Text */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, MenuButtonTextStyle)

		// Slots

		/** The Contents of the Button */
		SLATE_NAMED_SLOT( FArguments, ButtonContent )
		
		/** Optional static menu content.  If the menu content needs to be dynamically built, use the OnGetMenuContent event */
		SLATE_NAMED_SLOT( FArguments, MenuContent )
		
		// Events

		// Called when the submenu is shown
		SLATE_EVENT( FMenuShown, OnSubmenuShown )

		// Called when the user selects a submenu item
		SLATE_EVENT(FUTSubMenuSelect, OnSubMenuSelect)
		
		SLATE_EVENT(FUTGetSubMenuOptions, OnGetSubMenuOptions)

		SLATE_EVENT(FOnClicked, OnClicked)

		/** Where should the menu be placed */
		SLATE_ATTRIBUTE( EMenuPlacement, MenuPlacement )

		SLATE_ATTRIBUTE( FMargin, ContentPadding )

		SLATE_ARGUMENT( FMargin, MenuBorderPadding )

		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )

		/** Comma separated list of menu items */
		SLATE_ARGUMENT( TArray<FMenuOptionData>, DefaultSubMenuItems)
		
		SLATE_ARGUMENT(FString, SearchTag)

	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs);


protected:
	FString SearchTag;

	TArray<FMenuOptionData> SubMenuItems;

	/** Area where the button's content resides */
	SHorizontalBox::FSlot* ButtonContentSlot;

	const FButtonStyle* MenuButtonStyle;
	const FTextBlockStyle* MenuButtonTextStyle;

	FUTSubMenuSelect OnSubMenuSelect;
	FMenuShown OnSubmenuShown;
	FUTGetSubMenuOptions OnGetSubMenuOptions;

	TWeakPtr<SWidget> WidgetToFocusPtr;

	/** Brush to use to add a "menu border" around the drop-down content */
	const FSlateBrush* MenuBorderBrush;

	/** Padding to use to add a "menu border" around the drop-down content */
	FMargin MenuBorderPadding;

	/** The content widget, if any, set by the user on creation */
	TWeakPtr<SWidget> ContentWidgetPtr;

	/** Can this button be focused? */
	bool bIsFocusable;

	// If true, only the right button opens the menu
	bool bRightClickOpensMenu;

	void RebuildSubMenu();

	FReply SubMenuButtonClicked(FName Tag);
	FReply UTOnButtonClicked(int32 ButtonIndex);

	TSharedPtr<SUTButton> MyButton;

};

#endif