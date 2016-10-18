// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SlateBasics.h"
#include "Slate/SlateTextures.h"
#include "Slate/SceneViewport.h"
#include "DebugCanvas.h"

#include "IHeadMountedDisplay.h"

extern EWindowMode::Type GetWindowModeType(EWindowMode::Type WindowMode);

FSceneViewport::FSceneViewport( FViewportClient* InViewportClient, TSharedPtr<SViewport> InViewportWidget )
	: FViewport( InViewportClient )
	, CurrentReplyState( FReply::Unhandled() )
	, CachedMousePos(-1, -1)
	, PreCaptureMousePos(-1, -1)
	, SoftwareCursorPosition( 0, 0 )
	, bIsSoftwareCursorVisible( false )	
	, DebugCanvasDrawer( new FDebugCanvasDrawer )
	, ViewportWidget( InViewportWidget )
	, NumMouseSamplesX( 0 )
	, NumMouseSamplesY( 0 )
	, MouseDelta( 0, 0 )
	, bIsCursorVisible( true )
	, bShouldCaptureMouseOnActivate( true )
	, bRequiresVsync( false )
	, bUseSeparateRenderTarget( InViewportWidget.IsValid() ? !InViewportWidget->ShouldRenderDirectly() : true )
	, bForceSeparateRenderTarget( false )
	, bIsResizing( false )
	, bPlayInEditorIsSimulate( false )
	, bCursorHiddenDueToCapture( false )
	, MousePosBeforeHiddenDueToCapture( -1, -1 )
	, RTTSize( 0, 0 )
	, NumBufferedFrames(1)
	, CurrentBufferedTargetIndex(0)
	, NextBufferedTargetIndex(0)
{
	bIsSlateViewport = true;
	RenderThreadSlateTexture = new FSlateRenderTargetRHI(nullptr, 0, 0);

	if (InViewportClient)
	{
		bShouldCaptureMouseOnActivate = InViewportClient->CaptureMouseOnLaunch();
	}
}

FSceneViewport::~FSceneViewport()
{
	Destroy();
	// Wait for resources to be deleted
	FlushRenderingCommands();
}

bool FSceneViewport::HasMouseCapture() const
{
	return ViewportWidget.IsValid() && ViewportWidget.Pin()->HasMouseCapture();
}

bool FSceneViewport::HasFocus() const
{
	return FSlateApplication::Get().GetUserFocusedWidget(0) == ViewportWidget.Pin();
}

void FSceneViewport::CaptureMouse( bool bCapture )
{
	if( bCapture )
	{
		CurrentReplyState.UseHighPrecisionMouseMovement( ViewportWidget.Pin().ToSharedRef() );
	}
	else
	{
		CurrentReplyState.ReleaseMouseCapture();
	}
}

void FSceneViewport::LockMouseToViewport( bool bLock )
{
	if( bLock )
	{
		CurrentReplyState.LockMouseToWidget( ViewportWidget.Pin().ToSharedRef() );
	}
	else
	{
		CurrentReplyState.ReleaseMouseLock();
	}
}

void FSceneViewport::ShowCursor( bool bVisible )
{
	if ( bVisible && !bIsCursorVisible )
	{
		if( bIsSoftwareCursorVisible )
		{
			const int32 ClampedMouseX = FMath::Clamp<int32>(SoftwareCursorPosition.X, 0, SizeX);
			const int32 ClampedMouseY = FMath::Clamp<int32>(SoftwareCursorPosition.Y, 0, SizeY);

			CurrentReplyState.SetMousePos( CachedGeometry.LocalToAbsolute( FVector2D(ClampedMouseX, ClampedMouseY) ).IntPoint() );
		}
		else
		{
			// Restore the old mouse position when we show the cursor.
			CurrentReplyState.SetMousePos( PreCaptureMousePos );
		}

		SetPreCaptureMousePosFromSlateCursor();
		bIsCursorVisible = true;
	}
	else if ( !bVisible && bIsCursorVisible )
	{
		// Remember the current mouse position when we hide the cursor.
		SetPreCaptureMousePosFromSlateCursor();
		bIsCursorVisible = false;
	}
}

bool FSceneViewport::SetUserFocus(bool bFocus)
{
	if (bFocus)
	{
		CurrentReplyState.SetUserFocus(ViewportWidget.Pin().ToSharedRef(), EFocusCause::SetDirectly, true);
	}
	else
	{
		CurrentReplyState.ClearUserFocus(true);
	}

	return bFocus;
}

bool FSceneViewport::KeyState( FKey Key ) const
{
	return KeyStateMap.FindRef( Key );
}

void FSceneViewport::Destroy()
{
	ViewportClient = NULL;
	
	UpdateViewportRHI( true, 0, 0, EWindowMode::Windowed );	
}

int32 FSceneViewport::GetMouseX() const
{
	return CachedMousePos.X;
}

int32 FSceneViewport::GetMouseY() const
{
	return CachedMousePos.Y;
}

void FSceneViewport::GetMousePos( FIntPoint& MousePosition, const bool bLocalPosition )
{
	if (bLocalPosition)
	{
		MousePosition = CachedMousePos;
	}
	else
	{
		const FVector2D AbsoluteMousePos = CachedGeometry.LocalToAbsolute(FVector2D(CachedMousePos.X / CachedGeometry.Scale, CachedMousePos.Y / CachedGeometry.Scale));
		MousePosition.X = AbsoluteMousePos.X;
		MousePosition.Y = AbsoluteMousePos.Y;
	}
}

void FSceneViewport::SetMouse( int32 X, int32 Y )
{
	FVector2D AbsolutePos = CachedGeometry.LocalToAbsolute(FVector2D(X, Y));
	FSlateApplication::Get().SetCursorPos( AbsolutePos );
	CachedMousePos = FIntPoint(X, Y);
}

void FSceneViewport::ProcessInput( float DeltaTime )
{
	// Required 
}

void FSceneViewport::UpdateCachedMousePos( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	FVector2D LocalPixelMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	LocalPixelMousePos.X *= CachedGeometry.Scale;
	LocalPixelMousePos.Y *= CachedGeometry.Scale;

	CachedMousePos = LocalPixelMousePos.IntPoint();
}

void FSceneViewport::UpdateCachedGeometry( const FGeometry& InGeometry )
{
	CachedGeometry = InGeometry;
}

void FSceneViewport::UpdateModifierKeys( const FPointerEvent& InMouseEvent )
{
	KeyStateMap.Add(EKeys::LeftAlt, InMouseEvent.IsLeftAltDown());
	KeyStateMap.Add(EKeys::RightAlt, InMouseEvent.IsRightAltDown());
	KeyStateMap.Add(EKeys::LeftControl, InMouseEvent.IsLeftControlDown());
	KeyStateMap.Add(EKeys::RightControl, InMouseEvent.IsRightControlDown());
	KeyStateMap.Add(EKeys::LeftShift, InMouseEvent.IsLeftShiftDown());
	KeyStateMap.Add(EKeys::RightShift, InMouseEvent.IsRightShiftDown());
	KeyStateMap.Add(EKeys::LeftCommand, InMouseEvent.IsLeftCommandDown());
	KeyStateMap.Add(EKeys::RightCommand, InMouseEvent.IsRightCommandDown());
}

