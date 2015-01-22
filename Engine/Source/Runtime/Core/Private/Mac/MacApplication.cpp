// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "MacApplication.h"
#include "MacWindow.h"
#include "MacCursor.h"
#include "MacEvent.h"
#include "CocoaMenu.h"
#include "GenericApplicationMessageHandler.h"
#include "HIDInputInterface.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProvider.h"
#include "CocoaThread.h"
#include "ModuleManager.h"

static NSString* NSWindowDraggingFinished = @"NSWindowDraggingFinished";

FMacApplication* MacApplication = NULL;

#if WITH_EDITOR
typedef int32 (*MTContactCallbackFunction)(void*, void*, int32, double, int32);
extern "C" CFMutableArrayRef MTDeviceCreateList(void);
extern "C" void MTRegisterContactFrameCallback(void*, MTContactCallbackFunction);
extern "C" void MTDeviceStart(void*, int);
extern "C" bool MTDeviceIsBuiltIn(void*);

static int MTContactCallback(void* Device, void* Data, int32 NumFingers, double TimeStamp, int32 Frame)
{
	if (MacApplication)
	{
		const bool bIsTrackpad = MTDeviceIsBuiltIn(Device);
		MacApplication->SetIsUsingTrackpad(NumFingers > (bIsTrackpad ? 1 : 0));
	}
	return 1;
}
#endif

FMacApplication* FMacApplication::CreateMacApplication()
{
	MacApplication = new FMacApplication();
	return MacApplication;
}

NSEvent* FMacApplication::HandleNSEvent(NSEvent* Event)
{
	NSEvent* ReturnEvent = Event;

	const bool bIsMouseClickOrKeyEvent = [Event type] == NSLeftMouseDown || [Event type] == NSLeftMouseUp
		|| [Event type] == NSRightMouseDown || [Event type] == NSRightMouseUp
		|| [Event type] == NSOtherMouseDown || [Event type] == NSOtherMouseUp;
	const bool bIsResentEvent = [Event type] == NSApplicationDefined && (FMacApplicationEventTypes)[Event subtype] == FMacApplication::ResentEvent;

	if (MacApplication)
	{
		if (bIsResentEvent)
		{
			ReturnEvent = (NSEvent*)[Event data1];
		}

		if (!bIsResentEvent && (!bIsMouseClickOrKeyEvent || [Event window] == NULL))
		{
			FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
			
			if ([Event type] == NSKeyDown || [Event type] == NSKeyUp)
			{
				ReturnEvent = nil;
			}
		}

		if ([Event type] == NSLeftMouseUp)
		{
			NSNotification* Notification = [NSNotification notificationWithName:NSWindowDraggingFinished object:[Event window]];
			FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async);
		}
	}

	return ReturnEvent;
}

void FMacApplication::OnDisplayReconfiguration(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags, void* UserInfo)
{
	FMacApplication* App = (FMacApplication*)UserInfo;
	if(Flags & kCGDisplayDesktopShapeChangedFlag)
	{
		// Slate needs to know when desktop size changes.
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		App->BroadcastDisplayMetricsChanged(DisplayMetrics);
	}
	
	for (int32 WindowIndex=0; WindowIndex < App->Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FMacWindow > WindowRef = App->Windows[ WindowIndex ];
		WindowRef->OnDisplayReconfiguration(Display, Flags);
	}
}

FMacApplication::FMacApplication()
	: GenericApplication( MakeShareable( new FMacCursor() ) )
	, bUsingHighPrecisionMouseInput( false )
	, bUsingTrackpad( false )
	, HIDInput( HIDInputInterface::Create( MessageHandler ) )
	, DraggedWindow( NULL )
	, MouseCaptureWindow( NULL )
	, bIsMouseCaptureEnabled( false )
	, bIsMouseCursorLocked( false )
	, bSystemModalMode( false )
	, bIsProcessingNSEvent( false )
	, ModifierKeysFlags( 0 )
	, CurrentModifierFlags( 0 )
	, bIsWorkspaceSessionActive( true )
{
	CGDisplayRegisterReconfigurationCallback(FMacApplication::OnDisplayReconfiguration, this);

	[NSEvent addGlobalMonitorForEventsMatchingMask:NSMouseMovedMask handler:^(NSEvent* Event){
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}];

	EventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask handler:^(NSEvent* IncomingEvent)
	{
		NSEvent* ReturnEvent = HandleNSEvent(IncomingEvent);
		return ReturnEvent;
	}];

#if WITH_EDITOR
	NSMutableArray* MultiTouchDevices = (__bridge NSMutableArray*)MTDeviceCreateList();
	for (id Device in MultiTouchDevices)
	{
		MTRegisterContactFrameCallback((void*)Device, MTContactCallback);
		MTDeviceStart((void*)Device, 0);
	}
