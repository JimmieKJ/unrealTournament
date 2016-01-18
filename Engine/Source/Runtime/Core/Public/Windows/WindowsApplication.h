// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericApplication.h"

#include "AllowWindowsPlatformTypes.h"
	#include "Ole2.h"
	#include "OleIdl.h"
	#include <ShObjIdl.h>
#include "HideWindowsPlatformTypes.h"
#include "IInputInterface.h"
#include "IForceFeedbackSystem.h"
#include "WindowsTextInputMethodSystem.h"


DECLARE_LOG_CATEGORY_EXTERN(LogWindowsDesktop, Log, All);

class FWindowsWindow;
class FGenericApplicationMessageHandler;


namespace ETaskbarProgressState
{
	enum CORE_API Type
	{
		//Stops displaying progress and returns the button to its normal state.
		NoProgress = 0x0,

		//The progress indicator does not grow in size, but cycles repeatedly along the 
		//length of the task bar button. This indicates activity without specifying what 
		//proportion of the progress is complete. Progress is taking place, but there is 
		//no prediction as to how long the operation will take.
		Indeterminate = 0x1,

		//The progress indicator grows in size from left to right in proportion to the 
		//estimated amount of the operation completed. This is a determinate progress 
		//indicator; a prediction is being made as to the duration of the operation.
		Normal = 0x2,

		//The progress indicator turns red to show that an error has occurred in one of 
		//the windows that is broadcasting progress. This is a determinate state. If the 
		//progress indicator is in the indeterminate state, it switches to a red determinate 
		//display of a generic percentage not indicative of actual progress.
		Error = 0x4,

		//The progress indicator turns yellow to show that progress is currently stopped in 
		//one of the windows but can be resumed by the user. No error condition exists and 
		//nothing is preventing the progress from continuing. This is a determinate state. 
		//If the progress indicator is in the indeterminate state, it switches to a yellow 
		//determinate display of a generic percentage not indicative of actual progress.
		Paused = 0x8,
	};
}


/**
 * Allows access to task bar lists.
 *
 * This class can be used to change the appearance of a window's entry in the windows task bar,
 * such as setting an overlay icon or showing a progress indicator.
 */
class CORE_API FTaskbarList
{
public:

	/**
	 * Create and initialize a new task bar list.
	 *
	 * @return The new task bar list.
	 */
	static TSharedRef<FTaskbarList> Create();

	/**
	 * Sets the overlay icon of a task bar entry.
	 *
	 * @param NativeWindow The native window to change the overlay icon for.
	 * @param Icon The overlay icon to set.
	 * @param Description The overlay icon's description text.
	 */
	void SetOverlayIcon(const TSharedRef<FGenericWindow>& NativeWindow, HICON Icon, FText Description);

	/**
	 * Sets the progress state of a task bar entry.
	 *
	 * @param NativeWindow The native window to change the progress state for.
	 * @param State The new progress state.
	 */
	void SetProgressState(const TSharedRef<FGenericWindow>& NativeWindow, ETaskbarProgressState::Type State);

	/**
	 * Sets the progress value of a task bar entry.
	 *
	 * @param NativeWindow The native window to change the progress value for.
	 * @param Current The current progress value.
	 * @param Total The total progress value.
	 */
	void SetProgressValue(const TSharedRef<FGenericWindow>& NativeWindow, uint64 Current, uint64 Total);

	/** Destructor. */
	~FTaskbarList();

private:

	/** Hidden constructor (use FTaskbarList::Create). */
	FTaskbarList();

	/** Initializes the task bar list instance. */
	void Initialize();

private:

	/** Holds the internal task bar object. */
	ITaskbarList3* TaskBarList3;
};


struct FDeferredWindowsMessage
{
	FDeferredWindowsMessage( const TSharedPtr<FWindowsWindow>& InNativeWindow, HWND InHWnd, uint32 InMessage, WPARAM InWParam, LPARAM InLParam, int32 InX=0, int32 InY=0, uint32 InRawInputFlags = 0 )
		: NativeWindow( InNativeWindow )
		, hWND( InHWnd )
		, Message( InMessage )
		, wParam( InWParam )
		, lParam( InLParam )
		, X( InX )
		, Y( InY )
		, RawInputFlags( InRawInputFlags )
	{ }

