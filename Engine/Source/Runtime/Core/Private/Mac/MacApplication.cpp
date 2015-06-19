// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "MacApplication.h"
#include "MacWindow.h"
#include "MacCursor.h"
#include "CocoaMenu.h"
#include "GenericApplicationMessageHandler.h"
#include "HIDInputInterface.h"
#include "IInputDeviceModule.h"
#include "IInputDevice.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProvider.h"
#include "CocoaThread.h"
#include "ModuleManager.h"
#include "CocoaTextView.h"

FMacApplication* MacApplication = nullptr;

const uint32 RESET_EVENT_SUBTYPE = 0x0f00;

#if WITH_EDITOR
typedef int32 (*MTContactCallbackFunction)(void*, void*, int32, double, int32);
extern "C" CFMutableArrayRef MTDeviceCreateList(void);
extern "C" void MTRegisterContactFrameCallback(void*, MTContactCallbackFunction);
extern "C" void MTDeviceStart(void*, int);
extern "C" bool MTDeviceIsBuiltIn(void*);
#endif

FMacApplication* FMacApplication::CreateMacApplication()
{
	MacApplication = new FMacApplication();
	return MacApplication;
}

FMacApplication::FMacApplication()
:	GenericApplication(MakeShareable(new FMacCursor()))
,	bUsingHighPrecisionMouseInput(false)
,	bUsingTrackpad(false)
,	LastPressedMouseButton(EMouseButtons::Invalid)
,	bIsProcessingDeferredEvents(false)
,	HIDInput(HIDInputInterface::Create(MessageHandler))
,   bHasLoadedInputPlugins(false)
,	DraggedWindow(nullptr)
,	bSystemModalMode(false)
,	ModifierKeysFlags(0)
,	CurrentModifierFlags(0)
,	bIsWorkspaceSessionActive(true)
{
	TextInputMethodSystem = MakeShareable(new FMacTextInputMethodSystem);
	if (!TextInputMethodSystem->Initialize())
	{
		TextInputMethodSystem.Reset();
	}

	AppActivationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidBecomeActiveNotification
																			  object:[NSApplication sharedApplication]
																			   queue:[NSOperationQueue mainQueue]
																		  usingBlock:^(NSNotification* Notification) { OnApplicationDidBecomeActive(); }];

	AppDeactivationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationWillResignActiveNotification
																				object:[NSApplication sharedApplication]
																				 queue:[NSOperationQueue mainQueue]
																			usingBlock:^(NSNotification* Notification) { OnApplicationWillResignActive(); }];

	WorkspaceActivationObserver = [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceSessionDidBecomeActiveNotification
																								  object:[NSWorkspace sharedWorkspace]
																								   queue:[NSOperationQueue mainQueue]
																							  usingBlock:^(NSNotification* Notification){ bIsWorkspaceSessionActive = true; }];

	WorkspaceDeactivationObserver = [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceSessionDidResignActiveNotification
																									object:[NSWorkspace sharedWorkspace]
																									 queue:[NSOperationQueue mainQueue]
																								usingBlock:^(NSNotification* Notification){ bIsWorkspaceSessionActive = false; }];

	MouseMovedEventMonitor = [NSEvent addGlobalMonitorForEventsMatchingMask:NSMouseMovedMask handler:^(NSEvent* Event) { DeferEvent(Event); }];
	EventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask handler:^(NSEvent* Event) { return HandleNSEvent(Event); }];

	CGDisplayRegisterReconfigurationCallback(FMacApplication::OnDisplayReconfiguration, this);

#if WITH_EDITOR
	NSMutableArray* MultiTouchDevices = (__bridge NSMutableArray*)MTDeviceCreateList();
	for (id Device in MultiTouchDevices)
	{
		MTRegisterContactFrameCallback((void*)Device, FMacApplication::MTContactCallback);
		MTDeviceStart((void*)Device, 0);
	}

	FMemory::Memzero(GestureUsage);
	LastGestureUsed = EGestureEvent::None;
#endif
}

FMacApplication::~FMacApplication()
{
	if (MouseMovedEventMonitor)
	{
		[NSEvent removeMonitor:MouseMovedEventMonitor];
	}
	if (EventMonitor)
	{
		[NSEvent removeMonitor:EventMonitor];
	}
	if (AppActivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:AppActivationObserver];
	}
	if (AppDeactivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:AppDeactivationObserver];
	}
	if (WorkspaceActivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:WorkspaceActivationObserver];
	}
	if (WorkspaceDeactivationObserver)
	{
		[[NSNotificationCenter defaultCenter] removeObserver:WorkspaceDeactivationObserver];
	}

	CGDisplayRemoveReconfigurationCallback(FMacApplication::OnDisplayReconfiguration, this);

	if (TextInputMethodSystem.IsValid())
	{
		TextInputMethodSystem->Terminate();
	}

	MacApplication = nullptr;
}

void FMacApplication::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	HIDInput->SetMessageHandler(InMessageHandler);
}

void FMacApplication::PollGameDeviceState(const float TimeDelta)
{
	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>( IInputDeviceModule::GetModularFeatureName() );
		for( auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt )
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
            if (Device.IsValid())
            {
                UE_LOG(LogInit, Log, TEXT("Adding external input plugin."));
                ExternalInputDevices.Add(Device);
            }
        }

		bHasLoadedInputPlugins = true;
	}

	// Poll game device state and send new events
	HIDInput->SendControllerEvents();

	// Poll externally-implemented devices
	for( auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt )
	{
		(*DeviceIt)->Tick( TimeDelta );
		(*DeviceIt)->SendControllerEvents();
	}
}

void FMacApplication::PumpMessages(const float TimeDelta)
{
	FPlatformMisc::PumpMessages(true);
}