#endif

	TextInputMethodSystem = MakeShareable( new FMacTextInputMethodSystem );
	if(!TextInputMethodSystem->Initialize())
	{
		TextInputMethodSystem.Reset();
	}

	AppActivationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidBecomeActiveNotification object:[NSApplication sharedApplication] queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* Notification)
							{
								for (int32 Index = 0; Index < SavedWindowsOrder.Num(); Index++)
								{
									const FSavedWindowOrderInfo& SavedWindowLevel = SavedWindowsOrder[Index];
									NSWindow* Window = [NSApp windowWithWindowNumber:SavedWindowLevel.WindowNumber];
									if (Window)
									{
										[Window setLevel:SavedWindowLevel.Level];
									}
								}

								if (SavedWindowsOrder.Num() > 0)
								{
									[[NSApp windowWithWindowNumber:SavedWindowsOrder[0].WindowNumber] orderWindow:NSWindowAbove relativeTo:0];
									for (int32 Index = 1; Index < SavedWindowsOrder.Num(); Index++)
									{
										const FSavedWindowOrderInfo& SavedWindowLevel = SavedWindowsOrder[Index];
										NSWindow* Window = [NSApp windowWithWindowNumber:SavedWindowLevel.WindowNumber];
										if (Window)
										{
											[Window orderWindow:NSWindowBelow relativeTo:SavedWindowsOrder[Index - 1].WindowNumber];
										}
									}
								}

								// If editor thread doesn't have the focus, don't suck up too much CPU time.
								if (GIsEditor)
								{
									// Boost our priority back to normal.
									struct sched_param Sched;
									FMemory::Memzero(&Sched, sizeof(struct sched_param));
									Sched.sched_priority = 15;
									pthread_setschedparam(pthread_self(), SCHED_RR, &Sched);
								}

								// app is active, allow sound
								FApp::SetVolumeMultiplier( 1.0f );
							}];

	AppDeactivationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationWillResignActiveNotification object:[NSApplication sharedApplication] queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* Notification)
							{
								SavedWindowsOrder.Empty();

								NSArray* OrderedWindows = [NSApp orderedWindows];
								for (NSWindow* Window in OrderedWindows)
								{
									if ([Window isKindOfClass:[FCocoaWindow class]] && [Window isVisible] && ![Window hidesOnDeactivate])
									{
										SavedWindowsOrder.Add(FSavedWindowOrderInfo([Window windowNumber], [Window level]));
										[Window setLevel:NSNormalWindowLevel];
									}
								}

								if (SavedWindowsOrder.Num() > 0)
								{
									[[NSApp windowWithWindowNumber:SavedWindowsOrder[0].WindowNumber] orderWindow:NSWindowAbove relativeTo:0];
									for (int32 Index = 1; Index < SavedWindowsOrder.Num(); Index++)
									{
										const FSavedWindowOrderInfo& SavedWindowLevel = SavedWindowsOrder[Index];
										NSWindow* Window = [NSApp windowWithWindowNumber:SavedWindowLevel.WindowNumber];
										if (Window)
										{
											[Window orderWindow:NSWindowBelow relativeTo:SavedWindowsOrder[Index - 1].WindowNumber];
										}
									}
								}

								// If editor thread doesn't have the focus, don't suck up too much CPU time.
								if (GIsEditor)
								{
									// Drop our priority to speed up whatever is in the foreground.
									struct sched_param Sched;
									FMemory::Memzero(&Sched, sizeof(struct sched_param));
									Sched.sched_priority = 5;
									pthread_setschedparam(pthread_self(), SCHED_RR, &Sched);

									// Sleep for a bit to not eat up all CPU time.
									FPlatformProcess::Sleep(0.005f);
								}

								// app is inactive, apply multiplier
								FApp::SetVolumeMultiplier(FApp::GetUnfocusedVolumeMultiplier());
							}];

	WorkspaceActivationObserver = [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceSessionDidBecomeActiveNotification object:[NSWorkspace sharedWorkspace] queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* Notification){
									   bIsWorkspaceSessionActive = true;
								   }];

	WorkspaceDeactivationObserver = [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceSessionDidResignActiveNotification object:[NSWorkspace sharedWorkspace] queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* Notification){
									   bIsWorkspaceSessionActive = false;
								   }];

#if WITH_EDITOR
	FMemory::MemZero(GestureUsage);
	LastGestureUsed = EGestureEvent::None;
#endif
}

FMacApplication::~FMacApplication()
{
	if(EventMonitor)
	{
		[NSEvent removeMonitor:EventMonitor];
		EventMonitor = nil;
	}
	
	if(AppActivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:AppActivationObserver];
		AppActivationObserver = nil;
	}
	
	if(AppDeactivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:AppDeactivationObserver];
		AppDeactivationObserver = nil;
	}
	
	if(WorkspaceActivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:WorkspaceActivationObserver];
		WorkspaceActivationObserver = nil;
	}

	if(WorkspaceDeactivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:WorkspaceDeactivationObserver];
		WorkspaceDeactivationObserver = nil;
	}

	CGDisplayRemoveReconfigurationCallback(FMacApplication::OnDisplayReconfiguration, this);
	if (MouseCaptureWindow)
	{
		[MouseCaptureWindow close];
		MouseCaptureWindow = NULL;
	}
	
	TextInputMethodSystem->Terminate();

	MacApplication = NULL;
}

TSharedRef< FGenericWindow > FMacApplication::MakeWindow()
{
	return FMacWindow::Make();
}

void FMacApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FMacWindow > Window = StaticCastSharedRef< FMacWindow >( InWindow );
	const TSharedPtr< FMacWindow > ParentWindow = StaticCastSharedPtr< FMacWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FMacApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler( InMessageHandler );
	HIDInput->SetMessageHandler( InMessageHandler );
}

static TSharedPtr< FMacWindow > FindWindowByNSWindow( const TArray< TSharedRef< FMacWindow > >& WindowsToSearch, FCriticalSection* const Mutex, FCocoaWindow* const WindowHandle )
{
	FScopeLock Lock(Mutex);
	for (int32 WindowIndex=0; WindowIndex < WindowsToSearch.Num(); ++WindowIndex)
	{
		TSharedRef< FMacWindow > Window = WindowsToSearch[ WindowIndex ];
		if ( Window->GetWindowHandle() == WindowHandle )
		{
			return Window;
		}
	}

	return TSharedPtr< FMacWindow >( NULL );
}

void FMacApplication::ProcessEvent(FMacEvent const* const Event)
{
	// Must have an event
	check(Event);
	
	// Determine the type of the event to forward to the right handling routine
	NSEvent* AppKitEvent = Event->GetEvent();
	NSNotification* Notification = Event->GetNotification();

	if(AppKitEvent)
	{
		// Process a standard NSEvent as we always did
		MacApplication->ProcessNSEvent(AppKitEvent);
	}
	else if(Notification)
	{
		// Notifications need to mapped to the right handler function, that's no longer handled in the
		// call location.
		NSString* const NotificationName = [Notification name];
		FCocoaWindow* CocoaWindow = [[Notification object] isKindOfClass:[FCocoaWindow class]] ? (FCocoaWindow*)[Notification object] : nullptr;
		if (CocoaWindow)
		{
			if(NotificationName == NSWindowDidResizeNotification)
			{
				MacApplication->OnWindowDidResize( CocoaWindow );
			}
			else if(NotificationName == NSWindowDidEnterFullScreenNotification)
			{
				MacApplication->OnWindowDidResize( CocoaWindow );
			}
			else if(NotificationName == NSWindowDidExitFullScreenNotification)
			{
				MacApplication->OnWindowDidResize( CocoaWindow );
			}
			else if(NotificationName == NSWindowDidBecomeKeyNotification)
			{
				MacApplication->OnWindowDidBecomeKey( CocoaWindow );
			}
			else if(NotificationName == NSWindowDidResignKeyNotification)
			{
				MacApplication->OnWindowDidResignKey( CocoaWindow );
			}
			else if(NotificationName == NSWindowWillMoveNotification)
			{
				MacApplication->OnWindowWillMove( CocoaWindow );
			}
			else if(NotificationName == NSWindowDidMoveNotification)
			{
				MacApplication->OnWindowDidMove( CocoaWindow );
			}
			else if(NotificationName == NSWindowWillCloseNotification)
			{
				MacApplication->OnWindowDidClose( CocoaWindow );
			}
			else if(NotificationName == NSWindowRedrawContents)
			{
				MacApplication->OnWindowRedrawContents( CocoaWindow );
			}
			else if(NotificationName == NSWindowDraggingFinished)
			{
				MacApplication->OnWindowDraggingFinished();
			}
			else
			{
				check(false);
			}
		}
		else if ([[Notification object] conformsToProtocol:@protocol(NSDraggingInfo)])
		{
			CocoaWindow = (FCocoaWindow*)[(id<NSDraggingInfo>)[Notification object] draggingDestinationWindow];
			if(NotificationName == NSDraggingExited)
			{
				MacApplication->OnDragOut( CocoaWindow );
			}
			else if(NotificationName == NSDraggingUpdated)
			{
				MacApplication->OnDragOver( CocoaWindow );
			}
			else if(NotificationName == NSPrepareForDragOperation)
			{
				MacApplication->OnDragEnter( CocoaWindow, [(id < NSDraggingInfo >)[Notification object] draggingPasteboard] );
			}
			else if(NotificationName == NSPerformDragOperation)
			{
				MacApplication->OnDragDrop( CocoaWindow );
			}
			else
			{
				check(false);
			}
		}
	}
	
	// We are responsible for deallocating the event
	delete Event;
}