	/** Native window that received the message */
	TWeakPtr<FWindowsWindow> NativeWindow;

	/** Window handle */
	HWND hWND;

	/** Message code */
	uint32 Message;

	/** Message data */
	WPARAM wParam;
	LPARAM lParam;

	/** Mouse coordinates */
	int32 X;
	int32 Y;
	uint32 RawInputFlags;
};


namespace EWindowsDragDropOperationType
{
	enum Type
	{
		DragEnter,
		DragOver,
		DragLeave,
		Drop
	};
}


struct FDragDropOLEData
{
	enum EWindowsOLEDataType
	{
		None,
		Text,
		Files
	};

	FDragDropOLEData()
		: Type(None)
	{ }

	FString OperationText;
	TArray<FString> OperationFilenames;
	EWindowsOLEDataType Type;
};


struct FDeferredWindowsDragDropOperation
{
private:

	// Private constructor. Use the factory functions below.
	FDeferredWindowsDragDropOperation()
		: HWnd(NULL)
		, KeyState(0)
	{
		CursorPosition.x = 0;
		CursorPosition.y = 0;
	}

public:

	static FDeferredWindowsDragDropOperation MakeDragEnter(HWND InHwnd, const FDragDropOLEData& InOLEData, ::DWORD InKeyState, POINTL InCursorPosition)
	{
		FDeferredWindowsDragDropOperation NewOperation;
		NewOperation.OperationType = EWindowsDragDropOperationType::DragEnter;
		NewOperation.HWnd = InHwnd;
		NewOperation.OLEData = InOLEData;
		NewOperation.KeyState = InKeyState;
		NewOperation.CursorPosition = InCursorPosition;
		return NewOperation;
	}

	static FDeferredWindowsDragDropOperation MakeDragOver(HWND InHwnd, ::DWORD InKeyState, POINTL InCursorPosition)
	{
		FDeferredWindowsDragDropOperation NewOperation;
		NewOperation.OperationType = EWindowsDragDropOperationType::DragOver;
		NewOperation.HWnd = InHwnd;
		NewOperation.KeyState = InKeyState;
		NewOperation.CursorPosition = InCursorPosition;
		return NewOperation;
	}

	static FDeferredWindowsDragDropOperation MakeDragLeave(HWND InHwnd)
	{
		FDeferredWindowsDragDropOperation NewOperation;
		NewOperation.OperationType = EWindowsDragDropOperationType::DragLeave;
		NewOperation.HWnd = InHwnd;
		return NewOperation;
	}

	static FDeferredWindowsDragDropOperation MakeDrop(HWND InHwnd, const FDragDropOLEData& InOLEData, ::DWORD InKeyState, POINTL InCursorPosition)
	{
		FDeferredWindowsDragDropOperation NewOperation;
		NewOperation.OperationType = EWindowsDragDropOperationType::Drop;
		NewOperation.HWnd = InHwnd;
		NewOperation.OLEData = InOLEData;
		NewOperation.KeyState = InKeyState;
		NewOperation.CursorPosition = InCursorPosition;
		return NewOperation;
	}

	EWindowsDragDropOperationType::Type OperationType;

	HWND HWnd;
	FDragDropOLEData OLEData;
	::DWORD KeyState;
	POINTL CursorPosition;
};


//disable warnings from overriding the deprecated force feedback.  
//calls to the deprecated function will still generate warnings.
PRAGMA_DISABLE_DEPRECATION_WARNINGS


/**
 * Interface for classes that handle Windows events.
 */
class IWindowsMessageHandler
{
public:

	/**
	 * Processes a Windows message.
	 *
	 * @param hwnd Handle to the window that received the message.
	 * @param msg The message.
	 * @param wParam Additional message information.
	 * @param lParam Additional message information.
	 * @param OutResult Will contain the result if the message was handled.
	 * @return true if the message was handled, false otherwise.
	 */
	virtual bool ProcessMessage(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam, int32& OutResult) = 0;
};


