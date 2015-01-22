// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "LinuxApplication.h"
#include "LinuxWindow.h"
#include "LinuxCursor.h"
#include "GenericApplicationMessageHandler.h"
#include "IInputInterface.h"
#include "IInputDeviceModule.h"
#include "IInputDevice.h"
#if STEAM_CONTROLLER_SUPPORT
	#include "SteamControllerInterface.h"
#endif // STEAM_CONTROLLER_SUPPORT

#if WITH_EDITOR
#include "ModuleManager.h"
#endif

//
// GameController thresholds
//
#define GAMECONTROLLER_LEFT_THUMB_DEADZONE  7849
#define GAMECONTROLLER_RIGHT_THUMB_DEADZONE 8689
#define GAMECONTROLLER_TRIGGER_THRESHOLD    30

float ShortToNormalFloat(short AxisVal)
{
	// normalize [-32768..32767] -> [-1..1]
	const float Norm = (AxisVal <= 0 ? 32768.f : 32767.f);
	return float(AxisVal) / Norm;
}

FLinuxApplication* LinuxApplication = NULL;

FLinuxApplication* FLinuxApplication::CreateLinuxApplication()
{
	if (!FPlatformMisc::PlatformInitMultimedia()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Fatal, TEXT("FLinuxApplication::CreateLinuxApplication() : PlatformInitMultimedia() failed, cannot create application instance."));
		// unreachable
		return nullptr;
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_EVENTS);
	check(InitializedSubsystems & SDL_INIT_JOYSTICK);
	check(InitializedSubsystems & SDL_INIT_GAMECONTROLLER);
#endif // DO_CHECK

	LinuxApplication = new FLinuxApplication();

	SDLControllerState *controllerState = LinuxApplication->ControllerStates;
	for (int i = 0; i < SDL_NumJoysticks(); ++i) {
	    if (SDL_IsGameController(i)) {
		    controllerState->controller = SDL_GameControllerOpen(i);
			if ( controllerState++->controller == NULL ) {
				UE_LOG(LogLoad, Warning, TEXT("Could not open gamecontroller %i: %s\n"), i, SDL_GetError() );
			}
		}
	}
	return LinuxApplication;
}


FLinuxApplication::FLinuxApplication() : GenericApplication( MakeShareable( new FLinuxCursor() ) )
#if STEAM_CONTROLLER_SUPPORT
	, SteamInput( SteamControllerInterface::Create(MessageHandler) )
#endif // STEAM_CONTROLLER_SUPPORT
	, bIsMouseCursorLocked(false)
	, bIsMouseCaptureEnabled(false)
	, bHasLoadedInputPlugins(false)
{
	bUsingHighPrecisionMouseInput = false;
	bAllowedToDeferMessageProcessing = true;
	MouseCaptureWindow = NULL;
	ControllerStates = new SDLControllerState[SDL_NumJoysticks()];
	memset( ControllerStates, 0, sizeof(SDLControllerState) * SDL_NumJoysticks() );

	fMouseWheelScrollAccel = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("X11.Tweaks"), TEXT( "MouseWheelScrollAcceleration" ), fMouseWheelScrollAccel, GEngineIni);
	}
}

