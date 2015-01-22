// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// @todo Slate: this is highly sketchy, as FDragDropOperation is declared in Slate,
// but cannot currently be brought into SlateCore due to dependencies to SWindow.
class FDragDropOperation;
class SWidget;

/**
 * A Reply is something that a Slate event returns to the system to notify it about certain aspect of how an event was handled.
 * For example, a widget may handle an OnMouseDown event by asking the system to give mouse capture to a specific Widget.
 * To do this, return FReply::CaptureMouse( NewMouseCapture ).
 */
class FReply : public TReplyBase<FReply>
{
public:
		
	/** An event should return a FReply::Handled().CaptureMouse( SomeWidget ) as a means of asking the system to forward all mouse events to SomeWidget */
	FReply& CaptureMouse( TSharedRef<SWidget> InMouseCaptor )
	{
		this->MouseCaptor = InMouseCaptor;
		return Me();
	}

	/**
	 * Enables the use of high precision (raw input) mouse movement, for more accurate mouse movement without mouse ballistics
	 * NOTE: This implies mouse capture and hidden mouse movement.  Releasing mouse capture deactivates this mode.
	 */
	FReply& UseHighPrecisionMouseMovement( TSharedRef<SWidget> InMouseCaptor )
	{
		this->MouseCaptor = InMouseCaptor;
		this->bUseHighPrecisionMouse = true;
		return Me();
	}

	/**
	 * An even should return FReply::Handled().SetMousePos to ask Slate to move the mouse cursor to a different location
	 */
	FReply& SetMousePos( const FIntPoint& NewMousePos )
	{
		this->RequestedMousePos = NewMousePos;
		return Me();
	}

	/** An event should return FReply::Handled().SetUserFocus( SomeWidget ) as a means of asking the system to set users focus to the provided widget*/
	FReply& SetUserFocus(TSharedRef<SWidget> GiveMeFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly, bool bInAllUsers = false)
	{
		this->bSetUserFocus = true;
		this->FocusRecipient = GiveMeFocus;
		this->FocusChangeReason = ReasonFocusIsChanging;
		this->bReleaseUserFocus = false;
		this->bAllUsers = bInAllUsers;
		return Me();
	}
	
	DEPRECATED(4.6, "FReply::CaptureJoystick() is deprecated, use FReply::SetUserFocus() instead.")
	FReply& CaptureJoystick(TSharedRef<SWidget> InJoystickCaptor, bool bInAllJoysticks = false)
	{
		return SetUserFocus(InJoystickCaptor, EFocusCause::SetDirectly, bInAllJoysticks);
	}

	DEPRECATED(4.6, "FReply::SetKeyboardFocus() is deprecated, use FReply::SetUserFocus() instead.")
	FReply& SetKeyboardFocus(TSharedRef<SWidget> GiveMeFocus, EFocusCause ReasonFocusIsChanging)
	{
		return SetUserFocus(GiveMeFocus, ReasonFocusIsChanging, false);
	}

	/** An event should return a FReply::Handled().ClearUserFocus() to ask the system to clear user focus*/
	FReply& ClearUserFocus(bool bInAllUsers = false)
	{
		return ClearUserFocus(EFocusCause::SetDirectly, bInAllUsers);
	}

	/** An event should return a FReply::Handled().ClearUserFocus() to ask the system to clear user focus*/
	FReply& ClearUserFocus(EFocusCause ReasonFocusIsChanging, bool bInAllUsers = false)
	{
		this->FocusRecipient = nullptr;
		this->FocusChangeReason = ReasonFocusIsChanging;
		this->bReleaseUserFocus = true;
		this->bSetUserFocus = false;
		this->bAllUsers = bInAllUsers;
		return Me();
	}

	DEPRECATED(4.6, "FReply::ReleaseJoystickCapture() is deprecated, use FReply::ClearUserFocus() instead.")
	FReply& ReleaseJoystickCapture(bool bInAllJoysticks = false)
	{
		return ClearUserFocus(bInAllJoysticks);
	}