void FSceneViewport::ApplyModifierKeys( const FModifierKeysState& InKeysState )
{
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if ( InKeysState.IsLeftAltDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::LeftAlt, IE_Pressed );
		}
		if ( InKeysState.IsRightAltDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::RightAlt, IE_Pressed );
		}
		if ( InKeysState.IsLeftControlDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::LeftControl, IE_Pressed );
		}
		if ( InKeysState.IsRightControlDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::RightControl, IE_Pressed );
		}
		if ( InKeysState.IsLeftShiftDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::LeftShift, IE_Pressed );
		}
		if ( InKeysState.IsRightShiftDown() )
		{
			ViewportClient->InputKey( this, 0, EKeys::RightShift, IE_Pressed );
		}
	}
}

void FSceneViewport::ProcessAccumulatedPointerInput()
{
	if( !ViewportClient )
	{
		return;
	}

	// Switch to the viewport clients world before processing input
	FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

	const bool bViewportHasCapture = ViewportWidget.IsValid() && ViewportWidget.Pin()->HasMouseCapture();

	if (NumMouseSamplesX > 0 || NumMouseSamplesY > 0)
	{
		const float DeltaTime = FApp::GetDeltaTime();
		ViewportClient->InputAxis( this, 0, EKeys::MouseX, MouseDelta.X, DeltaTime, NumMouseSamplesX );
		ViewportClient->InputAxis( this, 0, EKeys::MouseY, MouseDelta.Y, DeltaTime, NumMouseSamplesY );
	}

	MouseDelta = FIntPoint::ZeroValue;
	NumMouseSamplesX = 0;
	NumMouseSamplesY = 0;
}

FVector2D FSceneViewport::VirtualDesktopPixelToViewport(FIntPoint VirtualDesktopPointPx) const
{
	// Virtual Desktop Pixel to local slate unit
	const FVector2D TransformedPoint = CachedGeometry.AbsoluteToLocal(FVector2D(VirtualDesktopPointPx.X, VirtualDesktopPointPx.Y));

	// Pixels to normalized coordinates
	return FVector2D( TransformedPoint.X / SizeX, TransformedPoint.Y / SizeY );
}

FIntPoint FSceneViewport::ViewportToVirtualDesktopPixel(FVector2D ViewportCoordinate) const
{
	// Normalized to pixels transform
	const FVector2D LocalCoordinateInSu = FVector2D( ViewportCoordinate.X * SizeX, ViewportCoordinate.Y * SizeY );
	// Local slate unit to virtual desktop pixel.
	const FVector2D TransformedPoint = FVector2D( CachedGeometry.LocalToAbsolute( LocalCoordinateInSu ) );

	return FIntPoint( FMath::TruncToInt(TransformedPoint.X), FMath::TruncToInt(TransformedPoint.Y) );
}

void FSceneViewport::OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled )
{
	// Switch to the viewport clients world before resizing
	FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

	/** Check to see if the viewport should be resized */
	FIntPoint DrawSize = FIntPoint( FMath::RoundToInt( AllottedGeometry.GetDrawSize().X ), FMath::RoundToInt( AllottedGeometry.GetDrawSize().Y ) );
	if( GetSizeXY() != DrawSize )
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef() );
		if ( Window.IsValid() )
		{
			//@HACK VREDITOR
			//check(Window.IsValid());
			if ( Window->IsViewportSizeDrivenByWindow() )
			{
				ResizeViewport(FMath::Max(0, DrawSize.X), FMath::Max(0, DrawSize.Y), Window->GetWindowMode());
			}
		}
	}
	
	// Cannot pass negative canvas positions
	float CanvasMinX = FMath::Max(0.0f, AllottedGeometry.AbsolutePosition.X);
	float CanvasMinY = FMath::Max(0.0f, AllottedGeometry.AbsolutePosition.Y);
	FIntRect CanvasRect(
		FMath::RoundToInt( CanvasMinX ),
		FMath::RoundToInt( CanvasMinY ),
		FMath::RoundToInt( CanvasMinX + AllottedGeometry.Size.X * AllottedGeometry.Scale ), 
		FMath::RoundToInt( CanvasMinY + AllottedGeometry.Size.Y * AllottedGeometry.Scale ) );


	DebugCanvasDrawer->BeginRenderingCanvas( CanvasRect );

	// Draw above everything else
	uint32 MaxLayer = MAX_uint32;
	FSlateDrawElement::MakeCustom( OutDrawElements, MAX_uint32, DebugCanvasDrawer );

}

bool FSceneViewport::IsForegroundWindow() const
{
	bool bIsForeground = false;
	if( ViewportWidget.IsValid() )
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef() );
		if( Window.IsValid() )
		{
			bIsForeground = Window->GetNativeWindow()->IsForegroundWindow();
		}
	}

	return bIsForeground;
}

FCursorReply FSceneViewport::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent )
{
	if (bCursorHiddenDueToCapture)
	{
		return FCursorReply::Cursor(EMouseCursor::None);
	}

	// In game mode we may be using a borderless window, which needs OnCursorQuery call to handle window resize cursors
	if (IsRunningGame() && GEngine && GEngine->GameViewport)
	{
		TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if (Window.IsValid())
		{
			FCursorReply Reply = Window->OnCursorQuery(MyGeometry, CursorEvent);
			if (Reply.IsEventHandled())
			{
				return Reply;
			}
		}
	}

	EMouseCursor::Type MouseCursorToUse = EMouseCursor::Default;

	// If the cursor should be hidden, use EMouseCursor::None,
	// only when in the foreground, or we'll hide the mouse in the window/program above us.
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		MouseCursorToUse = ViewportClient->GetCursor( this, GetMouseX(), GetMouseY() );
	}

	// Use the default cursor if there is no viewport client or we dont have focus
	return FCursorReply::Cursor(MouseCursorToUse);
}

TOptional<TSharedRef<SWidget>> FSceneViewport::OnMapCursor(const FCursorReply& CursorReply)
{
	if (ViewportClient && GetSizeXY() != FIntPoint::ZeroValue)
	{
		return ViewportClient->MapCursor(this, CursorReply);
	}
	return ISlateViewport::OnMapCursor(CursorReply);
}