/**
 * Windows-specific application implementation.
 */
class CORE_API FWindowsApplication
	: public GenericApplication
	, public IForceFeedbackSystem
{
public:

	/**
	 * Static: Creates a new Win32 application
	 *
	 * @param InstanceHandle Win32 instance handle.
	 * @param IconHandle Win32 application icon handle.
	 * @return New application object.
	 */
	static FWindowsApplication* CreateWindowsApplication( const HINSTANCE InstanceHandle, const HICON IconHandle );

	/** Virtual destructor. */
	virtual ~FWindowsApplication();

public:	

	/** Called by a window when an OLE Drag and Drop operation occurred on a non-game thread */
	void DeferDragDropOperation( const FDeferredWindowsDragDropOperation& DeferredDragDropOperation );

	TSharedPtr<FTaskbarList> GetTaskbarList();

	/** Invoked by a window when an OLE Drag and Drop first enters it. */
	HRESULT OnOLEDragEnter( const HWND HWnd, const FDragDropOLEData& OLEData, ::DWORD KeyState, POINTL CursorPosition, ::DWORD *CursorEffect);

	/** Invoked by a window when an OLE Drag and Drop moves over the window. */
	HRESULT OnOLEDragOver( const HWND HWnd, ::DWORD KeyState, POINTL CursorPosition, ::DWORD *CursorEffect);

	/** Invoked by a window when an OLE Drag and Drop exits the window. */
	HRESULT OnOLEDragOut( const HWND HWnd );

	/** Invoked by a window when an OLE Drag and Drop is dropped onto the window. */
	HRESULT OnOLEDrop( const HWND HWnd, const FDragDropOLEData& OLEData, ::DWORD KeyState, POINTL CursorPosition, ::DWORD *CursorEffect);

	/**
	 * Adds a Windows message handler with the application instance.
	 *
	 * @param MessageHandler The message handler to register.
	 * @see RemoveMessageHandler
	 */
	virtual void AddMessageHandler(IWindowsMessageHandler& InMessageHandler);

	/**
	 * Removes a Windows message handler with the application instance.
	 *
	 * @param MessageHandler The message handler to register.
	 * @see AddMessageHandler
	 */
	virtual void RemoveMessageHandler(IWindowsMessageHandler& InMessageHandler);

public:

	// GenericApplication overrides

	virtual void SetMessageHandler( const TSharedRef< class FGenericApplicationMessageHandler >& InMessageHandler ) override;
	virtual void PollGameDeviceState( const float TimeDelta ) override;
	virtual void PumpMessages( const float TimeDelta ) override;
	virtual void ProcessDeferredEvents( const float TimeDelta ) override;
	virtual void Tick( const float TimeDelta ) override;
	virtual TSharedRef< FGenericWindow > MakeWindow() override;
	virtual void InitializeWindow( const TSharedRef< FGenericWindow >& Window, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately ) override;
	virtual void SetCapture( const TSharedPtr< FGenericWindow >& InWindow ) override;
	virtual void* GetCapture( void ) const override;
	virtual void SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow ) override;
	virtual bool IsUsingHighPrecisionMouseMode() const override { return bUsingHighPrecisionMouseInput; }
	virtual bool IsMouseAttached() const override { return bIsMouseAttached; }
	virtual FModifierKeysState GetModifierKeys() const override;
	virtual bool IsCursorDirectlyOverSlateWindow() const override;
	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;
	virtual bool TryCalculatePopupWindowPosition( const FPlatformRect& InAnchor, const FVector2D& InSize, const EPopUpOrientation::Type Orientation, /*OUT*/ FVector2D* const CalculatedPopUpPosition ) const override;
	virtual void GetInitialDisplayMetrics( FDisplayMetrics& OutDisplayMetrics ) const override;
	virtual EWindowTitleAlignment::Type GetWindowTitleAlignment() const override;
	virtual EWindowTransparency GetWindowTransparencySupport() const override;
	virtual void DestroyApplication() override;

	DEPRECATED(4.7, "Please use GetInputInterface()")
	virtual IForceFeedbackSystem* GetForceFeedbackSystem() override
	{
		return this; 
	}

	virtual IForceFeedbackSystem* DEPRECATED_GetForceFeedbackSystem() override
	{
		return this;
	}

	virtual IInputInterface* GetInputInterface() override
	{
		return this;
	}

	virtual ITextInputMethodSystem *GetTextInputMethodSystem() override
	{
		return TextInputMethodSystem.Get();
	}

	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice> InputDevice);

