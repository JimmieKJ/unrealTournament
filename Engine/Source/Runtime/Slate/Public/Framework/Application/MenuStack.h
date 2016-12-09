// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/PopupMethodReply.h"
#include "Framework/Application/IMenu.h"
#include "Runtime/Slate/Private/Framework/Application/Menu.h"
#include "Layout/SlateRect.h"
#include "Widgets/SWidget.h"
#include "Types/SlateStructs.h"
#include "Widgets/SWindow.h"
#include "Application/ThrottleManager.h"

class FWidgetPath;
class SMenuPanel;

typedef TArray<TSharedPtr<SWindow>> FMenuWindowList;	// deprecated

#if UE_BUILD_DEBUG
	// Debugging is harder with the inline allocator
	typedef TArray<TSharedPtr<IMenu>> FMenuList;
#else
	typedef TArray<TSharedPtr<IMenu>, TInlineAllocator<4>> FMenuList;
#endif


/** Describes a simple animation for popup window introductions */
struct FPopupTransitionEffect
{
	/** Direction to slide in from */
	enum ESlideDirection
	{
		/** No sliding */
		None,

		/** Sliding direction for a combo button */
		ComboButton,

		/** Sliding direction for a top-level pull-down menu or combo box */
		TopMenu,

		/** Sliding direction for a sub-menu */
		SubMenu,

		/** Sliding direction for a popup that lets the user type in data */
		TypeInPopup,

		/** Sliding direction preferred for context menu popups */
		ContextMenu,

	} SlideDirection;


	/** Constructor */
	FPopupTransitionEffect( const ESlideDirection InitSlideDirection )
		: SlideDirection(InitSlideDirection)
	{ }
};

/**
 * Represents a stack of open menus. The last item in the stack is the top most menu
 * Menus are described as IMenus. Implementations of IMenu can control how the menus are created and presented (e.g. in their own window)
 */
class FMenuStack
{
public:
	/** Constructor */
	FMenuStack()
		: bHostWindowGuard(false)
	{}

	/**
	 * [Deprecated] Pushes a new menu onto the stack.  Automatically removes windows from the stack that are no longer valid
	 * Invalid windows are those that are not parents of the window menu being pushed and any windows on the same stack level
	 * This is the old method for creating menus.
	 *
	 * @param InParentWindow		The parent window of the menu
	 * @param InContent				The menu's content
	 * @param SummonLocation		Location where the menu should appear
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bFocusImmediately		Should the popup steal focus when shown?
	 * @param bShouldAutoSize		True if the new window should automatically size itself to its content
	 * @param WindowSize			When bShouldAutoSize=false, this must be set to the size of the window to be created
	 * @param SummonLocationSize	An optional size around the summon location which describes an area in which the menu may not appear
	 */
	DEPRECATED(4.9, "PushMenu() returning an SWindow is deprecated. Use Push() that returns an IMenu instead.")
	TSharedRef<SWindow> PushMenu( const TSharedRef<SWindow>& InParentWindow, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately = true, const bool bShouldAutoSize = true, const FVector2D& WindowSize = FVector2D::ZeroVector, const FVector2D& SummonLocationSize = FVector2D::ZeroVector );

	/**
	 * Pushes a new menu onto the stack. The widget path will be searched for existing menus and the new menu will be parented appropriately.
	 * Menus are always auto-sized. Use fixed-size content if a fixed size is required.
	 * 
	 * @param InOwnerPath			The widget path for the parent widget of this menu.
	 * @param InContent				The menu's content
	 * @param SummonLocation		Location in screen-space where the menu should appear
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bFocusImmediately		Should the popup steal focus when shown?
	 * @param SummonLocationSize	An optional size around the summon location which describes an area in which the menu may not appear
	 * @param InMethod				An optional popup method that will override the default method for the widgets in InOwnerPath
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 * @param bEnablePerPixelTransparency Does the menu's content require per pixel transparency?
	 */
	TSharedRef<IMenu> Push(const FWidgetPath& InOwnerPath, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately = true, const FVector2D& SummonLocationSize = FVector2D::ZeroVector, TOptional<EPopupMethod> InMethod = TOptional<EPopupMethod>(), const bool bIsCollapsedByParent = true, const bool bEnablePerPixelTransparency = false);
	