void FMacApplication::PumpMessages( const float TimeDelta )
{
	FPlatformMisc::PumpMessages( true );
}

void FMacApplication::OnWindowDraggingFinished()
{
	if( DraggedWindow )
	{
		SCOPED_AUTORELEASE_POOL;
		DraggedWindow = NULL;
	}
}

bool FMacApplication::IsWindowMovable(FCocoaWindow* Win, bool* OutMovableByBackground)
{
	if(OutMovableByBackground)
	{
		*OutMovableByBackground = false;
	}

	FCocoaWindow* NativeWindow = (FCocoaWindow*)Win;
	TSharedPtr< FMacWindow > CurrentEventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, NativeWindow );
	if( CurrentEventWindow.IsValid() )
	{
		const FVector2D CursorPos = static_cast<FMacCursor*>( Cursor.Get() )->GetPosition();
		const int32 LocalMouseX = CursorPos.X - CurrentEventWindow->PositionX;
		const int32 LocalMouseY = CursorPos.Y - CurrentEventWindow->PositionY;
		
		const EWindowZone::Type Zone = MessageHandler->GetWindowZoneForPoint( CurrentEventWindow.ToSharedRef(), LocalMouseX, LocalMouseY );
		bool IsMouseOverTitleBar = Zone == EWindowZone::TitleBar;
		switch(Zone)
		{
			case EWindowZone::NotInWindow:
			case EWindowZone::TopLeftBorder:
			case EWindowZone::TopBorder:
			case EWindowZone::TopRightBorder:
			case EWindowZone::LeftBorder:
			case EWindowZone::RightBorder:
			case EWindowZone::BottomLeftBorder:
			case EWindowZone::BottomBorder:
			case EWindowZone::BottomRightBorder:
				return true;
			case EWindowZone::TitleBar:
				if(OutMovableByBackground)
				{
					*OutMovableByBackground = true;
				}
				return true;
			case EWindowZone::ClientArea:
			case EWindowZone::MinimizeButton:
			case EWindowZone::MaximizeButton:
			case EWindowZone::CloseButton:
			case EWindowZone::SysMenu:
			default:
				return false;
		}
	}
	return true;
}

void FMacApplication::HandleModifierChange(NSUInteger NewModifierFlags, NSUInteger FlagsShift, NSUInteger UE4Shift, EMacModifierKeys TranslatedCode)
{
	bool CurrentPressed = (CurrentModifierFlags & FlagsShift) != 0;
	bool NewPressed = (NewModifierFlags & FlagsShift) != 0;
	if(CurrentPressed != NewPressed)
	{
		if( NewPressed )
		{
			ModifierKeysFlags |= 1 << UE4Shift;
			MessageHandler->OnKeyDown( TranslatedCode, 0, false );
		}
		else
		{
			ModifierKeysFlags &= ~(1 << UE4Shift);
			MessageHandler->OnKeyUp( TranslatedCode, 0, false );
		}
	}
}