public:

	// IForceFeedbackSystem overrides

	virtual void SetForceFeedbackChannelValue (int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values) override;
	virtual void SetLightColor(int32 ControllerId, FColor Color) override { }

protected:

	/** Windows callback for message processing (forwards messages to the FWindowsApplication instance). */
	static LRESULT CALLBACK AppWndProc(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

	/** Processes a single Windows message. */
	int32 ProcessMessage( HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam );

	/** Processes a deferred Windows message. */
	int32 ProcessDeferredMessage( const FDeferredWindowsMessage& DeferredMessage );

	/** Processes deferred drag and drop operations. */
	void ProcessDeferredDragDropOperation(const FDeferredWindowsDragDropOperation& Op);

private:

	/** Hidden constructor. */
	FWindowsApplication( const HINSTANCE HInstance, const HICON IconHandle );

	/** Registers the Windows class for windows and assigns the application instance and icon */
	static bool RegisterClass( const HINSTANCE HInstance, const HICON HIcon );

	/**  @return  True if a windows message is related to user input (mouse, keyboard) */
	static bool IsInputMessage( uint32 msg );

	/** Defers a Windows message for later processing. */
	void DeferMessage( TSharedPtr<FWindowsWindow>& NativeWindow, HWND InHWnd, uint32 InMessage, WPARAM InWParam, LPARAM InLParam, int32 MouseX = 0, int32 MouseY = 0, uint32 RawInputFlags = 0 );

	/** Checks a key code for release of the Shift key. */
	void CheckForShiftUpEvents(const int32 KeyCode);

	/** Shuts down the application (called after an unrecoverable error occurred). */
	void ShutDownAfterError();

	/** Enables or disables Windows accessibility features, such as sticky keys. */
	void AllowAccessibilityShortcutKeys(const bool bAllowKeys);

	/** Queries and caches the number of connected mouse devices. */
	void QueryConnectedMice();

private:

	static const FIntPoint MinimizedWindowPosition;

	HINSTANCE InstanceHandle;

	bool bUsingHighPrecisionMouseInput;

	bool bIsMouseAttached;

	TArray<FDeferredWindowsMessage> DeferredMessages;

	TArray<FDeferredWindowsDragDropOperation> DeferredDragDropOperations;

	/** Registered Windows message handlers. */
	TArray<IWindowsMessageHandler*> MessageHandlers;

	TArray<TSharedRef<FWindowsWindow>> Windows;

	TSharedRef<class XInputInterface> XInput;

	/** List of input devices implemented in external modules. */
	TArray<TSharedPtr<class IInputDevice>> ExternalInputDevices;
	bool bHasLoadedInputPlugins;

	TArray<int32> PressedModifierKeys;

	FModifierKeysState CachedModifierKeyState;

	int32 bAllowedToDeferMessageProcessing;
	
	FAutoConsoleVariableRef CVarDeferMessageProcessing;
	
	/** True if we are in the middle of a windows modal size loop */
	bool bInModalSizeLoop;

	FDisplayMetrics InitialDisplayMetrics;

	TSharedPtr<FWindowsTextInputMethodSystem> TextInputMethodSystem;

	TSharedPtr<FTaskbarList> TaskbarList;

	// Accessibility shortcut keys
	STICKYKEYS							StartupStickyKeys;
	TOGGLEKEYS							StartupToggleKeys;
	FILTERKEYS							StartupFilterKeys;
};


PRAGMA_ENABLE_DEPRECATION_WARNINGS