	/**
	 * Pushes a new child menu onto the stack.
	 * Menus are always auto-sized. Use fixed-size content if a fixed size is required.
	 * 
	 * @param InParentMenu			The parent menu for this menu
	 * @param InContent				The menu's content
	 * @param SummonLocation		Location in screen-space where the menu should appear
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bFocusImmediately		Should the popup steal focus when shown?
	 * @param SummonLocationSize	An optional size around the summon location which describes an area in which the menu may not appear
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 * @param bEnablePerPixelTransparency Does the menu's content require per pixel transparency?
	 */
	TSharedRef<IMenu> Push(const TSharedPtr<IMenu>& InParentMenu, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately = true, const FVector2D& SummonLocationSize = FVector2D::ZeroVector, const bool bIsCollapsedByParent = true, const bool bEnablePerPixelTransparency = false);

	/**
	 * Pushes a new menu onto the stack that is drawn by an external host widget.
	 * The widget path will be searched for existing menus and the new menu will be parented appropriately.
	 * Menus are always auto-sized. Use fixed-size content if a fixed size is required.
	 * 
	 * @param InOwnerPath			The widget path for the parent widget of this menu
	 * @param InMenuHost			The host widget that draws the menu's content
	 * @param InContent				The menu's content
	 * @param OutWrappedContent		Returns the InContent wrapped with widgets needed by the menu stack system. This is what should be drawn by the host after this call.
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 */
	TSharedRef<IMenu> PushHosted(const FWidgetPath& InOwnerPath, const TSharedRef<IMenuHost>& InMenuHost, const TSharedRef<SWidget>& InContent, TSharedPtr<SWidget>& OutWrappedContent, const FPopupTransitionEffect& TransitionEffect, EShouldThrottle ShouldThrottle, const bool bIsCollapsedByParent = true);
	
	/**
	 * Pushes a new child menu onto the stack that is drawn by an external host widget.
	 * Menus are always auto-sized. Use fixed-size content if a fixed size is required.
	 * 
	 * @param InParentMenu			The parent menu for this menu
	 * @param InMenuHost			The host widget that draws the menu's content
	 * @param InContent				The menu's content
	 * @param OutWrappedContent		Returns the InContent wrapped with widgets needed by the menu stack system. This is what should be drawn by the host after this call.
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 */
	TSharedRef<IMenu> PushHosted(const TSharedPtr<IMenu>& InParentMenu, const TSharedRef<IMenuHost>& InMenuHost, const TSharedRef<SWidget>& InContent, TSharedPtr<SWidget>& OutWrappedContent, const FPopupTransitionEffect& TransitionEffect, EShouldThrottle ShouldThrottle, const bool bIsCollapsedByParent = true);

	/**
	 * [Deprecated] Dismisses the menu stack up to a certain level (by default removes all levels in the stack)
	 * Dismisses in reverse order so to keep just the first level you would pass in 1
	 *
	 * @param FirstStackIndexToRemove	The first level of the stack to remove
	 */
	DEPRECATED(4.9, "Dismiss() taking a stack index is deprecated. Use DismissFrom() or DismissAll().")
	void Dismiss( int32 FirstStackIndexToRemove = 0 );
	
	/**
	 * Dismisses the menu stack including InFromMenu and all its child menus
	 * Dismisses in reverse order (children first)
	 *
	 * @param InFromMenu		The menu to remove along with its children
	 */
	void DismissFrom(const TSharedPtr<IMenu>& InFromMenu);

	/**
	 * Dismisses the entire menu stack
	 */
	void DismissAll();