void FMacApplication::ProcessDeferredEvents(const float TimeDelta)
{
	TArray<FDeferredMacEvent> EventsToProcess;

	EventsMutex.Lock();
	EventsToProcess.Append(DeferredEvents);
	DeferredEvents.Empty();
	EventsMutex.Unlock();

	const bool bAlreadyProcessingDeferredEvents = bIsProcessingDeferredEvents;
	bIsProcessingDeferredEvents = true;

	for (int32 Index = 0; Index < EventsToProcess.Num(); ++Index)
	{
		ProcessEvent(EventsToProcess[Index]);
	}

	bIsProcessingDeferredEvents = bAlreadyProcessingDeferredEvents;

	InvalidateTextLayouts();
	CloseQueuedWindows();
}

TSharedRef<FGenericWindow> FMacApplication::MakeWindow()
{
	return FMacWindow::Make();
}

void FMacApplication::InitializeWindow(const TSharedRef<FGenericWindow>& InWindow, const TSharedRef<FGenericWindowDefinition>& InDefinition, const TSharedPtr<FGenericWindow>& InParent, const bool bShowImmediately)
{
	const TSharedRef<FMacWindow> Window = StaticCastSharedRef<FMacWindow >(InWindow);
	const TSharedPtr<FMacWindow> ParentWindow = StaticCastSharedPtr<FMacWindow>(InParent);

	Windows.Add(Window);
	Window->Initialize(this, InDefinition, ParentWindow, bShowImmediately);
}

FModifierKeysState FMacApplication::GetModifierKeys() const
{
	uint32 CurrentFlags = ModifierKeysFlags;

	const bool bIsLeftShiftDown			= (CurrentFlags & (1 << 0)) != 0;
	const bool bIsRightShiftDown		= (CurrentFlags & (1 << 1)) != 0;
	const bool bIsLeftControlDown		= (CurrentFlags & (1 << 6)) != 0; // Mac pretends the Command key is Control
	const bool bIsRightControlDown		= (CurrentFlags & (1 << 7)) != 0; // Mac pretends the Command key is Control
	const bool bIsLeftAltDown			= (CurrentFlags & (1 << 4)) != 0;
	const bool bIsRightAltDown			= (CurrentFlags & (1 << 5)) != 0;
	const bool bIsLeftCommandDown		= (CurrentFlags & (1 << 2)) != 0; // Mac pretends the Control key is Command
	const bool bIsRightCommandDown		= (CurrentFlags & (1 << 3)) != 0; // Mac pretends the Control key is Command
	const bool bAreCapsLocked           = (CurrentFlags & (1 << 8)) != 0;

	return FModifierKeysState(bIsLeftShiftDown, bIsRightShiftDown, bIsLeftControlDown, bIsRightControlDown, bIsLeftAltDown, bIsRightAltDown, bIsLeftCommandDown, bIsRightCommandDown, bAreCapsLocked);
}

bool FMacApplication::IsCursorDirectlyOverSlateWindow() const
{
	SCOPED_AUTORELEASE_POOL;
	const NSInteger WindowNumber = [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0];
	NSWindow* const Window = [NSApp windowWithWindowNumber:WindowNumber];
	return Window && [Window isKindOfClass:[FCocoaWindow class]] && Window != DraggedWindow;
}

void FMacApplication::SetHighPrecisionMouseMode(const bool Enable, const TSharedPtr<FGenericWindow>& InWindow)
{
	bUsingHighPrecisionMouseInput = Enable;
	((FMacCursor*)Cursor.Get())->SetHighPrecisionMouseMode(Enable);
}

