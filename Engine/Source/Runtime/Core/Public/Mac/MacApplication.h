// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "GenericApplication.h"
#include "MacWindow.h"
#include "MacTextInputMethodSystem.h"
#include "IInputInterface.h"

struct FDeferredMacEvent
{
	FDeferredMacEvent()
	:	Event(nullptr)
	,	Window(nullptr)
	,	Type(0)
	,	LocationInWindow(FVector2D::ZeroVector)
	,	ModifierFlags(0)
	,	Timestamp(0.0)
	,	WindowNumber(0)
	,	Context(nullptr)
	,	Delta(FVector2D::ZeroVector)
	,	ScrollingDelta(FVector2D::ZeroVector)
	,	ButtonNumber(0)
	,	ClickCount(0)
	,	Phase(NSEventPhaseNone)
	,	MomentumPhase(NSEventPhaseNone)
	,	IsDirectionInvertedFromDevice(false)
	,	Characters(nullptr)
	,	CharactersIgnoringModifiers(nullptr)
	,	IsRepeat(false)
	,	KeyCode(0)
	,	NotificationName(nullptr)
	,	DraggingPasteboard(nullptr)
	{
	}

	FDeferredMacEvent(const FDeferredMacEvent& Other)
	:	Event(Other.Event ? [Other.Event retain] : nullptr)
	,	Window(Other.Window)
	,	Type(Other.Type)
	,	LocationInWindow(Other.LocationInWindow)
	,	ModifierFlags(Other.ModifierFlags)
	,	Timestamp(Other.Timestamp)
	,	WindowNumber(Other.WindowNumber)
	,	Context(Other.Context ? [Other.Context retain] : nullptr)
	,	Delta(Other.Delta)
	,	ScrollingDelta(Other.ScrollingDelta)
	,	ButtonNumber(Other.ButtonNumber)
	,	ClickCount(Other.ClickCount)
	,	Phase(Other.Phase)
	,	MomentumPhase(Other.MomentumPhase)
	,	IsDirectionInvertedFromDevice(Other.IsDirectionInvertedFromDevice)
	,	Characters(Other.Characters ? [Other.Characters retain] : nullptr)
	,	CharactersIgnoringModifiers(Other.CharactersIgnoringModifiers ? [Other.CharactersIgnoringModifiers retain] : nullptr)
	,	IsRepeat(Other.IsRepeat)
	,	KeyCode(Other.KeyCode)
	,	NotificationName(Other.NotificationName ? [Other.NotificationName retain] : nullptr)
	,	DraggingPasteboard(Other.DraggingPasteboard ? [Other.DraggingPasteboard retain] : nullptr)
	{
	}

	~FDeferredMacEvent()
	{
		SCOPED_AUTORELEASE_POOL;
		if (Event)
		{
			[Event release];
		}
		if (Context)
		{
			[Context release];
		}
		if (Characters)
		{
			[Characters release];
		}
		if (CharactersIgnoringModifiers)
		{
			[CharactersIgnoringModifiers release];
		}
		if (NotificationName)
		{
			[NotificationName release];
		}
		if (DraggingPasteboard)
		{
			[DraggingPasteboard release];
		}
	}

	// Using NSEvent on the game thread is unsafe, so we copy of all its properties and use them when processing the event.
	// However, in some cases we need the original NSEvent (highlighting menus, resending unhandled key events), so we store it as well.
	NSEvent* Event;

	FCocoaWindow* Window;

	int32 Type;
	FVector2D LocationInWindow;
	uint32 ModifierFlags;
	NSTimeInterval Timestamp;
	int32 WindowNumber;
	NSGraphicsContext* Context;
	FVector2D Delta;
	FVector2D ScrollingDelta;
	int32 ButtonNumber;
	int32 ClickCount;
	NSEventPhase Phase;
	NSEventPhase MomentumPhase;
	bool IsDirectionInvertedFromDevice;
	NSString* Characters;
	NSString* CharactersIgnoringModifiers;
	bool IsRepeat;
	uint32 KeyCode;

	NSString* NotificationName;
	NSPasteboard* DraggingPasteboard;
};

/**
 * Mac-specific application implementation.
 */
class CORE_API FMacApplication : public GenericApplication
{
public:

	static FMacApplication* CreateMacApplication();

public:	

	~FMacApplication();

public:

	virtual void SetMessageHandler(const TSharedRef<class FGenericApplicationMessageHandler>& InMessageHandler) override;

	virtual void PollGameDeviceState(const float TimeDelta) override;

	virtual void PumpMessages(const float TimeDelta) override;

	virtual void ProcessDeferredEvents(const float TimeDelta) override;

	virtual TSharedRef<FGenericWindow> MakeWindow() override;

	virtual void InitializeWindow(const TSharedRef<FGenericWindow>& Window, const TSharedRef<FGenericWindowDefinition>& InDefinition, const TSharedPtr<FGenericWindow>& InParent, const bool bShowImmediately) override;

	virtual FModifierKeysState GetModifierKeys() const override;

	virtual bool IsCursorDirectlyOverSlateWindow() const override;

	virtual void SetHighPrecisionMouseMode(const bool Enable, const TSharedPtr<FGenericWindow>& InWindow) override;

	virtual bool IsUsingHighPrecisionMouseMode() const override { return bUsingHighPrecisionMouseInput; }

	virtual bool IsUsingTrackpad() const override { return bUsingTrackpad; }

	virtual FPlatformRect GetWorkArea(const FPlatformRect& CurrentWindow) const override;

	virtual EWindowTitleAlignment::Type GetWindowTitleAlignment() const override { return EWindowTitleAlignment::Center; }

