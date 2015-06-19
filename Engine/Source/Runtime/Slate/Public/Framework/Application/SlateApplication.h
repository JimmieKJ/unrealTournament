// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericApplicationMessageHandler.h"
#include "MenuStack.h"
#include "SlateDelegates.h"
#include "SlateApplicationBase.h"


class SToolTip;
class SViewport;
class SWindow;
enum class EHittestDirection;
class FHittestGrid;
struct FPopupTransitionEffect;
class FMenuStack;
class IWidgetReflector;
class IInputProcessor;


/** A Delegate for querying whether source code access is possible */
DECLARE_DELEGATE_RetVal(bool, FQueryAccessSourceCode);

/** Delegates for when modal windows open or close */
DECLARE_DELEGATE(FModalWindowStackStarted)
DECLARE_DELEGATE(FModalWindowStackEnded)

extern SLATE_API const FName NAME_UnrealOS;


/**
 * Design constraints for Slate applications
 */
namespace SlateApplicationDefs
{
	/** How many users can we support at once? */
	static const int32 MaxUsers = 8;
}


/** Allow widgets to find out when someone clicked outside them. Currently needed by MenuAnchros. */
class SLATE_API FPopupSupport
{
	public:

	/**
	 * Given a WidgetPath that was clicked, send notifications to any subscribers that were not under the mouse.
	 * i.e. Send the "Someone clicked outside me" notifications.
	 */
	void SendNotifications( const FWidgetPath& WidgetsUnderCursor );

	/**
	* Register for a notification when the user clicks outside a specific widget.
	*
	* @param NotifyWhenClickedOutsideMe    When the user clicks outside this widget, fire a notification.
	* @param InNotification                The notification to invoke.
	*/
	FDelegateHandle RegisterClickNotification(const TSharedRef<SWidget>& NotifyWhenClickedOutsideMe, const FOnClickedOutside& InNotification);

	/**
	* NOTE: Only necessary if notification no longer desired.
	*       Stale notifications are cleaned up automatically.
	*
	* Unregister the notification because it is no longer desired.
	*/
	DELEGATE_DEPRECATED("This overload of UnregisterClickNotification is deprecated, instead pass the result of RegisterClickNotification.")
	void UnregisterClickNotification(const FOnClickedOutside& InNotification);

	/**
	* NOTE: Only necessary if notification no longer desired.
	*       Stale notifications are cleaned up automatically.
	*
	* Unregister the notification because it is no longer desired.
	*/
	void UnregisterClickNotification(FDelegateHandle InHandle);

	private:

	/** A single subscription about clicks happening outside the widget. */
	struct FClickSubscriber
	{
		FClickSubscriber( const TSharedRef<SWidget>& DetectClicksOutsideThisWidget, const FOnClickedOutside& InNotification )
		: DetectClicksOutsideMe( DetectClicksOutsideThisWidget )
		, Notification( InNotification )
		{}

		bool ShouldKeep() const
		{
			return DetectClicksOutsideMe.IsValid() && Notification.IsBound();
		}

		/** If a click occurs outside this widget, we'll send the notification */
		TWeakPtr<SWidget> DetectClicksOutsideMe;
		/** Notification to send */
		FOnClickedOutside Notification;
	};

	/** List of subscriptions that want to be notified when the user clicks outside a certain widget. */
	TArray<FClickSubscriber> ClickZoneNotifications;

};