FReply FSceneViewport::OnMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	// Prevent throttling when interacting with the viewport so we can move around in it
	CurrentReplyState = FReply::Handled().PreventThrottling();

	KeyStateMap.Add(InMouseEvent.GetEffectingButton(), true);
	UpdateModifierKeys(InMouseEvent);
	UpdateCachedMousePos(InGeometry, InMouseEvent);
	UpdateCachedGeometry(InGeometry);

	// Switch to the viewport clients world before processing input
	FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);
	if (ViewportClient && GetSizeXY() != FIntPoint::ZeroValue)
	{
		// If we're obtaining focus, we have to copy the modifier key states prior to processing this mouse button event, as this is the only point at which the mouse down
		// event is processed when focus initially changes and the modifier keys need to be in-place to detect any unique drag-like events.
		if (!HasFocus())
		{
			FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
			ApplyModifierKeys(KeysState);
		}

		const bool bTemporaryCapture = 
			ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CaptureDuringMouseDown ||
			(ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CaptureDuringRightMouseDown && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton);

		// Process primary input if we aren't currently a game viewport, we already have capture, or we are permanent capture that dosn't consume the mouse down.
		const bool bProcessInputPrimary = !IsCurrentlyGameViewport() || HasMouseCapture() || (ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);

		const bool bAnyMenuWasVisible = FSlateApplication::Get().AnyMenusVisible();

		// Process the mouse event
		if (bTemporaryCapture || bProcessInputPrimary)
		{
			if (!ViewportClient->InputKey(this, InMouseEvent.GetUserIndex(), InMouseEvent.GetEffectingButton(), IE_Pressed))
			{
				CurrentReplyState = FReply::Unhandled();
			}
		}
		//else
		//{
		//	TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.Pin().ToSharedRef();
		//	if ( ViewportWidgetRef->HasUserFocusedDescendants(InMouseEvent.GetUserIndex()) )
		//	{
		//		// If we're still focused on a descendant, force it to stay focused.  Need to do this to fool SlateApplication
		//		// otherwise it will reassign focus thinking that nobody requested it.
		//		TSharedPtr<SWidget> FocusedWidgetPtr = FSlateApplication::Get().GetUserFocusedWidget(InMouseEvent.GetUserIndex());
		//		if ( FocusedWidgetPtr.IsValid() )
		//		{
		//			CurrentReplyState.SetUserFocus(FocusedWidgetPtr.ToSharedRef(), EFocusCause::SetDirectly, false);
		//		}
		//	}
		//}


		// a new menu was opened if there was previously not a menu visible but now there is
		const bool bNewMenuWasOpened = !bAnyMenuWasVisible && FSlateApplication::Get().AnyMenusVisible();

		const bool bPermanentCapture =
			(ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CapturePermanently) ||
			(ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);

		if (FSlateApplication::Get().IsActive() && !ViewportClient->IgnoreInput() &&
			!bNewMenuWasOpened && // We should not focus the viewport if a menu was opened as it would close the menu
			(bPermanentCapture || bTemporaryCapture))
		{
			CurrentReplyState = AcquireFocusAndCapture(FIntPoint(InMouseEvent.GetScreenSpacePosition().X, InMouseEvent.GetScreenSpacePosition().Y));
		}
	}

	// Re-set prevent throttling here as it can get reset when inside of InputKey()
	CurrentReplyState.PreventThrottling();

	return CurrentReplyState;
}

FReply FSceneViewport::AcquireFocusAndCapture(FIntPoint MousePosition)
{
	bShouldCaptureMouseOnActivate = false;

	FReply ReplyState = FReply::Handled().PreventThrottling();

	TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.Pin().ToSharedRef();

	// Mouse down should focus viewport for user input
	ReplyState.SetUserFocus(ViewportWidgetRef, EFocusCause::SetDirectly, true);

	UWorld* World = ViewportClient->GetWorld();
	if (World && World->IsGameWorld() && World->GetGameInstance() && (World->GetGameInstance()->GetFirstLocalPlayerController() || World->IsPlayInEditor()))
	{
		ReplyState.CaptureMouse(ViewportWidgetRef);

		if (ViewportClient->LockDuringCapture())
		{
			ReplyState.LockMouseToWidget(ViewportWidgetRef);
		}

		APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();
		bool bShouldShowMouseCursor = PC && PC->ShouldShowMouseCursor();
		if (ViewportClient->HideCursorDuringCapture() && bShouldShowMouseCursor)
		{
			bCursorHiddenDueToCapture = true;
			MousePosBeforeHiddenDueToCapture = MousePosition;
		}
		if (bCursorHiddenDueToCapture || !bShouldShowMouseCursor)
		{
			ReplyState.UseHighPrecisionMouseMovement(ViewportWidgetRef);
		}
	}
	else
	{
		ReplyState.UseHighPrecisionMouseMovement(ViewportWidgetRef);
	}

	return ReplyState;
}

bool FSceneViewport::IsCurrentlyGameViewport()
{
	// Either were game code only or were are currently play in editor.
	return (FApp::IsGame() && !GIsEditor) || IsPlayInEditorViewport();
}

FReply FSceneViewport::OnMouseButtonUp( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	KeyStateMap.Add( InMouseEvent.GetEffectingButton(), false );
	UpdateModifierKeys( InMouseEvent );
	UpdateCachedMousePos( InGeometry, InMouseEvent );
	UpdateCachedGeometry(InGeometry);

	// Switch to the viewport clients world before processing input
	FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );
	bool bCursorVisible = true;
	bool bReleaseMouse = true;
	
	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue )
	{
		if (!ViewportClient->InputKey(this, InMouseEvent.GetUserIndex(), InMouseEvent.GetEffectingButton(), IE_Released))
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
		bCursorVisible = ViewportClient->GetCursor(this, GetMouseX(), GetMouseY()) != EMouseCursor::None;
		bReleaseMouse = 
			bCursorVisible || 
			ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CaptureDuringMouseDown ||
			( ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CaptureDuringRightMouseDown && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton );
	}

	if (!IsCurrentlyGameViewport() || bReleaseMouse)
	{
		// On mouse up outside of the game (editor viewport) or if the cursor is visible in game, we should make sure the mouse is no longer captured
		// as long as the left or right mouse buttons are not still down
		if( !InMouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) && !InMouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ))
		{
			if( bCursorHiddenDueToCapture )
			{
				bCursorHiddenDueToCapture = false;
				CurrentReplyState.SetMousePos( MousePosBeforeHiddenDueToCapture );
				MousePosBeforeHiddenDueToCapture = FIntPoint( -1, -1 );
			}

			CurrentReplyState.ReleaseMouseCapture();

			if (bCursorVisible && !ViewportClient->ShouldAlwaysLockMouse())
			{
				CurrentReplyState.ReleaseMouseLock();
			}
		}
	}

	return CurrentReplyState;
}

void FSceneViewport::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	UpdateCachedMousePos( MyGeometry, MouseEvent );
	ViewportClient->MouseEnter( this, GetMouseX(), GetMouseY() );
}

void FSceneViewport::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	if( ViewportClient )
	{
		ViewportClient->MouseLeave( this );
	
		if (IsCurrentlyGameViewport())
		{
			CachedMousePos = FIntPoint(-1, -1);
		}
	}
}