void FMacApplication::ProcessNSEvent(NSEvent* const Event)
{
	SCOPED_AUTORELEASE_POOL;

	const bool bWasProcessingNSEvent = bIsProcessingNSEvent; // This function can be called recursively
	bIsProcessingNSEvent = true;

	if (CurrentModifierFlags != [Event modifierFlags])
	{
		NSUInteger ModifierFlags = [Event modifierFlags];

		HandleModifierChange(ModifierFlags, (1<<4), 7, MMK_RightCommand);
		HandleModifierChange(ModifierFlags, (1<<3), 6, MMK_LeftCommand);
		HandleModifierChange(ModifierFlags, (1<<1), 0, MMK_LeftShift);
		HandleModifierChange(ModifierFlags, (1<<16), 8, MMK_CapsLock);
		HandleModifierChange(ModifierFlags, (1<<5), 4, MMK_LeftAlt);
		HandleModifierChange(ModifierFlags, (1<<0), 2, MMK_LeftControl);
		HandleModifierChange(ModifierFlags, (1<<2), 1, MMK_RightShift);
		HandleModifierChange(ModifierFlags, (1<<6), 5, MMK_RightAlt);
		HandleModifierChange(ModifierFlags, (1<<13), 3, MMK_RightControl);

		CurrentModifierFlags = ModifierFlags;
	}

	FCocoaWindow* NativeWindow = FindEventWindow(Event);
	TSharedPtr<FMacWindow> CurrentEventWindow = FindWindowByNSWindow(MacApplication->Windows, &MacApplication->WindowsMutex, NativeWindow);

	const NSEventType EventType = [Event type];
	switch (EventType)
	{
		case NSMouseMoved:
		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:
		{
			if (CurrentEventWindow.IsValid() && CurrentEventWindow->IsRegularWindow())
			{
				bool IsMouseOverTitleBar = false;
				const bool IsMovable = IsWindowMovable(NativeWindow, &IsMouseOverTitleBar);
				[NativeWindow setMovable:IsMovable];
				[NativeWindow setMovableByWindowBackground:IsMouseOverTitleBar];
			}

			FMacCursor* MacCursor = (FMacCursor*)Cursor.Get();

			// Cocoa does not update NSWindow's frame until user stops dragging the window, so while window is being dragged, we calculate
			// its position based on mouse move delta
			if (CurrentEventWindow.IsValid() && DraggedWindow && DraggedWindow == NativeWindow)
			{
				const int32 X = FMath::TruncToInt(CurrentEventWindow->PositionX + [Event deltaX]);
				const int32 Y = FMath::TruncToInt(CurrentEventWindow->PositionY + [Event deltaY]);
				CurrentEventWindow->PositionX = X;
				CurrentEventWindow->PositionY = Y;
				MessageHandler->OnMovedWindow(CurrentEventWindow.ToSharedRef(), X, Y);
			}

			if (bUsingHighPrecisionMouseInput)
			{
				// Under OS X we disassociate the cursor and mouse position during hi-precision mouse input.
				// The game snaps the mouse cursor back to the starting point when this is disabled, which
				// accumulates mouse delta that we want to ignore.
				const FVector2D AccumDelta = MacCursor->GetMouseWarpDelta(true);

				// Find the screen the cursor is currently on.
				NSEnumerator *ScreenEnumerator = [[NSScreen screens] objectEnumerator];
				NSScreen *Screen;
				while ((Screen = [ScreenEnumerator nextObject]) && !NSMouseInRect(NSMakePoint(HighPrecisionMousePos.X, HighPrecisionMousePos.Y), Screen.frame, NO))
					;

				// Clamp to no more than the reported delta - a single event of no mouse movement won't be noticed
				// but going in the wrong direction will.
				const FVector2D FullDelta([Event deltaX], [Event deltaY]);
				const FVector2D WarpDelta(FMath::Abs(AccumDelta.X)<FMath::Abs(FullDelta.X) ? AccumDelta.X : FullDelta.X, FMath::Abs(AccumDelta.Y)<FMath::Abs(FullDelta.Y) ? AccumDelta.Y : FullDelta.Y);

				FVector2D Delta = ((FullDelta - WarpDelta) / 2.f) * MacCursor->GetMouseScaling();

				HighPrecisionMousePos = MacCursor->GetPosition() + Delta;
				MacCursor->UpdateCursorClipping(HighPrecisionMousePos);

				// Clamp to the current screen and avoid the menu bar and dock to prevent popups and other
				// assorted potential for mouse abuse.
				NSRect VisibleFrame = [Screen visibleFrame];
				// Avoid the menu bar & dock disclosure borders at the top & bottom of fullscreen windows
				if (CurrentEventWindow.IsValid() && CurrentEventWindow->GetWindowMode() != EWindowMode::Windowed)
				{
					VisibleFrame.origin.y += 5;
					VisibleFrame.size.height -= 10;
				}
				NSRect FullFrame = [Screen frame];
				VisibleFrame.origin.y = (FullFrame.origin.y+FullFrame.size.height) - (VisibleFrame.origin.y + VisibleFrame.size.height);

				HighPrecisionMousePos.X = FMath::Clamp(HighPrecisionMousePos.X / MacCursor->GetMouseScaling().X, (float)VisibleFrame.origin.x, (float)(VisibleFrame.origin.x + VisibleFrame.size.width)-1.f);
				HighPrecisionMousePos.Y = FMath::Clamp(HighPrecisionMousePos.Y / MacCursor->GetMouseScaling().Y, (float)VisibleFrame.origin.y, (float)(VisibleFrame.origin.y + VisibleFrame.size.height)-1.f);

				MacCursor->WarpCursor(HighPrecisionMousePos.X, HighPrecisionMousePos.Y);
				MessageHandler->OnRawMouseMove(Delta.X, Delta.Y);
			}
			else
			{
				NSPoint CursorPos = [Event locationInWindow];
				if ([Event GetWindow])
				{
					CursorPos.x += [Event windowPosition].x;
					CursorPos.y += [Event windowPosition].y;
				}
				CursorPos.y--; // The y coordinate in the point returned by locationInWindow starts from a base of 1
				const FVector2D MousePosition = FVector2D(CursorPos.x, FPlatformMisc::ConvertSlateYPositionToCocoa(CursorPos.y));
				FVector2D CurrentPosition = MousePosition * MacCursor->GetMouseScaling();
				if (MacCursor->UpdateCursorClipping(CurrentPosition))
				{
					MacCursor->SetPosition(CurrentPosition.X, CurrentPosition.Y);
				}
				else
				{
					MacCursor->UpdateCurrentPosition(MousePosition);
				}
				MessageHandler->OnMouseMove();
			}

			if (CurrentEventWindow.IsValid() && !DraggedWindow && !GetCapture())
			{
				MessageHandler->OnCursorSet();
			}
			break;
		}

		case NSLeftMouseDown:
		case NSRightMouseDown:
		case NSOtherMouseDown:
		{
			EMouseButtons::Type Button = [Event type] == NSLeftMouseDown ? EMouseButtons::Left : EMouseButtons::Right;
			if ([Event type] == NSOtherMouseDown)
			{
				switch ([Event buttonNumber])
				{
					case 2:
						Button = EMouseButtons::Middle;
						break;

					case 3:
						Button = EMouseButtons::Thumb01;
						break;

					case 4:
						Button = EMouseButtons::Thumb02;
						break;
				}
			}

			if (CurrentEventWindow.IsValid())
			{
				if (Button == LastPressedMouseButton && ([Event clickCount] % 2) == 0)
				{
					MessageHandler->OnMouseDoubleClick(CurrentEventWindow, Button);

					bool IsMouseOverTitleBar = false;
					const bool IsMovable = IsWindowMovable(NativeWindow, &IsMouseOverTitleBar);
					if (IsMouseOverTitleBar)
					{
						const bool bShouldMinimize = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleMiniaturizeOnDoubleClick"];
						if (bShouldMinimize)
						{
							MainThreadCall(^{ [NativeWindow performMiniaturize:nil]; }, NSDefaultRunLoopMode, true);
						}
						else if (!FPlatformMisc::IsRunningOnMavericks())
						{
							MainThreadCall(^{ [NativeWindow performZoom:nil]; }, NSDefaultRunLoopMode, true);
						}
					}
				}
				else
				{
					MessageHandler->OnMouseDown(CurrentEventWindow, Button);
				}

				if (!DraggedWindow && !GetCapture())
				{
					MessageHandler->OnCursorSet();
				}
			}

			LastPressedMouseButton = Button;

			break;
		}

		case NSLeftMouseUp:
		case NSRightMouseUp:
		case NSOtherMouseUp:
		{
			EMouseButtons::Type Button = [Event type] == NSLeftMouseUp ? EMouseButtons::Left : EMouseButtons::Right;
			if ([Event type] == NSOtherMouseUp)
			{
				switch ([Event buttonNumber])
				{
					case 2:
						Button = EMouseButtons::Middle;
						break;

					case 3:
						Button = EMouseButtons::Thumb01;
						break;

					case 4:
						Button = EMouseButtons::Thumb02;
						break;
				}
			}

			MessageHandler->OnMouseUp(Button);

			if (CurrentEventWindow.IsValid() && !DraggedWindow && !GetCapture())
			{
				MessageHandler->OnCursorSet();
			}

			FPlatformMisc::bChachedMacMenuStateNeedsUpdate = true;
			break;
		}

		case NSScrollWheel:
		{
			const float DeltaX = ([Event modifierFlags] & NSShiftKeyMask) ? [Event deltaY] : [Event deltaX];
			const float DeltaY = ([Event modifierFlags] & NSShiftKeyMask) ? [Event deltaX] : [Event deltaY];

			NSEventPhase Phase = [Event phase];

			if ([Event momentumPhase] != NSEventPhaseNone || [Event phase] != NSEventPhaseNone)
			{
				const bool bInverted = [Event isDirectionInvertedFromDevice];

				const FVector2D ScrollDelta([Event scrollingDeltaX], [Event scrollingDeltaY]);

				// This is actually a scroll gesture from trackpad
				MessageHandler->OnTouchGesture(EGestureEvent::Scroll, bInverted ? -ScrollDelta : ScrollDelta, DeltaY);
				RecordUsage(EGestureEvent::Scroll);
			}
			else
			{
				MessageHandler->OnMouseWheel(DeltaY);
			}

			if (CurrentEventWindow.IsValid() && !DraggedWindow && !GetCapture())
			{
				MessageHandler->OnCursorSet();
			}
			break;
		}

		case NSEventTypeMagnify:
		{
			MessageHandler->OnTouchGesture(EGestureEvent::Magnify, FVector2D([Event magnification], [Event magnification]), 0);
			RecordUsage(EGestureEvent::Magnify);
			break;
		}

		case NSEventTypeSwipe:
		{
			MessageHandler->OnTouchGesture(EGestureEvent::Swipe, FVector2D([Event deltaX], [Event deltaY]), 0);
			RecordUsage(EGestureEvent::Swipe);
			break;
		}

		case NSEventTypeRotate:
		{
			MessageHandler->OnTouchGesture(EGestureEvent::Rotate, FVector2D([Event rotation], [Event rotation]), 0);
			RecordUsage(EGestureEvent::Rotate);
			break;
		}

		case NSEventTypeBeginGesture:
		{
			MessageHandler->OnBeginGesture();
			break;
		}

		case NSEventTypeEndGesture:
		{
			MessageHandler->OnEndGesture();
#if WITH_EDITOR
			LastGestureUsed = EGestureEvent::None;
#endif
			break;
		}

		case NSKeyDown:
		{
			NSString *Characters = [Event characters];
			bool bHandled = false;
			if (!bSystemModalMode && [Characters length] && CurrentEventWindow.IsValid())
			{
				bHandled = CurrentEventWindow->OnIMKKeyDown(Event);
				if (!bHandled)
				{
					const bool IsRepeat = [Event isARepeat];
					const TCHAR Character = ConvertChar([Characters characterAtIndex:0]);
					const TCHAR CharCode = [[Event charactersIgnoringModifiers] characterAtIndex:0];
					const uint32 KeyCode = [Event keyCode];
					const bool IsPrintable = IsPrintableKey(Character);

					bHandled = MessageHandler->OnKeyDown(KeyCode, TranslateCharCode(CharCode, KeyCode), IsRepeat);

					// First KeyDown, then KeyChar. This is important, as in-game console ignores first character otherwise
					bool bCmdKeyPressed = [Event modifierFlags] & 0x18;
					if (!bCmdKeyPressed && IsPrintable)
					{
						MessageHandler->OnKeyChar(Character, IsRepeat);
					}
				}
			}
			if (bHandled)
			{
				FCocoaMenu* MainMenu = [[NSApp mainMenu] isKindOfClass:[FCocoaMenu class]] ? (FCocoaMenu*)[NSApp mainMenu]: nil;
				if (MainMenu)
				{
					MainThreadCall(^{ [MainMenu highlightKeyEquivalent:Event]; }, NSDefaultRunLoopMode, true);
				}
			}
			else
			{
				ResendEvent(Event);
			}
			break;
		}

		case NSKeyUp:
		{
			NSString *Characters = [Event characters];
			bool bHandled = false;
			if (!bSystemModalMode && [Characters length])
			{
				const bool IsRepeat = [Event isARepeat];
				const TCHAR Character = ConvertChar([Characters characterAtIndex:0]);
				const TCHAR CharCode = [[Event charactersIgnoringModifiers] characterAtIndex:0];
				const uint32 KeyCode = [Event keyCode];
				const bool IsPrintable = IsPrintableKey(Character);

				bHandled = MessageHandler->OnKeyUp(KeyCode, TranslateCharCode(CharCode, KeyCode), IsRepeat);
			}
			if (!bHandled)
			{
				ResendEvent(Event);
			}
			FPlatformMisc::bChachedMacMenuStateNeedsUpdate = true;
			break;
		}
	}

	[Event ResetWindow];
	bIsProcessingNSEvent = bWasProcessingNSEvent;
}