FPlatformRect FMacApplication::GetWorkArea(const FPlatformRect& CurrentWindow) const
{
	SCOPED_AUTORELEASE_POOL;

	NSScreen* Screen = FindScreenByPoint(CurrentWindow.Left, CurrentWindow.Top);

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

#if WITH_EDITOR
void FMacApplication::SendAnalytics(IAnalyticsProvider* Provider)
{
	static_assert(EGestureEvent::Count == 5, "If the number of gestures changes you need to add more entries below!");

	TArray<FAnalyticsEventAttribute> GestureAttributes;
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Scroll"),	GestureUsage[EGestureEvent::Scroll]));
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Magnify"),	GestureUsage[EGestureEvent::Magnify]));
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Swipe"),	GestureUsage[EGestureEvent::Swipe]));
	GestureAttributes.Add(FAnalyticsEventAttribute(FString("Rotate"),	GestureUsage[EGestureEvent::Rotate]));

	Provider->RecordEvent(FString("Mac.Gesture.Usage"), GestureAttributes);

	FMemory::Memzero(GestureUsage);
	LastGestureUsed = EGestureEvent::None;
}
#endif

void FMacApplication::CloseWindow(TSharedRef<FMacWindow> Window)
{
	MessageHandler->OnWindowClose(Window);
}

void FMacApplication::DeferEvent(NSObject* Object)
{
	FDeferredMacEvent DeferredEvent;

	if (Object && [Object isKindOfClass:[NSEvent class]])
	{
		NSEvent* Event = (NSEvent*)Object;
		DeferredEvent.Window = FindEventWindow(Event);
		DeferredEvent.Event = [Event retain];
		DeferredEvent.Type = [Event type];
		DeferredEvent.LocationInWindow = FVector2D([Event locationInWindow].x, [Event locationInWindow].y);
		DeferredEvent.ModifierFlags = [Event modifierFlags];
		DeferredEvent.Timestamp = [Event timestamp];
		DeferredEvent.WindowNumber = [Event windowNumber];
		DeferredEvent.Context = [[Event context] retain];

		if (DeferredEvent.Type == NSKeyDown)
		{
			if (DeferredEvent.Window && [DeferredEvent.Window openGLView])
			{
				FCocoaTextView* View = (FCocoaTextView*)[DeferredEvent.Window openGLView];
				if (View && [View imkKeyDown:Event])
				{
					return;
				}
			}
		}

		switch (DeferredEvent.Type)
		{
			case NSMouseMoved:
			case NSLeftMouseDragged:
			case NSRightMouseDragged:
			case NSOtherMouseDragged:
			case NSEventTypeSwipe:
				DeferredEvent.Delta = FVector2D([Event deltaX], [Event deltaY]);
				break;

			case NSLeftMouseDown:
			case NSRightMouseDown:
			case NSOtherMouseDown:
			case NSLeftMouseUp:
			case NSRightMouseUp:
			case NSOtherMouseUp:
				DeferredEvent.ButtonNumber = [Event buttonNumber];
				DeferredEvent.ClickCount = [Event clickCount];
				break;

			case NSScrollWheel:
				DeferredEvent.Delta = FVector2D([Event deltaX], [Event deltaY]);
				DeferredEvent.ScrollingDelta = FVector2D([Event scrollingDeltaX], [Event scrollingDeltaY]);
				DeferredEvent.Phase = [Event phase];
				DeferredEvent.MomentumPhase = [Event momentumPhase];
				DeferredEvent.IsDirectionInvertedFromDevice = [Event isDirectionInvertedFromDevice];
				break;

			case NSEventTypeMagnify:
				DeferredEvent.Delta = FVector2D([Event magnification], [Event magnification]);
				break;

			case NSEventTypeRotate:
				DeferredEvent.Delta = FVector2D([Event rotation], [Event rotation]);
				break;

			case NSKeyDown:
			case NSKeyUp:
			{
				if ([[Event characters] length] > 0)
				{
					DeferredEvent.Characters = [[Event characters] retain];
					DeferredEvent.CharactersIgnoringModifiers = [[Event charactersIgnoringModifiers] retain];
					DeferredEvent.IsRepeat = [Event isARepeat];
					DeferredEvent.KeyCode = [Event keyCode];
				}
				else
				{
					return;
				}
				break;
			}
		}
	}
	else if (Object && [Object isKindOfClass:[NSNotification class]])
	{
		NSNotification* Notification = (NSNotification*)Object;
		DeferredEvent.NotificationName = [[Notification name] retain];
		if ([[Notification object] isKindOfClass:[FCocoaWindow class]])
		{
			DeferredEvent.Window = (FCocoaWindow*)[Notification object];

			if (DeferredEvent.NotificationName == NSWindowDidResizeNotification)
			{
				if (DeferredEvent.Window)
				{
					GameThreadCall(^{
						TSharedPtr<FMacWindow> Window = FindWindowByNSWindow(DeferredEvent.Window);
						OnWindowDidResize(Window.ToSharedRef()); }, @[ NSDefaultRunLoopMode, UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ], true);
				}
				return;
			}
		}
		else if ([[Notification object] conformsToProtocol:@protocol(NSDraggingInfo)])
		{
            NSWindow* NotificationWindow = [(id<NSDraggingInfo>)[Notification object] draggingDestinationWindow];
            
            if (NotificationWindow && [NotificationWindow isKindOfClass:[FCocoaWindow class]])
            {
           		DeferredEvent.Window = (FCocoaWindow*)NotificationWindow;
            }
            
			if (DeferredEvent.NotificationName == NSPrepareForDragOperation)
			{
				DeferredEvent.DraggingPasteboard = [[(id<NSDraggingInfo>)[Notification object] draggingPasteboard] retain];
			}
		}
	}

	FScopeLock Lock(&EventsMutex);
	DeferredEvents.Add(DeferredEvent);
}

TSharedPtr<FMacWindow> FMacApplication::FindWindowByNSWindow(FCocoaWindow* WindowHandle)
{
	FScopeLock Lock(&WindowsMutex);

	for (int32 WindowIndex=0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef<FMacWindow> Window = Windows[WindowIndex];
		if (Window->GetWindowHandle() == WindowHandle)
		{
			return Window;
		}
	}

	return TSharedPtr<FMacWindow>(nullptr);
}

void FMacApplication::InvalidateTextLayout(FCocoaWindow* Window)
{
	WindowsRequiringTextInvalidation.AddUnique(Window);
}

NSEvent* FMacApplication::HandleNSEvent(NSEvent* Event)
{
	NSEvent* ReturnEvent = Event;

	if (MacApplication && !MacApplication->bSystemModalMode)
	{
		const bool bIsResentEvent = [Event type] == NSApplicationDefined && [Event subtype] == RESET_EVENT_SUBTYPE;

		if (bIsResentEvent)
		{
			ReturnEvent = (NSEvent*)[Event data1];
		}
		else
		{
			MacApplication->DeferEvent(Event);

			if ([Event type] == NSKeyDown || [Event type] == NSKeyUp)
			{
				ReturnEvent = nil;
			}
		}
	}

	return ReturnEvent;
}

void FMacApplication::OnDisplayReconfiguration(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags, void* UserInfo)
{
	FMacApplication* App = (FMacApplication*)UserInfo;
	if (Flags & kCGDisplayDesktopShapeChangedFlag)
	{
		// Slate needs to know when desktop size changes.
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		App->BroadcastDisplayMetricsChanged(DisplayMetrics);
	}

	for (int32 WindowIndex=0; WindowIndex < App->Windows.Num(); ++WindowIndex)
	{
		TSharedRef<FMacWindow> WindowRef = App->Windows[WindowIndex];
		WindowRef->OnDisplayReconfiguration(Display, Flags);
	}
}

#if WITH_EDITOR
int32 FMacApplication::MTContactCallback(void* Device, void* Data, int32 NumFingers, double TimeStamp, int32 Frame)
{
	if (MacApplication)
	{
		const bool bIsTrackpad = MTDeviceIsBuiltIn(Device);
		MacApplication->bUsingTrackpad = NumFingers > (bIsTrackpad ? 1 : 0);
	}
	return 1;
}
#endif

void FMacApplication::ProcessEvent(const FDeferredMacEvent& Event)
{
	TSharedPtr<FMacWindow> EventWindow = FindWindowByNSWindow(Event.Window);
	if (Event.Type)
	{
		if (CurrentModifierFlags != Event.ModifierFlags)
		{
			NSUInteger ModifierFlags = Event.ModifierFlags;

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

		switch (Event.Type)
		{
			case NSMouseMoved:
			case NSLeftMouseDragged:
			case NSRightMouseDragged:
			case NSOtherMouseDragged:
				ProcessMouseMovedEvent(Event, EventWindow);
				break;

			case NSLeftMouseDown:
			case NSRightMouseDown:
			case NSOtherMouseDown:
				ProcessMouseDownEvent(Event, EventWindow);
				break;

			case NSLeftMouseUp:
			case NSRightMouseUp:
			case NSOtherMouseUp:
				ProcessMouseUpEvent(Event, EventWindow);
				break;

			case NSScrollWheel:
				ProcessScrollWheelEvent(Event, EventWindow);
				break;

			case NSEventTypeMagnify:
			case NSEventTypeSwipe:
			case NSEventTypeRotate:
			case NSEventTypeBeginGesture:
			case NSEventTypeEndGesture:
				ProcessGestureEvent(Event);
				break;

			case NSKeyDown:
				ProcessKeyDownEvent(Event, EventWindow);
				break;

			case NSKeyUp:
				ProcessKeyUpEvent(Event);
				break;
		}
	}
	else if (EventWindow.IsValid())
	{
		if (Event.NotificationName == NSWindowWillStartLiveResizeNotification)
		{
			MessageHandler->BeginReshapingWindow(EventWindow.ToSharedRef());
		}
		else if (Event.NotificationName == NSWindowDidEndLiveResizeNotification)
		{
			MessageHandler->FinishedReshapingWindow(EventWindow.ToSharedRef());
		}
		else if (Event.NotificationName == NSWindowDidEnterFullScreenNotification)
		{
			OnWindowDidResize(EventWindow.ToSharedRef());
		}
		else if (Event.NotificationName == NSWindowDidExitFullScreenNotification)
		{
			OnWindowDidResize(EventWindow.ToSharedRef());
		}
		else if (Event.NotificationName == NSWindowDidBecomeMainNotification)
		{
			MessageHandler->OnWindowActivationChanged(EventWindow.ToSharedRef(), EWindowActivation::Activate);
			OnWindowsReordered(![NSApp isActive]);
		}
		else if (Event.NotificationName == NSWindowDidResignMainNotification)
		{
			MessageHandler->OnWindowActivationChanged(EventWindow.ToSharedRef(), EWindowActivation::Deactivate);
			OnWindowsReordered(![NSApp isActive]);
		}
		else if (Event.NotificationName == NSWindowWillMoveNotification)
		{
			DraggedWindow = EventWindow->GetWindowHandle();
		}
		else if (Event.NotificationName == NSWindowDidMoveNotification)
		{
			OnWindowDidMove(EventWindow.ToSharedRef());
		}
		else if (Event.NotificationName == NSDraggingExited)
		{
			MessageHandler->OnDragLeave(EventWindow.ToSharedRef());
		}
		else if (Event.NotificationName == NSDraggingUpdated)
		{
			MessageHandler->OnDragOver(EventWindow.ToSharedRef());
		}
		else if (Event.NotificationName == NSPrepareForDragOperation)
		{
			SCOPED_AUTORELEASE_POOL;

			// Decipher the pasteboard data
			const bool bHaveText = [[Event.DraggingPasteboard types] containsObject:NSPasteboardTypeString];
			const bool bHaveFiles = [[Event.DraggingPasteboard types] containsObject:NSFilenamesPboardType];

			if (bHaveFiles)
			{
				TArray<FString> FileList;

				NSArray *Files = [Event.DraggingPasteboard propertyListForType:NSFilenamesPboardType];
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
				NSString* Text = [Event.DraggingPasteboard stringForType:NSPasteboardTypeString];
				MessageHandler->OnDragEnterText(EventWindow.ToSharedRef(), FString(Text));
			}
		}
		else if (Event.NotificationName == NSPerformDragOperation)
		{
			MessageHandler->OnDragDrop(EventWindow.ToSharedRef());
		}
	}
}

void FMacApplication::ResendEvent(NSEvent* Event)
{
	MainThreadCall(^{
		NSEvent* Wrapper = [NSEvent otherEventWithType:NSApplicationDefined location:[Event locationInWindow] modifierFlags:[Event modifierFlags] timestamp:[Event timestamp] windowNumber:[Event windowNumber] context:[Event context] subtype:RESET_EVENT_SUBTYPE data1:(NSInteger)Event data2:0];
		[NSApp sendEvent:Wrapper];
	}, NSDefaultRunLoopMode, true);
}

void FMacApplication::ProcessMouseMovedEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow)
{
	if (EventWindow.IsValid() && EventWindow->IsRegularWindow())
	{
		bool IsMouseOverTitleBar = false;
		const bool IsMovable = IsWindowMovable(EventWindow.ToSharedRef(), &IsMouseOverTitleBar);
		[EventWindow->GetWindowHandle() setMovable:IsMovable];
		[EventWindow->GetWindowHandle() setMovableByWindowBackground:IsMouseOverTitleBar];
	}

	FMacCursor* MacCursor = (FMacCursor*)Cursor.Get();

	FVector2D const MouseScaling = MacCursor->GetMouseScaling();

	if (bUsingHighPrecisionMouseInput)
	{
		// Under OS X we disassociate the cursor and mouse position during hi-precision mouse input.
		// The game snaps the mouse cursor back to the starting point when this is disabled, which
		// accumulates mouse delta that we want to ignore.
		const FVector2D AccumDelta = MacCursor->GetMouseWarpDelta() * MouseScaling;

		// Get the mouse position
		FVector2D HighPrecisionMousePos = MacCursor->GetPosition();

		// Find the screen the cursor is currently on.
		NSEnumerator *ScreenEnumerator = [[NSScreen screens] objectEnumerator];
		NSScreen *Screen;
		while ((Screen = [ScreenEnumerator nextObject]) && !NSMouseInRect(NSMakePoint(HighPrecisionMousePos.X / MouseScaling.X, FPlatformMisc::ConvertSlateYPositionToCocoa(HighPrecisionMousePos.Y) / MouseScaling.Y), Screen.frame, NO))
			;

		// Account for warping delta's
		FVector2D Delta = FVector2D(Event.Delta.X, Event.Delta.Y) * MouseScaling;
		const FVector2D WarpDelta(FMath::Abs(AccumDelta.X)<FMath::Abs(Delta.X) ? AccumDelta.X : Delta.X, FMath::Abs(AccumDelta.Y)<FMath::Abs(Delta.Y) ? AccumDelta.Y : Delta.Y);
		Delta -= WarpDelta;

		// Update to latest position
		HighPrecisionMousePos += Delta;

		// Clip to lock rect
		MacCursor->UpdateCursorClipping(HighPrecisionMousePos);

		// Clamp to the current screen and avoid the menu bar and dock to prevent popups and other
		// assorted potential for mouse abuse.
		NSRect VisibleFrame = [Screen visibleFrame];
		// Avoid the menu bar & dock disclosure borders at the top & bottom of fullscreen windows
		if (EventWindow.IsValid() && EventWindow->GetWindowMode() != EWindowMode::Windowed)
		{
			VisibleFrame.origin.y += 5;
			VisibleFrame.size.height -= 10;
		}
		int32 ScaledX = (int32)(HighPrecisionMousePos.X / MouseScaling.X);
		int32 ScaledY = (int32)(FPlatformMisc::ConvertSlateYPositionToCocoa(HighPrecisionMousePos.Y) / MouseScaling.Y);
		int32 ClampedPosX = FMath::Clamp(ScaledX, (int32)VisibleFrame.origin.x, (int32)(VisibleFrame.origin.x + VisibleFrame.size.width)-1);
		int32 ClampedPosY = FMath::Clamp(ScaledY, (int32)VisibleFrame.origin.y, (int32)(VisibleFrame.origin.y + VisibleFrame.size.height)-1);
		MacCursor->SetPosition(ClampedPosX * MouseScaling.X, FPlatformMisc::ConvertSlateYPositionToCocoa(ClampedPosY) * MouseScaling.Y);

		// Forward the delta on to Slate
		MessageHandler->OnRawMouseMove(Delta.X, Delta.Y);
	}
	else
	{
		NSPoint CursorPos = [NSEvent mouseLocation];
		CursorPos.y--; // The y coordinate of the point returned by mouseLocation starts from a base of 1
		const FVector2D MousePosition = FVector2D(CursorPos.x, FPlatformMisc::ConvertSlateYPositionToCocoa(CursorPos.y));
		FVector2D CurrentPosition = MousePosition * MouseScaling;
		const FVector2D MouseDelta = CurrentPosition - MacCursor->GetPosition();
		if (MacCursor->UpdateCursorClipping(CurrentPosition))
		{
			MacCursor->SetPosition(CurrentPosition.X, CurrentPosition.Y);
		}
		else
		{
			MacCursor->UpdateCurrentPosition(CurrentPosition);
		}

		if (EventWindow.IsValid())
		{
			// Cocoa does not update NSWindow's frame until user stops dragging the window, so while window is being dragged, we calculate
			// its position based on mouse move delta
			if (DraggedWindow && DraggedWindow == EventWindow->GetWindowHandle())
			{
				const int32 X = FMath::TruncToInt(EventWindow->PositionX + MouseDelta.X);
				const int32 Y = FMath::TruncToInt(EventWindow->PositionY + MouseDelta.Y);
				MessageHandler->OnMovedWindow(EventWindow.ToSharedRef(), X, Y);
				EventWindow->PositionX = X;
				EventWindow->PositionY = Y;
			}

			MessageHandler->OnMouseMove();
		}
	}

	if (EventWindow.IsValid() && EventWindow->GetWindowHandle() && !DraggedWindow && !GetCapture())
	{
		MessageHandler->OnCursorSet();
	}
}

void FMacApplication::ProcessMouseDownEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow)
{
	EMouseButtons::Type Button = Event.Type == NSLeftMouseDown ? EMouseButtons::Left : EMouseButtons::Right;
	if (Event.Type == NSOtherMouseDown)
	{
		switch (Event.ButtonNumber)
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

	if (EventWindow.IsValid())
	{
		if (Button == LastPressedMouseButton && (Event.ClickCount % 2) == 0)
		{
			bool IsMouseOverTitleBar = false;
			const bool IsMovable = IsWindowMovable(EventWindow.ToSharedRef(), &IsMouseOverTitleBar);
			if (IsMouseOverTitleBar)
			{
				const bool bShouldMinimize = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleMiniaturizeOnDoubleClick"];
				FCocoaWindow* WindowHandle = EventWindow->GetWindowHandle();
				if (bShouldMinimize)
				{
					MainThreadCall(^{ [WindowHandle performMiniaturize:nil]; }, NSDefaultRunLoopMode, true);
				}
				else if (!FPlatformMisc::IsRunningOnMavericks())
				{
					MainThreadCall(^{ [WindowHandle zoom:nil]; }, NSDefaultRunLoopMode, true);
				}
			}
			else
			{
				MessageHandler->OnMouseDoubleClick(EventWindow, Button);
			}
		}
		else
		{
			MessageHandler->OnMouseDown(EventWindow, Button);
		}

		if (EventWindow.IsValid() && EventWindow->GetWindowHandle() && !DraggedWindow && !GetCapture())
		{
			MessageHandler->OnCursorSet();
		}
	}

	LastPressedMouseButton = Button;
}

void FMacApplication::ProcessMouseUpEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow)
{
	EMouseButtons::Type Button = Event.Type == NSLeftMouseUp ? EMouseButtons::Left : EMouseButtons::Right;
	if (Event.Type == NSOtherMouseUp)
	{
		switch (Event.ButtonNumber)
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

	if (EventWindow.IsValid() && EventWindow->GetWindowHandle() && !DraggedWindow && !GetCapture())
	{
		MessageHandler->OnCursorSet();
	}

	FPlatformMisc::bChachedMacMenuStateNeedsUpdate = true;
	DraggedWindow = nullptr;
}

void FMacApplication::ProcessScrollWheelEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow)
{
	const float DeltaX = (Event.ModifierFlags & NSShiftKeyMask) ? Event.Delta.Y : Event.Delta.X;
	const float DeltaY = (Event.ModifierFlags & NSShiftKeyMask) ? Event.Delta.X : Event.Delta.Y;

	NSEventPhase Phase = Event.Phase;

	if (Event.MomentumPhase != NSEventPhaseNone || Event.Phase != NSEventPhaseNone)
	{
		const FVector2D ScrollDelta(Event.ScrollingDelta.X, Event.ScrollingDelta.Y);

		// This is actually a scroll gesture from trackpad
		MessageHandler->OnTouchGesture(EGestureEvent::Scroll, Event.IsDirectionInvertedFromDevice ? -ScrollDelta : ScrollDelta, DeltaY);
		RecordUsage(EGestureEvent::Scroll);
	}
	else
	{
		MessageHandler->OnMouseWheel(DeltaY);
	}

	if (EventWindow.IsValid() && EventWindow->GetWindowHandle() && !DraggedWindow && !GetCapture())
	{
		MessageHandler->OnCursorSet();
	}
}