FLinuxApplication::~FLinuxApplication()
{
	if ( GConfig )
	{
		GConfig->GetFloat(TEXT("X11.Tweaks"), TEXT("MouseWheelScrollAcceleration"), fMouseWheelScrollAccel, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}
	delete [] ControllerStates;
}

void FLinuxApplication::DestroyApplication()
{
	for (int i = 0; i < SDL_NumJoysticks(); ++i) {
		if(ControllerStates[i].controller != NULL)
		{
			SDL_GameControllerClose(ControllerStates[i].controller);
		}
	}
}

TSharedRef< FGenericWindow > FLinuxApplication::MakeWindow()
{
	return FLinuxWindow::Make();
}

void FLinuxApplication::InitializeWindow(	const TSharedRef< FGenericWindow >& InWindow,
											const TSharedRef< FGenericWindowDefinition >& InDefinition,
											const TSharedPtr< FGenericWindow >& InParent,
											const bool bShowImmediately )
{
	const TSharedRef< FLinuxWindow > Window = StaticCastSharedRef< FLinuxWindow >( InWindow );
	const TSharedPtr< FLinuxWindow > ParentWindow = StaticCastSharedPtr< FLinuxWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FLinuxApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
#if STEAM_CONTROLLER_SUPPORT
	SteamInput->SetMessageHandler(InMessageHandler);
#endif // STEAM_CONTROLLER_SUPPORT
}

namespace
{
	TSharedPtr< FLinuxWindow > FindWindowBySDLWindow(const TArray< TSharedRef< FLinuxWindow > >& WindowsToSearch, SDL_HWindow const WindowHandle)
	{
		for (int32 WindowIndex=0; WindowIndex < WindowsToSearch.Num(); ++WindowIndex)
		{
			TSharedRef< FLinuxWindow > Window = WindowsToSearch[WindowIndex];
			if (Window->GetHWnd() == WindowHandle)
			{
				return Window;
			}
		}

		return TSharedPtr< FLinuxWindow >(nullptr);
	}
}

TSharedPtr< FLinuxWindow > FLinuxApplication::FindWindowBySDLWindow(SDL_Window *win)
{
	return ::FindWindowBySDLWindow(Windows, win);
}

void FLinuxApplication::PumpMessages( const float TimeDelta )
{
	FPlatformMisc::PumpMessages( true );
}


void FLinuxApplication::AddPendingEvent( SDL_Event SDLEvent )
{
	if( GPumpingMessagesOutsideOfMainLoop && bAllowedToDeferMessageProcessing )
	{
		PendingEvents.Add( SDLEvent );
	}
	else
	{
		// When not deferring messages, process them immediately
		ProcessDeferredMessage( SDLEvent );
	}
}

bool FLinuxApplication::GeneratesKeyCharMessage(const SDL_KeyboardEvent & KeyDownEvent)
{
	bool bCmdKeyPressed = (KeyDownEvent.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
	const SDL_Keycode Sym = KeyDownEvent.keysym.sym;

	// filter out command keys, non-ASCI and arrow keycodes that don't generate WM_CHAR under Windows (TODO: find a table?)
	return !bCmdKeyPressed && Sym < 128 &&
		(Sym != SDLK_DOWN && Sym != SDLK_LEFT && Sym != SDLK_RIGHT && Sym != SDLK_UP && Sym != SDLK_DELETE);
}

void FLinuxApplication::TrackActivationChanges(const TSharedPtr<FLinuxWindow> Window, EWindowActivation::Type Event)
{
	bool bNeedToNotify = (Window != CurrentlyActiveWindow) || Event == EWindowActivation::Deactivate;
	
	UE_LOG(LogLinuxWindow, Verbose, TEXT("TrackActivationChanges: bNeedToNotify=%s (Window: %p, CurrentlyActiveWindow: %p, Event: %d)"),
		bNeedToNotify ? TEXT("true") : TEXT("false"),
		Window.Get(),
		CurrentlyActiveWindow.Get(),
		static_cast<int32>(Event));
	
	if (bNeedToNotify)
	{
		// see if previous window needs to be deactivated
		if (CurrentlyActiveWindow.IsValid())
		{
			UE_LOG(LogLinuxWindow, Verbose, TEXT("TrackActivationChanges: Deactivating previous window %p"), CurrentlyActiveWindow.Get());
			MessageHandler->OnWindowActivationChanged(CurrentlyActiveWindow.ToSharedRef(), EWindowActivation::Deactivate);
		}
			
		if (Event != EWindowActivation::Deactivate)
		{
			CurrentlyActiveWindow = Window;
			UE_LOG(LogLinuxWindow, Verbose, TEXT("TrackActivationChanges: New active window is %p, notifying it"), CurrentlyActiveWindow.Get());
			
			if (CurrentlyActiveWindow.IsValid())
			{
				MessageHandler->OnWindowActivationChanged(CurrentlyActiveWindow.ToSharedRef(), Event);
			}
		}
		else
		{
			CurrentlyActiveWindow = nullptr;
			UE_LOG(LogLinuxWindow, Verbose, TEXT("TrackActivationChanges: don't have any active window"));
		}
	}
}

void FLinuxApplication::ProcessDeferredMessage( SDL_Event Event )
{
	// This function can be reentered when entering a modal tick loop.
	// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice
	SDL_HWindow NativeWindow = NULL;

	// get pointer to window that received this event
	TSharedPtr< FLinuxWindow > CurrentEventWindow = FindEventWindow(&Event);

	if (CurrentEventWindow.IsValid())
	{
		LastEventWindow = CurrentEventWindow;
		NativeWindow = CurrentEventWindow->GetHWnd();
	}
	if (!NativeWindow)
	{
		return;
	}
	switch(Event.type)
	{
	case SDL_KEYDOWN:
		{
			SDL_KeyboardEvent KeyEvent = Event.key;
			const SDL_Keycode KeyCode = KeyEvent.keysym.scancode;
			const bool bIsRepeated = KeyEvent.repeat != 0;

			// Text input is now handled in SDL_TEXTINPUT: see below
			MessageHandler->OnKeyDown(KeyCode, KeyEvent.keysym.sym, bIsRepeated);

			// Backspace input in only caught here.
			if (KeyCode == SDL_SCANCODE_BACKSPACE || KeyCode == SDL_SCANCODE_RETURN)
			{
				const TCHAR Character = SDL_GetKeyFromScancode(Event.key.keysym.scancode);
				MessageHandler->OnKeyChar(Character, bIsRepeated);
			}
		}
		break;
	case SDL_KEYUP:
		{
			SDL_KeyboardEvent keyEvent = Event.key;
			const SDL_Keycode KeyCode = keyEvent.keysym.scancode;
			const bool IsRepeat = keyEvent.repeat != 0;

			MessageHandler->OnKeyUp( KeyCode, keyEvent.keysym.sym, IsRepeat );
		}
		break;
	case SDL_TEXTINPUT:
		{
			// Slate now gets all its text from here, I hope.
			// Don't know if this will work with ingame text or not.
			const bool bIsRepeated = Event.key.repeat != 0;
			const TCHAR Character = *ANSI_TO_TCHAR(Event.text.text);

			MessageHandler->OnKeyChar(Character, bIsRepeated);
		}
		break;
	case SDL_MOUSEMOTION:
		{
			SDL_MouseMotionEvent motionEvent = Event.motion;
			FLinuxCursor *LinuxCursor = (FLinuxCursor*)Cursor.Get();

			if(SDL_ShowCursor(-1) == 0)
			{
				int width, height;
				SDL_GetWindowSize( NativeWindow, &width, &height );
				if( motionEvent.x != (width / 2) || motionEvent.y != (height / 2) )
				{
					int xOffset, yOffset;
					SDL_GetWindowPosition( NativeWindow, &xOffset, &yOffset );
					LinuxCursor->SetPosition( width / 2 + xOffset, height / 2 + yOffset );
				}
				else
				{
					break;
				}
			}
			else
			{
				FVector2D CurrentPosition = LinuxCursor->GetPosition();
				if( LinuxCursor->UpdateCursorClipping( CurrentPosition ) )
				{
					LinuxCursor->SetPosition( CurrentPosition.X, CurrentPosition.Y );
				}
				if( !CurrentEventWindow->GetDefinition().HasOSWindowBorder )
				{
					if ( CurrentEventWindow->IsRegularWindow() )
					{
						int xOffset, yOffset;
						SDL_GetWindowPosition( NativeWindow, &xOffset, &yOffset );
						MessageHandler->GetWindowZoneForPoint( CurrentEventWindow.ToSharedRef(), CurrentPosition.X - xOffset, CurrentPosition.Y - yOffset );
						MessageHandler->OnCursorSet();
					}
				}
			}

			if(bUsingHighPrecisionMouseInput)
			{
					// maintain "shadow" global position
					if (LinuxCursor->IsHidden())
					{
						LinuxCursor->AddOffset(motionEvent.xrel, motionEvent.yrel);
					}
 					MessageHandler->OnRawMouseMove(motionEvent.xrel, motionEvent.yrel);
			}
			else
			{
				MessageHandler->OnMouseMove();
			}

		}
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		{
			SDL_MouseButtonEvent buttonEvent = Event.button;

			EMouseButtons::Type button;
			switch(buttonEvent.button)
			{
			case SDL_BUTTON_LEFT:
				button = EMouseButtons::Left;
				break;
			case SDL_BUTTON_MIDDLE:
				button = EMouseButtons::Middle;
				break;
			case SDL_BUTTON_RIGHT:
				button = EMouseButtons::Right;
				break;
			case SDL_BUTTON_X1:
				button = EMouseButtons::Thumb01;
				break;
			case SDL_BUTTON_X2:
				button = EMouseButtons::Thumb02;
				break;
			default:
				button = EMouseButtons::Invalid;
				break;
			}
			
			if (buttonEvent.type == SDL_MOUSEBUTTONUP)
			{
				MessageHandler->OnMouseUp(button);
			}
			else
			{
				if (buttonEvent.clicks == 2)
				{
					MessageHandler->OnMouseDoubleClick(CurrentEventWindow, button);
				}
				else
				{
					MessageHandler->OnMouseDown(CurrentEventWindow, button);
				}
			}
		}
		break;
	case SDL_MOUSEWHEEL:
		{
			SDL_MouseWheelEvent *WheelEvent = &Event.wheel;
			float Amount = WheelEvent->y * fMouseWheelScrollAccel;

			MessageHandler->OnMouseWheel(Amount);
		}
		break;
	case SDL_CONTROLLERAXISMOTION:
		{
			SDL_ControllerAxisEvent caxisEvent = Event.caxis;
			EControllerButtons::Type analog;
			float value = ShortToNormalFloat(caxisEvent.value);

			switch (caxisEvent.axis)
			{
			case SDL_CONTROLLER_AXIS_LEFTX:
				analog = EControllerButtons::LeftAnalogX;
				if(caxisEvent.value > GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[0])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::LeftStickRight, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[0] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[0])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::LeftStickRight, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[0] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[1])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::LeftStickLeft, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[1] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[1])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::LeftStickLeft, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[1] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_LEFTY:
				analog = EControllerButtons::LeftAnalogY;
				value *= -1;
				if(caxisEvent.value > GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[2])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::LeftStickDown, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[2] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[2])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::LeftStickDown, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[2] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[3])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::LeftStickUp, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[3] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[3])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::LeftStickUp, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[3] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_RIGHTX:
				analog = EControllerButtons::RightAnalogX;
				if(caxisEvent.value > GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[4])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::RightStickRight, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[4] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[4])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::RightStickRight, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[4] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[5])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::RightStickLeft, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[5] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[5])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::RightStickLeft, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[5] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_RIGHTY:
				analog = EControllerButtons::RightAnalogY;
				value *= -1;
				if(caxisEvent.value > GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[6])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::RightStickDown, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[6] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[6])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::RightStickDown, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[6] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[7])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::RightStickUp, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[7] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[7])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::RightStickUp, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[7] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
				analog = EControllerButtons::LeftTriggerAnalog;
				if(caxisEvent.value > GAMECONTROLLER_TRIGGER_THRESHOLD)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[8])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::LeftTriggerThreshold, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[8] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[8])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::LeftTriggerThreshold, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[8] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
				analog = EControllerButtons::LeftTriggerAnalog;
				if(caxisEvent.value > GAMECONTROLLER_TRIGGER_THRESHOLD)
				{
					if(!ControllerStates[caxisEvent.which].analogOverThreshold[9])
					{
						MessageHandler->OnControllerButtonPressed(EControllerButtons::RightTriggerThreshold, caxisEvent.which, false);
						ControllerStates[caxisEvent.which].analogOverThreshold[9] = true;
					}
				}
				else if(ControllerStates[caxisEvent.which].analogOverThreshold[9])
				{
					MessageHandler->OnControllerButtonReleased(EControllerButtons::RightTriggerThreshold, caxisEvent.which, false);
					ControllerStates[caxisEvent.which].analogOverThreshold[9] = false;
				}
				break;
			default:
				analog = EControllerButtons::Invalid;
				break;
			}

			MessageHandler->OnControllerAnalog(analog, caxisEvent.which, value);
		}
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		{
			SDL_ControllerButtonEvent cbuttonEvent = Event.cbutton;
			EControllerButtons::Type button;

			switch (cbuttonEvent.button)
			{
			case SDL_CONTROLLER_BUTTON_A:
				button = EControllerButtons::FaceButtonBottom;
				break;
			case SDL_CONTROLLER_BUTTON_B:
				button = EControllerButtons::FaceButtonRight;
				break;
			case SDL_CONTROLLER_BUTTON_X:
				button = EControllerButtons::FaceButtonLeft;
				break;
			case SDL_CONTROLLER_BUTTON_Y:
				button = EControllerButtons::FaceButtonTop;
				break;
			case SDL_CONTROLLER_BUTTON_BACK:
				button = EControllerButtons::SpecialLeft;
				break;
			case SDL_CONTROLLER_BUTTON_START:
				button = EControllerButtons::SpecialRight;
				break;
			case SDL_CONTROLLER_BUTTON_LEFTSTICK:
				button = EControllerButtons::LeftStickDown;
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
				button = EControllerButtons::RightStickDown;
				break;
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
				button = EControllerButtons::LeftShoulder;
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
				button = EControllerButtons::RightShoulder;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_UP:
				button = EControllerButtons::DPadUp;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
				button = EControllerButtons::DPadDown;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
				button = EControllerButtons::DPadLeft;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
				button = EControllerButtons::DPadRight;
				break;
			default:
				button = EControllerButtons::Invalid;
				break;
			}

			if(cbuttonEvent.type == SDL_CONTROLLERBUTTONDOWN)
			{
				MessageHandler->OnControllerButtonPressed(button, cbuttonEvent.which, false);
			}
			else
			{
				MessageHandler->OnControllerButtonReleased(button, cbuttonEvent.which, false);
			}
		}
		break;

	case SDL_WINDOWEVENT:
		{
			SDL_WindowEvent windowEvent = Event.window;

			switch (windowEvent.event)
			{
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					{
						int NewWidth  = windowEvent.data1;
						int NewHeight = windowEvent.data2;

						MessageHandler->OnSizeChanged(
							CurrentEventWindow.ToSharedRef(),
							NewWidth,
							NewHeight,
							//	bWasMinimized
							false
						);
					}
					break;

				case SDL_WINDOWEVENT_RESIZED:
					{
						int NewWidth  = windowEvent.data1;
						int NewHeight = windowEvent.data2;

						MessageHandler->OnSizeChanged(
							CurrentEventWindow.ToSharedRef(),
							NewWidth,
							NewHeight,
							//	bWasMinimized
							false
						);

						MessageHandler->OnResizingWindow( CurrentEventWindow.ToSharedRef() );
					}
					break;

				case SDL_WINDOWEVENT_CLOSE:
					{
						MessageHandler->OnWindowClose( CurrentEventWindow.ToSharedRef() );
					}
					break;

				case SDL_WINDOWEVENT_SHOWN:
					{
						int Width, Height;

						SDL_GetWindowSize(NativeWindow, &Width, &Height);
						
						MessageHandler->OnSizeChanged(
							CurrentEventWindow.ToSharedRef(),
							Width,
							Height,
							false
						);
					
					}
					break;

				case SDL_WINDOWEVENT_MOVED:
					{
						int32 ClientScreenX = windowEvent.data1;
						int32 ClientScreenY = windowEvent.data2;
						SDL_Rect Borders;
						if (SDL_GetWindowBordersSize(NativeWindow, &Borders) == 0)
						{
							ClientScreenX += Borders.x;
							ClientScreenY += Borders.y;
						}
						else
						{
							UE_LOG(LogLinuxWindow, Verbose, TEXT("Could not get Window border sizes!"));
						}
						MessageHandler->OnMovedWindow(CurrentEventWindow.ToSharedRef(), ClientScreenX, ClientScreenY);
					}
					break;

				case SDL_WINDOWEVENT_MAXIMIZED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Maximize);
					}
					break;

				case SDL_WINDOWEVENT_RESTORED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Restore);
					}
					break;

				case SDL_WINDOWEVENT_ENTER:
					{
						if (CurrentEventWindow.IsValid())
						{
							MessageHandler->OnCursorSet();
						}
					}
					break;

				case SDL_WINDOWEVENT_LEAVE:
					{
						if (CurrentEventWindow.IsValid() && GetCapture() != NULL)
						{
							UpdateMouseCaptureWindow((SDL_HWindow)GetCapture());
						}
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_GAINED:
					{
						if (CurrentEventWindow.IsValid())
						{
							TrackActivationChanges(CurrentEventWindow, EWindowActivation::Activate);                            
						}
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_LOST:
					{
						if (CurrentEventWindow.IsValid())
						{
							TrackActivationChanges(CurrentEventWindow, EWindowActivation::Deactivate);
						}
					}
					break;

				case SDL_WINDOWEVENT_HIDDEN:		// intended fall-through
				case SDL_WINDOWEVENT_EXPOSED:		// intended fall-through
				case SDL_WINDOWEVENT_MINIMIZED:		// intended fall-through
				default:
					break;
			}
		}
		break;
	}
}