void FMacApplication::ResendEvent(NSEvent* Event)
{
	MainThreadCall(^{
		NSEvent* Wrapper = [NSEvent otherEventWithType:NSApplicationDefined location:[Event locationInWindow] modifierFlags:[Event modifierFlags] timestamp:[Event timestamp] windowNumber:[Event windowNumber] context:[Event context] subtype:FMacApplication::ResentEvent data1:(NSInteger)Event data2:0];
		
		[NSApp sendEvent:Wrapper];
	}, NSDefaultRunLoopMode, true);
}

FCocoaWindow* FMacApplication::FindEventWindow( NSEvent* Event )
{
	SCOPED_AUTORELEASE_POOL;

	bool IsMouseEvent = false;
	switch ([Event type])
	{
		case NSMouseMoved:
		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:
		case NSLeftMouseDown:
		case NSRightMouseDown:
		case NSOtherMouseDown:
		case NSLeftMouseUp:
		case NSRightMouseUp:
		case NSOtherMouseUp:
		case NSScrollWheel:
			IsMouseEvent = true;
			break;
	}

	FCocoaWindow* EventWindow = (FCocoaWindow*)[Event GetWindow];

	if ([Event type] == NSMouseMoved)
	{
		// Ignore windows owned by other applications
		NSInteger WindowNumber = [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0];
		if ([NSApp windowWithWindowNumber:WindowNumber] == NULL)
		{
			return NULL;
		}
	}

	if( IsMouseEvent )
	{
		if( DraggedWindow )
		{
			EventWindow = DraggedWindow;
		}
		else if( MouseCaptureWindow && MouseCaptureWindow == [Event GetWindow] && [MouseCaptureWindow targetWindow] )
		{
			EventWindow = [MouseCaptureWindow targetWindow];
		}
		else
		{
			NSPoint CursorPos = [Event locationInWindow];
			if ([Event GetWindow])
			{
				CursorPos.x += [Event windowPosition].x;
				CursorPos.y += [Event windowPosition].y;
			}
			CursorPos.y--; // The y coordinate in the point returned by locationInWindow starts from a base of 1
			TSharedPtr<FMacWindow> WindowUnderCursor = LocateWindowUnderCursor(CursorPos);
			if (WindowUnderCursor.IsValid())
			{
				EventWindow = WindowUnderCursor->GetWindowHandle();
			}
		}
	}

	return EventWindow;
}