void FMacApplication::ProcessGestureEvent(const FDeferredMacEvent& Event)
{
	if (Event.Type == NSEventTypeBeginGesture)
	{
		MessageHandler->OnBeginGesture();
	}
	else if (Event.Type == NSEventTypeEndGesture)
	{
		MessageHandler->OnEndGesture();
#if WITH_EDITOR
		LastGestureUsed = EGestureEvent::None;
#endif
	}
	else
	{
		const EGestureEvent::Type GestureType = Event.Type == NSEventTypeMagnify ? EGestureEvent::Magnify : (Event.Type == NSEventTypeSwipe ? EGestureEvent::Swipe : EGestureEvent::Rotate);
		MessageHandler->OnTouchGesture(GestureType, Event.Delta, 0);
		RecordUsage(GestureType);
	}
}

void FMacApplication::ProcessKeyDownEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow)
{
	bool bHandled = false;
	if (!bSystemModalMode && EventWindow.IsValid())
	{
		const TCHAR Character = ConvertChar([Event.Characters characterAtIndex:0]);
		const TCHAR CharCode = [Event.CharactersIgnoringModifiers characterAtIndex:0];
		const bool IsPrintable = IsPrintableKey(Character);

		bHandled = MessageHandler->OnKeyDown(Event.KeyCode, TranslateCharCode(CharCode, Event.KeyCode), Event.IsRepeat);

		// First KeyDown, then KeyChar. This is important, as in-game console ignores first character otherwise
		bool bCmdKeyPressed = Event.ModifierFlags & 0x18;
		if (!bCmdKeyPressed && IsPrintable)
		{
			MessageHandler->OnKeyChar(Character, Event.IsRepeat);
		}
	}
	if (bHandled)
	{
		FCocoaMenu* MainMenu = [[NSApp mainMenu] isKindOfClass:[FCocoaMenu class]] ? (FCocoaMenu*)[NSApp mainMenu]: nil;
		if (MainMenu)
		{
			NSEvent* NativeEvent = Event.Event;
			MainThreadCall(^{ [MainMenu highlightKeyEquivalent:NativeEvent]; }, NSDefaultRunLoopMode, true);
		}
	}
	else
	{
		ResendEvent(Event.Event);
	}
}

