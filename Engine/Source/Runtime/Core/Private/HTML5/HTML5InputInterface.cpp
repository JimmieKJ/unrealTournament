// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "HTML5Cursor.h"
#include "HTML5InputInterface.h"
#include <SDL.h>

DECLARE_LOG_CATEGORY_EXTERN(LogHTML5, Log, All);
DEFINE_LOG_CATEGORY(LogHTML5);


#if PLATFORM_HTML5_BROWSER
#include "emscripten.h"
#include "html5.h"

static EControllerButtons::Type AxisMapping[4] =
{
	EControllerButtons::LeftAnalogX,
	EControllerButtons::LeftAnalogY, 
	EControllerButtons::RightAnalogX,
	EControllerButtons::RightAnalogY
};

// Axis Mapping, reversed or not. 
static int Reversed[4] =
{
	1,
	-1, 
	1,
	-1
};

// all are digital except Left and Right Trigger Analog. 
static EControllerButtons::Type  ButtonMapping[16] = 
{
	EControllerButtons::FaceButtonBottom,
	EControllerButtons::FaceButtonRight,
	EControllerButtons::FaceButtonLeft,
	EControllerButtons::FaceButtonTop,
	EControllerButtons::LeftShoulder,
	EControllerButtons::RightShoulder,
	EControllerButtons::LeftTriggerThreshold, 
	EControllerButtons::RightTriggerThreshold,
	EControllerButtons::SpecialLeft, 
	EControllerButtons::SpecialRight,
	EControllerButtons::LeftStickDown,
	EControllerButtons::RightStickDown,
	EControllerButtons::DPadUp,
	EControllerButtons::DPadDown,
	EControllerButtons::DPadLeft,
	EControllerButtons::DPadRight
};

#endif 


TSharedRef< FHTML5InputInterface > FHTML5InputInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor )
{
	return MakeShareable( new FHTML5InputInterface( InMessageHandler, InCursor ) );
}

FHTML5InputInterface::FHTML5InputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor )
	: MessageHandler( InMessageHandler )
	, Cursor( InCursor )
{
}

void FHTML5InputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