TSharedPtr<FMacWindow> FMacApplication::LocateWindowUnderCursor( const NSPoint Position )
{
	NSScreen* MouseScreen = nil;
	if ([NSScreen screensHaveSeparateSpaces])
	{
		// New default mode which uses a separate Space per display
		// Find the screen the cursor is currently on so we can ignore invisible window regions.
		NSEnumerator* ScreenEnumerator = [[NSScreen screens] objectEnumerator];
		while ((MouseScreen = [ScreenEnumerator nextObject]) && !NSMouseInRect(Position, MouseScreen.frame, NO))
			;
	}

	NSArray* AllWindows = [NSApp orderedWindows];
	for (int32 Index = 0; Index < [AllWindows count]; Index++)
	{
		NSWindow* NativeWindow = (NSWindow*)[AllWindows objectAtIndex: Index];
		if ([NativeWindow isMiniaturized] || ![NativeWindow isVisible] || ![NativeWindow isOnActiveSpace] || ![NativeWindow isKindOfClass: [FCocoaWindow class]])
		{
			continue;
		}

        NSRect VisibleFrame = [NativeWindow frame];
#if WITH_EDITOR
        if(MouseScreen != nil)
        {
            VisibleFrame = NSIntersectionRect([MouseScreen frame], VisibleFrame);
        }
#endif

		if (NSPointInRect(Position, VisibleFrame))
		{
			TSharedPtr<FMacWindow> MacWindow = FindWindowByNSWindow(Windows, &WindowsMutex, (FCocoaWindow*)NativeWindow);
			return MacWindow;
		}
	}

	return NULL;
}

void FMacApplication::PollGameDeviceState( const float TimeDelta )
{
	// Poll game device state and send new events
	HIDInput->SendControllerEvents();
}

void FMacApplication::SetCapture( const TSharedPtr< FGenericWindow >& InWindow )
{
	bIsMouseCaptureEnabled = InWindow.IsValid();
	UpdateMouseCaptureWindow( bIsMouseCaptureEnabled ? ((FMacWindow*)InWindow.Get())->GetWindowHandle() : NULL );
}

void* FMacApplication::GetCapture( void ) const
{
	return ( bIsMouseCaptureEnabled && MouseCaptureWindow ) ? [MouseCaptureWindow targetWindow] : NULL;
}

void FMacApplication::UseMouseCaptureWindow( bool bUseMouseCaptureWindow )
{
	bIsMouseCaptureEnabled = bUseMouseCaptureWindow;
	if(bUseMouseCaptureWindow)
	{
		// Bring the mouse capture window to front
		if(MouseCaptureWindow)
		{
			[MouseCaptureWindow orderFront: nil];
		}
	}
	else
	{
		if(MouseCaptureWindow)
		{
			[MouseCaptureWindow orderOut: nil];
		}
	}
}

void FMacApplication::UpdateMouseCaptureWindow( FCocoaWindow* TargetWindow )
{
	SCOPED_AUTORELEASE_POOL;

	// To prevent mouse events to get passed to the Dock or top menu bar, we create a transparent, full screen window above everything.
	// This assures that only the editor (and with that, its active window) gets the mouse events while mouse capture is active.

	const bool bEnable = bIsMouseCaptureEnabled || bIsMouseCursorLocked;

	if( bEnable )
	{
		if( !TargetWindow )
		{
			if( [MouseCaptureWindow targetWindow] )
			{
				TargetWindow = [MouseCaptureWindow targetWindow];
			}
			else if( [[NSApp keyWindow] isKindOfClass: [FCocoaWindow class]] )
			{
				TargetWindow = (FCocoaWindow*)[NSApp keyWindow];
			}
		}

		if( !MouseCaptureWindow )
		{
			MouseCaptureWindow = [[FMouseCaptureWindow alloc] initWithTargetWindow: TargetWindow];
			check(MouseCaptureWindow);
		}
		else
		{
			[MouseCaptureWindow setTargetWindow: TargetWindow];
		}

		// Bring the mouse capture window to front
		[MouseCaptureWindow orderFront: nil];
	}
	else
	{
		// Hide the mouse capture window
		if( MouseCaptureWindow )
		{
			[MouseCaptureWindow orderOut: nil];
			[MouseCaptureWindow setTargetWindow: NULL];
		}
	}
}

void FMacApplication::SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow )
{
	bUsingHighPrecisionMouseInput = Enable;
	HighPrecisionMousePos = static_cast<FMacCursor*>( Cursor.Get() )->GetPosition();
	static_cast<FMacCursor*>( Cursor.Get() )->AssociateMouseAndCursorPosition( !Enable );
}

FModifierKeysState FMacApplication::GetModifierKeys() const
{
	uint32 CurrentFlags = ModifierKeysFlags;

	const bool bIsLeftShiftDown			= ( CurrentFlags & ( 1 << 0 ) ) != 0;
	const bool bIsRightShiftDown		= ( CurrentFlags & ( 1 << 1 ) ) != 0;
	const bool bIsLeftControlDown		= ( CurrentFlags & ( 1 << 6 ) ) != 0; // Mac pretends the Command key is Control
	const bool bIsRightControlDown		= ( CurrentFlags & ( 1 << 7 ) ) != 0; // Mac pretends the Command key is Control
	const bool bIsLeftAltDown			= ( CurrentFlags & ( 1 << 4 ) ) != 0;
	const bool bIsRightAltDown			= ( CurrentFlags & ( 1 << 5 ) ) != 0;
	const bool bIsLeftCommandDown		= ( CurrentFlags & ( 1 << 2 ) ) != 0; // Mac pretends the Control key is Command
	const bool bIsRightCommandDown		= ( CurrentFlags & ( 1 << 3 ) ) != 0; // Mac pretends the Control key is Command
	const bool bAreCapsLocked           = ( CurrentFlags & ( 1 << 8 ) ) != 0; 

	return FModifierKeysState(bIsLeftShiftDown, bIsRightShiftDown, bIsLeftControlDown, bIsRightControlDown, bIsLeftAltDown, bIsRightAltDown, bIsLeftCommandDown, bIsRightCommandDown, bAreCapsLocked);
}