FReply FSceneViewport::OnMouseMove( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	UpdateCachedMousePos(InGeometry, InMouseEvent);
	UpdateCachedGeometry(InGeometry);

	const bool bViewportHasCapture = ViewportWidget.IsValid() && ViewportWidget.Pin()->HasMouseCapture();
	if ( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);

		if ( bViewportHasCapture )
		{
			ViewportClient->CapturedMouseMove(this, GetMouseX(), GetMouseY());
		}
		else
		{
			ViewportClient->MouseMove(this, GetMouseX(), GetMouseY());
		}

		if ( bViewportHasCapture )
		{
			// Accumulate delta changes to mouse movement.  Depending on the sample frequency of a mouse we may get many per frame.
			//@todo Slate: In directinput, number of samples in x/y could be different...
			const FVector2D CursorDelta = InMouseEvent.GetCursorDelta();
			MouseDelta.X += CursorDelta.X;
			++NumMouseSamplesX;

			MouseDelta.Y -= CursorDelta.Y;
			++NumMouseSamplesY;
		}

		if ( bCursorHiddenDueToCapture )
		{
			// If hidden during capture, don't actually move the cursor
			FVector2D RevertedCursorPos( MousePosBeforeHiddenDueToCapture.X, MousePosBeforeHiddenDueToCapture.Y );
			FSlateApplication::Get().SetCursorPos(RevertedCursorPos);
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnMouseWheel( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	UpdateCachedMousePos( InGeometry, InMouseEvent );
	UpdateCachedGeometry(InGeometry);

	if( ViewportClient  && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		// The viewport client accepts two different keys depending on the direction of scroll.  
		FKey const ViewportClientKey = InMouseEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;

		// Pressed and released should be sent
		ViewportClient->InputKey(this, InMouseEvent.GetUserIndex(), ViewportClientKey, IE_Pressed);
		ViewportClient->InputKey(this, InMouseEvent.GetUserIndex(), ViewportClientKey, IE_Released);
		ViewportClient->InputAxis(this, InMouseEvent.GetUserIndex(), EKeys::MouseWheelAxis, InMouseEvent.GetWheelDelta(), FApp::GetDeltaTime());
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnMouseButtonDoubleClick( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	// Note: When double-clicking, the following message sequence is sent:
	//	WM_*BUTTONDOWN
	//	WM_*BUTTONUP
	//	WM_*BUTTONDBLCLK	(Needs to set the KeyStates[*] to true)
	//	WM_*BUTTONUP
	KeyStateMap.Add( InMouseEvent.GetEffectingButton(), true );
	UpdateCachedMousePos( InGeometry, InMouseEvent );
	UpdateCachedGeometry(InGeometry);

	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if (!ViewportClient->InputKey(this, InMouseEvent.GetUserIndex(), InMouseEvent.GetEffectingButton(), IE_DoubleClick))
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& TouchEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled().PreventThrottling(); 

	UpdateCachedMousePos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		const FVector2D TouchPosition = MyGeometry.AbsoluteToLocal(TouchEvent.GetScreenSpacePosition());

		if( !ViewportClient->InputTouch( this, TouchEvent.GetUserIndex(), TouchEvent.GetPointerIndex(), ETouchType::Began, TouchPosition, FDateTime::Now(), TouchEvent.GetTouchpadIndex()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& TouchEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	UpdateCachedMousePos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		const FVector2D TouchPosition = MyGeometry.AbsoluteToLocal(TouchEvent.GetScreenSpacePosition());

		if( !ViewportClient->InputTouch( this, TouchEvent.GetUserIndex(), TouchEvent.GetPointerIndex(), ETouchType::Moved, TouchPosition, FDateTime::Now(), TouchEvent.GetTouchpadIndex()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& TouchEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	UpdateCachedMousePos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		const FVector2D TouchPosition = MyGeometry.AbsoluteToLocal(TouchEvent.GetScreenSpacePosition());

		if( !ViewportClient->InputTouch( this, TouchEvent.GetUserIndex(), TouchEvent.GetPointerIndex(), ETouchType::Ended, TouchPosition, FDateTime::Now(), TouchEvent.GetTouchpadIndex()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	UpdateCachedMousePos( MyGeometry, GestureEvent );
	UpdateCachedGeometry( MyGeometry );

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		FSlateApplication::Get().SetKeyboardFocus(ViewportWidget.Pin());

		if( !ViewportClient->InputGesture( this, GestureEvent.GetGestureType(), GestureEvent.GetGestureDelta(), GestureEvent.IsDirectionInvertedFromDevice() ) )
		{
			CurrentReplyState = FReply::Unhandled();
		}
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& MotionEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	if( ViewportClient )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if( !ViewportClient->InputMotion( this, MotionEvent.GetUserIndex(), MotionEvent.GetTilt(), MotionEvent.GetRotationRate(), MotionEvent.GetGravity(), MotionEvent.GetAcceleration()) )
		{
			CurrentReplyState = FReply::Unhandled(); 
		}
	}

	return CurrentReplyState;
}

FPopupMethodReply FSceneViewport::OnQueryPopupMethod() const
{
	if (ViewportClient != nullptr)
	{
		return ViewportClient->OnQueryPopupMethod();
	}
	else
	{
		return FPopupMethodReply::Unhandled();
	}
}

TOptional<bool> FSceneViewport::OnQueryShowFocus(const EFocusCause InFocusCause) const
{
	if (ViewportClient)
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);

		return ViewportClient->QueryShowFocus(InFocusCause);
	}

	return TOptional<bool>();
}

void FSceneViewport::OnFinishedPointerInput()
{
	ProcessAccumulatedPointerInput();
}

FReply FSceneViewport::OnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	FKey Key = InKeyEvent.GetKey();
	if (Key.IsValid())
	{
		KeyStateMap.Add(Key, true);

		//@todo Slate Viewports: FWindowsViewport checks for Alt+Enter or F11 and toggles fullscreen.  Unknown if fullscreen via this method will be needed for slate viewports. 
		if (ViewportClient && GetSizeXY() != FIntPoint::ZeroValue)
		{
			// Switch to the viewport clients world before processing input
			FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);

			if (!ViewportClient->InputKey(this, InKeyEvent.GetUserIndex(), Key, InKeyEvent.IsRepeat() ? IE_Repeat : IE_Pressed, 1.0f, Key.IsGamepadKey()))
			{
				CurrentReplyState = FReply::Unhandled();
			}
		}
	}
	else
	{
		CurrentReplyState = FReply::Unhandled();
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnKeyUp( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	FKey Key = InKeyEvent.GetKey();
	if (Key.IsValid())
	{
		KeyStateMap.Add(Key, false);

		if (ViewportClient && GetSizeXY() != FIntPoint::ZeroValue)
		{
			// Switch to the viewport clients world before processing input
			FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);

			if (!ViewportClient->InputKey(this, InKeyEvent.GetUserIndex(), Key, IE_Released, 1.0f, Key.IsGamepadKey()))
			{
				CurrentReplyState = FReply::Unhandled();
			}
		}
	}
	else
	{
		CurrentReplyState = FReply::Unhandled();
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	FKey Key = InAnalogInputEvent.GetKey();
	if (Key.IsValid())
	{
		KeyStateMap.Add(Key, true);

		if (ViewportClient)
		{
			// Switch to the viewport clients world before processing input
			FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);

			if (!ViewportClient->InputAxis(this, InAnalogInputEvent.GetUserIndex(), Key, Key == EKeys::Gamepad_RightY ? -InAnalogInputEvent.GetAnalogValue() : InAnalogInputEvent.GetAnalogValue(), FApp::GetDeltaTime(), 1, Key.IsGamepadKey()))
			{
				CurrentReplyState = FReply::Unhandled();
			}
		}
	}
	else
	{
		CurrentReplyState = FReply::Unhandled();
	}

	return CurrentReplyState;
}

FReply FSceneViewport::OnKeyChar( const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent )
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled(); 

	if( ViewportClient && GetSizeXY() != FIntPoint::ZeroValue  )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		if (!ViewportClient->InputChar(this, InCharacterEvent.GetUserIndex(), InCharacterEvent.GetCharacter()))
		{
			CurrentReplyState = FReply::Unhandled();
		}
	}
	return CurrentReplyState;
}

FReply FSceneViewport::OnFocusReceived(const FFocusEvent& InFocusEvent)
{
	CurrentReplyState = FReply::Handled(); 

	if ( InFocusEvent.GetUser() == 0 )
	{
		if ( ViewportClient != nullptr )
		{
			FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);
			ViewportClient->ReceivedFocus(this);
		}

		// Update key state mappings so that the the viewport modifier states are valid upon focus.
		const FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
		KeyStateMap.Add(EKeys::LeftAlt, KeysState.IsLeftAltDown());
		KeyStateMap.Add(EKeys::RightAlt, KeysState.IsRightAltDown());
		KeyStateMap.Add(EKeys::LeftControl, KeysState.IsLeftControlDown());
		KeyStateMap.Add(EKeys::RightControl, KeysState.IsRightControlDown());
		KeyStateMap.Add(EKeys::LeftShift, KeysState.IsLeftShiftDown());
		KeyStateMap.Add(EKeys::RightShift, KeysState.IsRightShiftDown());
		KeyStateMap.Add(EKeys::LeftCommand, KeysState.IsLeftCommandDown());
		KeyStateMap.Add(EKeys::RightCommand, KeysState.IsRightCommandDown());


		if ( IsCurrentlyGameViewport() )
		{
			FSlateApplication& SlateApp = FSlateApplication::Get();

			const bool bPermanentCapture =
				( ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CapturePermanently ) ||
				( ViewportClient->CaptureMouseOnClick() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown );

			if ( SlateApp.IsActive() && !ViewportClient->IgnoreInput() && bPermanentCapture )
			{
				TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.Pin().ToSharedRef();

				FWidgetPath PathToWidget;
				SlateApp.GeneratePathToWidgetUnchecked(ViewportWidgetRef, PathToWidget);

				return AcquireFocusAndCapture(GetSizeXY() / 2);
			}
		}
	}

	return CurrentReplyState;
}

void FSceneViewport::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	// If the focus loss event isn't the for the primary 'keyboard' user, don't worry about it.
	if ( InFocusEvent.GetUser() != 0 )
	{
		return;
	}

	bCursorHiddenDueToCapture = false;
	KeyStateMap.Empty();
	if ( ViewportClient != nullptr )
	{
		FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);
		ViewportClient->LostFocus(this);

		TSharedPtr<SWidget> ViewportWidgetPin = ViewportWidget.Pin();
		if ( ViewportWidgetPin.IsValid() )
		{
			FSlateApplication::Get().ForEachUser([&] (FSlateUser* User) {
				if ( User->GetFocusedWidget() == ViewportWidgetPin )
				{
					FSlateApplication::Get().ClearUserFocus(User->GetUserIndex());
				}
			});
		}
	}
}

void FSceneViewport::OnViewportClosed()
{
	if( ViewportClient )
	{
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );
		ViewportClient->CloseRequested( this );
	}
}