class SLATE_API FSlateApplication
	: public FSlateApplicationBase
	, public FGenericApplicationMessageHandler
{
public:

	/** Virtual destructor. */
	virtual ~FSlateApplication();

public:

	/**
	 * Returns the running average delta time (smoothed over several frames)
	 *
	 * @return  The average time delta
	 */
	const float GetAverageDeltaTime() const
	{
		return AverageDeltaTime;
	}

	/**
	 * Returns the real time delta since Slate last ticked widgets
	 *
	 * @return  The time delta since last tick
	 */
	const float GetDeltaTime() const
	{
		return (float)( CurrentTime - LastTickTime );
	}

	/**
	 * Returns the running average delta time (smoothed over several frames)
	 * Unlike GetAverageDeltaTime() it excludes exceptional
	 * situations, such as when throttling mode is active.
	 *
	 * @return  The average time delta
	 */
	float GetAverageDeltaTimeForResponsiveness() const
	{
		return AverageDeltaTimeForResponsiveness;
	}

public:

	static void Create();
	static TSharedRef<FSlateApplication> Create(const TSharedRef<class GenericApplication>& InPlatformApplication);

	static TSharedRef<FSlateApplication> InitializeAsStandaloneApplication(const TSharedRef< class FSlateRenderer >& PlatformRenderer);
	
	static TSharedRef<FSlateApplication> InitializeAsStandaloneApplication(const TSharedRef< class FSlateRenderer >& PlatformRenderer, const TSharedRef<class GenericApplication>& PlatformApplication);

	/**
	 * Returns true if a Slate application instance is currently initialized and ready
	 *
	 * @return  True if Slate application is initialized
	 */
	static bool IsInitialized()
	{
		return CurrentApplication.IsValid();
	}

	/**
	 * Returns the current instance of the application. The application should have been initialized before
	 * this method is called
	 *
	 * @return  Reference to the application
	 */
	static FSlateApplication& Get()
	{
		check( IsInGameThread() );
		return *CurrentApplication;
	}

	static void Shutdown();

	/** @return the global tab manager */
	static TSharedRef<class FGlobalTabmanager> GetGlobalTabManager();

	/** @return the root style node, which is the entry point to the style graph representing all the current style rules. */
	const class FStyleNode* GetRootStyle() const;

	/**
	 * Initializes the renderer responsible for drawing all elements in this application
	 *
	 * @param InRenderer The renderer to use.
	 */
	virtual void InitializeRenderer( TSharedRef<FSlateRenderer> InRenderer );

	/** Set the slate sound provider that the slate app should use. */
	virtual void InitializeSound( const TSharedRef<ISlateSoundDevice>& InSlateSoundDevice );

	void DestroyRenderer();

	/** Play SoundToPlay. Interrupt previous sound if one is playing. */
	void PlaySound( const FSlateSound& SoundToPlay, int32 UserIndex = 0 ) const;

	/** @return The duration of the given sound resource */
	float GetSoundDuration(const FSlateSound& Sound) const;

	/** @return The force feedback interface for this application */
	DEPRECATED(4.7, "Please use GetInputInterface().")
	IForceFeedbackSystem* GetForceFeedbackSystem() const { return PlatformApplication->DEPRECATED_GetForceFeedbackSystem(); }

	IInputInterface* GetInputInterface() const { return PlatformApplication->GetInputInterface(); }

	/** @return Whether or not the current platform supports system help */
	bool SupportsSystemHelp() const { return PlatformApplication->SupportsSystemHelp(); }

	void ShowSystemHelp() { PlatformApplication->ShowSystemHelp(); }

	/** @return The text input method interface for this application */
	ITextInputMethodSystem *GetTextInputMethodSystem() const { return PlatformApplication->GetTextInputMethodSystem(); }

	/** 
	 * Sets the position of the cursor.
	 *
	 * @param MouseCoordinate		The new position.
	 */
	void SetCursorPos( const FVector2D& MouseCoordinate );

	/** Polls game devices for input */
	void PollGameDeviceState();

	/** Occurs before Tick(), after all pointer and keyboard input has been processed. */
	void FinishedInputThisFrame();

	/** Ticks this application */
	void Tick();

	/** Pumps OS messages when a modal window or intra-frame debugging session exists */
	void PumpMessages();

	/** Returns true if this slate application is ready to open modal windows */
	bool CanAddModalWindow() const;

	/** Returns true if this slate application is ready to display windows. */
	bool CanDisplayWindows() const;
	
	/**
	 * Adds a modal window to the application.  
	 * In most cases, this function does not return until the modal window is closed (the only exception is a modal window for slow tasks)  
	 *
	 * @param InSlateWindow		A SlateWindow to which to add a native window.
	 * @param InParentWindow	The parent of the modal window.  All modal windows must have a parent.
	 * @param bSlowTaskWindow	true if the window is for a slow task and this function should return before the window is closed
	 */
	void AddModalWindow( TSharedRef<SWindow> InSlateWindow, const TSharedPtr<const SWidget> InParentWidget, bool bSlowTaskWindow = false );

	/** Sets the delegate for when a modal window stack begins */
	void SetModalWindowStackStartedDelegate(FModalWindowStackStarted StackStartedDelegate);

	/** Sets the delegate for when a modal window stack ends */
	void SetModalWindowStackEndedDelegate(FModalWindowStackEnded StackEndedDelegate);

	/**
	 * Associates a top level Slate Window with a native window, and "natively" parents that window to the specified Slate window.
	 * Although the window still a top level window in Slate, it will be considered a child window to the operating system.
	 *
	 * @param	InSlateWindow		A Slate window to which to add a native window.
	 * @param	InParentWindow		Slate window that the window being added should be a native child of
	 * @param	bShowImmediately	True to show the window.  Pass false if you're going to call ShowWindow() yourself later.
	 *
	 * @return a reference to the SWindow that was just added.
	 */
	TSharedRef<SWindow> AddWindowAsNativeChild( TSharedRef<SWindow> InSlateWindow, TSharedRef<SWindow> InParentWindow, const bool bShowImmediately = true );

	/**
	 * Creates a new Menu window and add it to the menu stack.
	 *
	 * @param InWindow				The parent of the menu.  If there is already an open menu this parent must exist in the menu stack or the menu stack is dismissed and a new one started
	 * @param Content				The content to be placed inside the new window
	 * @param SummonLocation		The location where this menu should be summoned
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bFocusImmediately		Should the popup steal focus when shown?
	 * @param bShouldAutoSize		True if the newCalculatePopupWindowPosition window should automatically size itself to its content
	 * @param WindowSize			When bShouldAutoSize=false, this must be set to the size of the window to be created
	 * @param SummonLocationSize	An optional rect which describes an area in which the menu may not appear
	 */
	TSharedRef<SWindow> PushMenu( const TSharedRef<SWidget>& InParentContent, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately = true, const bool bShouldAutoSize = true, const FVector2D& WindowSize = FVector2D::ZeroVector, const FVector2D& SummonLocationSize = FVector2D::ZeroVector );

	/** @return Returns whether the window has child menus. */
	bool HasOpenSubMenus( TSharedRef<SWindow> Window ) const;

	/** @return	Returns true if there are any pop-up menus summoned */
	bool AnyMenusVisible() const;

	/** Dismisses any open menus */
	void DismissAllMenus();

	/**
	 * Dismisses a menu window and all its children
	 *
	 * @param MenuWindowToDismiss	The window to dismiss, any children, grandchildren etc will also be dismissed
	 */
	void DismissMenu( TSharedRef<SWindow> MenuWindowToDismiss );

	/**
	 * Finds the window in the menu stack
	 *
	 * @param WindowToFind	The window to look for
	 * @return The level in the stack  that the window is in or INDEX_NONE if it is not found
	 */
	int32 GetLocationInMenuStack( TSharedRef<SWindow> WindowToFind ) const;

	/**
	 * HACK: Don't use this unless shutting down a game viewport
	 * Game viewport windows need to be destroyed instantly or else the viewport could tick and access deleted data
	 *
	 * @param WindowToDestroy		Window for destruction
	 */
	void DestroyWindowImmediately( TSharedRef<SWindow> WindowToDestroy );

	/**
	 * Disable Slate components when an external, non-slate, modal window is brought up.  In the case of multiple
	 * external modal windows, we will only increment our tracking counter.
	 */
	void ExternalModalStart();

	/**
	 * Re-enable disabled Slate components when a non-slate modal window is dismissed.  Slate components
	 * will only be re-enabled when all tracked external modal windows have been dismissed.
	 */
	void ExternalModalStop();

	/** 
	 * Removes references to FViewportRHI's.  
	 * This has to be done explicitly instead of using the FRenderResource mechanism because FViewportRHI's are managed by the game thread.
	 * This is needed before destroying the RHI device. 
	 */
	void InvalidateAllViewports();

	/**
	 * Registers a game viewport with the Slate application so that specific messages can be routed directly to a viewport
	 * 
	 * @param InViewport	The viewport to register.  Note there is currently only one registered viewport
	 */
	void RegisterGameViewport( TSharedRef<SViewport> InViewport );

	/**
	 * Returns the game viewport registered with the slate application
	 *
	 * @return registered game viewport
	 */
	TSharedPtr<SViewport> GetGameViewport() const;

	/** 
	 * Unregisters the current game viewport from Slate.  This method sends a final deactivation message to the viewport
	 * to allow it to do a final cleanup before being closed.
	 */
	void UnregisterGameViewport();

	/**
	 * Sets specified user focus to the SWidget representing the currently active game viewport
	 */
	void SetUserFocusToGameViewport(uint32 UserIndex, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	DEPRECATED(4.6, "FSlateApplication::SetFocusToGameViewport() is deprecated, use FSlateApplication::SetUserFocusToGameViewport() instead.")
	void SetFocusToGameViewport();

	/**
	 * Sets all users focus to the SWidget representing the currently active game viewport
	 */
	void SetAllUserFocusToGameViewport(EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	DEPRECATED(4.6, "FSlateApplication::SetJoystickCaptorToGameViewport() is deprecated, use FSlateApplication::SetAllUserFocusToGameViewport() instead.")
	void SetJoystickCaptorToGameViewport();

	/**
	 * Sets specified user focus to the SWidget passed in.
	 *
	 * @param UserIndex Index of the user to change focus for
	 * @param WidgetToFocus the widget to set focus to
	 * @param ReasonFocusIsChanging the contextual reason for the focus change
	 */
	void SetUserFocus(uint32 UserIndex, const TSharedPtr<SWidget>& WidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/** @return a pointer to the Widget that currently has the users focus; Empty pointer when the user has no focus. */
	TSharedPtr< SWidget > GetUserFocusedWidget(uint32 UserIndex) const;

	DEPRECATED(4.6, "FSlateApplication::GetJoystickCaptor() is deprecated, use FSlateApplication::GetUserFocusedWidget() instead.")
	TSharedPtr< SWidget > GetJoystickCaptor(uint32 UserIndex) const;

	/** Releases the users focus from whatever it currently is on. */
	void ClearUserFocus(uint32 UserIndex, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	DEPRECATED(4.6, "FSlateApplication::ReleaseJoystickCapture() is deprecated, use FSlateApplication::ClearUserFocus() instead.")
	void ReleaseJoystickCapture(uint32 UserIndex);

	/**
	 * Sets the Keyboard focus to the specified SWidget
	 */
	void SetKeyboardFocus(const TSharedPtr<SWidget>& OptionalWidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/**
	 * Clears keyboard focus, if any widget is currently focused
	 *
	 * @param ReasonFocusIsChanging The reason that keyboard focus is changing
	 */
	void ClearKeyboardFocus(const EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/**
	 * Returns the current modifier keys state
	 *
	 * @return  State of modifier keys
	 */
	FModifierKeysState GetModifierKeys() const;

	/**
	 *	Restores all input settings to their original values
	 * 
	 *  Clears all user focus
	 *  Shows the mouse, clears any locks and captures, turns off high precision input
	 */
	void ResetToDefaultInputSettings();
	
	/**
	 *	Restores all pointer input settings to their original values
	 * 
	 *  Shows the mouse, clears any locks and captures, turns off high precision input
	 */
	void ResetToDefaultPointerInputSettings();

	/**
	 * Mouse capture
	 */

	/** returning platform-specific value designating window that captures mouse, or nullptr if mouse isn't captured */
	virtual void* GetMouseCaptureWindow( void ) const;


	/** Releases the mouse capture from whatever it currently is on. */
	void ReleaseMouseCapture();

	/** @return The active modal window or nullptr if there is no modal window. */
	TSharedPtr<SWindow> GetActiveModalWindow() const;

	/**
	 * Assign a delegate to be called when this application is requesting an exit (e.g. when the last window is closed).
	 *
	 * @param OnExitRequestHandler  Function to execute when the application wants to quit
	 */
	void SetExitRequestedHandler( const FSimpleDelegate& OnExitRequestedHandler );
		
	/**
	 * @todo slate: Remove this method or make it private.
	 * Searches for the specified widget and generates a full path to it.  Note that this is
	 * a relatively slow operation!  It can fail, in which case OutWidgetPath.IsValid() will be false.
	 * 
	 * @param  InWidget       Widget to generate a path to
	 * @param  OutWidgetPath  The generated widget path
	 * @param  VisibilityFilter	Widgets must have this type of visibility to be included the path
	 */
	bool GeneratePathToWidgetUnchecked( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible ) const;
	
	/**
	 * @todo slate: Remove this method or make it private.
	 * Searches for the specified widget and generates a full path to it.  Note that this is
	 * a relatively slow operation!  Asserts if the widget isn't found.
	 * 
	 * @param  InWidget       Widget to generate a path to
	 * @param  OutWidgetPath  The generated widget path
	 * @param  VisibilityFilter	Widgets must have this type of visibility to be included the path
	 */
	void GeneratePathToWidgetChecked( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible ) const;
	
	/**
	 * Finds the window that the provided widget resides in
	 * 
	 * @param InWidget		The widget to find the window for
	 * @return The window where the widget resides, or null if the widget wasn't found.  Remember, a widget might not be found simply because its parent decided not to report the widget in ArrangeChildren.
	 */
	TSharedPtr<SWindow> FindWidgetWindow( TSharedRef< const SWidget > InWidget ) const;

	/**
	 * Finds the window that the provided widget resides in
	 * 
	 * @param InWidget		The widget to find the window for
	 * @param OutWidgetPath Full widget path generated 
	 * @return The window where the widget resides, or null if the widget wasn't found.  Remember, a widget might not be found simply because its parent decided not to report the widget in ArrangeChildren.
	 */
	TSharedPtr<SWindow> FindWidgetWindow( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath) const;

	/**
	 * @return True if the application is currently routing high precision mouse movement events (OS specific)
	 * If this value is true, the mouse is captured and hidden by the widget that originally made the request.
	 */
	bool IsUsingHighPrecisionMouseMovment() const { return PlatformApplication.IsValid() ? PlatformApplication->IsUsingHighPrecisionMouseMode() : false; }
	
	/**
	 * @return True if the last mouse event was from a trackpad.
	 */
	bool IsUsingTrackpad() const { return PlatformApplication.IsValid() ? PlatformApplication->IsUsingTrackpad() : false; }

	/**
	 * @return True if there is a mouse device attached
	 */
	bool IsMouseAttached() const { return PlatformApplication.IsValid() ? PlatformApplication->IsMouseAttached() : false; }

	/**
	 * Sets the widget reflector.
	 *
	 * @param WidgetReflector The widget reflector to set.
	 */
	void SetWidgetReflector( const TSharedRef<IWidgetReflector>& WidgetReflector );

	/** @param AccessDelegate The delegate to pass along to the widget reflector */
	void SetWidgetReflectorSourceAccessDelegate(FAccessSourceCode AccessDelegate)
	{
		SourceCodeAccessDelegate = AccessDelegate;
	}

	/** @param QueryAccessDelegate The delegate to pass along to the widget reflector */
	void SetWidgetReflectorQuerySourceAccessDelegate(FQueryAccessSourceCode QueryAccessDelegate)
	{
		QuerySourceCodeAccessDelegate = QueryAccessDelegate;
	}

	/** @param AccessDelegate The delegate to pass along to the widget reflector */
	void SetWidgetReflectorAssetAccessDelegate(FAccessAsset AccessDelegate)
	{
		AssetAccessDelegate = AccessDelegate;
	}

	/** @param Scale  Sets the ratio SlateUnit / ScreenPixel */
	void SetApplicationScale( float InScale ){ Scale = InScale; }

	virtual void GetInitialDisplayMetrics( FDisplayMetrics& OutDisplayMetrics ) const { PlatformApplication->GetInitialDisplayMetrics( OutDisplayMetrics ); }

	/** Are we drag-dropping right now? */
	bool IsDragDropping() const;

	/** Get the current drag-dropping content */
	TSharedPtr<class FDragDropOperation> GetDragDroppingContent() const;

	DEPRECATED(4.6, "Use CancelDragDrop")
	void EndDragDrop()
	{
		CancelDragDrop();
	}

	/** Cancels any in flight drag and drops */
	void CancelDragDrop();

	/**
	 * Returns the attribute that can be used by widgets to check if the application is in normal execution mode
	 * Don't hold a reference to this anywhere that can exist when this application closes
	 */
	const TAttribute<bool>& GetNormalExecutionAttribute() const { return NormalExecutionGetter; }

	/**
	 * @return true if not in debugging mode
	 */
	bool IsNormalExecution() const { return !GIntraFrameDebuggingGameThread; }

	/**
	 * @return true if in debugging mode
	 */
	bool InKismetDebuggingMode() const { return GIntraFrameDebuggingGameThread; }

	/**
	 * Enters debugging mode which is a special state that causes the
	 * Slate application to tick in place which in the middle of a stack frame
	 */
	void EnterDebuggingMode();

	/**
	 * Leaves debugging mode
	 *
	 * @param bLeavingDebugForSingleStep	Whether or not we are leaving debug mode due to single stepping
	 */
	void LeaveDebuggingMode( bool bLeavingDebugForSingleStep = false );
	
	/**
	 * Calculates the popup window position from the passed in window position and size. 
	 * Adjusts position for popup windows which are outside of the work area of the monitor where they reside
	 *
	 * @param InAnchor	The current(suggseted) window position and size of an area which may not be covered by the popup.
	 * @param InSize		The size of the window
	 * @param Orientation	The direction of the popup.
	 *						If vertical it will attempt to open below the anchor but will open above if there is no room.
	 *						If horizontal it will attempt to open below the anchor but will open above if there is no room.
	 * @return The adjusted position
	 */
	virtual FVector2D CalculatePopupWindowPosition( const FSlateRect& InAnchor, const FVector2D& InSize, const EOrientation Orientation = Orient_Vertical ) const;


	/**
	 * Is the window in the app's destroy queue? If so it will be destroyed next tick.
	 *
	 * @param Window		The window to find in the destroy list
	 * @return				true if Window is in the destroy queue
	 */
	bool IsWindowInDestroyQueue(TSharedRef<SWindow> Window) const;

	/** @return	Returns true if the application's average frame rate is at least as high as our target frame rate that satisfies our requirement for a smooth and responsive UI experience */
	bool IsRunningAtTargetFrameRate() const;

	/** @return Returns true if transition effects for new menus and windows should be played */
	bool AreMenuAnimationsEnabled() const;

	/** 
	 * Sets whether transition effects for new menus and windows should be played.  This can be called at any time.
	 *
	 * @param	bEnableAnimations	True if animations should be used, otherwise false.
	 */
	void EnableMenuAnimations( const bool bEnableAnimations );

	/** Set the global application icon */
	void SetAppIcon(const FSlateBrush* const InAppIcon);

	/** Sets the display state of external UI such as Steam. */
	void ExternalUIChange(bool bIsOpening)
	{
		bIsExternalUIOpened = bIsOpening;
	}

	/**
	 * Shows or hides an onscreen keyboard
	 *
	 * @param bShow	true to show the keyboard, false to hide it
	 * @param TextEntryWidget The widget that will receive the input from the virtual keyboard
	 */
	void ShowVirtualKeyboard( bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget = nullptr );

	/** Get the work area that has the largest intersection with the specified rectangle */
	FSlateRect GetWorkArea( const FSlateRect& InRect ) const;

	/**
	 * Shows or hides an onscreen keyboard
	 *
	 * @param bShow	true to show the keyboard, false to hide it
	 */
	virtual void NativeApp_ShowKeyboard( bool bShow, FString InitialString = "", int32 SelectionStart = -1, int32 SelectionEnd = -1 )
	{
		// empty default functionality
	}

	/** @return true if the current platform allows source code access */
	bool SupportsSourceAccess() const;

	/** Opens the current platform's code editing IDE (if necessary) and focuses the specified line in the specified file. Will only work if SupportsSourceAccess() returns true */
	void GotoLineInSource(const FString& FileName, int32 LineNumber) const;

	/** @return Service that supports popups; helps with auto-hiding of popups */
	FPopupSupport& GetPopupSupport() { return PopupSupport; }

	/**
	 * Forces the window to redraw immediately.
	 */
	void ForceRedrawWindow( TSharedRef<SWindow>& InWindowToDraw );

	/**
	 * Takes a screenshot of the widget writing the results into the color buffer provided.  Note that the format is BGRA.
	 * the size of the resulting image is also output.
	 * 
	 * @return true if taking the screenshot was successful.
	 */
	bool TakeScreenshot(TSharedRef<SWidget>& Widget, TArray<FColor>&OutColorData, FIntVector& OutSize);

	/**
	 * Takes a screenshot of the widget writing the results into the color buffer provided, this version allows you to provide 
	 * an inner area to screenshot.  Note that the format is BGRA.  The size of the resulting image is also output.
	 *
	 * @return true if taking the screenshot was successful.
	 */
	bool TakeScreenshot(TSharedRef<SWidget>& Widget, const FIntRect& InnerWidgetArea, TArray<FColor>& OutColorData, FIntVector& OutSize);

protected:

	friend class FAnalogCursor;
	friend class FEventRouter;

	virtual bool HasMouseCapture(const TSharedPtr<const SWidget> Widget) const override;
	virtual TOptional<EFocusCause> HasUserFocus(const TSharedPtr<const SWidget> Widget, int32 UserIndex) const override;
	virtual TOptional<EFocusCause> HasAnyUserFocus(const TSharedPtr<const SWidget> Widget) const override;
	virtual bool ShowUserFocus(const TSharedPtr<const SWidget> Widget) const override;

	/** 
	 * Ticks a slate window and all of its children
	 *
	 * @param WindowToTick	The window to tick
	 */
	void TickWindowAndChildren( TSharedRef<SWindow> WindowToTick );

	/** Draws Slate windows. Should only be called by the application's main loop or renderer. */
	void DrawWindows();

	/**
	 * Draws slate windows, optionally only drawing the passed in window
	 */
	void PrivateDrawWindows( TSharedPtr<SWindow> DrawOnlyThisWindow = nullptr );

	/**
	 * Pre-pass step before drawing windows to compute geometry size and reshape autosized windows
	 */
	void DrawPrepass( TSharedPtr<SWindow> DrawOnlyThisWindow );

	/**
	 * Draws a window and its children
	 */
	void DrawWindowAndChildren( const TSharedRef<SWindow>& WindowToDraw, struct FDrawWindowArgs& DrawWindowArgs );

	/**
	 * Gets all visible child windows of a window.
	 */
	void GetAllVisibleChildWindows(TArray< TSharedRef<SWindow> >& OutWindows, TSharedRef<SWindow> CurrentWindow);


	/** Engages or disengages application throttling based on user behavior */
	void ThrottleApplicationBasedOnMouseMovement();

	virtual FWidgetPath LocateWidgetInWindow(FVector2D ScreenspaceMouseCoordinate, const TSharedRef<SWindow>& Window, bool bIgnoreEnabledStatus) const override;

public:

	/**
	 * Called by the native application in response to a mouse move. Routs the event to Slate Widgets.
	 *
	 * @param  InMouseEvent  Mouse event
	 * @param  bIsSynthetic  True when the even is synthesized by slate.
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseMoveEvent( FPointerEvent& MouseEvent, bool bIsSynthetic = false );

	/**
	 * Called by the native application in response to a mouse button press. Routs the event to Slate Widgets.
	 *
	 * @param  PlatformWindow  The platform window the event originated from, used to set focus at the platform level. 
	 *                         If Invalid the Mouse event will work but there will be no effect on the platform.
	 * @param  InMouseEvent    Mouse event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseButtonDownEvent(const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& InMouseEvent);

	/**
	 * Called by the native application in response to a mouse button release. Routs the event to Slate Widgets.
	 *
	 * @param  InMouseEvent  Mouse event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseButtonUpEvent( FPointerEvent& MouseEvent );

	/**
	 * Called by the native application in response to a mouse release. Routs the event to Slate Widgets.
	 *
	 * @param  InMouseEvent  Mouse event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseButtonDoubleClickEvent( const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& InMouseEvent );
	
	/**
	 * Called by the native application in response to a mouse wheel spin or a touch gesture. Routs the event to Slate Widgets.
	 *
	 * @param  InWheelEvent    Mouse wheel event details
	 * @param  InGestureEvent  Optional gesture event details
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseWheelOrGestureEvent( FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent );

	/**
	 * Called when a character is entered
	 *
	 * @param  InCharacterEvent  Character event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessKeyCharEvent( FCharacterEvent& InCharacterEvent );

	/**
	 * Called when a key is pressed
	 *
	 * @param  InKeyEvent  Keyb event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessKeyDownEvent( FKeyEvent& InKeyEvent );

	/**
	 * Called when a key is released
	 *
	 * @param  InKeyEvent  Key event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessKeyUpEvent( FKeyEvent& InKeyEvent );
	
	/**
	 * Called when a analog input values change
	 *
	 * @param  InAnalogInputEvent Analog input event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessAnalogInputEvent(FAnalogInputEvent& InAnalogInputEvent);

	/**
	 * Called when a drag from an external (non-slate) source enters a window
	 *
	 * @param WindowEntered  The window that was entered by the drag and drop
	 * @param DragDropEvent  Describes the mouse state (position, pressed buttons, etc) and associated payload
	 * @return true if the drag enter was handled and can be processed by some widget in this window; false otherwise
	 */
	bool ProcessDragEnterEvent( TSharedRef<SWindow> WindowEntered, FDragDropEvent& DragDropEvent );

	/**
	 * Called when a touchpad touch is started (finger down) when polling game device state
	 * 
	 * @param ControllerEvent	The touch event generated
	 */
	void ProcessTouchStartedEvent( const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& InTouchEvent );

	/**
	 * Called when a touchpad touch is moved  (finger moved) when polling game device state
	 * 
	 * @param ControllerEvent	The touch event generated
	 */
	void ProcessTouchMovedEvent( FPointerEvent& InTouchEvent );

	/**
	 * Called when a touchpad touch is ended (finger lifted) when polling game device state
	 * 
	 * @param ControllerEvent	The touch event generated
	 */
	void ProcessTouchEndedEvent( FPointerEvent& InTouchEvent );

	/**
	 * Called when motion is detected (controller or device) when polling game device state
	 * 
	 * @param MotionEvent		The motion event generated
	 */
	void ProcessMotionDetectedEvent( FMotionEvent& InMotionEvent );

	/**
	 * Called by the native application in response to an activation or deactivation. 
	 *
	 * @param ActivateEvent Information about the window activation/deactivation
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessWindowActivatedEvent( const FWindowActivateEvent& ActivateEvent );
	
	/**
	 * Called when the application is activated (i.e. one of its windows becomes active) or deactivated.
	 *
	 * @param InAppActivated Whether the application was activated.
	 */
	void ProcessApplicationActivationEvent( bool InAppActivated );

public:

	/** Called when the slate application is being shut down. */
	void OnShutdown();

	/** Closes all active windows immediately */
	void CloseAllWindowsImmediately();

	/**
	 * Destroy the native and slate windows in the array provided.
	 *
	 * @param WindowsToDestroy   Destroy these windows
	 */
	void DestroyWindowsImmediately();

	/**
	 * Apply any requests from the Reply to the application. E.g. Capture mouse
	 *
	 * @param CurrentEventPath   The WidgetPath along which the reply-generated event was routed
	 * @param TheReply           The reply generated by an event that was being processed.
	 * @param WidgetsUnderMouse  Optional widgets currently under the mouse; initiating drag and drop needs access to widgets under the mouse.
	 * @param InMouseEvent       Optional mouse event that caused this action.
	 * @param UserIndex			 User index that generated the event we are replying to (defaults to 0, at least for now)
	 */
	void ProcessReply(const FWidgetPath& CurrentEventPath, const FReply TheReply, const FWidgetPath* WidgetsUnderMouse, const FPointerEvent* InMouseEvent, uint32 UserIndex = 0);
	
	/** Bubble a request for which cursor to display for widgets under the mouse or the widget that captured the mouse. */
	void QueryCursor();

	/**
	 * Apply any requests from the CursorReply
	 *
	 * @param CursorReply        The reply generated by an event that was being processed.
	 */
	void ProcessCursorReply(const FCursorReply& CursorReply);
	
	/**
	 * Spawns a tool tip window.  If an existing tool tip window is open, it will be dismissed first.
	 *
	 * @param	InToolTip           Widget to display.
	 * @param	InSpawnLocation     Screen space location to show the tool tip (window top left)
	 */
	void SpawnToolTip( const TSharedRef<IToolTip>& InToolTip, const FVector2D& InSpawnLocation );

	/** Closes the open tool-tip, if a tool-tip is open */
	void CloseToolTip();

	/**
	 * Updates tool tip
	 */
	void UpdateToolTip( bool AllowSpawningOfNewToolTips );

	/** @return an array of top-level windows that can be interacted with. e.g. when a modal window is up, only return the modal window */
	TArray< TSharedRef<SWindow> > GetInteractiveTopLevelWindows();

	/** Gets all visible slate windows ordered from back to front based on child hierarchies */
	void GetAllVisibleWindowsOrdered(TArray< TSharedRef<SWindow> >& OutWindows);
	
	/** Tell the slate application to log a string to it's ui log */
	void OnLogSlateEvent(enum EEventLog::Type Event, const FString& AdditionalContent = FString());

	/** Tell the slate application to log a FText to it's ui log */
	void OnLogSlateEvent(enum EEventLog::Type Event, const FText& AdditionalContent );

	/** Sets the slate event logger */
	void SetSlateUILogger(TSharedPtr<class IEventLogger> InEventLogger = TSharedPtr<class IEventLogger>());

	/** @return true if mouse events are being turned into touch events, and touch UI should be forced on */
	bool IsFakingTouchEvents() const;

	/** Sets whether the application is treating mouse events as imitating touch events.  Optional CursorLocation can be supplied to override the platform's belief of where the cursor is */
	void SetGameIsFakingTouchEvents(const bool bIsFaking, FVector2D* CursorLocation = nullptr);

	/** Sets the handler for otherwise unhandled key down events. This is used by the editor to provide a global action list, if the key was not consumed by any widget. */
	void SetUnhandledKeyDownEventHandler( const FOnKeyEvent& NewHandler );

	/** @return the last time a user interacted with a keyboard, mouse, touch device, or controller */
	double GetLastUserInteractionTime() const { return LastUserInteractionTime; }

	/** @return the deadzone size for dragging in screen pixels (aka virtual desktop pixels) */
	float GetDragTriggerDistance() const;

	/** Set the size of the deadzone for dragging in screen pixels */
	void SetDragTriggerDistance( float ScreenPixels );
	
	/** Set the analog cursor to be enabled or disabled. */
	void SetInputPreProcessor(bool bEnable, TSharedPtr<class IInputProcessor> NewInputProcessor = nullptr);

	/** Sets the hit detection radius of the cursor */
	void SetCursorRadius(float NewRadius);

	/** Getter for the cursor radius */
	float GetCursorRadius() const;

public:

	// Begin FSlateApplicationBase interface

	virtual TSharedRef<SWindow> AddWindow( TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately = true ) override;

	virtual void ArrangeWindowToFrontVirtual( TArray<TSharedRef<SWindow>>& Windows, const TSharedRef<SWindow>& WindowToBringToFront ) override
	{
		FSlateWindowHelper::ArrangeWindowToFront(Windows, WindowToBringToFront);
	}

	virtual bool FindPathToWidget( TSharedRef<const SWidget> InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible ) override
	{
		return FSlateWindowHelper::FindPathToWidget(GetInteractiveTopLevelWindows(), InWidget, OutWidgetPath, VisibilityFilter);
	}

	virtual const double GetCurrentTime() const override
	{
		return CurrentTime;
	}

	virtual TSharedPtr<SWindow> GetActiveTopLevelWindow() const override;
	virtual const FSlateBrush* GetAppIcon() const override;

	virtual float GetApplicationScale() const override
	{
		return Scale;
	}

	virtual FVector2D GetCursorPos() const override;
	virtual FVector2D GetLastCursorPos() const override;
	virtual FVector2D GetCursorSize() const override;	

	virtual bool GetSoftwareCursorAvailable() const override
	{
		return bSoftwareCursorAvailable;
	}

	virtual EVisibility GetSoftwareCursorVis() const override;	
	virtual TSharedPtr<SWidget> GetKeyboardFocusedWidget() const override;

	virtual EWindowTransparency GetWindowTransparencySupport() const override
	{
		return PlatformApplication->GetWindowTransparencySupport();
	}

protected:

	virtual TSharedPtr< SWidget > GetMouseCaptorImpl() const override;

public:

	// FSlateApplicationBase interface

	virtual bool HasAnyMouseCaptor() const override;
	virtual FSlateRect GetPreferredWorkArea() const override;
	virtual bool HasFocusedDescendants( const TSharedRef<const SWidget>& Widget ) const override;
	virtual bool IsExternalUIOpened() override;
	virtual FWidgetPath LocateWindowUnderMouse( FVector2D ScreenspaceMouseCoordinate, const TArray<TSharedRef<SWindow>>& Windows, bool bIgnoreEnabledStatus = false ) override;
	virtual bool IsWindowHousingInteractiveTooltip(const TSharedRef<const SWindow>& WindowToTest) const override;
	virtual TSharedRef<SWidget> MakeImage( const TAttribute<const FSlateBrush*>& Image, const TAttribute<FSlateColor>& Color, const TAttribute<EVisibility>& Visibility ) const override;
	virtual TSharedRef<SWidget> MakeWindowTitleBar( const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& CenterContent, EHorizontalAlignment CenterContentAlignment, TSharedPtr<IWindowTitleBar>& OutTitleBar ) const override;
	DEPRECATED(4.8, "Passing text to Slate as FString is deprecated, please use FText instead (likely via a LOCTEXT).")
	virtual TSharedRef<IToolTip> MakeToolTip( const TAttribute<FString>& ToolTipString ) override;
	virtual TSharedRef<IToolTip> MakeToolTip( const TAttribute<FText>& ToolTipText ) override;
	virtual TSharedRef<IToolTip> MakeToolTip( const FText& ToolTipText ) override;
	virtual void RequestDestroyWindow( TSharedRef<SWindow> WindowToDestroy ) override;
	virtual bool SetKeyboardFocus( const FWidgetPath& InFocusPath, const EFocusCause InCause ) override;
	virtual bool SetUserFocus(const uint32 InUserIndex, const FWidgetPath& InFocusPath, const EFocusCause InCause) override;

public:

	// FGenericApplicationMessageHandler interface

	virtual bool ShouldProcessUserInputMessages( const TSharedPtr< FGenericWindow >& PlatformWindow ) const override;
	virtual bool OnKeyChar( const TCHAR Character, const bool IsRepeat ) override;
	virtual bool OnKeyDown( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat ) override;
	virtual bool OnKeyUp( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat ) override;
	virtual bool OnMouseDown( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button ) override;
	virtual bool OnMouseUp( const EMouseButtons::Type Button ) override;
	virtual bool OnMouseDoubleClick( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button ) override;
	virtual bool OnMouseWheel( const float Delta ) override;
	virtual bool OnMouseMove() override;
	virtual bool OnRawMouseMove( const int32 X, const int32 Y ) override;
	virtual bool OnCursorSet() override;
	virtual bool OnControllerAnalog( EControllerButtons::Type Button, int32 ControllerId, float AnalogValue ) override;
	virtual bool OnControllerButtonPressed( EControllerButtons::Type Button, int32 ControllerId, bool IsRepeat ) override;
	virtual bool OnControllerButtonReleased( EControllerButtons::Type Button, int32 ControllerId, bool IsRepeat ) override;
	virtual bool OnTouchGesture( EGestureEvent::Type GestureType, const FVector2D& Delta, float WheelDelta ) override;
	virtual bool OnTouchStarted( const TSharedPtr< FGenericWindow >& PlatformWindow, const FVector2D& Location, int32 TouchIndex, int32 ControllerId ) override;
	virtual bool OnTouchMoved( const FVector2D& Location, int32 TouchIndex, int32 ControllerId ) override;
	virtual bool OnTouchEnded( const FVector2D& Location, int32 TouchIndex, int32 ControllerId ) override;
	virtual bool OnMotionDetected(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration, int32 ControllerId) override;
	virtual bool OnSizeChanged( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 Width, const int32 Height, bool bWasMinimized = false ) override;
	virtual void OnOSPaint( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual FWindowSizeLimits GetSizeLimitsForWindow(const TSharedRef<FGenericWindow>& Window) const override;
	virtual void OnResizingWindow( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual bool BeginReshapingWindow( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual void FinishedReshapingWindow( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual void OnMovedWindow( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 X, const int32 Y ) override;
	virtual bool OnWindowActivationChanged( const TSharedRef< FGenericWindow >& PlatformWindow, const EWindowActivation::Type ActivationType ) override;
	virtual bool OnApplicationActivationChanged( const bool IsActive ) override;
	virtual bool OnConvertibleLaptopModeChanged() override;
	virtual EWindowZone::Type GetWindowZoneForPoint( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 X, const int32 Y ) override;
	virtual void OnWindowClose( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual EDropEffect::Type OnDragEnterText( const TSharedRef< FGenericWindow >& Window, const FString& Text ) override;
	virtual EDropEffect::Type OnDragEnterFiles( const TSharedRef< FGenericWindow >& Window, const TArray< FString >& Files ) override;

	EDropEffect::Type OnDragEnter( const TSharedRef< SWindow >& Window, const TSharedRef<FExternalDragOperation>& DragDropOperation );

	virtual EDropEffect::Type OnDragOver( const TSharedPtr< FGenericWindow >& Window ) override;
	virtual void OnDragLeave( const TSharedPtr< FGenericWindow >& Window ) override;
	virtual EDropEffect::Type OnDragDrop( const TSharedPtr< FGenericWindow >& Window ) override;
	virtual bool OnWindowAction( const TSharedRef< FGenericWindow >& PlatformWindow, const EWindowAction::Type InActionType ) override;

private:

	TSharedRef< FGenericWindow > MakeWindow( TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately );

	/**
	 * Destroys an SWindow, removing it and all its children from the Slate window list.  Notifies the native window to destroy itself and releases rendering resources
	 *
	 * @param DestroyedWindow The window to destroy
	 */
	void PrivateDestroyWindow( const TSharedRef<SWindow>& DestroyedWindow );

	/**
	 * Attempts to navigate to the next widget in the direction specified
	 *
	 * @return if a new widget was navigated too
	 */
	bool AttemptNavigation(const FNavigationEvent& NavigationEvent, const FNavigationReply& NavigationReply, const FArrangedWidget& BoundaryWidget);

	/**
	 * @return int user index that the keyboard is mapped to. -1 if the keyboard isn't mapped
	 */
	int32 GetUserIndexForKeyboard() const;

	/** 
	 * @return int user index that this controller is mapped to. -1 if the controller isn't mapped
	 */
	int32 GetUserIndexForController(int32 ControllerId) const;

private:

	/** Lock the cursor such that it cannot leave the bounds of the specified widget. Null widget implies no cursor lock. */
	void LockCursor(const TSharedPtr<SWidget>& Widget);

	/**
	 * Lock the cursor such that it cannot leave the bounds of the leaf-most widget of the specified path.
	 * Assumes a valid path.
	 */
	void LockCursorToPath(const FWidgetPath& WidgetPath);

	/** Clear any cursor locks */
	void UnlockCursor();

	/** Make sure that the cursor lock region matcher the locking widget's geometry */
	void UpdateCursorLockRegion();

	/** State related to cursor locking. */
	struct
	{
		/** Path to widget that currently holds the cursor lock; invalid path if no cursor lock. */
		FWeakWidgetPath PathToLockingWidget;

		/** Desktop Space Rect that bounds the cursor. */
		FSlateRect LastComputedBounds;
	} CursorLock;

private:

	// Hidden default constructor.
	FSlateApplication();

private:

	/**
	 * Creates a mouse move event for the last known cursor position.  This should be called every tick to make
	 * sure that widgets that appear (or vanish from) underneath the cursor have hover state set appropriately.
	 */
	void SynthesizeMouseMove();

	/** Signal that a synthesized mouse move will be required after this operation. */
	void QueueSynthesizedMouseMove();

	/**
	 * Will be invoked when the size of the geometry of the virtual
	 * desktop changes (e.g. resolution change or monitors re-arranged)
	 */
	void OnVirtualDesktopSizeChanged(const FDisplayMetrics& NewDisplayMetric);

	/** Application singleton */
	static TSharedPtr< FSlateApplication > CurrentApplication;

	TSet<FKey> PressedMouseButtons;

	/** After processing an event or performing an active timer, we need to synthesize a mouse move. @see SynthesizeMouseMove */
	int32 SynthesizeMouseMovePending;

	/** true when the slate app is active; i.e. the current foreground window is from our Slate app*/
	bool bAppIsActive;

	/** true if any slate window is currently active (not just top level windows) */
	bool bSlateWindowActive;

	/** Application-wide scale for supporting monitors of varying pixel density */
	float Scale;

	/** The dead zone distance in virtual desktop pixels (a.k.a screen pixels) that the user has to move their finder before it is considered a drag.*/
	float DragTriggerDistance;

	/** All the top-level windows owned by this application; they are tracked here in a platform-agnostic way. */
	TArray< TSharedRef<SWindow> > SlateWindows;

	/** The currently active slate window that is a top-level window (full fledged window; not a menu or tooltip)*/
	TWeakPtr<SWindow> ActiveTopLevelWindow;
	
	/** List of active modal windows.  The last item in the list is the top-most modal window */
	TArray< TSharedPtr<SWindow> > ActiveModalWindows;

	/** These windows will be destroyed next tick. */
	TArray< TSharedRef<SWindow> > WindowDestroyQueue;
	
	/** The stack of menus that are open */
	FMenuStack MenuStack;

	/** A vertical slice through the tree of widgets on screen; it represents widgets that were under the cursor last time an event was processed */
	FWeakWidgetPath WidgetsUnderCursorLastEvent;

	/**
	 * A helper class to wrap the weak path functionality. The advantage of using this
	 * class is that the path can be validated and the current mouse captor (if any) can
	 * be informed they have lost mouse capture in the case of a number of events, such as
	 * application losing focus, or the widget being hidden.
	 */
	class MouseCaptorHelper
	{
	public:
		/**
		 * Returns whether or not there are any active pointer captures.
		 */
		bool HasCapture() const;

		/**
		 * Returns whether or not the particular PointerIndex has capture.
		 */
		bool HasCaptureForPointerIndex(uint32 PointerIndex) const;

		/**
		 * Sets a new mouse captor widget for a specific pointer index, invalidating the previous one if any and calling
		 * its OnMouseCaptureLost() handler.
		 *
		 * @param PointerIndex	The index of the pointer which initiated the capture.
		 * @param EventPath		The path to the event.
		 * @param Widget		The widget that wants to capture the mouse.
		 */
		void SetMouseCaptor(uint32 PointerIndex, const FWidgetPath& EventPath, TSharedPtr< SWidget > Widget );

		/** Invalidates all current mouse captors. Calls OnMouseCaptureLost() on the current mouse captor if one exists */
		void InvalidateCaptureForAllPointers();

		/** Invalidates a specific mouse captor. Calls OnMouseCaptureLost() on the specific mouse captor if one exists */
		void InvalidateCaptureForPointer(uint32 PointIndex);

		/**
		 * Retrieves a resolved FWidgetPath for a specific pointer index, if possible.
		 * If the path is invalid or truncated (i.e. the widget is no longer relevant) then the current mouse captor's
		 * OnMouseCaptureLost() handler is called and the path is invalidated.
		 *
		 * @param PointerIndex				The index of the pointer which has capture.
		 * @param InterruptedPathHandling	How to handled incomplete paths. "Truncate" will return a partial path, "ReturnInvalid" will return an empty path.
		 */
		FWidgetPath ToWidgetPath(uint32 PointerIndex, FWeakWidgetPath::EInterruptedPathHandling::Type InterruptedPathHandling = FWeakWidgetPath::EInterruptedPathHandling::Truncate );

		FWidgetPath ToWidgetPath( FWeakWidgetPath::EInterruptedPathHandling::Type InterruptedPathHandling = FWeakWidgetPath::EInterruptedPathHandling::Truncate, const FPointerEvent* PointerEvent = nullptr );

		/**
		 * Retrieves an array of the resolved widget paths for any active captures.
		 */
		TArray<FWidgetPath> ToWidgetPaths();

		/** Retrieves the weak path for a current mouse captor with a specific pointer index */
		FWeakWidgetPath ToWeakPath(uint32 PointerIndex) const;
		

		/* Walks the weak path and retrieves the widget that is set as the current mouse captor with a specific pointer index */
		TSharedPtr< SWidget > ToSharedWidget(uint32 PointerIndex) const;

		/*
		 * Retrieves an array of shared widget pointers for the active mouse captures.
		 */
		TArray<TSharedRef<SWidget>> ToSharedWidgets() const;

		/* Walks the weak path and retrieves the window for the widget belonging to the mouse captor with the specified pointer index */
		TSharedPtr< SWidget > ToSharedWindow(uint32 PointerIndex);

	protected:
		/** Call the OnMouseCaptureLost() handler for the widget captured by the specific pointer index */
		void InformCurrentCaptorOfCaptureLoss(uint32 PointerIndex) const;

		/** A map of pointer indices to weak widget paths for the active mouse captures */
		TMap<uint32, FWeakWidgetPath> PointerIndexToMouseCaptorWeakPathMap;
	};
	/** The current mouse captor for the application, if any. */
	MouseCaptorHelper MouseCaptor;

	/** An input preprocessor, gets an opportunity to parse input before anything else. */
	TSharedPtr<IInputProcessor> InputPreProcessor;
	
	/** The cursor widget and window to render that cursor for the current software cursor.*/
	TWeakPtr<SWindow> CursorWindowPtr;
	TWeakPtr<SWidget> CursorWidgetPtr;

	/** The hit-test radius of the cursor. Default value is 0. */
	float CursorRadius;


	struct FUserFocusEntry
	{
		/** A weak path to the widget currently focused by a user, if any. */
		FWeakWidgetPath WidgetPath;
		/** Reason a widget was focused by a user, if any. */
		EFocusCause FocusCause;
		/** If we should show this focus */
		bool ShowFocus;
	};

	/** State of focus for all users */
	FUserFocusEntry UserFocusEntries[SlateApplicationDefs::MaxUsers];

	/**
	 * Application throttling
	 */

	/** Holds a current request to ensure Slate is responsive in low FPS situations, based in mouse button pressed state */
	FThrottleRequest MouseButtonDownResponsivnessThrottle;

	/** Separate throttle handle that engages automatically based on mouse movement and other user behavior */
	FThrottleRequest UserInteractionResponsivnessThrottle;

	/** The last real time that the user pressed a key or mouse button */
	double LastUserInteractionTime;

	/** Subset of LastUserInteractionTime that is used only when considering when to throttle */
	double LastUserInteractionTimeForThrottling;

	/** Used when considering whether to put Slate to sleep */
	double LastMouseMoveTime;

	/** Helper for detecting when a drag should begin */
	struct FDragDetector
	{
		FDragDetector()
		: DetectDragStartLocation( FVector2D::ZeroVector )
		, DetectDragButton( EKeys::Invalid )
		{
		}
		/** If not null, a widget has request that we detect a drag being triggered in this widget and send an OnDragDetected() event*/
		FWeakWidgetPath DetectDragForWidget;
		/** Location from which be begin detecting the drag */
		FVector2D DetectDragStartLocation;
		/** Button that must be pressed to trigger the drag */
		FKey DetectDragButton;
	} DragDetector;

	/** Support for auto-dismissing pop-ups */
	FPopupSupport PopupSupport;
	
	/** Pointer to the currently registered game viewport widget if any */
	TWeakPtr<SViewport> GameViewportWidget;

	TSharedPtr<ISlateSoundDevice> SlateSoundDevice;

	/** The current cached absolute real time, right before we tick widgets */
	double CurrentTime;

	/** Last absolute real time that we ticked */
	double LastTickTime;

	/** Running average time in seconds between calls to Tick (used for monitoring responsiveness) */
	float AverageDeltaTime;

	/** Average delta time for application responsiveness tracking.  This is like AverageDeltaTime, but it excludes frame
	    deltas spent while the the application is in a throttled state */
	float AverageDeltaTimeForResponsiveness;

	
	/**
	 * Provides a platform-agnostic method for requesting that the application exit.
	 * Implementations should assign a handler that terminates the process when this delegate is invoked.
	 */
	FSimpleDelegate OnExitRequested;
	
	/** A Widget that introspects the current UI hierarchy */
	TWeakPtr<IWidgetReflector> WidgetReflectorPtr;

	/** Delegate for accessing source code, to pass to any widget inspectors. */
	FAccessSourceCode SourceCodeAccessDelegate;

	/** Delegate for querying if source code access is available */
	FQueryAccessSourceCode QuerySourceCodeAccessDelegate;

	/** Delegate for accessing assets, to pass to any widget inspectors. */
	FAccessAsset AssetAccessDelegate;

	/** System for logging all relevant slate events to a log file */
	TSharedPtr<class IEventLogger> EventLogger;

	/** Allows us to track the number of non-slate modal windows active. */
	int32 NumExternalModalWindowsActive;


	/**
	 * Tool-tips
	 */

	/** Window that we'll re-use for spawned tool tips */
	TWeakPtr< SWindow > ToolTipWindow;

	/** Widget that last visualized the tooltip;  */
	TWeakPtr< SWidget > TooltipVisualizerPtr;

	/** The tool tip that is currently being displayed; can be invalid (i.e. not showing tooltip) */
	TWeakPtr<IToolTip> ActiveToolTip;

	/** The widget that sourced the currently active tooltip widget */
	TWeakPtr<SWidget> ActiveToolTipWidgetSource;

	/** Whether tool-tips are allowed to spawn at all.  Used only for debugging purposes. */
	int32 bAllowToolTips;

	/** How long before the tool tip should start fading in? */
	float ToolTipDelay;

	/** Tool-tip fade-in duration */
	float ToolTipFadeInDuration;

	/** Absolute real time that the current tool-tip window was summoned */
	double ToolTipSummonTime;

	/** Desired tool tip position in screen space, updated whenever the mouse moves */
	FVector2D DesiredToolTipLocation;

	/** Direction that tool-tip is being repelled from a force field in.  We cache this to avoid tool-tips
	    teleporting between different offset directions as the user moves the mouse cursor around. */
	struct EToolTipOffsetDirection
	{
		enum Type
		{
			Undetermined, Down, Right
		};
	};
	EToolTipOffsetDirection::Type ToolTipOffsetDirection;

	/** The top of the Style tree. */
	const class FStyleNode* RootStyleNode;

	/**
	 * Drag and Drop (DragDrop)
	 */
	
	/** When not null, the content of the current drag drop operation. */
	TSharedPtr< FDragDropOperation > DragDropContent;

	/** The window the drag drop content is over. */
	TWeakPtr< SWindow > DragDropWindowPtr;

	/** Whether or not we are requesting that we leave debugging mode after the tick is complete */
	bool bRequestLeaveDebugMode;
	/** Whether or not we need to leave debug mode for single stepping */
	bool bLeaveDebugForSingleStep;
	TAttribute<bool> NormalExecutionGetter;

	/**
	 * Console objects
	 */

	/** AllowToolTips console variable **/
	FAutoConsoleVariableRef CVarAllowToolTips;
	/** ToolTipDelay console variable **/
	FAutoConsoleVariableRef CVarToolTipDelay;
	/** ToolTipFadeInDuration console variable **/
	FAutoConsoleVariableRef CVarToolTipFadeInDuration;

	/**
	 * Modal Windows
	 */

	/** Delegates for when modal windows open or close */
	FModalWindowStackStarted ModalWindowStackStartedDelegate;
	FModalWindowStackEnded ModalWindowStackEndedDelegate;

	/** Keeps track of whether or not the UI for services such as Steam is open. */
	bool bIsExternalUIOpened;

	/** Handle to a throttle request made to ensure the window is responsive in low FPS situations */
	FThrottleRequest ThrottleHandle;

	/** When an drag and drop is happening, we keep track of whether slate knew what to do with the payload on last mouse move */
	bool DragIsHandled;

	TMap<uint32, FVector2D> PointerIndexLastPositionMap;

	/**
	 * Virtual keyboard text field
	 */
	IPlatformTextField* SlateTextField;

	/** For desktop platforms that want to test touch style input, pass -faketouches or -simmobile on the commandline to set this */
	bool bIsFakingTouch;

	/** For games that want to allow mouse to imitate touch */
	bool bIsGameFakingTouch;

	/**For desktop platforms that the touch move event be called when this variable is true */
	bool bIsFakingTouched;

	/** Delegate for when a key down event occurred but was not handled in any other way by ProcessKeyDownMessage */
	FOnKeyEvent UnhandledKeyDownEventHandler;

	/** controls whether unhandled touch events fall back to sending mouse events */
	bool bTouchFallbackToMouse;

	/** .ini controlled option to allow or disallow software cursor rendering */
	bool bSoftwareCursorAvailable;	

	/**
	 * Slate look and feel
	 */

	/** Globally enables or disables transition effects for pop-up menus (menu stacks) */
	bool bMenuAnimationsEnabled;

	/** The icon to use on application windows */
	const FSlateBrush *AppIcon;

	//
	// Hittest 2.0
	//

	// The rectangle that bounds all the physical monitors given their arrangement.
	// Info comes from the native platform.
	// e.g. On windows the origin (coordinates X=0, Y=0) is the upper left of the primary monitor,
	// but there could be another monitor on any of the sides.
	FSlateRect VirtualDesktopRect;
};
