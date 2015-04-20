// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SUTButton.h"


#if !UE_SERVER

class SUTComboButton;

DECLARE_DELEGATE_TwoParams( FUTButtonSubMenuSelect, int32, TSharedPtr<SUTComboButton> );

class SUTComboButton : public SMenuAnchor
{
	SLATE_BEGIN_ARGS(SUTComboButton)
		: _ComboButtonStyle( &FCoreStyle::Get().GetWidgetStyle< FComboButtonStyle >( "ComboButton" ) )
		, _ButtonStyle(nullptr)
		, _MenuButtonStyle(nullptr)
		, _MenuButtonTextStyle(nullptr)
		, _ButtonContent()
		, _MenuContent()
		, _IsFocusable(true)
		, _HasDownArrow(true)
		, _bRightClickOpensMenu(false)
		, _MenuPlacement(MenuPlacement_ComboBox)
		, _ContentPadding(FMargin(5))
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Center)
		, _IsToggleButton(false)
		{}

		// Styles

		SLATE_STYLE_ARGUMENT( FComboButtonStyle, ComboButtonStyle )

		/** The visual style of the button (overrides ComboButtonStyle) */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )
		
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
		SLATE_EVENT( FOnComboBoxOpened, OnSubmenuShown )

		// Called when the button is clicked.  If the sub menu opens this will not be shown
		SLATE_EVENT( FOnClicked, OnClicked )

		// Called when the user selects a submenu item
		SLATE_EVENT(FUTButtonSubMenuSelect, OnButtonSubMenuSelect)
		
		// Args and Attribs

		SLATE_ARGUMENT( bool, IsFocusable )

		SLATE_ARGUMENT( bool, HasDownArrow )

		/** If true, only right clicking on this widget will open the menu.  Very useful for custom buttons */
		SLATE_ARGUMENT( bool, bRightClickOpensMenu );

		/** Where should the menu be placed */
		SLATE_ATTRIBUTE( EMenuPlacement, MenuPlacement )

		SLATE_ATTRIBUTE( FMargin, ContentPadding )

		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )

		/** Comma separated list of menu items */
		SLATE_ARGUMENT( FString, DefaultMenuItems)
		
		/** Determines if this is a toggle button or not. */
		SLATE_ARGUMENT( bool, IsToggleButton )

	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	
	void AddSubMenuItem(const FText& NewItem);
	void ClearSubMenuItems();

	void SetButtonStyle(const FButtonStyle* NewButtonStyle);

	virtual void UnPressed();
	virtual void BePressed();

protected:

	/** Area where the button's content resides */
	SHorizontalBox::FSlot* ButtonContentSlot;

	TSharedPtr<SImage> DownArrowImage;
	TSharedPtr<SUTButton> MyButton;

	const FButtonStyle* MenuButtonStyle;
	const FTextBlockStyle* MenuButtonTextStyle;

	FUTButtonSubMenuSelect OnButtonSubMenuSelect;
	FOnComboBoxOpened OnSubmenuShown;
	FOnClicked OnClicked;

	TWeakPtr<SWidget> WidgetToFocusPtr;

	/** Brush to use to add a "menu border" around the drop-down content */
	const FSlateBrush* MenuBorderBrush;

	/** Padding to use to add a "menu border" around the drop-down content */
	FMargin MenuBorderPadding;

	/** The content widget, if any, set by the user on creation */
	TWeakPtr<SWidget> ContentWidgetPtr;

	/** Can this button be focused? */
	bool bIsFocusable;

	/** If true, we are showing the down arrow */
	bool bShowDownArrow;

	// If true, only the right button opens the menu
	bool bRightClickOpensMenu;

	TArray<FText> SubMenuItems;
	void RebuildSubMenu();

	void UTOnButtonClicked(int32 ButtonIndex);

	// Handles when the user clicks on a submenu button
	FReply SubMenuButtonClicked(int32 ButtonIndex);

	virtual void SetMenus( const TSharedRef< SWidget >& InContent );

};

#endif