TWeakPtr<SWidget> FSceneViewport::GetWidget()
{
	return GetViewportWidget();
}

FReply FSceneViewport::OnViewportActivated(const FWindowActivateEvent& InActivateEvent)
{
	if (ViewportClient != nullptr)
	{
		FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);
		ViewportClient->Activated(this, InActivateEvent);
		
		// If we are activating and had Mouse Capture on deactivate then we should get focus again
		// It's important to note in the case of:
		//    InActivateEvent.ActivationType == FWindowActivateEvent::EA_ActivateByMouse
		// we do NOT acquire focus the reasoning is that the click itself will give us a chance on Mouse down to get capture.
		// This also means we don't go and grab capture in situations like:
		//    - the user clicked on the application header
		//    - the user clicked on some UI
		//    - the user clicked in our window but not an area our viewport covers.
		if (InActivateEvent.GetActivationType() == FWindowActivateEvent::EA_Activate && bShouldCaptureMouseOnActivate)
		{
			return AcquireFocusAndCapture(GetSizeXY() / 2);
		}
	}

	return FReply::Unhandled();
}
void FSceneViewport::OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent)
{
	// We backup if we have capture for us on activation, however we also maintain "true" if it's already true!
	// The reasoning behind maintaining "true" is that if the viewport is activated, 
	// however doesn't reclaim capture we want to claim capture next time we activate unless something else gets focus.
	// So we reset bHadMouseCaptureOnDeactivate in AcquireFocusAndCapture() and in OnFocusLost()
	//
	// This is not ideal, however the better fix probably requires that slate fundamentally chance when it "activates" a window or maybe just the viewport
	// Which there simply doesn't exist the right hooks currently.
	//
	// This fixes the case where the application is deactivated, then the user click on the windows header
	// this activates the window but we do not capture the mouse, then the User Alt-Tabs to the application.
	// We properly acquire capture because we maintained the "true" through the activation where nothing was focuses
	bShouldCaptureMouseOnActivate = bShouldCaptureMouseOnActivate || HasMouseCapture();

	KeyStateMap.Empty();
	if (ViewportClient != nullptr)
	{
		FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);
		ViewportClient->Deactivated(this, InActivateEvent);
	}
}

FSlateShaderResource* FSceneViewport::GetViewportRenderTargetTexture() const
{ 
	check(IsThreadSafeForSlateRendering());
	return (BufferedSlateHandles.Num() != 0) ? BufferedSlateHandles[CurrentBufferedTargetIndex] : nullptr;
}

void FSceneViewport::ResizeFrame(uint32 NewWindowSizeX, uint32 NewWindowSizeY, EWindowMode::Type NewWindowMode)
{
	// Resizing the window directly is only supported in the game
	if( FApp::IsGame() && NewWindowSizeX > 0 && NewWindowSizeY > 0 )
	{		
		FWidgetPath WidgetPath;
		TSharedPtr<SWindow> WindowToResize = FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef(), WidgetPath );

		if( WindowToResize.IsValid() )
		{
			NewWindowMode = GetWindowModeType(NewWindowMode);

			const FVector2D WindowPos = WindowToResize->GetPositionInScreen();
			const FVector2D WindowSize = WindowToResize->GetClientSizeInScreen();

			TOptional<FVector2D> NewWindowPos;
			FVector2D NewWindowSize(NewWindowSizeX, NewWindowSizeY);

			const FSlateRect BestWorkArea = FSlateApplication::Get().GetWorkArea(FSlateRect::FromPointAndExtent(WindowPos, WindowSize));

			// A switch to window mode should position the window to be in the center of the work-area (we don't do this if we were already in window mode to allow the user to move the window)
			// Fullscreen modes should position the window to the top-left of the work-area
			if (NewWindowMode == EWindowMode::Windowed)
			{
				if (WindowMode == EWindowMode::Windowed && NewWindowSize == WindowSize)
				{
					// Leave the window position alone!
					NewWindowPos.Reset();
				}
				else
				{
					const FVector2D BestWorkAreaTopLeft = BestWorkArea.GetTopLeft();
					const FVector2D BestWorkAreaSize = BestWorkArea.GetSize();

					FVector2D CenteredWindowPos = BestWorkAreaTopLeft;

					if (NewWindowSize.X < BestWorkAreaSize.X)
					{
						CenteredWindowPos.X += FMath::Max(0.0f, (BestWorkAreaSize.X - NewWindowSize.X) * 0.5f);
					}

					if (NewWindowSize.Y < BestWorkAreaSize.Y)
					{
						CenteredWindowPos.Y += FMath::Max(0.0f, (BestWorkAreaSize.Y - NewWindowSize.Y) * 0.5f);
					}

					NewWindowPos = CenteredWindowPos;
				}
			}
			else
			{
				NewWindowPos = BestWorkArea.GetTopLeft();
			}

			// If we're going into windowed fullscreen mode, we always want the window to fill the entire screen.
			// When we calculate the scene view, we'll check the fullscreen mode and configure the screen percentage
			// scaling so we actual render to the resolution we've been asked for.
			if (NewWindowMode == EWindowMode::WindowedFullscreen)
			{
				FDisplayMetrics DisplayMetrics;
				FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);

				// todo: this assumes that all your displays have the same resolution as your primary display
				// we need a way to query the display size for a rect (the work area size isn't the same thing), but for now we force the window to be on the primary display
				NewWindowPos = FVector2D(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left, DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);

				NewWindowSize.X = DisplayMetrics.PrimaryDisplayWidth;
				NewWindowSize.Y = DisplayMetrics.PrimaryDisplayHeight;
			}

			IHeadMountedDisplay::MonitorInfo MonitorInfo;
			if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->GetHMDMonitorInfo(MonitorInfo))
			{
				if (MonitorInfo.DesktopX > 0 || MonitorInfo.DesktopY > 0)
				{
					NewWindowSize.X = MonitorInfo.ResolutionX;
					NewWindowSize.Y = MonitorInfo.ResolutionY;
					NewWindowPos = FVector2D(MonitorInfo.DesktopX, MonitorInfo.DesktopY);
				}
			}

			// Resize window
			if (NewWindowSize != WindowSize || (NewWindowPos.IsSet() && NewWindowPos != WindowPos) || NewWindowMode != WindowMode)
			{
				WindowToResize->SetWindowMode(NewWindowMode);
				LockMouseToViewport(!CurrentReplyState.ShouldReleaseMouseLock());
				if (NewWindowPos.IsSet())
				{
					WindowToResize->ReshapeWindow(NewWindowPos.GetValue(), NewWindowSize);
				}
				else
				{
					WindowToResize->Resize(NewWindowSize);
				}
			}

			// Resize viewport