void FMacApplication::ProcessKeyUpEvent(const FDeferredMacEvent& Event)
{
	bool bHandled = false;
	if (!bSystemModalMode)
	{
		const TCHAR Character = ConvertChar([Event.Characters characterAtIndex:0]);
		const TCHAR CharCode = [Event.CharactersIgnoringModifiers characterAtIndex:0];

		bHandled = MessageHandler->OnKeyUp(Event.KeyCode, TranslateCharCode(CharCode, Event.KeyCode), Event.IsRepeat);
	}
	if (!bHandled)
	{
		ResendEvent(Event.Event);
	}
	FPlatformMisc::bChachedMacMenuStateNeedsUpdate = true;
}

void FMacApplication::OnWindowDidMove(TSharedRef<FMacWindow> Window)
{
	SCOPED_AUTORELEASE_POOL;

	NSRect WindowFrame = [Window->GetWindowHandle() frame];
	NSRect OpenGLFrame = [Window->GetWindowHandle() openGLFrame];

	const int32 X = (int32)WindowFrame.origin.x;
	int32 Y = 0;

	if ([Window->GetWindowHandle() windowMode] != EWindowMode::Fullscreen)
	{
		Y = FPlatformMisc::ConvertCocoaYPositionToSlate(WindowFrame.origin.y + OpenGLFrame.size.height);
	}

	MessageHandler->OnMovedWindow(Window, X, Y);
	Window->PositionX = X;
	Window->PositionY = Y;
}