EWindowZone::Type FLinuxApplication::WindowHitTest(const TSharedPtr< FLinuxWindow > &Window, int x, int y)
{
	return MessageHandler->GetWindowZoneForPoint(Window.ToSharedRef(), x, y);
}

void FLinuxApplication::ProcessDeferredEvents( const float TimeDelta )
{
	// This function can be reentered when entering a modal tick loop.
	// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice
	SDL_HWindow NativeWindow = NULL;

	TArray< SDL_Event > Events( PendingEvents );
	PendingEvents.Empty();

	for( int32 Index = 0; Index < Events.Num(); ++Index )
	{
		ProcessDeferredMessage( Events[Index] );
	}
}

void FLinuxApplication::PollGameDeviceState( const float TimeDelta )
{
	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			if (Device.IsValid())
			{
				ExternalInputDevices.Add(Device);
			}
		}

		bHasLoadedInputPlugins = true;
	}
	
#if STEAM_CONTROLLER_SUPPORT
	// Poll game device states and send new events
	SteamInput->SendControllerEvents();
#endif // STEAM_CONTROLLER_SUPPORT

	// Poll externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(TimeDelta);
		(*DeviceIt)->SendControllerEvents();
	}
}

TCHAR FLinuxApplication::ConvertChar( SDL_Keysym Keysym )
{
	if (SDL_GetKeyFromScancode(Keysym.scancode) >= 128)
	{
		return 0;
	}

	TCHAR Char = SDL_GetKeyFromScancode(Keysym.scancode);

    if (Keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
    {
        // Convert to uppercase (FIXME: what about CAPS?)
		if( SDL_GetKeyFromScancode(Keysym.scancode)  >= 97 && SDL_GetKeyFromScancode(Keysym.scancode)  <= 122)
        {
            return Keysym.sym - 32;
        }
		else if( SDL_GetKeyFromScancode(Keysym.scancode) >= 91 && SDL_GetKeyFromScancode(Keysym.scancode)  <= 93)
        {
			return SDL_GetKeyFromScancode(Keysym.scancode) + 32; // [ \ ] -> { | }
        }
        else
        {
			switch(SDL_GetKeyFromScancode(Keysym.scancode) )
            {
                case '`': // ` -> ~
                    Char = TEXT('`');
                    break;

                case '-': // - -> _
                    Char = TEXT('_');
                    break;

                case '=': // - -> _
                    Char = TEXT('+');
                    break;

                case ',':
                    Char = TEXT('<');
                    break;

                case '.':
                    Char = TEXT('>');
                    break;

                case ';':
                    Char = TEXT(':');
                    break;

                case '\'':
                    Char = TEXT('\"');
                    break;

                case '/':
                    Char = TEXT('?');
                    break;

                case '0':
                    Char = TEXT(')');
                    break;

                case '9':
                    Char = TEXT('(');
                    break;

                case '8':
                    Char = TEXT('*');
                    break;

                case '7':
                    Char = TEXT('&');
                    break;

                case '6':
                    Char = TEXT('^');
                    break;

                case '5':
                    Char = TEXT('%');
                    break;

                case '4':
                    Char = TEXT('$');
                    break;

                case '3':
                    Char = TEXT('#');
                    break;

                case '2':
                    Char = TEXT('@');
                    break;

                case '1':
                    Char = TEXT('!');
                    break;

                default:
                    break;
            }
        }
    }

    return Char;
}

TSharedPtr< FLinuxWindow > FLinuxApplication::FindEventWindow( SDL_Event* Event )
{
	uint16 WindowID = 0;
	switch (Event->type)
	{
		case SDL_TEXTINPUT:
			WindowID = Event->text.windowID;
			break;
		case SDL_TEXTEDITING:
			WindowID = Event->edit.windowID;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			WindowID = Event->key.windowID;
			break;
		case SDL_MOUSEMOTION:
			WindowID = Event->motion.windowID;
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			WindowID = Event->button.windowID;
			break;
		case SDL_MOUSEWHEEL:
			WindowID = Event->wheel.windowID;
			break;

		case SDL_WINDOWEVENT:
			WindowID = Event->window.windowID;
			break;
		default:
			return TSharedPtr< FLinuxWindow >(nullptr);
	}

	for (int32 WindowIndex=0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[WindowIndex];
		
		if (SDL_GetWindowID(Window->GetHWnd()) == WindowID)
		{
			return Window;
		}
	}

	return TSharedPtr< FLinuxWindow >(nullptr);
}

void FLinuxApplication::RemoveEventWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[ WindowIndex ];
		
		if ( Window->GetHWnd() == HWnd )
		{
			Windows.RemoveAt(WindowIndex);
			return;
		}
	}
}

