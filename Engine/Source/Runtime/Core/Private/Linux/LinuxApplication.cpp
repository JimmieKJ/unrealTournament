// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "LinuxApplication.h"
#include "LinuxWindow.h"
#include "LinuxCursor.h"
#include "GenericApplicationMessageHandler.h"
#include "IInputInterface.h"
#include "IInputDeviceModule.h"
#include "IInputDevice.h"

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

	SDLControllerState* ControllerState = LinuxApplication->ControllerStates;
	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_IsGameController(i))
		{
			ControllerState->controller = SDL_GameControllerOpen(i);
			if (ControllerState++->controller == nullptr)
			{
				UE_LOG(LogLoad, Warning, TEXT("Could not open gamecontroller %i: %s\n"), i, UTF8_TO_TCHAR(SDL_GetError()));
			}
		}
	}
	return LinuxApplication;
}


FLinuxApplication::FLinuxApplication() 
	:	GenericApplication( MakeShareable( new FLinuxCursor() ) )
	,	bIsMouseCursorLocked(false)
	,	bIsMouseCaptureEnabled(false)
	,	bHasLoadedInputPlugins(false)
	,	bInsideOwnWindow(false)
	,	bIsDragWindowButtonPressed(false)
	,	bActivateApp(false)
	,	bLockToCurrentMouseType(false)
	,	LastTimeCachedDisplays(-1.0)
	,	bEscapeKeyPressed(false)
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

	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
	Windows.Add(Window);

	// Add the windows into the focus stack.
	if (!Window->IsTooltipWindow() || Window->IsActivateWhenFirstShown())
	{
		RevertFocusStack.Add(Window);
	}

	// Add the window into the notification list if it is a notification window.
	if(Window->IsNotificationWindow())
	{
		NotificationWindows.Add(Window);
	}
}

void FLinuxApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
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

