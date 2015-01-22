// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "GenericApplication.h"
#include "MacWindow.h"
#include "MacTextInputMethodSystem.h"

class FMacEvent;

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

	virtual void SetMessageHandler( const TSharedRef< class FGenericApplicationMessageHandler >& InMessageHandler ) override;

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual void PumpMessages( const float TimeDelta ) override;

	virtual TSharedRef< FGenericWindow > MakeWindow() override;

	virtual void InitializeWindow( const TSharedRef< FGenericWindow >& Window, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately ) override;

	virtual void SetCapture( const TSharedPtr< FGenericWindow >& InWindow ) override;

	virtual void* GetCapture( void ) const override;

	virtual void SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow ) override;

	virtual bool IsUsingHighPrecisionMouseMode() const override { return bUsingHighPrecisionMouseInput; }

	virtual bool IsUsingTrackpad() const override { return bUsingTrackpad; }

	virtual FModifierKeysState GetModifierKeys() const override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	virtual EWindowTitleAlignment::Type GetWindowTitleAlignment() const override
	{
		return EWindowTitleAlignment::Center;
	}

	virtual ITextInputMethodSystem *GetTextInputMethodSystem() override
	{
		return TextInputMethodSystem.Get();
	}

	void ProcessNSEvent(NSEvent* const Event);

	void OnWindowDraggingFinished();

	bool IsWindowMovable(FCocoaWindow* Win, bool* OutMovableByBackground);

	void ResetModifierKeys() { ModifierKeysFlags = 0; }

	TSharedPtr<FMacWindow> GetKeyWindow();

	uint32 GetModifierKeysFlags() { return ModifierKeysFlags; }

	void UseMouseCaptureWindow(bool bUseMouseCaptureWindow);

	bool IsProcessingNSEvent() const { return bIsProcessingNSEvent; }

	bool IsWorkspaceSessionActive() const { return bIsWorkspaceSessionActive; }

#if WITH_EDITOR
    virtual void SendAnalytics(IAnalyticsProvider* Provider) override;

	void SetIsUsingTrackpad(bool bInUsingTrackpad)
	{
		bUsingTrackpad = bInUsingTrackpad;
	}
#endif

	void SystemModalMode(bool const bInSystemModalMode);

public:

	void OnDragEnter(FCocoaWindow* Window, NSPasteboard* InPasteboard);
	void OnDragOver( FCocoaWindow* Window );
	void OnDragOut( FCocoaWindow* Window );
	void OnDragDrop( FCocoaWindow* Window );

	void OnWindowDidBecomeKey( FCocoaWindow* Window );
	void OnWindowDidResignKey( FCocoaWindow* Window );
	void OnWindowWillMove( FCocoaWindow* Window );
	void OnWindowDidMove( FCocoaWindow* Window );
	void OnWindowDidResize( FCocoaWindow* Window );
	void OnWindowRedrawContents( FCocoaWindow* Window );
	void OnWindowDidClose( FCocoaWindow* Window );
	bool OnWindowDestroyed( FCocoaWindow* Window );
	void OnWindowClose( FCocoaWindow* Window );

	void OnMouseCursorLock( bool bLockEnabled );

	static void ProcessEvent(FMacEvent const* const Event);

	const TArray<TSharedRef<FMacWindow>>& GetAllWindows() const { return Windows; }

private:
	enum FMacApplicationEventTypes
	{
		ResentEvent = 0x0f00
	};

	static NSEvent* HandleNSEvent(NSEvent* Event);
	static void OnDisplayReconfiguration(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags, void* UserInfo);

	FMacApplication();

	bool IsPrintableKey( uint32 Character );
	TCHAR ConvertChar( TCHAR Character );
	TCHAR TranslateCharCode( TCHAR CharCode, uint32 KeyCode );

	void ResendEvent( NSEvent* Event );
	FCocoaWindow* FindEventWindow( NSEvent* CocoaEvent );
	TSharedPtr<FMacWindow> LocateWindowUnderCursor( const NSPoint Position );

	NSScreen* FindScreenByPoint( int32 X, int32 Y ) const;

	void UpdateMouseCaptureWindow( FCocoaWindow* TargetWindow );

	void HandleModifierChange(NSUInteger NewModifierFlags, NSUInteger FlagsShift, NSUInteger UE4Shift, EMacModifierKeys TranslatedCode);

#if WITH_EDITOR
	void RecordUsage(EGestureEvent::Type Gesture);
#else
	void RecordUsage(EGestureEvent::Type Gesture) { }
#endif

private:

	bool bUsingHighPrecisionMouseInput;

	bool bUsingTrackpad;

	FVector2D HighPrecisionMousePos;

	EMouseButtons::Type LastPressedMouseButton;

	FCriticalSection WindowsMutex;
	TArray<TSharedRef<FMacWindow>> Windows;

	struct FSavedWindowOrderInfo
	{
		int32 WindowNumber;
		int32 Level;
		FSavedWindowOrderInfo(int32 InWindowNumber, int32 InLevel) : WindowNumber(InWindowNumber), Level(InLevel) {}
	};
	TArray<FSavedWindowOrderInfo> SavedWindowsOrder;

	TSharedRef<class HIDInputInterface> HIDInput;

	FCocoaWindow* DraggedWindow;

	FMouseCaptureWindow* MouseCaptureWindow;
	bool bIsMouseCaptureEnabled;
	bool bIsMouseCursorLocked;

	bool bSystemModalMode;
	bool bIsProcessingNSEvent;

	/** The current set of modifier keys that are pressed. This is used to detect differences between left and right modifier keys on key up events*/
	uint32 ModifierKeysFlags;

	/** The current set of Cocoa modifier flags, used to detect when Mission Control has been invoked & returned so that we can synthesis the modifier events it steals */
	NSUInteger CurrentModifierFlags;

	TArray< TSharedRef< FMacWindow > > KeyWindows;

	TSharedPtr<FMacTextInputMethodSystem> TextInputMethodSystem;

	bool bIsWorkspaceSessionActive;

	/** Notification center observer for application activation events */
	id AppActivationObserver;

	/** Notification center observer for application deactivation events */
	id AppDeactivationObserver;

	/** Notification center observer for workspace activation events */
	id WorkspaceActivationObserver;

	/** Notification center observer for workspace deactivation events */
	id WorkspaceDeactivationObserver;

#if WITH_EDITOR
	/** Holds the last gesture used to try and capture unique uses for gestures. */
	EGestureEvent::Type LastGestureUsed;

	/** Stores the number of times a gesture has been used for analytics */
	int32 GestureUsage[EGestureEvent::Count];
#endif

	id EventMonitor;

	friend class FMacWindow;
	friend class FMacEvent;
};

extern FMacApplication* MacApplication;