FPlatformRect FMacApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	SCOPED_AUTORELEASE_POOL;

	NSScreen* Screen = FindScreenByPoint( CurrentWindow.Left, CurrentWindow.Top );

	const int32 ScreenHeight = FMath::TruncToInt([Screen frame].size.height);
	const NSRect VisibleFrame = [Screen visibleFrame];
	
	NSArray* AllScreens = [NSScreen screens];
	NSScreen* PrimaryScreen = (NSScreen*)[AllScreens objectAtIndex: 0];
	NSRect PrimaryFrame = [PrimaryScreen frame];

	FPlatformRect WorkArea;
	WorkArea.Left = VisibleFrame.origin.x;
	WorkArea.Top = (PrimaryFrame.origin.y + PrimaryFrame.size.height) - (VisibleFrame.origin.y + VisibleFrame.size.height);
	WorkArea.Right = WorkArea.Left + VisibleFrame.size.width;
	WorkArea.Bottom = WorkArea.Top + VisibleFrame.size.height;

	return WorkArea;
}

NSScreen* FMacApplication::FindScreenByPoint( int32 X, int32 Y ) const
{
	NSPoint Point = {0};
	Point.x = X;
	Point.y = FPlatformMisc::ConvertSlateYPositionToCocoa(Y);
	
	NSArray* AllScreens = [NSScreen screens];
	NSScreen* TargetScreen = [AllScreens objectAtIndex: 0];
	for(NSScreen* Screen in AllScreens)
	{
		if(Screen && NSPointInRect(Point, [Screen frame]))
		{
			TargetScreen = Screen;
			break;
		}
	}

	return TargetScreen;
}

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	SCOPED_AUTORELEASE_POOL;

	NSArray* AllScreens = [NSScreen screens];
	NSScreen* PrimaryScreen = (NSScreen*)[AllScreens objectAtIndex: 0];

	NSRect ScreenFrame = [PrimaryScreen frame];
	NSRect VisibleFrame = [PrimaryScreen visibleFrame];

	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = ScreenFrame.size.width;
	OutDisplayMetrics.PrimaryDisplayHeight = ScreenFrame.size.height;

	// Virtual desktop area
	NSRect WholeWorkspace = {{0,0},{0,0}};
	for (NSScreen* Screen in AllScreens)
	{
		if (Screen)
		{
			WholeWorkspace = NSUnionRect(WholeWorkspace, [Screen frame]);
		}
	}
	OutDisplayMetrics.VirtualDisplayRect.Left = WholeWorkspace.origin.x;
	OutDisplayMetrics.VirtualDisplayRect.Top = FMath::Min((ScreenFrame.size.height - (WholeWorkspace.origin.y + WholeWorkspace.size.height)), 0.0);
	OutDisplayMetrics.VirtualDisplayRect.Right = WholeWorkspace.origin.x + WholeWorkspace.size.width;
	OutDisplayMetrics.VirtualDisplayRect.Bottom = WholeWorkspace.size.height + OutDisplayMetrics.VirtualDisplayRect.Top;
	
	// Get the screen rect of the primary monitor, excluding taskbar etc.
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = VisibleFrame.origin.x;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = ScreenFrame.size.height - (VisibleFrame.origin.y + VisibleFrame.size.height);
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = VisibleFrame.origin.x + VisibleFrame.size.width;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top + VisibleFrame.size.height;
}

void FMacApplication::OnDragEnter(FCocoaWindow* Window, NSPasteboard* Pasteboard)
{
	SCOPED_AUTORELEASE_POOL;

	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow(Windows, &WindowsMutex, Window);
	if (!EventWindow.IsValid())
	{
		return;
	}

	// Decipher the pasteboard data
	const bool bHaveText = [[Pasteboard types] containsObject:NSPasteboardTypeString];
	const bool bHaveFiles = [[Pasteboard types] containsObject:NSFilenamesPboardType];

	if (bHaveFiles)
	{
		TArray<FString> FileList;

		NSArray *Files = [Pasteboard propertyListForType:NSFilenamesPboardType];
		for (int32 Index = 0; Index < [Files count]; Index++)
		{
			NSString* FilePath = [Files objectAtIndex: Index];
			const FString ListElement = FString([FilePath fileSystemRepresentation]);
			FileList.Add(ListElement);
		}

		MessageHandler->OnDragEnterFiles(EventWindow.ToSharedRef(), FileList);
	}
	else if (bHaveText)
	{
		NSString* Text = [Pasteboard stringForType:NSPasteboardTypeString];
		MessageHandler->OnDragEnterText(EventWindow.ToSharedRef(), FString(Text));
	}
}

void FMacApplication::OnDragOver( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		MessageHandler->OnDragOver( EventWindow.ToSharedRef() );
	}
}

void FMacApplication::OnDragOut( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		MessageHandler->OnDragLeave( EventWindow.ToSharedRef() );
	}
}

void FMacApplication::OnDragDrop( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		MessageHandler->OnDragDrop( EventWindow.ToSharedRef() );
	}
}

void FMacApplication::OnWindowDidBecomeKey( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		MessageHandler->OnWindowActivationChanged( EventWindow.ToSharedRef(), EWindowActivation::Activate );
		KeyWindows.Add( EventWindow.ToSharedRef() );
	}
}

void FMacApplication::OnWindowDidResignKey( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		MessageHandler->OnWindowActivationChanged( EventWindow.ToSharedRef(), EWindowActivation::Deactivate );
	}
}

void FMacApplication::OnWindowWillMove( FCocoaWindow* Window )
{
	SCOPED_AUTORELEASE_POOL;

	DraggedWindow = Window;
}

void FMacApplication::OnWindowDidMove( FCocoaWindow* Window )
{
	SCOPED_AUTORELEASE_POOL;

	NSRect WindowFrame = (Window.bDeferSetFrame || Window.bDeferSetOrigin) ? Window.DeferFrame : [Window frame];
	NSRect OpenGLFrame = [Window openGLFrame];
	
	const int32 X = (int32)WindowFrame.origin.x;
	const int32 Y = ([Window windowMode] != EWindowMode::Fullscreen) ? FPlatformMisc::ConvertSlateYPositionToCocoa( (int32)WindowFrame.origin.y ) - OpenGLFrame.size.height + 1 : 0;
	
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		MessageHandler->OnMovedWindow( EventWindow.ToSharedRef(), X, Y );
		EventWindow->PositionX = X;
		EventWindow->PositionY = Y;
	}
}

void FMacApplication::OnWindowDidResize( FCocoaWindow* Window )
{
	SCOPED_AUTORELEASE_POOL;

	OnWindowDidMove( Window );
	
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		// default is no override
		uint32 Width = [Window openGLFrame].size.width;
		uint32 Height = [Window openGLFrame].size.height;
		
		if([Window windowMode] == EWindowMode::WindowedFullscreen)
		{
			// Grab current monitor data for sizing
			Width = FMath::TruncToInt([[Window screen] frame].size.width);
			Height = FMath::TruncToInt([[Window screen] frame].size.height);
		}
		
		MessageHandler->OnSizeChanged( EventWindow.ToSharedRef(), Width, Height );
	}
}