FModifierKeysState FLinuxApplication::GetModifierKeys() const
{
	SDL_Keymod modifiers = SDL_GetModState();

	const bool bIsLeftShiftDown		= (modifiers & KMOD_LSHIFT) != 0;
	const bool bIsRightShiftDown	= (modifiers & KMOD_RSHIFT) != 0;
	const bool bIsLeftControlDown	= (modifiers & KMOD_LCTRL) != 0;
	const bool bIsRightControlDown	= (modifiers & KMOD_RCTRL) != 0;
	const bool bIsLeftAltDown		= (modifiers & KMOD_LALT) != 0;
	const bool bIsRightAltDown		= (modifiers & KMOD_RALT) != 0;
	const bool bAreCapsLocked		= (modifiers & KMOD_CAPS) != 0;

	return FModifierKeysState( bIsLeftShiftDown, bIsRightShiftDown, bIsLeftControlDown, bIsRightControlDown, bIsLeftAltDown, bIsRightAltDown, false, false, bAreCapsLocked );
}


void FLinuxApplication::SetCapture( const TSharedPtr< FGenericWindow >& InWindow )
{
	bIsMouseCaptureEnabled = InWindow.IsValid();
	UpdateMouseCaptureWindow( bIsMouseCaptureEnabled ? ((FLinuxWindow*)InWindow.Get())->GetHWnd() : NULL );
}