	/**
	 * Removes a window from the stack and all of its children.  Assumes the window is cleaned up elsewhere
	 *
	 * @param WindowToRemove	The window to remove (will remove all children of this window as well)	
	 */
	DEPRECATED(4.9, "RemoveWindow() is deprecated. Do not use.")
	void RemoveWindow( TSharedRef<SWindow> WindowToRemove );

	/**
	 * Called by the application when any window is destroyed.
	 *
	 * @param InWindow	The window that was destroyed
	 */
	void OnWindowDestroyed(TSharedRef<SWindow> InWindow);

	/**
	 * Notifies the stack that a new window was activated.  The menu stack will be dismissed if the activated window is not a menu in the stack
	 *
	 * @param ActivatedWindow	The window that was just activated
	 */
	void OnWindowActivated( TSharedRef<SWindow> ActivatedWindow );

	/**
	 * [Deprecated] Finds a window in the stack.  (Deprecated but not using the deprecation macro. This is to avoid warnings as this is still called from a deprecated FSlateApplication method)
	 * This is deprecated because menus are no longer necessarily windows. Callers should also not need to know the stack levels by index. Menus are identified using the IMenu interface.
	 *
	 * @param	WindowToFind	The window to look for
	 * @return	The level of the stack where the window is located or INDEX_NONE if the window could not be found
	 */
	int32 FindLocationInStack( TSharedPtr<SWindow> WindowToFind ) const;

	/**
	 * Finds a menu in the stack that owns InWindow.
	 * Although this method isn't deprecated, it is intended for private use inside FMenuStack
	 * and its use as a public method should be avoided. Menus should be identified in client code as IMenu interfaces.
	 *
	 * @param	InWindow	The window to look for
	 * @return	The menu in the stack that owns InWindow, or an invalid ptr if not found
	 */
	TSharedPtr<IMenu> FindMenuFromWindow(TSharedRef<SWindow> InWindow) const;

	/**
	 * Searches from bottom to top of the widget path for a menu in the stack.
	 * If the widget at the end of PathToQuery is inside a menu, there will be a menu content wrapper widget in the path.
	 *
	 * @param	PathToQuery		The widget path to search for a menu
	 * @return	The menu in the stack that contains the widget at the end of PathToQuery, or an invalid ptr if not found
	 */
	TSharedPtr<IMenu> FindMenuInWidgetPath(const FWidgetPath& PathToQuery) const;

	/**
	 * Called by the application when showing tooltips. It prevents tooltips from drawing over menu items.
	 *
	 * @param InMenu				A menu in the stack, used to generate the force field rect
	 * @param PathContainingMenu	A widget path containing InMenu. This could be generated but the application has the path so it helps performance to pass it in here. 
	 * @return						A rectangle enclosing the menu's children (created from the clipping rects in the geometry in PathContainingMenu)
	 */
	FSlateRect GetToolTipForceFieldRect(TSharedRef<IMenu> InMenu, const FWidgetPath& PathContainingMenu) const;

	/**
	 * [Deprecated]
	 * @return	Returns the size of the menu stack
	 */
	DEPRECATED(4.9, "GetNumStackLevels() is deprecated. Consider HasMenus().")
	int32 GetNumStackLevels( ) const;

	/**
	 * @return	Returns the window that is the parent of everything in the stack, if any. If the stack is empty, returns an invalid ptr.
	 */
	TSharedPtr<SWindow> GetHostWindow() const;

	/**
	* @return	True if the stack has one or more menus in it, false if it is empty.
	*/
	bool HasMenus() const;

	/**
	 * [Deprecated] This is deprecated because menus are no longer necessarily windows. Menus are identified using the IMenu interface.
	 *
	 * @return Returns whether the window has child menus. If the window is not a menu, returns false.
	 */
	DEPRECATED(4.9, "HasOpenSubMenus() taking a window is deprecated. Use HasOpenSubMenus() taking an IMenu as a parameter.")
	bool HasOpenSubMenus( const TSharedRef<SWindow>& Window ) const;