// @HSL_CHANGE_BEGIN - ngreen@hardsuitlabs.com - 5/31/2016 - Fixing windowed mode
			FVector2D ViewportSize = WindowToResize->GetWindowSizeFromClientSize(FVector2D(SizeX, SizeY));
// @HSL_CHANGE_END - ngreen@hardsuitlabs.com - 5/31/2016
			FVector2D NewViewportSize = WindowToResize->GetViewportSize();

			if (NewViewportSize != ViewportSize || NewWindowMode != WindowMode)
			{
				ResizeViewport(NewViewportSize.X, NewViewportSize.Y, NewWindowMode);
			}

			// Resize backbuffer
			FVector2D BackBufferSize = WindowToResize->IsMirrorWindow() ? WindowSize : ViewportSize;
			FVector2D NewBackbufferSize = WindowToResize->IsMirrorWindow() ? NewWindowSize : NewViewportSize;
			
			if (NewBackbufferSize != BackBufferSize)
			{
				FSlateApplicationBase::Get().GetRenderer()->UpdateFullscreenState(WindowToResize.ToSharedRef(), NewBackbufferSize.X, NewBackbufferSize.Y);
			}

			UCanvas::UpdateAllCanvasSafeZoneData();
		}
	}
}

void FSceneViewport::SetViewportSize(uint32 NewViewportSizeX, uint32 NewViewportSizeY)
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.Pin().ToSharedRef());
	if (Window.IsValid())
	{
		Window->SetIndependentViewportSize(FVector2D(NewViewportSizeX, NewViewportSizeY));
		const FVector2D vp = Window->IsMirrorWindow() ? Window->GetSizeInScreen() : Window->GetViewportSize();
		FSlateApplicationBase::Get().GetRenderer()->UpdateFullscreenState(Window.ToSharedRef(), vp.X, vp.Y);
		ResizeViewport(NewViewportSizeX, NewViewportSizeY, Window->GetWindowMode());
	}
}

TSharedPtr<SWindow> FSceneViewport::FindWindow()
{
	if ( ViewportWidget.IsValid() )
	{
		TSharedPtr<SViewport> PinnedViewportWidget = ViewportWidget.Pin();
		return FSlateApplication::Get().FindWidgetWindow(PinnedViewportWidget.ToSharedRef());
	}

	return TSharedPtr<SWindow>();
}

bool FSceneViewport::IsStereoRenderingAllowed() const
{
	if (ViewportWidget.IsValid())
	{
		return ViewportWidget.Pin()->IsStereoRenderingAllowed();
	}
	return false;
}

void FSceneViewport::ResizeViewport(uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode)
{
	// Do not resize if the viewport is an invalid size or our UI should be responsive
	if( NewSizeX > 0 && NewSizeY > 0 )
	{
		bIsResizing = true;

		UpdateViewportRHI(false, NewSizeX, NewSizeY, NewWindowMode);

		if (ViewportClient)
		{
			// Invalidate, then redraw immediately so the user isn't left looking at an empty black viewport
			// as they continue to resize the window.
			Invalidate();

			if ( ViewportClient->GetWorld() )
			{
				Draw();
			}
		}

		//if we have a delegate, fire it off
		if(FApp::IsGame() && OnSceneViewportResizeDel.IsBound())
		{
			OnSceneViewportResizeDel.Execute(FVector2D(NewSizeX, NewSizeY));
		}

		bIsResizing = false;
	}
}

void FSceneViewport::InvalidateDisplay()
{
	// Dirty the viewport.  It will be redrawn next time the editor ticks.
	if( ViewportClient != NULL )
	{
		ViewportClient->RedrawRequested( this );
	}
}

void FSceneViewport::DeferInvalidateHitProxy()
{
	if( ViewportClient != NULL )
	{
		ViewportClient->RequestInvalidateHitProxy( this );
	}
}

FCanvas* FSceneViewport::GetDebugCanvas()
{
	return DebugCanvasDrawer->GetGameThreadDebugCanvas();
}

const FTexture2DRHIRef& FSceneViewport::GetRenderTargetTexture() const
{
	if (IsInRenderingThread())
	{
		return RenderTargetTextureRenderThreadRHI;
	}
	return 	RenderTargetTextureRHI;
}

FSlateShaderResource* FSceneViewport::GetViewportRenderTargetTexture()
{
	if (IsInRenderingThread())
	{
		return RenderThreadSlateTexture;
	}
	return (BufferedSlateHandles.Num() != 0) ? BufferedSlateHandles[CurrentBufferedTargetIndex] : nullptr;
}

void FSceneViewport::SetRenderTargetTextureRenderThread(FTexture2DRHIRef& RT)
{
	check(IsInRenderingThread());
	RenderTargetTextureRenderThreadRHI = RT;
	if (RT.IsValid())
	{
		RenderThreadSlateTexture->SetRHIRef(RenderTargetTextureRenderThreadRHI, RT->GetSizeX(), RT->GetSizeY());
	}
	else
	{
		RenderThreadSlateTexture->SetRHIRef(nullptr, 0, 0);
	}
}

void FSceneViewport::UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode)
{
	{
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		// Update the viewport attributes.
		// This is done AFTER the command flush done by UpdateViewportRHI, to avoid disrupting rendering thread accesses to the old viewport size.
		SizeX = NewSizeX;
		SizeY = NewSizeY;
		WindowMode = NewWindowMode;

		// Release the viewport's resources.
		BeginReleaseResource(this);

		if( !bDestroyed )
		{
			BeginInitResource(this);
				
			if( !UseSeparateRenderTarget() )
			{
				// Get the viewport for this window from the renderer so we can render directly to the backbuffer
				TSharedPtr<FSlateRenderer> Renderer = FSlateApplication::Get().GetRenderer();
				FWidgetPath WidgetPath;
				void* ViewportResource = Renderer->GetViewportResource( *FSlateApplication::Get().FindWidgetWindow( ViewportWidget.Pin().ToSharedRef(), WidgetPath ) );
				if( ViewportResource )
				{
					ViewportRHI = *((FViewportRHIRef*)ViewportResource);
				}
			}

			ViewportResizedEvent.Broadcast(this, 0);
		}
		else
		{
			// Enqueue a render command to delete the handle.  It must be deleted on the render thread after the resource is released
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(DeleteSlateRenderTarget, TArray<FSlateRenderTargetRHI*>&, BufferedSlateHandles, BufferedSlateHandles,
																				FSlateRenderTargetRHI*&, RenderThreadSlateTexture, RenderThreadSlateTexture,
			{
				for (int32 i = 0; i < BufferedSlateHandles.Num(); ++i)
				{
					delete BufferedSlateHandles[i];
					BufferedSlateHandles[i] = nullptr;

					delete RenderThreadSlateTexture;
					RenderThreadSlateTexture = nullptr;
				}						
			});

		}
	}
}