void* FLinuxApplication::GetCapture( void ) const
{
	return ( bIsMouseCaptureEnabled && MouseCaptureWindow ) ? MouseCaptureWindow : NULL;
}

void FLinuxApplication::UpdateMouseCaptureWindow(SDL_HWindow TargetWindow)
{
	const bool bEnable = bIsMouseCaptureEnabled || bIsMouseCursorLocked;
	FLinuxCursor *LinuxCursor = static_cast<FLinuxCursor*>(Cursor.Get());

	// this is a hacky heuristic which makes QA-ClickHUD work while not ruining SlateViewer...
	bool bShouldGrab = (IS_PROGRAM != 0 || GIsEditor) && !LinuxCursor->IsHidden();

	if (bEnable)
	{
		if (TargetWindow)
		{
			MouseCaptureWindow = TargetWindow;
		}
		if (bShouldGrab && MouseCaptureWindow)
		{
			SDL_CaptureMouse(SDL_TRUE);
		}
	}
	else
	{
		if (MouseCaptureWindow)
		{
			if (bShouldGrab)
			{
				SDL_CaptureMouse(SDL_FALSE);
			}
			MouseCaptureWindow = nullptr;
		}
	}
}

void FLinuxApplication::SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow )
{
	MessageHandler->OnCursorSet();
	bUsingHighPrecisionMouseInput = Enable;
	((FLinuxCursor*)Cursor.Get())->ResetOffset();
}


