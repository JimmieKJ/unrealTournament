// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SUTButton.h"


#if !UE_SERVER

class SUTComboButton;

DECLARE_DELEGATE_TwoParams( FUTButtonSubMenuSelect, int32, TSharedPtr<SUTComboButton> );

struct FUTComboSubMenuItem
{
	FText MenuCaption;
	bool bSpacer;
	FOnClicked OnClickedDelegate;

	FUTComboSubMenuItem()
		: MenuCaption(FText::GetEmpty())
		, bSpacer(true)
		, OnClickedDelegate(nullptr)

	{
	}

	FUTComboSubMenuItem(FText inText, FOnClicked inOnClick)
		: MenuCaption(inText)
		, bSpacer(false)
		, OnClickedDelegate(inOnClick)
	{
	}
};

class UNREALTOURNAMENT_API SUTComboButton : public SMenuAnchor
{
	SLATE_BEGIN_ARGS(SUTComboButton)
		: _ComboButtonStyle( &SUTStyle::Get().GetWidgetStyle< FComboButtonStyle >( "UT.ComboButton" ) )
		, _ButtonStyle(nullptr)
		, _TextStyle( &SUTStyle::Get().GetWidgetStyle< FTextBlockStyle >("UT.Font.NormalText.Small.Bold") )
		, _MenuButtonStyle(&SUTStyle::Get().GetWidgetStyle<FButtonStyle>("UT.Button.MenuBar"))
		, _MenuButtonTextStyle(&SUTStyle::Get().GetWidgetStyle< FTextBlockStyle >("UT.Font.NormalText.Small"))
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
		, _ContentHAlign(HAlign_Left)
		{}

		// Styles

		SLATE_STYLE_ARGUMENT( FComboButtonStyle, ComboButtonStyle )

		/** The visual style of the button (overrides ComboButtonStyle) */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )
		
		/** The text style of the button */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )

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

		/** The text to display in this button, if no custom content is specified */
		SLATE_ATTRIBUTE( FText, Text )

		SLATE_ARGUMENT( EHorizontalAlignment, ContentHAlign )


	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	
	void AddSpacer(bool bUpdate = false);
	void AddSubMenuItem(const FText& NewItem, FOnClicked OnClickDelegate, bool bUpdate = false);
	void ClearSubMenuItems();

	// Rebuilds the submenu
	void RebuildMenuContent();


	void SetButtonStyle(const FButtonStyle* NewButtonStyle);

	virtual void UnPressed();
	virtual void BePressed();

	int32 GetSubMenuItemCount();

	virtual void SetIsOpen( bool InIsOpen, const bool bFocusMenu = true ) override;

protected:

	EHorizontalAlignment ContentHAlign;

	/** Area where the button's content resides */
	SHorizontalBox::FSlot* ButtonContentSlot;

	TSharedPtr<SImage> DownArrowImage;
	TSharedPtr<SUTButton> MyButton;

	TSharedPtr<SVerticalBox> MenuBox;

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

	TArray<FUTComboSubMenuItem> SubMenuItems;

	void UTOnButtonClicked(int32 ButtonIndex);

	// The internal handler for when a menu item is clicked.  This will look up the menu item in the 
	// array and pass along the delegate call
	FReply InteralSubMenuButtonClickedHandler(int32 MenuItemIndex);

	// This is a simple handler for the DefaultMenuItems array.  The goal here is to make setting up a simple menu
	// as painless and possible.  You pass in a an array of strings, and a single delegate for handling the results.  This
	// is great for simple drop-downs that have only a few limited options.
	FReply SimpleSubMenuButtonClicked(int32 MenuItemIndex);

	virtual void SetMenus( const TSharedRef< SWidget >& InContent );

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void OnMenuDismissed();
};

#endif