void FSceneViewport::EnqueueBeginRenderFrame()
{
	check( IsInGameThread() );

	CurrentBufferedTargetIndex = NextBufferedTargetIndex;
	NextBufferedTargetIndex = (CurrentBufferedTargetIndex + 1) % BufferedSlateHandles.Num();
	if (BufferedRenderTargetsRHI[CurrentBufferedTargetIndex])
	{
		RenderTargetTextureRHI = BufferedRenderTargetsRHI[CurrentBufferedTargetIndex];
	}

	// check if we need to reallocate rendertarget for HMD and update HMD rendering viewport 
	if (GEngine->StereoRenderingDevice.IsValid() && IsStereoRenderingAllowed())
	{
		bool bHMDWantsSeparateRenderTarget = GEngine->StereoRenderingDevice->ShouldUseSeparateRenderTarget();
		if (bHMDWantsSeparateRenderTarget != bForceSeparateRenderTarget ||
		    (bHMDWantsSeparateRenderTarget && GEngine->StereoRenderingDevice->NeedReAllocateViewportRenderTarget(*this)))
		{
			// This will cause RT to be allocated (or freed)
			bForceSeparateRenderTarget = bHMDWantsSeparateRenderTarget;
			UpdateViewportRHI(false, SizeX, SizeY, WindowMode);
		}
	}

	DebugCanvasDrawer->InitDebugCanvas(GetClient()->GetWorld());

	// Note: ViewportRHI is only updated on the game thread

	// If we dont have the ViewportRHI then we need to get it before rendering
	// Note, we need ViewportRHI even if UseSeparateRenderTarget() is true when stereo rendering
	// is enabled.
	if (!IsValidRef(ViewportRHI) && (!UseSeparateRenderTarget() || (GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled())) )
	{
		// Get the viewport for this window from the renderer so we can render directly to the backbuffer
		TSharedPtr<FSlateRenderer> Renderer = FSlateApplication::Get().GetRenderer();
		FWidgetPath WidgetPath;
		if (ViewportWidget.IsValid())
		{
			auto WidgetWindow = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.Pin().ToSharedRef(), WidgetPath);
			if (WidgetWindow.IsValid())
			{
				void* ViewportResource = Renderer->GetViewportResource(*WidgetWindow);
				if (ViewportResource)
				{
					ViewportRHI = *((FViewportRHIRef*)ViewportResource);
				}
			}
		}
	}


	//set the rendertarget visible to the render thread
	//must come before any render thread frame handling.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetRenderThreadViewportTarget,
		FSceneViewport*, Viewport, this,
		FTexture2DRHIRef, RT, RenderTargetTextureRHI,		
		{

		Viewport->SetRenderTargetTextureRenderThread(RT);			
	});		

	FViewport::EnqueueBeginRenderFrame();

	if (GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->UpdateViewport(UseSeparateRenderTarget(), *this, ViewportWidget.Pin().Get());	
	}
}

void FSceneViewport::BeginRenderFrame(FRHICommandListImmediate& RHICmdList)
{
	check( IsInRenderingThread() );
	if (UseSeparateRenderTarget())
	{		
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, RenderTargetTextureRenderThreadRHI);
		SetRenderTarget(RHICmdList,  RenderTargetTextureRenderThreadRHI,  FTexture2DRHIRef(), true);
	}
	else if( IsValidRef( ViewportRHI ) ) 
	{
		// Get the backbuffer render target to render directly to it
		RenderTargetTextureRenderThreadRHI = RHICmdList.GetViewportBackBuffer(ViewportRHI);
		RenderThreadSlateTexture->SetRHIRef(RenderTargetTextureRenderThreadRHI, RenderTargetTextureRenderThreadRHI->GetSizeX(), RenderTargetTextureRenderThreadRHI->GetSizeY());
		if (GRHIRequiresEarlyBackBufferRenderTarget)
		{
			// unused set render targets are bad on Metal
			SetRenderTarget(RHICmdList, RenderTargetTextureRenderThreadRHI, FTexture2DRHIRef(), true);
		}
	}
}

void FSceneViewport::EndRenderFrame(FRHICommandListImmediate& RHICmdList, bool bPresent, bool bLockToVsync)
{
	check( IsInRenderingThread() );
	if (UseSeparateRenderTarget())
	{
		if (BufferedSlateHandles[CurrentBufferedTargetIndex])
		{			
			RHICmdList.CopyToResolveTarget(RenderTargetTextureRenderThreadRHI, RenderTargetTextureRenderThreadRHI, false, FResolveParams());
		}
	}
	else
	{
		// Set the active render target(s) to nothing to release references in the case that the viewport is resized by slate before we draw again
		SetRenderTarget(RHICmdList,  FTexture2DRHIRef(), FTexture2DRHIRef() );
		// Note: this releases our reference but does not release the resource as it is owned by slate (this is intended)
		RenderTargetTextureRenderThreadRHI.SafeRelease();
		RenderThreadSlateTexture->SetRHIRef(nullptr, 0, 0);
	}
}

void FSceneViewport::Tick( const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime )
{
	UpdateCachedGeometry(AllottedGeometry);
	ProcessInput( DeltaTime );
}

void FSceneViewport::OnPlayWorldViewportSwapped( const FSceneViewport& OtherViewport )
{
	// Play world viewports should always be the same size.  Resize to other viewports size
	if( GetSizeXY() != OtherViewport.GetSizeXY() )
	{
		// Switch to the viewport clients world before processing input
		FScopedConditionalWorldSwitcher WorldSwitcher( ViewportClient );

		UpdateViewportRHI( false, OtherViewport.GetSizeXY().X, OtherViewport.GetSizeXY().Y, EWindowMode::Windowed );

		// Invalidate, then redraw immediately so the user isn't left looking at an empty black viewport
		// as they continue to resize the window.
		Invalidate();
	}

	// Play world viewports should transfer active stats so it doesn't appear like a seperate viewport
	SwapStatCommands(OtherViewport);
}


void FSceneViewport::SwapStatCommands( const FSceneViewport& OtherViewport )
{
	FViewportClient* ClientA = GetClient();
	FViewportClient* ClientB = OtherViewport.GetClient();
	check(ClientA && ClientB);
	// Only swap if both viewports have stats
	const TArray<FString>* StatsA = ClientA->GetEnabledStats();
	const TArray<FString>* StatsB = ClientB->GetEnabledStats();
	if (StatsA && StatsB)
	{
		const TArray<FString> StatsCopy = *StatsA;
		ClientA->SetEnabledStats(*StatsB);
		ClientB->SetEnabledStats(StatsCopy);
	}
}

