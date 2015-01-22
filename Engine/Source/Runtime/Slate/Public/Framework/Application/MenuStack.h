// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#if UE_BUILD_DEBUG
	// Debugging is harder with the inline allocator
	typedef TArray<TSharedPtr<SWindow>> FMenuWindowList;
#else
	typedef TArray<TSharedPtr<SWindow>, TInlineAllocator<4>> FMenuWindowList;
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
 * Represents a stack of open menus.  The last item in the stack is the top most menu
 * Each stack level can have any number of windows opened on that level but they all must be parented to the same window
 * which must reside in the previous level of the stack
 */
class FMenuStack
{
public:

	/**
	 * Pushes a new menu onto the stack.  Automatically removes windows from the stack that are no longer valid
	 * Invalid windows are those that are not parents of the window menu being pushed and any windows on the same stack level
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
	TSharedRef<SWindow> PushMenu( const TSharedRef<SWindow>& InParentWindow, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately = true, const bool bShouldAutoSize = true, const FVector2D& WindowSize = FVector2D::ZeroVector, const FVector2D& SummonLocationSize = FVector2D::ZeroVector );

	/**
	 * Dismisses the menu stack up to a certian level (by default removes all levels in the stack)
	 * Dismisses in reverse order so to keep just the first level you would pass in 1
	 *
	 * @param FirstStackIndexToRemove	The first level of the stack to remove
	 */
	void Dismiss( int32 FirstStackIndexToRemove = 0 );
	
	/**
	 * Removes a window from the stack and all of its children.  Assumes the window is cleaned up elsewhere
	 *
	 * @param WindowToRemove	The window to remove (will remove all children of this window as well)	
	 */
	void RemoveWindow( TSharedRef<SWindow> WindowToRemove );

	/**
	 * Notifies the stack that a new window was activated.  The menu stack will be dismissed if the activated window is not in the menu stack
	 *
	 * @param ActivatedWindow	The window that was just activated
	 */
	void OnWindowActivated( TSharedRef<SWindow> ActivatedWindow );

	/**
	 * Finds a window in the stack
	 *
	 * @param WindowToFind	The window to look fort
	 * @param The level of the stack where the window is located or INDEX_NONE if the window could not be found
	 */
	int32 FindLocationInStack( TSharedPtr<SWindow> WindowToFind ) const;

	/**
	 * @return	Returns the size of the menu stack
	 */
	int32 GetNumStackLevels( ) const;

	/** @return Returns whether the window has child menus. */
	bool HasOpenSubMenus( const TSharedRef<SWindow>& Window ) const;

	/**
	 * Returns all of the windows at the specified stack level index
	 *
	 * @param	StackLevelIndex	Index into the menu stack
	 *
	 * @return	List of windows at this stack level
	 */
	FMenuWindowList& GetWindowsAtStackLevel( const int32 StackLevelIndex );

private:

	/** The stack of windows, each level in the stack has an arbitrary number of siblings.  All siblings must belong to the same parent in the previous level */
	TArray<FMenuWindowList> Stack;
	
	/** Handle to a throttle request made to ensure the menu is responsive in low FPS situations */
	FThrottleRequest ThrottleHandle;
};