void FMacApplication::OnWindowDidResize(TSharedRef<FMacWindow> Window)
{
	SCOPED_AUTORELEASE_POOL;

	OnWindowDidMove(Window);

	// default is no override
	uint32 Width = [Window->GetWindowHandle() openGLFrame].size.width;
	uint32 Height = [Window->GetWindowHandle() openGLFrame].size.height;

	if ([Window->GetWindowHandle() windowMode] == EWindowMode::WindowedFullscreen)
	{
		// Grab current monitor data for sizing
		Width = FMath::TruncToInt([[Window->GetWindowHandle() screen] frame].size.width);
		Height = FMath::TruncToInt([[Window->GetWindowHandle() screen] frame].size.height);
	}

	// OnResizingWindow flushes the renderer commands which is needed before we start resizing, but also right after that
	// because window view's drawRect: can be called before Slate has a chance to flush them.
	MessageHandler->OnResizingWindow(Window);
	MessageHandler->OnSizeChanged(Window, Width, Height);
	MessageHandler->OnResizingWindow(Window);
}

bool FMacApplication::OnWindowDestroyed(TSharedRef<FMacWindow> Window)
{
	SCOPED_AUTORELEASE_POOL;
	if ([Window->GetWindowHandle() isKeyWindow])
	{
		MessageHandler->OnWindowActivationChanged(Window, EWindowActivation::Deactivate);
	}
	Windows.Remove(Window);
	if (!WindowsToClose.Contains(Window->GetWindowHandle()))
	{
		WindowsToClose.Add(Window->GetWindowHandle());
	}
	return true;
}