FPlatformRect FLinuxApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	// loop over all monitors to determine which one is the best
	int NumDisplays = SDL_GetNumVideoDisplays();
	if (NumDisplays <= 0)
	{
		// fake something
		return CurrentWindow;
	}
	
	SDL_Rect BestDisplayBounds;
	SDL_GetDisplayBounds(0, &BestDisplayBounds);
	
	// see if any other are better (i.e. cover top left)
	for (int DisplayIdx = 1; DisplayIdx < NumDisplays; ++DisplayIdx)
	{
		SDL_Rect DisplayBounds;
		SDL_GetDisplayBounds(DisplayIdx, &DisplayBounds);
		
		// only check top left corner for "bestness"
		if (DisplayBounds.x <= CurrentWindow.Left && DisplayBounds.x + DisplayBounds.w > CurrentWindow.Left &&
			DisplayBounds.y <= CurrentWindow.Top && DisplayBounds.y + DisplayBounds.h > CurrentWindow.Bottom)
		{
			BestDisplayBounds = DisplayBounds;
			// there can be only one, as we don't expect overlapping displays
			break;
		}
	}
	
	FPlatformRect WorkArea;
	WorkArea.Left	= BestDisplayBounds.x;
	WorkArea.Top	= BestDisplayBounds.y;
	WorkArea.Right	= BestDisplayBounds.x + BestDisplayBounds.w;
	WorkArea.Bottom	= BestDisplayBounds.y + BestDisplayBounds.h;

	return WorkArea;
}

