// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericApplication.h"
#include "LinuxWindow.h"

#define STEAM_CONTROLLER_SUPPORT				(WITH_STEAMWORKS && WITH_ENGINE && WITH_STEAMCONTROLLER)

typedef SDL_GameController* SDL_HController;

class FLinuxWindow;
class FGenericApplicationMessageHandler;


class FLinuxApplication : public GenericApplication
{

public:
	static FLinuxApplication* CreateLinuxApplication();

public:	
	virtual ~FLinuxApplication();
	
	virtual void DestroyApplication() override;

public:
	virtual void SetMessageHandler( const TSharedRef< class FGenericApplicationMessageHandler >& InMessageHandler ) override;

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual void PumpMessages( const float TimeDelta ) override;

	virtual void ProcessDeferredEvents( const float TimeDelta ) override;

	virtual TSharedRef< FGenericWindow > MakeWindow() override;

	virtual void InitializeWindow( const TSharedRef< FGenericWindow >& Window, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately ) override;

	virtual void SetCapture( const TSharedPtr< FGenericWindow >& InWindow ) override;

	virtual void* GetCapture( void ) const override;

	virtual void SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow ) override;

	virtual bool IsUsingHighPrecisionMouseMode() const override { return bUsingHighPrecisionMouseInput; }
	//	
	virtual FModifierKeysState GetModifierKeys() const override;
	//	
	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;
	//	X
	virtual bool TryCalculatePopupWindowPosition( const FPlatformRect& InAnchor, const FVector2D& InSize, const EPopUpOrientation::Type Orientation, /*OUT*/ FVector2D* const CalculatedPopUpPosition ) const override;

	void AddPendingEvent( SDL_Event event );

	void OnMouseCursorLock( bool bLockEnabled );
	
	void RemoveEventWindow(SDL_HWindow Window);

	EWindowZone::Type WindowHitTest( const TSharedPtr< FLinuxWindow > &window, int x, int y );
	TSharedPtr< FLinuxWindow > FindWindowBySDLWindow( SDL_Window *win );

private:
	FLinuxApplication();

	TCHAR ConvertChar( SDL_Keysym Keysym );

	TSharedPtr< FLinuxWindow > FindEventWindow( SDL_Event *pEvent );

	void UpdateMouseCaptureWindow( SDL_HWindow TargetWindow );

	void ProcessDeferredMessage( SDL_Event Event );

	/** 
	 * Determines whether this particular SDL_KEYDOWN event should also be routed to OnKeyChar()
	 *
	 * @param KeyDownEvent event in question
	 *
	 * @return true if character needs to be passed to OnKeyChar
	 */
	static bool GeneratesKeyCharMessage(const SDL_KeyboardEvent & KeyDownEvent);

	/**
	 * Tracks window activation changes and sends notifications as required
	 * 
	 * @param Window window that caused the activation event
	 * @param Event type of the activation change
	 */
	void TrackActivationChanges(const TSharedPtr<FLinuxWindow> Window, EWindowActivation::Type Event);

private:

	struct SDLControllerState
	{
		SDL_HController controller;

		// We need to remember if the "button" was previously pressed so we don't generate extra events
		bool analogOverThreshold[10];
	};

// 	static const FIntPoint MinimizedWindowPosition;

	bool bUsingHighPrecisionMouseInput;

	TArray< SDL_Event > PendingEvents;

	TArray< TSharedRef< FLinuxWindow > > Windows;

	int32 bAllowedToDeferMessageProcessing;

	bool bIsMouseCursorLocked;
	bool bIsMouseCaptureEnabled;

	TSharedPtr< FLinuxWindow > LastEventWindow;

	/** Window that we think has been activated last*/
	TSharedPtr< FLinuxWindow > CurrentlyActiveWindow;

	SDL_HWindow MouseCaptureWindow;

	SDLControllerState *ControllerStates;

	float fMouseWheelScrollAccel;

#if STEAM_CONTROLLER_SUPPORT
	TSharedPtr< class SteamControllerInterface > SteamInput;
#endif // STEAM_CONTROLLER_SUPPORT

	/** List of input devices implemented in external modules. */
	TArray< TSharedPtr<class IInputDevice> > ExternalInputDevices;

	/** Whether input plugins have been loaded */
	bool bHasLoadedInputPlugins;

};


extern FLinuxApplication* LinuxApplication;