void FMacApplication::OnApplicationDidBecomeActive()
{
	for (int32 Index = 0; Index < SavedWindowsOrder.Num(); Index++)
	{
		const FSavedWindowOrderInfo& Info = SavedWindowsOrder[Index];
		NSWindow* Window = [NSApp windowWithWindowNumber:Info.WindowNumber];
		if (Window)
		{
			[Window setLevel:Info.Level];
		}
	}

	if (SavedWindowsOrder.Num() > 0)
	{
		NSWindow* TopWindow = [NSApp windowWithWindowNumber:SavedWindowsOrder[0].WindowNumber];
		if ( TopWindow )
		{
			[TopWindow orderWindow:NSWindowAbove relativeTo:0];
		}
		for (int32 Index = 1; Index < SavedWindowsOrder.Num(); Index++)
		{
			const FSavedWindowOrderInfo& Info = SavedWindowsOrder[Index];
			NSWindow* Window = [NSApp windowWithWindowNumber:Info.WindowNumber];
			if (Window && TopWindow)
			{
				[Window orderWindow:NSWindowBelow relativeTo:[TopWindow windowNumber]];
				TopWindow = Window;
			}
		}
	}

	((FMacCursor*)Cursor.Get())->UpdateVisibility();

	// If editor thread doesn't have the focus, don't suck up too much CPU time.
	if (GIsEditor && !IsRunningCommandlet())
	{
		// Boost our priority back to normal.
		struct sched_param Sched;
		FMemory::Memzero(&Sched, sizeof(struct sched_param));
		Sched.sched_priority = 15;
		pthread_setschedparam(pthread_self(), SCHED_RR, &Sched);
	}

	// app is active, allow sound
	FApp::SetVolumeMultiplier(1.0f);

	GameThreadCall(^{
		MessageHandler->OnApplicationActivationChanged(true);
	}, @[ NSDefaultRunLoopMode ], false);
}

void FMacApplication::OnApplicationWillResignActive()
{
	OnWindowsReordered(true);

	if (SavedWindowsOrder.Num() > 0)
	{
		NSWindow* TopWindow = [NSApp windowWithWindowNumber:SavedWindowsOrder[0].WindowNumber];
		if ( TopWindow )
		{
			[TopWindow orderWindow:NSWindowAbove relativeTo:0];
		}
		for (int32 Index = 1; Index < SavedWindowsOrder.Num(); Index++)
		{
			const FSavedWindowOrderInfo& Info = SavedWindowsOrder[Index];
			NSWindow* Window = [NSApp windowWithWindowNumber:Info.WindowNumber];
			if (Window)
			{
				if (SavedWindowsOrder[Index - 1].Level == Info.Level && TopWindow)
				{
					[Window orderWindow:NSWindowBelow relativeTo:[TopWindow windowNumber]];
				}
				else
				{
					[Window orderWindow:NSWindowAbove relativeTo:0];
				}
				TopWindow = Window;
			}
		}
	}

	((FMacCursor*)Cursor.Get())->UpdateVisibility();

	// If editor thread doesn't have the focus, don't suck up too much CPU time.
	if (GIsEditor && !IsRunningCommandlet())
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

	GameThreadCall(^{
		MessageHandler->OnApplicationActivationChanged(false);
	}, @[ NSDefaultRunLoopMode ], false);
}

void FMacApplication::OnWindowsReordered(bool bIsAppInBackground)
{
	if (bIsAppInBackground)
	{
		TMap<int32, int32> Levels;

		for (int32 Index = 0; Index < SavedWindowsOrder.Num(); Index++)
		{
			const FSavedWindowOrderInfo& Info = SavedWindowsOrder[Index];
			Levels.Add(Info.WindowNumber, Info.Level);
		}

		SavedWindowsOrder.Empty();

		NSArray* OrderedWindows = [NSApp orderedWindows];
		for (NSWindow* Window in OrderedWindows)
		{
			if ([Window isKindOfClass:[FCocoaWindow class]] && [Window isVisible] && ![Window hidesOnDeactivate])
			{
				SavedWindowsOrder.Add(FSavedWindowOrderInfo([Window windowNumber], Levels.Contains([Window windowNumber]) ? Levels[[Window windowNumber]] : [Window level]));
				[Window setLevel:NSNormalWindowLevel];
			}
		}
	}
}

void FMacApplication::HandleModifierChange(NSUInteger NewModifierFlags, NSUInteger FlagsShift, NSUInteger UE4Shift, EMacModifierKeys TranslatedCode)
{
	const bool CurrentPressed = (CurrentModifierFlags & FlagsShift) != 0;
	const bool NewPressed = (NewModifierFlags & FlagsShift) != 0;
	if (CurrentPressed != NewPressed)
	{
		if (NewPressed)
		{
			ModifierKeysFlags |= 1 << UE4Shift;
			MessageHandler->OnKeyDown(TranslatedCode, 0, false);
		}
		else
		{
			ModifierKeysFlags &= ~(1 << UE4Shift);
			MessageHandler->OnKeyUp(TranslatedCode, 0, false);
		}
	}
}