	/** An event should return FReply::Handled().SetNavigation( NavigationType ) as a means of asking the system to attempt a navigation*/
	FReply& SetNavigation(EUINavigation InNavigationType)
	{
		this->NavigationType = InNavigationType;
		return Me();
	}

	/**
	 * An event should return FReply::Handled().LockMouseToWidget( SomeWidget ) as a means of asking the system to 
	 * Lock the mouse so it cannot move out of the bounds of the widget.
	 */
	FReply& LockMouseToWidget( TSharedRef<SWidget> InWidget )
	{
		this->MouseLockWidget = InWidget;
		this->bShouldReleaseMouseLock = false;
		return Me();
	}

	/** 
	 * An event should return a FReply::Handled().ReleaseMouseLock() to ask the system to release mouse lock on a widget
	 */
	FReply& ReleaseMouseLock()
	{
		this->bShouldReleaseMouseLock = true;
		MouseLockWidget.Reset();
		return Me();
	}

	/** 
	 * An event should return a FReply::Handled().ReleaseMouse() to ask the system to release mouse capture
	 * NOTE: Deactivates high precision mouse movement if activated.
	 */
	FReply& ReleaseMouseCapture()
	{
		this->bReleaseMouseCapture = true;
		this->bUseHighPrecisionMouse = false;
		return Me();
	}

	/**
	 * Ask Slate to detect if a user started dragging in this widget.
	 * If a drag is detected, Slate will send an OnDragDetected event.
	 *
	 * @param DetectDragInMe  Detect dragging in this widget
	 * @param MouseButton     This button should be pressed to detect the drag
	 */
	FReply& DetectDrag( const TSharedRef<SWidget>& DetectDragInMe, FKey MouseButton )
	{
		this->DetectDragForWidget = DetectDragInMe;
		this->DetectDragForMouseButton = MouseButton;
		return Me();
	}

	/**
	 * An event should return FReply::Handled().BeginDragDrop( Content ) to initiate a drag and drop operation.
	 *
	 * @param InDragDropContent  The content that is being dragged. This could be a widget, or some arbitrary data
	 *
	 * @return reference back to the FReply so that this call can be chained.
	 */
	FReply& BeginDragDrop(TSharedRef<FDragDropOperation> InDragDropContent)
	{
		this->DragDropContent = InDragDropContent;
		return Me();
	}

	/** An event should return FReply::Handled().EndDragDrop() to request that the current drag/drop operation be terminated. */
	FReply& EndDragDrop()
	{
		this->bEndDragDrop = true;
		return Me();
	}

	/** Ensures throttling for Slate UI responsiveness is not done on mouse down */
	FReply& PreventThrottling()
	{
		this->bPreventThrottling = true;
		return Me();
	}

public:

	/** True if this reply indicated that we should release mouse capture as a result of the event being handled */
	bool ShouldReleaseMouse() const { return bReleaseMouseCapture; }

	/** true if this reply indicated that we should set focus as a result of the event being handled */
	bool ShouldSetUserFocus() const { return bSetUserFocus; }

	/** true if this reply indicated that we should release focus as a result of the event being handled */
	bool ShouldReleaseUserFocus() const { return bReleaseUserFocus; }

	DEPRECATED(4.6, "FReply::ShouldReleaseJoystick() is deprecated, use FReply::ShouldReleaseUserFocus() instead.")
	bool ShouldReleaseJoystick() const { return ShouldReleaseUserFocus(); }

	/** If the event replied with a request to change the user focus whether it should do it for all users or just the current UserIndex */
	bool AffectsAllUsers() const { return bAllUsers; }

	/** True if this reply indicated that we should use high precision mouse movement */
	bool ShouldUseHighPrecisionMouse() const { return bUseHighPrecisionMouse; }

	/** True if the reply indicated that we should release mouse lock */
	bool ShouldReleaseMouseLock() const { return bShouldReleaseMouseLock; }

	/** Whether or not we should throttle on mouse down */
	bool ShouldThrottle() const { return !bPreventThrottling; }

	/** Returns the widget that the mouse should be locked to (if any) */
	const TSharedPtr<SWidget>& GetMouseLockWidget() const { return MouseLockWidget; }

