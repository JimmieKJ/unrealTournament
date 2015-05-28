// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "STextBlock.h"

/** Notification when popup is opened/closed. */
DECLARE_DELEGATE_OneParam(FOnIsOpenChanged, bool)


enum class EPopupMethod : uint8
{
	/** Creating a new window allows us to place popups outside of the window in which the menu anchor resides. */
	CreateNewWindow,
	/** Place the popup into the current window. Applications that intend to run in fullscreen cannot create new windows, so they must use this method.. */
	UseCurrentWindow
};

;

/**
 * A PopupAnchor summons a Popup relative to its content.
 * Summoning a popup relative to the cursor is accomplished via the application.
 */
class SLATE_API SMenuAnchor : public SPanel
{
public:
	DEPRECATED(4.7, "You probably do not need this setting any more. See OnQueryPopupMethod() in SWidget.h.")
	typedef ::EPopupMethod EMethod;
	static const EPopupMethod CreateNewWindow = EPopupMethod::CreateNewWindow;
	static const EPopupMethod UseCurrentWindow = EPopupMethod::UseCurrentWindow;
	
public:
	SLATE_BEGIN_ARGS( SMenuAnchor )
		: _Content()
		, _MenuContent( SNew(STextBlock) .Text( NSLOCTEXT("SMenuAnchor", "NoMenuContent", "No Menu Content Assigned; use .MenuContent") ) )
		, _OnGetMenuContent()
		, _Placement( MenuPlacement_BelowAnchor )
		, _Method()
		{}
		
		SLATE_DEFAULT_SLOT( FArguments, Content )
		
		SLATE_ARGUMENT( TSharedPtr<SWidget>, MenuContent )

		SLATE_EVENT( FOnGetContent, OnGetMenuContent )
		
		SLATE_EVENT( FOnIsOpenChanged, OnMenuOpenChanged )

		SLATE_ATTRIBUTE( EMenuPlacement, Placement )

		SLATE_ARGUMENT(TOptional<EPopupMethod>, Method)

	SLATE_END_ARGS()

	virtual ~SMenuAnchor();
	
	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
	
	SMenuAnchor();

	/** See Content Slot attribute */
	void SetContent(TSharedRef<SWidget> InContent);

	/** See MenuContent attribute */
	virtual void SetMenuContent(TSharedRef<SWidget> InMenuContent);
	
	/**
	 * Open or close the popup
	 *
	 * @param InIsOpen    If true, open the popup. Otherwise close it.
	 * @param bFocusMenu  Shoudl we focus the popup as soon as it opens?
	 */
	void SetIsOpen( bool InIsOpen, const bool bFocusMenu = true );
	
	/** @return true if the popup is open; false otherwise. */
	bool IsOpen() const;

	/** @return true if we should open the menu due to a click. Sometimes we should not, if
	   the same MouseDownEvent that just closed the menu is about to re-open it because it happens to land on the button.*/
	bool ShouldOpenDueToClick() const;

	/** @return The current menu position */
	FVector2D GetMenuPosition() const;

	/** @return Whether this menu has open submenus */
	bool HasOpenSubMenus() const;

protected:
	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	// End of SWidget interface

	/** Invoked when the popup window is being closed by the application */
	void RequestClosePopupWindow( const TSharedRef<SWindow>& PopupWindow );

	/** Invoked when a click occurred outside the widget that is our popup; used for popup auto-dismissal */
	void OnClickedOutsidePopup();

	/** @return true if the popup is currently open and reusing an existing window */
	bool IsOpenAndReusingWindow() const;

	/** @return true if the popup is currently open and we created a dedicated window for it */
	bool IsOpenViaCreatedWindow() const;

protected:
	/** A pointer to the window presenting this popup. Pointer is null when the popup is not visible */
	TWeakPtr<SWindow> PopupWindowPtr;

	/** Static menu content to use when the delegate used when OnGetMenuContent is not defined. */
	TSharedPtr<SWidget> MenuContent;
	
	/** Callback invoked when the popup is being summoned */
	FOnGetContent OnGetMenuContent;

	/** Callback invoked when the popup is being opened/closed */
	FOnIsOpenChanged OnMenuOpenChanged;

	/** How should the popup be placed relative to the anchor. */
	TAttribute<EMenuPlacement> Placement;

	/** Was the menu just dismissed this tick? */
	bool bDismissedThisTick;

	/** Should we summon a new window for this popup or  */
	TOptional<EPopupMethod> Method;

	/** Method currently being used to show the popup. No value if popup is closed. */
	TOptional<EPopupMethod> MethodInUse;

	/**
	 * @todo Slate : Unify geometry so that this is not necessary.
	 * Stores the local offset of the popup as we have to compute it in OnTick;
	 * Cannot compute in OnArrangeChildren() because it can be
	 * called in different spaces (window or desktop.)
	 */
	FVector2D LocalPopupPosition;

	TPanelChildren<FSimpleSlot> Children;

	/** Handle to the registered OnClickedOutsidePopup delegate */
	FDelegateHandle OnClickedOutsidePopupDelegateHandle;
};