static TCHAR ConvertChar( SDL_Keysym Keysym )
{
	if( Keysym.sym >= 128 )
	{
		return 0;
	}

	TCHAR Char = Keysym.sym;

	if (Keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
	{
		// Convert to uppercase (FIXME: what about CAPS?)
		if( Keysym.sym >= 97 && Keysym.sym <= 122)
		{
			return Keysym.sym - 32;
		}
		else if( Keysym.sym >= 91 && Keysym.sym <= 93)
		{
			return Keysym.sym + 32; // [ \ ] -> { | }
		}
		else
		{
			switch(Keysym.sym)
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


static bool GeneratesKeyCharMessage(const SDL_KeyboardEvent & KeyDownEvent)
{
	bool bCmdKeyPressed = (KeyDownEvent.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
	const SDL_Keycode Sym = KeyDownEvent.keysym.sym;

	// filter out command keys, non-ASCI and arrow keycodes that don't generate WM_CHAR under Windows (TODO: find a table?)
	return !bCmdKeyPressed && Sym < 128 &&
		(Sym != SDLK_DOWN && Sym != SDLK_LEFT && Sym != SDLK_RIGHT && Sym != SDLK_UP && Sym != SDLK_DELETE);
}


void FHTML5InputInterface::Tick(float DeltaTime, const SDL_Event& Event,TSharedRef < FGenericWindow>& ApplicationWindow )
{

	switch (Event.type)
	{
		case SDL_KEYDOWN:
			{
				SDL_KeyboardEvent KeyEvent = Event.key;
				const SDL_Keycode KeyCode = KeyEvent.keysym.scancode;
				const bool bIsRepeated = KeyEvent.repeat != 0;

				// First KeyDown, then KeyChar. This is important, as in-game console ignores first character otherwise
				MessageHandler->OnKeyDown(KeyCode, KeyEvent.keysym.sym, bIsRepeated);

				if (GeneratesKeyCharMessage(KeyEvent))
				{
					const TCHAR Character = ConvertChar(KeyEvent.keysym);
					MessageHandler->OnKeyChar(Character, bIsRepeated);
				}
			}
			break;
		case SDL_KEYUP:
			{
				SDL_KeyboardEvent keyEvent = Event.key;
				const SDL_Keycode KeyCode = keyEvent.keysym.scancode;
				const TCHAR Character = ConvertChar( keyEvent.keysym );
				const bool IsRepeat = keyEvent.repeat != 0;

				MessageHandler->OnKeyUp( KeyCode, keyEvent.keysym.sym, IsRepeat );
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			{
				EMouseButtons::Type MouseButton = Event.button.button == 1 ? EMouseButtons::Left : Event.button.button == 2 ? EMouseButtons::Middle : EMouseButtons::Right;
				MessageHandler->OnMouseDown(ApplicationWindow, MouseButton );
			}
			break;
		case SDL_MOUSEBUTTONUP:
			{
				EMouseButtons::Type MouseButton = Event.button.button == 1 ? EMouseButtons::Left : Event.button.button == 2 ? EMouseButtons::Middle : EMouseButtons::Right;
				MessageHandler->OnMouseUp(MouseButton );
			}
			break; 
		case SDL_MOUSEMOTION:
			{
				Cursor->SetPosition(Event.motion.x, Event.motion.y);
				MessageHandler->OnRawMouseMove(Event.motion.xrel, Event.motion.yrel);
				MessageHandler->OnMouseMove(); 
			} 
			break;
		case SDL_MOUSEWHEEL:
			{
				SDL_MouseWheelEvent* w = (SDL_MouseWheelEvent*)&Event;
				const float SpinFactor = 1 / 120.0f;
				MessageHandler->OnMouseWheel(w->y * SpinFactor);
			}
			break;
		default:
			// unhandled. 
			break; 
	}
}

void FHTML5InputInterface::SendControllerEvents()
{
#if PLATFORM_HTML5_BROWSER
	// game pads can only be polled.
	static int PrevNumGamepads = 0;
	static bool GamepadSupported = true;

	const double CurrentTime = FPlatformTime::Seconds();

	if (GamepadSupported)
	{
		int NumGamepads = emscripten_get_num_gamepads();
		if (NumGamepads != PrevNumGamepads)
		{
			if (NumGamepads == EMSCRIPTEN_RESULT_NOT_SUPPORTED)
			{
				GamepadSupported = false;
				return;
			}
		}

		for(int CurrentGamePad = 0; CurrentGamePad < NumGamepads && CurrentGamePad < 5; ++CurrentGamePad) // max 5 game pads.
		{
			EmscriptenGamepadEvent GamePadEvent;
			int Failed = emscripten_get_gamepad_status(CurrentGamePad, &GamePadEvent);
			if (!Failed)
			{
				check(CurrentGamePad == GamePadEvent.index);
				for(int CurrentAxis = 0; CurrentAxis < GamePadEvent.numAxes; ++CurrentAxis)
				{
					if (GamePadEvent.axis[CurrentAxis] != PrevGamePadState[CurrentGamePad].axis[CurrentAxis])
					{

						MessageHandler->OnControllerAnalog(AxisMapping[CurrentAxis],CurrentGamePad, Reversed[CurrentAxis]*GamePadEvent.axis[CurrentAxis]);
					}
				}
				// edge trigger. 
				for(int CurrentButton = 0; CurrentButton < GamePadEvent.numButtons; ++CurrentButton)
				{
					// trigger for digital buttons. 
					if ( GamePadEvent.digitalButton[CurrentButton]   != PrevGamePadState[CurrentGamePad].digitalButton[CurrentButton] )
					{
						bool Triggered = GamePadEvent.digitalButton[CurrentButton] != 0;
						if ( Triggered )
						{
							MessageHandler->OnControllerButtonPressed(ButtonMapping[CurrentButton],CurrentGamePad, false); 
							LastPressedTime[CurrentGamePad][CurrentButton] = CurrentTime;
						}
						else 
						{
							MessageHandler->OnControllerButtonReleased(ButtonMapping[CurrentButton],CurrentGamePad, false); 
						}
					}
				}
				// repeat trigger. 
				const float RepeatDelta = 0.2f; 
				for(int CurrentButton = 0; CurrentButton < GamePadEvent.numButtons; ++CurrentButton)
				{
					if ( GamePadEvent.digitalButton[CurrentButton]  )
					{
						if (CurrentTime - LastPressedTime[CurrentGamePad][CurrentButton] > RepeatDelta) 
						{
							MessageHandler->OnControllerButtonPressed(ButtonMapping[CurrentButton],CurrentGamePad, true); 
							LastPressedTime[CurrentGamePad][CurrentButton] = CurrentTime; 
						}
					}
				}
				PrevGamePadState[CurrentGamePad] = GamePadEvent;
			}
		}
	}

#endif

}