FCocoaWindow* FMacApplication::FindEventWindow(NSEvent* Event) const
{
	SCOPED_AUTORELEASE_POOL;

	FCocoaWindow* EventWindow = [[Event window] isKindOfClass:[FCocoaWindow class]] ? (FCocoaWindow*)[Event window] : nullptr;

	if ([Event type] != NSKeyDown && [Event type] != NSKeyUp)
	{
		NSInteger WindowNumber = [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0];
		NSWindow* WindowUnderCursor = [NSApp windowWithWindowNumber:WindowNumber];

		if ([Event type] == NSMouseMoved && WindowUnderCursor == nullptr)
		{
			// Ignore windows owned by other applications
			return nullptr;
		}
		else if (DraggedWindow)
		{
			EventWindow = DraggedWindow;
		}
		else if (WindowUnderCursor && [WindowUnderCursor isKindOfClass:[FCocoaWindow class]])
		{
			EventWindow = (FCocoaWindow*)WindowUnderCursor;
		}
	}

	return EventWindow;
}

NSScreen* FMacApplication::FindScreenByPoint(int32 X, int32 Y) const
{
	NSPoint Point = {0};
	Point.x = X;
	Point.y = FPlatformMisc::ConvertSlateYPositionToCocoa(Y);

	NSArray* AllScreens = [NSScreen screens];
	NSScreen* TargetScreen = [AllScreens objectAtIndex: 0];
	for(NSScreen* Screen in AllScreens)
	{
		if (Screen && NSPointInRect(Point, [Screen frame]))
		{
			TargetScreen = Screen;
			break;
		}
	}

	return TargetScreen;
}

bool FMacApplication::IsWindowMovable(TSharedRef<FMacWindow> Window, bool* OutMovableByBackground) const
{
	if (OutMovableByBackground)
	{
		*OutMovableByBackground = false;
	}

	const FVector2D CursorPos = ((FMacCursor*)Cursor.Get())->GetPosition();
	const int32 LocalMouseX = CursorPos.X - Window->PositionX;
	const int32 LocalMouseY = CursorPos.Y - Window->PositionY;

	const EWindowZone::Type Zone = MessageHandler->GetWindowZoneForPoint(Window, LocalMouseX, LocalMouseY);
	const bool IsMouseOverTitleBar = Zone == EWindowZone::TitleBar;
	switch (Zone)
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
			if (OutMovableByBackground)
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

bool FMacApplication::IsPrintableKey(uint32 Character) const
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

TCHAR FMacApplication::ConvertChar(TCHAR Character) const
{
	switch (Character)
	{
		case NSDeleteCharacter:
			return '\b';
		default:
			return Character;
	}
}

TCHAR FMacApplication::TranslateCharCode(TCHAR CharCode, uint32 KeyCode) const
{
	// Keys like F1-F12 or Enter do not need translation
	bool bNeedsTranslation = CharCode < NSOpenStepUnicodeReservedBase || CharCode > 0xF8FF;
	if (bNeedsTranslation)
	{
		// For non-numpad keys, the key code depends on the keyboard layout, so find out what was pressed by converting the key code to a Latin character
		TISInputSourceRef CurrentKeyboard = TISCopyCurrentKeyboardLayoutInputSource();
		if (CurrentKeyboard)
		{
			CFDataRef CurrentLayoutData = (CFDataRef)TISGetInputSourceProperty(CurrentKeyboard, kTISPropertyUnicodeKeyLayoutData);
			CFRelease(CurrentKeyboard);

			if (CurrentLayoutData)
			{
				const UCKeyboardLayout *KeyboardLayout = (UCKeyboardLayout*)CFDataGetBytePtr(CurrentLayoutData);
				if (KeyboardLayout)
				{
					UniChar Buffer[256] = { 0 };
					UniCharCount BufferLength = 256;
					uint32 DeadKeyState = 0;

					// To ensure we get a latin character, we pretend that command modifier key is pressed
					OSStatus Status = UCKeyTranslate(KeyboardLayout, KeyCode, kUCKeyActionDown, cmdKey >> 8, LMGetKbdType(), kUCKeyTranslateNoDeadKeysMask, &DeadKeyState, BufferLength, &BufferLength, Buffer);
					if (Status == noErr)
					{
						CharCode = Buffer[0];
					}
				}
			}
		}
	}
	else
	{
		// Private use range should not be returned
		CharCode = 0;
	}

	return CharCode;
}

void FMacApplication::CloseQueuedWindows()
{
	if (WindowsToClose.Num() > 0)
	{
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;

			// Set the new key window, if needed. We cannot trust Cocoa to set the key window to the actual top most window. It will prefer windows with title bars so,
			// for example, will choose the main window over a context menu window, when closing a submenu.
			NSArray* AllWindows = [NSApp orderedWindows];
			for (NSWindow* Window : AllWindows)
			{
				if ([Window isKindOfClass:[FCocoaWindow class]] && !WindowsToClose.Contains((FCocoaWindow*)Window) && [Window canBecomeKeyWindow])
				{
					if (Window != [NSApp keyWindow])
					{
						[Window makeKeyWindow];
					}
					break;
				}
			}

			for (FCocoaWindow* Window : WindowsToClose)
			{
				[Window close];
				[Window release];
			}
		}, UE4CloseEventMode, true);

		WindowsToClose.Empty();
	}
}

void FMacApplication::InvalidateTextLayouts()
{
	if (WindowsRequiringTextInvalidation.Num() > 0)
	{
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;

			for (FCocoaWindow* CocoaWindow : WindowsRequiringTextInvalidation)
			{
				if (CocoaWindow && [CocoaWindow openGLView])
				{
					FCocoaTextView* TextView = (FCocoaTextView*)[CocoaWindow openGLView];
					[[TextView inputContext] invalidateCharacterCoordinates];
				}
			}

		}, UE4IMEEventMode, true);

		WindowsRequiringTextInvalidation.Empty();
	}

}

#if WITH_EDITOR
void FMacApplication::RecordUsage(EGestureEvent::Type Gesture)
{
	if (LastGestureUsed != Gesture)
	{
		LastGestureUsed = Gesture;
		GestureUsage[Gesture] += 1;
	}
}
#endif

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