void FLinuxApplication::OnMouseCursorLock( bool bLockEnabled )
{
	bIsMouseCursorLocked = bLockEnabled;
	UpdateMouseCaptureWindow( NULL );
}


bool FLinuxApplication::TryCalculatePopupWindowPosition( const FPlatformRect& InAnchor, const FVector2D& InSize, const EPopUpOrientation::Type Orientation, /*OUT*/ FVector2D* const CalculatedPopUpPosition ) const
{
	return false;
}

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	if (!FPlatformMisc::PlatformInitMultimedia()) //	will not initialize more than once
	{
		// consider making non-fatal and just returning bullshit? (which can be checked for and handled more gracefully)
		UE_LOG(LogInit, Fatal, TEXT("FDisplayMetrics::GetDisplayMetrics: PlatformInitMultimedia() failed, cannot get display metrics"));
		// unreachable
		return;
	}

	// loop over all monitors to determine which one is the best
	int NumDisplays = SDL_GetNumVideoDisplays();
	if (NumDisplays <= 0)
	{
		OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = 0;
		OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = 0;
		OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = 0;
		OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = 0;
		OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
		OutDisplayMetrics.PrimaryDisplayWidth = 0;
		OutDisplayMetrics.PrimaryDisplayHeight = 0;

		return;
	}
	
	OutDisplayMetrics.MonitorInfo.Empty();
	
	FMonitorInfo Primary;
	SDL_Rect PrimaryBounds;
	SDL_GetDisplayBounds(0, &PrimaryBounds);

	Primary.Name = UTF8_TO_TCHAR(SDL_GetDisplayName(0));
	Primary.ID = TEXT("display0");
	Primary.NativeWidth = PrimaryBounds.w;
	Primary.NativeHeight = PrimaryBounds.h;
	Primary.bIsPrimary = true;
	OutDisplayMetrics.MonitorInfo.Add(Primary);

	// @TODO [RCL] 2014-09-30 - try to account for real work area and not just display size.
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = PrimaryBounds.x;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = PrimaryBounds.y;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = PrimaryBounds.x + PrimaryBounds.w;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = PrimaryBounds.y + PrimaryBounds.h;

	OutDisplayMetrics.PrimaryDisplayWidth = PrimaryBounds.w;
 	OutDisplayMetrics.PrimaryDisplayHeight = PrimaryBounds.h;

	// accumulate the total bound rect
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
	for (int DisplayIdx = 1; DisplayIdx < NumDisplays; ++DisplayIdx)
	{
		SDL_Rect DisplayBounds;
		FMonitorInfo Display;
		SDL_GetDisplayBounds(DisplayIdx, &DisplayBounds);
		
		Display.Name = UTF8_TO_TCHAR(SDL_GetDisplayName(DisplayIdx));
		Display.ID = FString::Printf(TEXT("display%d"), DisplayIdx);
		Display.NativeWidth = DisplayBounds.w;
		Display.NativeHeight = DisplayBounds.h;
		Display.bIsPrimary = false;
		OutDisplayMetrics.MonitorInfo.Add(Display);
		
		OutDisplayMetrics.VirtualDisplayRect.Left = FMath::Min(DisplayBounds.x, OutDisplayMetrics.VirtualDisplayRect.Left);
		OutDisplayMetrics.VirtualDisplayRect.Right = FMath::Max(OutDisplayMetrics.VirtualDisplayRect.Right, DisplayBounds.x + DisplayBounds.w);
		OutDisplayMetrics.VirtualDisplayRect.Top = FMath::Min(DisplayBounds.y, OutDisplayMetrics.VirtualDisplayRect.Top);
		OutDisplayMetrics.VirtualDisplayRect.Bottom = FMath::Max(OutDisplayMetrics.VirtualDisplayRect.Bottom, DisplayBounds.y + DisplayBounds.h);
	}
}