	/**
	 * @return Returns whether the menu has child menus. If the menu isn't in the stack, returns false.
	 */
	bool HasOpenSubMenus(TSharedPtr<IMenu> InMenu) const;

	/**
	 * [Deprecated] Returned all of the windows at the specified stack level index. Always returns an empty list because it's deprecated and the return type is not supported.
	 *
	 * @param	StackLevelIndex	Index into the menu stack
	 *
	 * @return	List of windows at this stack level (returns an empty list because it's deprecated)
	 */
	DEPRECATED(4.9, "GetWindowsAtStackLevel() is deprecated. Do not use.")
	FMenuWindowList& GetWindowsAtStackLevel( const int32 StackLevelIndex );

private:
	/**
	 * Queries the widgets in the path for a menu popup method.
	 *
	 * @param	PathToQuery	The path to query
	 *
	 * @return	The popup method chosen by a widget in the path. Defaults to EPopupMethod::CreateNewWindow if no widgets in the path have a preference.
	 */
	FPopupMethodReply QueryPopupMethod(const FWidgetPath& PathToQuery);

	/**
	 * Called by public Dismiss methods. Dismisses all menus in the stack from FirstStackIndexToRemove and below.
	 *
	 * @param	FirstStackIndexToRemove	Index into the stack from which menus are dismissed.
	 */
	void DismissInternal(int32 FirstStackIndexToRemove);

	/**
	 * Handles changes to the owner path, potentially locating and establishing the new host. Makes sure overlay slots are added/removed as appropriate.
	 */
	void SetHostPath(const FWidgetPath& InOwnerPath);

	/**
	 * Callback method called by all menus when they are dismissed.  Handles changes to the stack state when a menu is destroyed.
	 *
	 * @param	InMenu	The menu that was destroyed.
	 */
	void OnMenuDestroyed(TSharedRef<IMenu> InMenu);

	/**
	 * Callback method called by all menus when they lose focus.  Handles dismissing menus that have lost focus.
	 *
	 * @param	InFocussedPath	The new focus path.
	 */
	void OnMenuContentLostFocus(const FWidgetPath& InFocussedPath);

	/**
	 * Called by all menus during creation. Handles wrapping content in SMenuContentWrapper that acts as a marker in widget paths and in a box that sets minimum size.
	 *
	 * @param	InContent			The unwrapped content.
	 * @param	OptionalMinWidth	Optional minimum width for the wrapped content.
	 * @param	OptionalMinHeight	Optional minimum height for the wrapped content.
	 */
	TSharedRef<SWidget> WrapContent(TSharedRef<SWidget> InContent, FOptionalSize OptionalMinWidth = FOptionalSize(), FOptionalSize OptionalMinHeight = FOptionalSize());

	/** Contains all the options passed to the pre-push stage of the menu creation process */
	struct FPrePushArgs
	{
		FPrePushArgs() : TransitionEffect(FPopupTransitionEffect::None) {}

		TSharedPtr<SWidget> Content;
		FSlateRect Anchor;
		FPopupTransitionEffect TransitionEffect;
		bool bFocusImmediately;
		bool bIsCollapsedByParent;
	};

	/** Contains all the options returned from the pre-push stage of the menu creation process */
	struct FPrePushResults
	{
		TSharedPtr<SWidget> WrappedContent;
		TSharedPtr<SWidget> WidgetToFocus;
		FVector2D AnimStartLocation;
		FVector2D AnimFinalLocation;
		bool bAnchorSetsMinWidth;
		FVector2D ExpectedSize;
		bool bAllowAnimations;
		bool bFocusImmediately;
		bool bIsCollapsedByParent;
	};

	/**
	 * This is the pre-push stage of menu creation.
	 * It is responsible for wrapping content and calculating menu positioning. This stage is not used by "hosted" menus.
	 *
	 * @param	InArgs	Pre-push options.
	 *
	 * @return			Pre-push results.
	 */
	FPrePushResults PrePush(const FPrePushArgs& InArgs);