/** Queue an update to the Window's RT on the Renderthread */
void FSceneViewport::WindowRenderTargetUpdate(FSlateRenderer* Renderer, SWindow* Window)
{	
	check(IsInGameThread());
	if (Renderer)
	{
		if (UseSeparateRenderTarget())
		{
			if (Window)
			{
				// We need to pass a texture to the renderer only for stereo rendering. Otherwise, Editor will be rendered incorrectly.
				if (GEngine->IsStereoscopic3D(this))
				{
					//todo: mw Make this function take an FSlateTexture* rather than a void*
					Renderer->SetWindowRenderTarget(*Window, static_cast<IViewportRenderTargetProvider*>(this));
				}
				else
				{
					Renderer->SetWindowRenderTarget(*Window, nullptr);
				}
			}
		}
		else
		{
			if (Window)
			{
				Renderer->SetWindowRenderTarget(*Window, nullptr);
			}
		}
	}
}

void FSceneViewport::InitDynamicRHI()
{
	if(bRequiresHitProxyStorage)
	{
		// Initialize the hit proxy map.
		HitProxyMap.Init(SizeX,SizeY);
	}
	RTTSize = FIntPoint(0, 0);

	TSharedPtr<FSlateRenderer> Renderer = FSlateApplication::Get().GetRenderer();
	uint32 TexSizeX = SizeX, TexSizeY = SizeY;
	if (UseSeparateRenderTarget())
	{
		NumBufferedFrames = 1;
		
		// @todo vreditor switch: This code needs to be called when switching between stereo/non when going immersive.  Seems to always work out that way anyway though? (Probably due to resize)
		const bool bStereo = (IsStereoRenderingAllowed() && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabledOnNextFrame());
		bool bUseCustomPresentTexture = false;

		if (bStereo)
		{
			GEngine->StereoRenderingDevice->CalculateRenderTargetSize(*this, TexSizeX, TexSizeY);
			
			NumBufferedFrames = GEngine->StereoRenderingDevice->GetNumberOfBufferedFrames();
		}
		
		check(BufferedSlateHandles.Num() == BufferedRenderTargetsRHI.Num() && BufferedSlateHandles.Num() == BufferedShaderResourceTexturesRHI.Num());

		//clear existing entries
		for (int32 i = 0; i < BufferedSlateHandles.Num(); ++i)
		{
			if (!BufferedSlateHandles[i])
			{
				BufferedSlateHandles[i] = new FSlateRenderTargetRHI(nullptr, 0, 0);
			}
			BufferedRenderTargetsRHI[i] = nullptr;
			BufferedShaderResourceTexturesRHI[i] = nullptr;
		}

		if (BufferedSlateHandles.Num() < NumBufferedFrames)
		{
			//add sufficient entires for buffering.
			for (int32 i = BufferedSlateHandles.Num(); i < NumBufferedFrames; i++)
			{
				BufferedSlateHandles.Add(new FSlateRenderTargetRHI(nullptr, 0, 0));
				BufferedRenderTargetsRHI.Add(nullptr);
				BufferedShaderResourceTexturesRHI.Add(nullptr);
			}
		}
		else if (BufferedSlateHandles.Num() > NumBufferedFrames)
		{
			BufferedSlateHandles.SetNum(NumBufferedFrames);
			BufferedRenderTargetsRHI.SetNum(NumBufferedFrames);
			BufferedShaderResourceTexturesRHI.SetNum(NumBufferedFrames);
		}
		check(BufferedSlateHandles.Num() == BufferedRenderTargetsRHI.Num() && BufferedSlateHandles.Num() == BufferedShaderResourceTexturesRHI.Num());

		FRHIResourceCreateInfo CreateInfo;
		FTexture2DRHIRef BufferedRTRHI;
		FTexture2DRHIRef BufferedSRVRHI;

		for (int32 i = 0; i < NumBufferedFrames; ++i)
		{
			// try to allocate texture via StereoRenderingDevice; if not successful, use the default way
			if (!bStereo || !GEngine->StereoRenderingDevice->AllocateRenderTargetTexture(i, TexSizeX, TexSizeY, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, BufferedRTRHI, BufferedSRVRHI))
			{
				RHICreateTargetableShaderResource2D(TexSizeX, TexSizeY, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, BufferedRTRHI, BufferedSRVRHI);
			}
			BufferedRenderTargetsRHI[i] = BufferedRTRHI;
			BufferedShaderResourceTexturesRHI[i] = BufferedSRVRHI;

			if (BufferedSlateHandles[i])
			{
				BufferedSlateHandles[i]->SetRHIRef(BufferedShaderResourceTexturesRHI[0], TexSizeX, TexSizeY);
			}
		}

		// clear out any extra entries we have hanging around
		for (int32 i = NumBufferedFrames; i < BufferedSlateHandles.Num(); ++i)
		{
			if (BufferedSlateHandles[i])
			{
				BufferedSlateHandles[i]->SetRHIRef(nullptr, 0, 0);
			}
			BufferedRenderTargetsRHI[i] = nullptr;
			BufferedShaderResourceTexturesRHI[i] = nullptr;
		}

		CurrentBufferedTargetIndex = 0;
		NextBufferedTargetIndex = (CurrentBufferedTargetIndex + 1) % BufferedSlateHandles.Num();
		RenderTargetTextureRHI = BufferedShaderResourceTexturesRHI[CurrentBufferedTargetIndex];
	}
	else
	{
		check(BufferedSlateHandles.Num() == BufferedRenderTargetsRHI.Num() && BufferedSlateHandles.Num() == BufferedShaderResourceTexturesRHI.Num());
		if (BufferedSlateHandles.Num() == 0)
		{
			BufferedSlateHandles.Add(nullptr);
			BufferedRenderTargetsRHI.Add(nullptr);
			BufferedShaderResourceTexturesRHI.Add(nullptr);
		}		
		NumBufferedFrames = 1;

		RenderTargetTextureRHI = nullptr;		
		CurrentBufferedTargetIndex = NextBufferedTargetIndex = 0;
	}

	//how is this useful at all?  Pinning a weakptr to get a non-threadsafe shared ptr?  Pinning a weakptr is supposed to be protecting me from my weakptr dying underneath me...
	TSharedPtr<SWidget> PinnedViewport = ViewportWidget.Pin();
	if (PinnedViewport.IsValid())
	{

		FWidgetPath WidgetPath;
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(PinnedViewport.ToSharedRef(), WidgetPath);
		
		WindowRenderTargetUpdate(Renderer.Get(), Window.Get());
		if (UseSeparateRenderTarget())
		{
			RTTSize = FIntPoint(TexSizeX, TexSizeY);
		}
	}
}

void FSceneViewport::ReleaseDynamicRHI()
{
	FViewport::ReleaseDynamicRHI();

	ViewportRHI.SafeRelease();

	for (int32 i = 0; i < BufferedSlateHandles.Num(); ++i)
	{
		if (BufferedSlateHandles[i])
		{
			BufferedSlateHandles[i]->ReleaseDynamicRHI();
		}
	}
	if (RenderThreadSlateTexture)
	{
		RenderThreadSlateTexture->ReleaseDynamicRHI();
	}
}

void FSceneViewport::SetPreCaptureMousePosFromSlateCursor()
{
	PreCaptureMousePos = FSlateApplication::Get().GetCursorPos().IntPoint();
}