bool FLinuxApplication::IsCursorDirectlyOverSlateWindow() const
{
	return bInsideOwnWindow;
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

void FLinuxApplication::ProcessDeferredMessage( SDL_Event Event )
{
	// This function can be reentered when entering a modal tick loop.
	// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice
	SDL_HWindow NativeWindow = NULL;

	// get pointer to window that received this event
	bool bWindowlessEvent = false;
	TSharedPtr< FLinuxWindow > CurrentEventWindow = FindEventWindow(&Event, bWindowlessEvent);

	if (CurrentEventWindow.IsValid())
	{
		NativeWindow = CurrentEventWindow->GetHWnd();
	}
	if (!NativeWindow && !bWindowlessEvent)
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

			/*
				The variable bEscapeKeyPressed is used to figure out if the user tried to close a open 
				Popup Menu Window. We have to set this before OnKeyDown because Slate will destroy the 
				window before we can actually use that flag correctly in the RemoveRevertFocusWindow member 
				function.
			*/
			if (KeyCode == SDL_SCANCODE_ESCAPE)
			{
				bEscapeKeyPressed = true;
			}
			
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

			if (KeyCode == SDL_SCANCODE_ESCAPE)
			{
				bEscapeKeyPressed = false;
			}

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
			LinuxCursor->InvalidateCaches();

			if (LinuxCursor->IsHidden())
			{
				// Check if the mouse got locked for dragging in viewport.
				if (bLockToCurrentMouseType == false)
				{
					int width, height;
					SDL_GetWindowSize(NativeWindow, &width, &height);
					if (motionEvent.x != (width / 2) || motionEvent.y != (height / 2))
					{
						int xOffset, yOffset;
						SDL_GetWindowPosition(NativeWindow, &xOffset, &yOffset);
						LinuxCursor->SetPosition(width / 2 + xOffset, height / 2 + yOffset);
					}
					else
					{
						break;
					}
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
				
				if (buttonEvent.button == SDL_BUTTON_LEFT)
				{
					// Unlock the mouse dragging type.
					bLockToCurrentMouseType = false;

					bIsDragWindowButtonPressed = false;
				}
			}
			else
			{
				if (buttonEvent.button == SDL_BUTTON_LEFT)
				{
					// The user clicked an object and wants to drag maybe. We can use that to disable 
					// the resetting of the cursor. Before the user can drag objects, the pointer will change.
					// Usually it will be EMouseCursor::CardinalCross (Default added after IRC discussion how to fix selection in Front/Top/Side views). 
                    // If that happends and the user clicks the left mouse button, we know they want to move something.
					// TODO Is this always true? Need more checks.
					if (((FLinuxCursor*)Cursor.Get())->GetType() == EMouseCursor::CardinalCross || ((FLinuxCursor*)Cursor.Get())->GetType() == EMouseCursor::Default)
					{
						bLockToCurrentMouseType = true;
					}

					bIsDragWindowButtonPressed = true;
				}

				if (buttonEvent.clicks == 2)
				{
					MessageHandler->OnMouseDoubleClick(CurrentEventWindow, button);
				}
				else
				{
					if(CurrentlyActiveWindow != CurrentEventWindow)
					{
						SDL_RaiseWindow( CurrentEventWindow->GetHWnd() );
						SDL_SetWindowInputFocus( CurrentEventWindow->GetHWnd() );

						SDL_PushEvent(&Event);
					}
					else
					{
						MessageHandler->OnMouseDown(CurrentEventWindow, button);
					}
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
						// (re)cache native properties
						CurrentEventWindow->CacheNativeProperties();

						if (CurrentEventWindow->IsRegularWindow() && CurrentEventWindow->IsActivateWhenFirstShown())
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}
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

						int32 BorderSizeX, BorderSizeY;
						CurrentEventWindow->GetNativeBordersSize(BorderSizeX, BorderSizeY);
						ClientScreenX += BorderSizeX;
						ClientScreenY += BorderSizeY;

						MessageHandler->OnMovedWindow(CurrentEventWindow.ToSharedRef(), ClientScreenX, ClientScreenY);
					}
					break;

				case SDL_WINDOWEVENT_MAXIMIZED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Maximize);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Window: '%d' got maximized"), CurrentEventWindow->GetID());
					}
					break;

				case SDL_WINDOWEVENT_RESTORED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Restore);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Window: '%d' got restored"), CurrentEventWindow->GetID());
					}
					break;

				case SDL_WINDOWEVENT_ENTER:
					{
						if (CurrentEventWindow.IsValid())
						{
							CurrentEventWindow->OnPointerEnteredWindow(true);
							
							MessageHandler->OnCursorSet();

							bInsideOwnWindow = true;
							UE_LOG(LogLinuxWindow, Verbose, TEXT("Entered one of application windows"));
						}
					}
					break;

				case SDL_WINDOWEVENT_LEAVE:
					{
						if (CurrentEventWindow.IsValid())
						{
							CurrentEventWindow->OnPointerEnteredWindow(false);
							
							if (GetCapture() != nullptr)
							{
								UpdateMouseCaptureWindow((SDL_HWindow)GetCapture());
							}

							bInsideOwnWindow = false;
							UE_LOG(LogLinuxWindow, Verbose, TEXT("Left an application window we were hovering above."));
						}
					}
					break;

				case SDL_WINDOWEVENT_HIT_TEST:
					{
						if( CurrentEventWindow.IsValid())
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}

						// Raise notification windows if we have some.
						if(NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}
					}
					break;

				case SDL_WINDOWEVENT_TAKE_FOCUS:
					{
						// Check if we have an active (focused) window at all. If not we activate the applicaton.
						if (!CurrentlyActiveWindow.IsValid() && !bActivateApp)
						{
							MessageHandler->OnApplicationActivationChanged(true);
							UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATEAPP(TF), wParam = 1"));
							bActivateApp = true;
						}

						// Raise notification windows if we have some.
						if(NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_GAINED:
					{
						PreviousActiveWindow = CurrentlyActiveWindow;
						
						MessageHandler->OnWindowActivationChanged(CurrentEventWindow.ToSharedRef(), EWindowActivation::Activate);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATE(FG),    wParam = WA_ACTIVE       : %d"), CurrentEventWindow->GetID());
						CurrentlyActiveWindow = CurrentEventWindow;

						// Raise notification windows if we have some.
						if(NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_LOST:
					{
						// OK, the active window lost focus. This could mean the app went completely out of
						// focus. That means the app must be deactivated. To make sure that the user did
						// not click to another window we delay the deactivation.
						// TODO Figure out if the delay time may cause problems.
						if(CurrentlyActiveWindow == CurrentEventWindow)
						{
							// Only do if the application is active.
							if(bActivateApp)
							{
								SDL_Event event;
								event.type = SDL_USEREVENT;
								event.user.code = CheckForDeactivation;
								SDL_PushEvent(&event);
							}

							CurrentlyActiveWindow = nullptr;
						}
						MessageHandler->OnWindowActivationChanged(CurrentEventWindow.ToSharedRef(), EWindowActivation::Deactivate);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATE(FL),    wParam = WA_INACTIVE     : %d"), CurrentEventWindow->GetID());
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

	case SDL_DROPBEGIN:
		{
			check(DragAndDropQueue.Num() == 0);  // did we get confused?
			check(DragAndDropTextQueue.Num() == 0);  // did we get confused?
		}
		break;

	case SDL_DROPFILE:
		{
			FString tmp = StringUtility::UnescapeURI(UTF8_TO_TCHAR(Event.drop.file));
			DragAndDropQueue.Add(tmp);
			SDL_free(Event.drop.file);
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("File dropped: %s"), *tmp);
		}
		break;

	case SDL_DROPTEXT:
		{
			FString tmp = UTF8_TO_TCHAR(Event.drop.file);
			DragAndDropTextQueue.Add(tmp);
			SDL_free(Event.drop.file);
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Text dropped: %s"), *tmp);
		}
		break;

	case SDL_DROPCOMPLETE:
		{
			if (DragAndDropQueue.Num() > 0)
			{
				MessageHandler->OnDragEnterFiles(CurrentEventWindow.ToSharedRef(), DragAndDropQueue);
				MessageHandler->OnDragDrop(CurrentEventWindow.ToSharedRef());
				DragAndDropQueue.Empty();
			}

			if (DragAndDropTextQueue.Num() > 0)
			{
				for (const auto & Text : DragAndDropTextQueue)
				{
					MessageHandler->OnDragEnterText(CurrentEventWindow.ToSharedRef(), Text);
					MessageHandler->OnDragDrop(CurrentEventWindow.ToSharedRef());
				}
				DragAndDropTextQueue.Empty();
			}
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("DragAndDrop finished for Window              : %d"), CurrentEventWindow->GetID());
		}
		break;

	case SDL_USEREVENT:
		{
			if(Event.user.code == CheckForDeactivation)
			{
				// If we don't use bIsDragWindowButtonPressed the draged window will be destroyed because we
				// deactivate the whole appliacton. TODO Is that a bug? Do we have to do something?
				if (!CurrentlyActiveWindow.IsValid() && !bIsDragWindowButtonPressed)
				{
					MessageHandler->OnApplicationActivationChanged( false );
					UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATEAPP(UE), wParam = 0"));

					CurrentlyActiveWindow = nullptr;
					bActivateApp = false;
				}
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
				UE_LOG(LogInit, Log, TEXT("Adding external input plugin."));
				ExternalInputDevices.Add(Device);
			}
		}

		bHasLoadedInputPlugins = true;
	}
	
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

TSharedPtr< FLinuxWindow > FLinuxApplication::FindEventWindow(SDL_Event* Event, bool& bOutWindowlessEvent)
{
	uint16 WindowID = 0;
	bOutWindowlessEvent = false;

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
		case SDL_DROPBEGIN:
		case SDL_DROPFILE:
		case SDL_DROPTEXT:
		case SDL_DROPCOMPLETE:
			WindowID = Event->drop.windowID;
			break;

		default:
			bOutWindowlessEvent = true;
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
}

void FLinuxApplication::RefreshDisplayCache()
{
	const double kCacheLifetime = 5.0;	// ask once in 5 seconds
	
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastTimeCachedDisplays > kCacheLifetime)
	{
		CachedDisplays.Empty();

		int NumDisplays = SDL_GetNumVideoDisplays();

		for (int DisplayIdx = 0; DisplayIdx < NumDisplays; ++DisplayIdx)
		{
			SDL_Rect DisplayBounds;
			SDL_GetDisplayBounds(DisplayIdx, &DisplayBounds);
			
			CachedDisplays.Add(DisplayBounds);
		}

		LastTimeCachedDisplays = CurrentTime;
	}
}

FPlatformRect FLinuxApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	(const_cast<FLinuxApplication *>(this))->RefreshDisplayCache();

	// loop over all monitors to determine which one is the best
	int NumDisplays = CachedDisplays.Num();
	if (NumDisplays <= 0)
	{
		// fake something
		return CurrentWindow;
	}

	SDL_Rect BestDisplayBounds = CachedDisplays[0];

	// see if any other are better (i.e. cover top left)
	for (int DisplayIdx = 1; DisplayIdx < NumDisplays; ++DisplayIdx)
	{
		const SDL_Rect & DisplayBounds = CachedDisplays[DisplayIdx];

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
	SDL_Rect PrimaryBounds, PrimaryUsableBounds;
	SDL_GetDisplayBounds(0, &PrimaryBounds);
	SDL_GetDisplayUsableBounds(0, &PrimaryUsableBounds);

	Primary.Name = UTF8_TO_TCHAR(SDL_GetDisplayName(0));
	Primary.ID = TEXT("display0");
	Primary.NativeWidth = PrimaryBounds.w;
	Primary.NativeHeight = PrimaryBounds.h;
	Primary.bIsPrimary = true;
	OutDisplayMetrics.MonitorInfo.Add(Primary);

	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = PrimaryUsableBounds.x;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = PrimaryUsableBounds.y;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = PrimaryUsableBounds.x + PrimaryUsableBounds.w;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = PrimaryUsableBounds.y + PrimaryUsableBounds.h;

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

void FLinuxApplication::RemoveRevertFocusWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < RevertFocusStack.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = RevertFocusStack[ WindowIndex ];

		if (Window->GetHWnd() == HWnd)
		{
			RevertFocusStack.RemoveAt(WindowIndex);

			// Was the deleted window a Blueprint, Cascade, Matinee etc. window?
			if(Window->IsUtilityWindow() || Window->IsDialogWindow())
			{
				// OK, then raise its parent window.
				SDL_RaiseWindow( Window->GetParent()->GetHWnd() );
				SDL_SetWindowInputFocus( Window->GetParent()->GetHWnd() );
				
				// We reset this here because the user might have clicked the Escape key to close
				// a dialog window.
				bEscapeKeyPressed = false;
			}
			// Was the deleted window a top level window and we have still at least one other window in the stack?
			else if(Window->IsTopLevelWindow() && (RevertFocusStack.Num() > 0))
			{
				// OK, give focus to the one on top of the stack.
				TSharedPtr< FLinuxWindow > TopmostWindow = RevertFocusStack.Top();
				if (TopmostWindow.IsValid())
				{
					SDL_RaiseWindow( TopmostWindow->GetHWnd() );
					SDL_SetWindowInputFocus( TopmostWindow->GetHWnd() );

					// We reset this here because the user might have clicked the Escape key to close
					// a external window that runs the game.
					bEscapeKeyPressed = false;
				}
			}
			// Was it a popup menu?
			else if (Window->IsPopupMenuWindow() && bActivateApp )
			{
				// Did the user click an item and the popup menu got closed?
				if ( bIsDragWindowButtonPressed && Window->IsPointerInsideWindow() )
				{
					SDL_RaiseWindow(Window->GetParent()->GetHWnd() );
					SDL_SetWindowInputFocus(Window->GetParent()->GetHWnd() );

					// TODO If we have a popup menu and a sub popup menu open, we have
					// to fake the following. After getting destructed the condition above
					// 'Window->IsPointerInsideWindow()' will be false for the parent popup window
					// and the focus will be not reset. This is rather hackery and should be
					// removed later if possible.
					Window->GetParent()->OnPointerEnteredWindow(true);
				}
				// Did the user hit the Escape Key to close popup menu window with all its submenus?
				else if(bEscapeKeyPressed && bInsideOwnWindow)
				{
					/* 
						We have to revert back the focus to the previous submenu until we reach
						the one its parent is the top level window.
					*/
					SDL_RaiseWindow(Window->GetParent()->GetHWnd() );
 					SDL_SetWindowInputFocus(Window->GetParent()->GetHWnd() );

					// We reached to point where the parent of this destroyed popup menu window
					// is the top level window. Because we set already the focus we reset the
					// flag because all submenu of the popup menu window got closed.
					if(!Window->GetParent()->IsPopupMenuWindow())
					{
						bEscapeKeyPressed = false;
					}
				}
			}
			break;
		}
	}
}

void FLinuxApplication::RemoveNotificationWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < NotificationWindows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = NotificationWindows[ WindowIndex ];
		
		if ( Window->GetHWnd() == HWnd )
		{
			NotificationWindows.RemoveAt(WindowIndex);
			return;
		}
	}
}

void FLinuxApplication::RaiseNotificationWindows( const TSharedPtr< FLinuxWindow >& ParentWindow)
{
	// Raise notification window only for the correct parent window.
	// TODO Do we have to make this restriction?
	for (int32 WindowIndex=0; WindowIndex < NotificationWindows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > NotificationWindow = NotificationWindows[WindowIndex];
		if(ParentWindow == NotificationWindow->GetParent())
		{
			SDL_RaiseWindow(NotificationWindow->GetHWnd());
		}
	}
}