	virtual EWindowTransparency GetWindowTransparencySupport() const override { return EWindowTransparency::PerWindow; }

	virtual ITextInputMethodSystem *GetTextInputMethodSystem() override { return TextInputMethodSystem.Get(); }

#if WITH_EDITOR
	virtual void SendAnalytics(IAnalyticsProvider* Provider) override;
#endif

public:

	void CloseWindow(TSharedRef<FMacWindow> Window);

	void DeferEvent(NSObject* Object);

	bool IsProcessingDeferredEvents() const { return bIsProcessingDeferredEvents; }

	TSharedPtr<FMacWindow> FindWindowByNSWindow(FCocoaWindow* WindowHandle);

	/** Queues a window for text layout invalidation when safe */
	void InvalidateTextLayout(FCocoaWindow* Window);

	void ResetModifierKeys() { ModifierKeysFlags = 0; }

	bool IsWorkspaceSessionActive() const { return bIsWorkspaceSessionActive; }

	void SystemModalMode(bool const bInSystemModalMode) { bSystemModalMode = bInSystemModalMode; }

	const TArray<TSharedRef<FMacWindow>>& GetAllWindows() const { return Windows; }

private:

	static NSEvent* HandleNSEvent(NSEvent* Event);
	static void OnDisplayReconfiguration(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags, void* UserInfo);
#if WITH_EDITOR
	static int32 MTContactCallback(void* Device, void* Data, int32 NumFingers, double TimeStamp, int32 Frame);
#endif

	FMacApplication();

	void ProcessEvent(const FDeferredMacEvent& Event);
	void ResendEvent(NSEvent* Event);

	void ProcessMouseMovedEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow);
	void ProcessMouseDownEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow);
	void ProcessMouseUpEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow);
	void ProcessScrollWheelEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow);
	void ProcessGestureEvent(const FDeferredMacEvent& Event);
	void ProcessKeyDownEvent(const FDeferredMacEvent& Event, TSharedPtr<FMacWindow> EventWindow);
	void ProcessKeyUpEvent(const FDeferredMacEvent& Event);

	void OnWindowDidMove(TSharedRef<FMacWindow> Window);
	void OnWindowDidResize(TSharedRef<FMacWindow> Window);
	bool OnWindowDestroyed(TSharedRef<FMacWindow> Window);

	void OnApplicationDidBecomeActive();
	void OnApplicationWillResignActive();
	void OnWindowsReordered(bool bIsAppInBackground);

	void HandleModifierChange(NSUInteger NewModifierFlags, NSUInteger FlagsShift, NSUInteger UE4Shift, EMacModifierKeys TranslatedCode);

	FCocoaWindow* FindEventWindow(NSEvent* CocoaEvent) const;
	NSScreen* FindScreenByPoint(int32 X, int32 Y) const;
	bool IsWindowMovable(TSharedRef<FMacWindow> Window, bool* OutMovableByBackground) const;
	bool IsPrintableKey(uint32 Character) const;
	TCHAR ConvertChar(TCHAR Character) const;
	TCHAR TranslateCharCode(TCHAR CharCode, uint32 KeyCode) const;

	void CloseQueuedWindows();

	/** Invalidates all queued windows requiring text layout changes */
	void InvalidateTextLayouts();

#if WITH_EDITOR
	void RecordUsage(EGestureEvent::Type Gesture);
#else
	void RecordUsage(EGestureEvent::Type Gesture) { }
#endif

private:

	bool bUsingHighPrecisionMouseInput;

	bool bUsingTrackpad;

	EMouseButtons::Type LastPressedMouseButton;

	FCriticalSection EventsMutex;
	TArray<FDeferredMacEvent> DeferredEvents;

	FCriticalSection WindowsMutex;
	TArray<TSharedRef<FMacWindow>> Windows;

	bool bIsProcessingDeferredEvents;

	struct FSavedWindowOrderInfo
	{
		int32 WindowNumber;
		int32 Level;
		FSavedWindowOrderInfo(int32 InWindowNumber, int32 InLevel) : WindowNumber(InWindowNumber), Level(InLevel) {}
	};
	TArray<FSavedWindowOrderInfo> SavedWindowsOrder;

	TSharedRef<class HIDInputInterface> HIDInput;

	/** List of input devices implemented in external modules. */
	TArray<TSharedPtr<class IInputDevice>> ExternalInputDevices;
	bool bHasLoadedInputPlugins;

	FCocoaWindow* DraggedWindow;

	bool bSystemModalMode;

	/** The current set of modifier keys that are pressed. This is used to detect differences between left and right modifier keys on key up events*/
	uint32 ModifierKeysFlags;

	/** The current set of Cocoa modifier flags, used to detect when Mission Control has been invoked & returned so that we can synthesis the modifier events it steals */
	NSUInteger CurrentModifierFlags;

	TArray<FCocoaWindow*> WindowsToClose;

	TArray<FCocoaWindow*> WindowsRequiringTextInvalidation;

	TSharedPtr<FMacTextInputMethodSystem> TextInputMethodSystem;

	bool bIsWorkspaceSessionActive;

	/** Notification center observers */
	id AppActivationObserver;
	id AppDeactivationObserver;
	id WorkspaceActivationObserver;
	id WorkspaceDeactivationObserver;

	id EventMonitor;
	id MouseMovedEventMonitor;

#if WITH_EDITOR
	/** Holds the last gesture used to try and capture unique uses for gestures. */
	EGestureEvent::Type LastGestureUsed;

	/** Stores the number of times a gesture has been used for analytics */
	int32 GestureUsage[EGestureEvent::Count];
#endif

	friend class FMacWindow;
};

extern FMacApplication* MacApplication;