	/** When not nullptr, user focus has been requested to be set on the FocusRecipient. */
	const TSharedPtr<SWidget>& GetUserFocusRecepient() const { return FocusRecipient; }

	DEPRECATED(4.6, "FReply::GetFocusRecepient() is deprecated, use FReply::GetUserFocusRecepient() instead.")
	const TSharedPtr<SWidget>& GetFocusRecepient() const { return GetUserFocusRecepient(); }

	DEPRECATED(4.6, "FReply::GetJoystickCaptor() is deprecated, use FReply::GetUserFocusRecepient() instead.")
	const TSharedPtr<SWidget>& GetJoystickCaptor() const { return GetUserFocusRecepient(); }

	/** Get the reason that a focus change is being requested. */
	EFocusCause GetFocusCause() const { return FocusChangeReason; }

	/** If the event replied with a request to capture the mouse, this returns the desired mouse captor. Otherwise returns an invalid pointer. */
	const TSharedPtr<SWidget>& GetMouseCaptor() const { return MouseCaptor; }	

	/** Get the navigation type (Invalid if no navigation is requested). */
	EUINavigation GetNavigationType() const { return NavigationType; }

	/** @return the Content that we should use for the Drag and Drop operation; Invalid SharedPtr if a drag and drop operation is not requested*/
	const TSharedPtr<FDragDropOperation>& GetDragDropContent() const { return DragDropContent; }

	/** @return true if the user has asked us to terminate the ongoing drag/drop operation */
	bool ShouldEndDragDrop() const { return bEndDragDrop; }

	/** @return a widget for which to detect a drag; Invalid SharedPtr if no drag detection requested */
	const TSharedPtr<SWidget>& GetDetectDragRequest() const { return DetectDragForWidget; }  

	/** @return the mouse button for which we are detecting a drag */
	FKey GetDetectDragRequestButton() const { return DetectDragForMouseButton; }

	/** @return The coordinates of the requested mouse position */
	const TOptional<FIntPoint>& GetRequestedMousePos() const { return RequestedMousePos; }
		
public:

	/**
	 * An event should return a FReply::Handled() to let the system know that an event was handled.
	 */
	static FReply Handled( )
	{
		return FReply(true);
	}

	/**
	 * An event should return a FReply::Unhandled() to let the system know that an event was unhandled.
	 */
	static FReply Unhandled( )
	{
		return FReply(false);
	}

private:

	/**
	 * Hidden default constructor.
	 */
	FReply( bool bIsHandled )
		: TReplyBase<FReply>(bIsHandled)
		, RequestedMousePos()
		, EventHandler(nullptr)
		, MouseCaptor(nullptr)
		, FocusRecipient(nullptr)
		, MouseLockWidget(nullptr)
		, DragDropContent(nullptr)
		, FocusChangeReason(EFocusCause::SetDirectly)
		, NavigationType(EUINavigation::Invalid)
		, bReleaseMouseCapture(false)
		, bSetUserFocus(false)
		, bReleaseUserFocus(false)
		, bAllUsers(false)
		, bShouldReleaseMouseLock(false)
		, bUseHighPrecisionMouse(false)
		, bPreventThrottling(false)
		, bEndDragDrop(false)
	{ }
		

private:
	friend class FSlateApplication;

private:

	TOptional<FIntPoint> RequestedMousePos;
	TSharedPtr<SWidget> EventHandler;
	TSharedPtr<SWidget> MouseCaptor;
	TSharedPtr<SWidget> FocusRecipient;
	TSharedPtr<SWidget> MouseLockWidget;
	TSharedPtr<SWidget> DetectDragForWidget;
	FKey DetectDragForMouseButton;
	TSharedPtr<FDragDropOperation> DragDropContent;
	EFocusCause FocusChangeReason;
	EUINavigation NavigationType;
	uint32 bReleaseMouseCapture:1;
	uint32 bSetUserFocus:1;
	uint32 bReleaseUserFocus : 1;
	uint32 bAllUsers:1;
	uint32 bShouldReleaseMouseLock:1;
	uint32 bUseHighPrecisionMouse:1;
	uint32 bPreventThrottling:1;
	uint32 bEndDragDrop:1;
};