void FMacApplication::OnWindowRedrawContents( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		SCOPED_AUTORELEASE_POOL;
		MessageHandler->OnSizeChanged( EventWindow.ToSharedRef(), [Window openGLFrame].size.width, [Window openGLFrame].size.height );
	}
}

void FMacApplication::OnWindowDidClose( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		SCOPED_AUTORELEASE_POOL;
		if ([Window isKeyWindow])
		{
			MessageHandler->OnWindowActivationChanged( EventWindow.ToSharedRef(), EWindowActivation::Deactivate );
		}
		Windows.Remove( EventWindow.ToSharedRef() );
		KeyWindows.Remove( EventWindow.ToSharedRef() );
		MessageHandler->OnWindowClose( EventWindow.ToSharedRef() );
	}
}

bool FMacApplication::OnWindowDestroyed( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		if ([Window isKeyWindow])
		{
			MessageHandler->OnWindowActivationChanged( EventWindow.ToSharedRef(), EWindowActivation::Deactivate );
		}
		Windows.Remove( EventWindow.ToSharedRef() );
		KeyWindows.Remove( EventWindow.ToSharedRef() );
		return true;
	}
	return false;
}

void FMacApplication::OnWindowClose( FCocoaWindow* Window )
{
	TSharedPtr< FMacWindow > EventWindow = FindWindowByNSWindow( Windows, &WindowsMutex, Window );
	if( EventWindow.IsValid() )
	{
		MessageHandler->OnWindowClose( EventWindow.ToSharedRef() );
	}
}

void FMacApplication::OnMouseCursorLock( bool bLockEnabled )
{
	bIsMouseCursorLocked = bLockEnabled;
	UpdateMouseCaptureWindow( NULL );
}

bool FMacApplication::IsPrintableKey(uint32 Character)
{
	switch (Character)
	{
		case NSPauseFunctionKey:		// EKeys::Pause
		case 0x1b:						// EKeys::Escape
		case NSPageUpFunctionKey:		// EKeys::PageUp
		case NSPageDownFunctionKey:		// EKeys::PageDown
		case NSEndFunctionKey:			// EKeys::End
		case NSHomeFunctionKey:			// EKeys::Home
		case NSLeftArrowFunctionKey:	// EKeys::Left
		case NSUpArrowFunctionKey:		// EKeys::Up
		case NSRightArrowFunctionKey:	// EKeys::Right
		case NSDownArrowFunctionKey:	// EKeys::Down
		case NSInsertFunctionKey:		// EKeys::Insert
		case NSDeleteFunctionKey:		// EKeys::Delete
		case NSF1FunctionKey:			// EKeys::F1
		case NSF2FunctionKey:			// EKeys::F2
		case NSF3FunctionKey:			// EKeys::F3
		case NSF4FunctionKey:			// EKeys::F4
		case NSF5FunctionKey:			// EKeys::F5
		case NSF6FunctionKey:			// EKeys::F6
		case NSF7FunctionKey:			// EKeys::F7
		case NSF8FunctionKey:			// EKeys::F8
		case NSF9FunctionKey:			// EKeys::F9
		case NSF10FunctionKey:			// EKeys::F10
		case NSF11FunctionKey:			// EKeys::F11
		case NSF12FunctionKey:			// EKeys::F12
			return false;

		default:
			return true;
	}
}

TCHAR FMacApplication::ConvertChar(TCHAR Character)
{
	switch (Character)
	{
		case NSDeleteCharacter:
			return '\b';
		default:
			return Character;
	}
}

TCHAR FMacApplication::TranslateCharCode(TCHAR CharCode, uint32 KeyCode)
{
	// Keys like F1-F12 or Enter do not need translation
	bool bNeedsTranslation = CharCode < NSOpenStepUnicodeReservedBase || CharCode > 0xF8FF;
	if( bNeedsTranslation )
	{
		// For non-numpad keys, the key code depends on the keyboard layout, so find out what was pressed by converting the key code to a Latin character
		TISInputSourceRef CurrentKeyboard = TISCopyCurrentKeyboardLayoutInputSource();
		if( CurrentKeyboard )
		{
			CFDataRef CurrentLayoutData = ( CFDataRef )TISGetInputSourceProperty( CurrentKeyboard, kTISPropertyUnicodeKeyLayoutData );
			CFRelease( CurrentKeyboard );

			if( CurrentLayoutData )
			{
				const UCKeyboardLayout *KeyboardLayout = ( UCKeyboardLayout *)CFDataGetBytePtr( CurrentLayoutData );
				if( KeyboardLayout )
				{
					UniChar Buffer[256] = { 0 };
					UniCharCount BufferLength = 256;
					uint32 DeadKeyState = 0;

					// To ensure we get a latin character, we pretend that command modifier key is pressed
					OSStatus Status = UCKeyTranslate( KeyboardLayout, KeyCode, kUCKeyActionDown, cmdKey >> 8, LMGetKbdType(), kUCKeyTranslateNoDeadKeysMask, &DeadKeyState, BufferLength, &BufferLength, Buffer );
					if( Status == noErr )
					{
						CharCode = Buffer[0];
					}
				}
			}
		}
	}
	// Private use range should not be returned
	else
	{
		CharCode = 0;
	}

	return CharCode;
}

TSharedPtr<FMacWindow> FMacApplication::GetKeyWindow()
{
	TSharedPtr<FMacWindow> KeyWindow;
	if(KeyWindows.Num() > 0)
	{
		KeyWindow = KeyWindows.Top();
	}
	return KeyWindow;
}

#if WITH_EDITOR

void FMacApplication::RecordUsage(EGestureEvent::Type Gesture)
{
	if ( LastGestureUsed != Gesture )
	{
		LastGestureUsed = Gesture;
		GestureUsage[Gesture] += 1;
	}
}

void FMacApplication::SendAnalytics(IAnalyticsProvider* Provider)
{
	static_assert(EGestureEvent::Count == 5, "If the number of gestures changes you need to add more entries below!");

	TArray<FAnalyticsEventAttribute> GestureAttributes;
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Scroll"),	GestureUsage[EGestureEvent::Scroll]));
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Magnify"),	GestureUsage[EGestureEvent::Magnify]));
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Swipe"),	GestureUsage[EGestureEvent::Swipe]));
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Rotate"),	GestureUsage[EGestureEvent::Rotate]));

    Provider->RecordEvent(FString("Mac.Gesture.Usage"), GestureAttributes);

	FMemory::MemZero(GestureUsage);
	LastGestureUsed = EGestureEvent::None;
}

#endif

void FMacApplication::SystemModalMode(bool const bInSystemModalMode)
{
	bSystemModalMode = bInSystemModalMode;
}