	/**
	 * This is the common code used during menu creation. This stage is not used by "hosted" menus.
	 * It handles pre-push, actual menu creation and post-push stack updates.
	 *
	 * @param InParentMenu			The parent menu for this menu
	 * @param InContent				The menu's content
	 * @param Anchor				The anchor location for the menu (rect around the parent widget that is summoning the menu)
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bFocusImmediately		Should the popup steal focus when shown?
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 *
	 * @return			The newly created menu.
	 */
	TSharedRef<IMenu> PushInternal(const TSharedPtr<IMenu>& InParentMenu, const TSharedRef<SWidget>& InContent, FSlateRect Anchor, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately, EShouldThrottle ShouldThrottle, const bool bIsCollapsedByParent, const bool bEnablePerPixelTransparency);

	/**
	 * This is the actual menu object creation method for FMenuInWindow menus.
	 * It creates a new SWindow and new FMenuInWindow that uses it.
	 *
	 * @param InParentMenu			The parent menu for this menu
	 * @param InPrePushResults		The results of the pre-push stage
	 *
	 * @return			The newly created FMenuInWindow menu.
	 */
	TSharedRef<FMenuBase> PushNewWindow(TSharedPtr<IMenu> InParentMenu, const FPrePushResults& InPrePushResults, const bool bEnablePerPixelTransparency);

	/**
	 * This is the actual menu object creation method for FMenuInPopup menus.
	 * It creates a new FMenuInWindow and adds it into the overlay layer on the host window.
	 *
	 * @param InParentMenu			The parent menu for this menu
	 * @param InPrePushResults		The results of the pre-push stage
	 *
	 * @return			The newly created FMenuInPopup menu.
	 */
	TSharedRef<FMenuBase> PushPopup(TSharedPtr<IMenu> InParentMenu, const FPrePushResults& InPrePushResults);

	/**
	 * This is the post-push stage of menu creation.
	 * It is responsible for updating the stack with the new menu and removing any that it replaces.
	 *
	 * @param InParentMenu         The parent menu for this menu
	 * @param InMenu               A newly created menu
	 * @param ShouldThrottle       Should pushing this menu enable throttling of the engine for a more responsive UI
	 */
	void PostPush(TSharedPtr<IMenu> InParentMenu, TSharedRef<FMenuBase> InMenu, EShouldThrottle ShouldThrottle);

	/** The popup method currently used by the whole stack. It can only use one at a time */
	FPopupMethodReply ActiveMethod;

	/** The parent window of the root menu in the stack. NOT the actual menu window if it's a CreateNewWindow */
	TSharedPtr<SWindow> HostWindow;

	/** The parent window of the root menu in the stack. NOT the actual menu window if it's a CreateNewWindow */
	TSharedPtr<SMenuPanel> HostWindowPopupPanel;

	/** The popup layer that contains our HostWindowPopupPanel. */
	TSharedPtr<FPopupLayer> HostPopupLayer;

	/** The array of menus in the stack */
	TArray<TSharedPtr<class FMenuBase>> Stack;

	/** Maps top-level content widgets (should always be SMenuContentWrappers) to menus in the stack */
	TMap<TSharedPtr<const SWidget>, TSharedPtr<class FMenuBase>> CachedContentMap;
	
	/** Handle to a throttle request made to ensure the menu is responsive in low FPS situations */
	FThrottleRequest ThrottleHandle;

	/** Temporary ptr to a new window created during the menu creation process. Nulled before the Push() call returns. Stops activation of the new window collapsing the stack. */
	TSharedPtr<SWindow> PendingNewWindow;

	/** Temporary ptr to a new menu created during the menu creation process. Nulled before the Push() call returns. Stops it collapsing the stack when it gets focus. */
	TSharedPtr<class FMenuBase> PendingNewMenu;

	/** Guard to prevent the HostWindow and HostWindowPopupPanel being set reentrantly */
	bool bHostWindowGuard;
};
