// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "WinRTInputInterface.h"

#include "AllowWinRTPlatformTypes.h"
#pragma pack (push,8)
	#include <xinput.h>
#pragma pack (pop)
#include "HideWinRTPlatformTypes.h"

const uint32 WinRTKeyCount = 256;
bool WinRTKeyState[WinRTKeyCount];
bool WinRTKeyLastState[WinRTKeyCount];
bool WinRTCharState[WinRTKeyCount];
bool WinRTCharLastState[WinRTKeyCount];

void appWinRTKeyEvent(uint32 InCode, bool bInWasPressed)
{
	if (InCode < WinRTKeyCount)
	{
		WinRTKeyState[InCode] = bInWasPressed;
	}
	else
	{
		// Gamepad inputs actually come in as key presses...
		//TEXT("appWinRTKeyEvent: Key code outside of currently set range: %d vs %d"), InCode, WinRTKeyCount);
	}
}

void appWinRTCharEvent(uint32 InCode, bool bInWasPressed)
{
	if (InCode < WinRTKeyCount)
	{
		WinRTCharState[InCode] = bInWasPressed;
	}
}

TSharedRef< FWinRTInputInterface > FWinRTInputInterface::Create( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	return MakeShareable( new FWinRTInputInterface( InMessageHandler ) );
}

FWinRTInputInterface::FWinRTInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
	, ControllerScanTime( 0 )
	, InitialButtonRepeatDelay( 0 )
	, ButtonRepeatDelay( 0 )
	, NextControllerToScan( 0 )
{
	XInputEnable(true);
	FMemory::Memzero(WinRTKeyState, WinRTKeyCount);
	FMemory::Memzero(WinRTKeyLastState, WinRTKeyCount);
	FMemory::Memzero(WinRTCharState, WinRTKeyCount);
	FMemory::Memzero(WinRTCharLastState, WinRTKeyCount);

	for ( int32 ControllerIndex=0; ControllerIndex < MAX_NUM_XINPUT_CONTROLLERS; ++ControllerIndex )
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];
		FMemory::Memzero( &ControllerState, sizeof(FControllerState) );

		ControllerState.ControllerId = ControllerIndex;
	}

	ControllerScanTime = 0.5f;
	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	// In the engine, all controllers map to xbox controllers for consistency 
	X360ToXboxControllerMapping[0] = 0;		// A
	X360ToXboxControllerMapping[1] = 1;		// B
	X360ToXboxControllerMapping[2] = 2;		// X
	X360ToXboxControllerMapping[3] = 3;		// Y
	X360ToXboxControllerMapping[4] = 4;		// L1
	X360ToXboxControllerMapping[5] = 5;		// R1
	X360ToXboxControllerMapping[6] = 7;		// Back 
	X360ToXboxControllerMapping[7] = 6;		// Start
	X360ToXboxControllerMapping[8] = 8;		// Left thumbstick
	X360ToXboxControllerMapping[9] = 9;		// Right thumbstick
	X360ToXboxControllerMapping[10] = 10;	// L2
	X360ToXboxControllerMapping[11] = 11;	// R2
	X360ToXboxControllerMapping[12] = 12;	// Dpad up
	X360ToXboxControllerMapping[13] = 13;	// Dpad down
	X360ToXboxControllerMapping[14] = 14;	// Dpad left
	X360ToXboxControllerMapping[15] = 15;	// Dpad right

	Buttons[0] = FGamepadKeyNames::FaceButtonBottom;
	Buttons[1] = FGamepadKeyNames::FaceButtonRight;
	Buttons[2] = FGamepadKeyNames::FaceButtonLeft;
	Buttons[3] = FGamepadKeyNames::FaceButtonTop;
	Buttons[4] = FGamepadKeyNames::LeftShoulder;
	Buttons[5] = FGamepadKeyNames::RightShoulder;
	Buttons[6] = FGamepadKeyNames::SpecialRight;
	Buttons[7] = FGamepadKeyNames::SpecialLeft;
	Buttons[8] = FGamepadKeyNames::LeftThumb;
	Buttons[9] = FGamepadKeyNames::RightThumb;
	Buttons[10] = FGamepadKeyNames::LeftTriggerThreshold;
	Buttons[11] = FGamepadKeyNames::RightTriggerThreshold;
	Buttons[12] = FGamepadKeyNames::DPadUp;
	Buttons[13] = FGamepadKeyNames::DPadDown;
	Buttons[14] = FGamepadKeyNames::DPadLeft;
	Buttons[15] = FGamepadKeyNames::DPadRight;
}

void FWinRTInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

void FWinRTInputInterface::Tick( float DeltaTime )
{
	ConditionalScanForControllerChanges( DeltaTime );
	ConditionalScanForKeyboardChanges(DeltaTime);
}

void FWinRTInputInterface::ConditionalScanForControllerChanges( float DeltaTime )
{
	// Occasionally check for new XInput controllers
	ControllerScanTime -= DeltaTime;
	if ( ControllerScanTime < 0.0f )
	{
		ControllerScanTime = 0.5f;
		for ( int32 ControllerIndex=0; ControllerIndex < MAX_NUM_XINPUT_CONTROLLERS; ++ControllerIndex )
		{
			FControllerState& ControllerState = ControllerStates[NextControllerToScan];
			NextControllerToScan = (NextControllerToScan + 1) % MAX_NUM_XINPUT_CONTROLLERS;
			if ( ControllerState.bIsConnected == false )
			{
				ControllerState.bCheckConnection = true;
				break;
			}
		}
	}
}

void FWinRTInputInterface::ConditionalScanForKeyboardChanges( float DeltaTime )
{
	bool bShiftIsDown = (WinRTKeyState[VK_LSHIFT] == true) | (WinRTKeyState[VK_RSHIFT] == true);
	FModifierKeysState ModifierKeyState(
		(WinRTKeyState[VK_LSHIFT] == true) ? true : false,
		(WinRTKeyState[VK_RSHIFT] == true) ? true : false,
		(WinRTKeyState[VK_LCONTROL] == true) ? true : false,
		(WinRTKeyState[VK_RCONTROL] == true) ? true : false,
		false,
		false,
		false,
		false
		);

	for (uint32 KeyIndex = 0; KeyIndex < WinRTKeyCount; KeyIndex++)
	{
		// Process key up/down
		if ((KeyIndex != VK_LSHIFT) && (KeyIndex != VK_RSHIFT) && 
			(KeyIndex != VK_LCONTROL) && (KeyIndex != VK_RCONTROL))
		{
			bool bKeyPressed = WinRTKeyState[KeyIndex];
			bool bKeyReleased = ((WinRTKeyState[KeyIndex] == false) && (WinRTKeyLastState[KeyIndex] == true));

			if ((bKeyPressed == true) || (bKeyReleased == true))
			{
				bool bIsRepeat = ((bKeyPressed == true) && (WinRTKeyLastState[KeyIndex] == true));
				
				// Get the character code from the virtual key pressed.  If 0, no translation from virtual key to character exists
				uint32 CharCode = WinRTMapVirtualKeyToCharacter(KeyIndex, bShiftIsDown);

				//@todo.WinRT: Put in a delay for registering repeats??
				if (bIsRepeat == false)
				{
					if (bKeyReleased == false)
					{
						MessageHandler->OnKeyDown( KeyIndex, CharCode, bIsRepeat );
					}
					else
					{
						MessageHandler->OnKeyUp( KeyIndex, CharCode, bIsRepeat );
					}
				}

				if (bKeyReleased == true)
				{
					uint32 CharKeyIndex = WinRTMapVirtualKeyToCharacter(KeyIndex, bShiftIsDown);
					// Mark the char as released...
					if (CharKeyIndex != 0)
					{
						WinRTCharState[CharKeyIndex] = false;
						WinRTCharLastState[CharKeyIndex] = false;
					}
				}
			}
		}

		// Process char
		bool bCharPressed = WinRTCharState[KeyIndex];
		if (bCharPressed == true)
		{
			bool bIsRepeat = WinRTCharLastState[KeyIndex];
			//@todo.WinRT: Put in a delay for registering repeats??
			if (bIsRepeat == false)
			{
				MessageHandler->OnKeyChar( KeyIndex, bIsRepeat );
			}
		}
		
		WinRTKeyLastState[KeyIndex] = WinRTKeyState[KeyIndex];
		WinRTCharLastState[KeyIndex] = WinRTCharState[KeyIndex];
	}
}

/**
 *	Convert the given keycode to the character it represents
 */
uint32 FWinRTInputInterface::WinRTMapVirtualKeyToCharacter(uint32 InVirtualKey, bool bShiftIsDown)
{
	uint32 TranslatedKeyCode = (uint32)InVirtualKey;

#define WINRT_SHIFT_KEY_MAPPER(x, y)			((bShiftIsDown == false) ? x : y)
	
	switch (InVirtualKey)
	{
	case VK_LBUTTON:
	case VK_RBUTTON:
	case VK_MBUTTON:
	case VK_XBUTTON1:
	case VK_XBUTTON2:
	case VK_BACK:
	case VK_TAB:
	case VK_RETURN:
	case VK_PAUSE:
	case VK_CAPITAL:
	case VK_ESCAPE:
	case VK_END:
	case VK_HOME:
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_INSERT:
	case VK_DELETE:
		TranslatedKeyCode = 0;
		break;

	case VK_SPACE:	TranslatedKeyCode = TEXT(' ');										break;
	// VK_0 - VK_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
	// 0x40 : unassigned
	// VK_A - VK_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
	case 0x41:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('a'), TEXT('A'));	break;
	case 0x42:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('b'), TEXT('B'));	break;
	case 0x43:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('c'), TEXT('C'));	break;
	case 0x44:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('d'), TEXT('D'));	break;
	case 0x45:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('e'), TEXT('E'));	break;
	case 0x46:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('f'), TEXT('F'));	break;
	case 0x47:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('g'), TEXT('G'));	break;
	case 0x48:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('h'), TEXT('H'));	break;
	case 0x49:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('i'), TEXT('I'));	break;
	case 0x4A:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('j'), TEXT('J'));	break;
	case 0x4B:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('k'), TEXT('K'));	break;
	case 0x4C:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('l'), TEXT('L'));	break;
	case 0x4D:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('m'), TEXT('M'));	break;
	case 0x4E:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('n'), TEXT('N'));	break;
	case 0x4F:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('o'), TEXT('O'));	break;
	case 0x50:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('p'), TEXT('P'));	break;
	case 0x51:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('q'), TEXT('Q'));	break;
	case 0x52:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('r'), TEXT('R'));	break;
	case 0x53:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('s'), TEXT('S'));	break;
	case 0x54:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('t'), TEXT('T'));	break;
	case 0x55:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('u'), TEXT('U'));	break;
	case 0x56:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('v'), TEXT('V'));	break;
	case 0x57:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('w'), TEXT('W'));	break;
	case 0x58:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('x'), TEXT('X'));	break;
	case 0x59:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('y'), TEXT('Y'));	break;
	case 0x5A:		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('z'), TEXT('Z'));	break;

	case 0x30:
	case VK_NUMPAD0:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('0'), TEXT(')'));
		break;
	case 0x31:
	case VK_NUMPAD1:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('1'), TEXT('!'));
		break;
	case 0x32:
	case VK_NUMPAD2:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('2'), TEXT('@'));
		break;
	case 0x33:
	case VK_NUMPAD3:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('3'), TEXT('#'));
		break;
	case 0x34:
	case VK_NUMPAD4:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('4'), TEXT('$'));
		break;
	case 0x35:
	case VK_NUMPAD5:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('5'), TEXT('%'));
		break;
	case 0x36:
	case VK_NUMPAD6:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('6'), TEXT('^'));
		break;
	case 0x37:
	case VK_NUMPAD7:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('7'), TEXT('&'));
		break;
	case 0x38:
	case VK_NUMPAD8:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('8'), TEXT('*'));
		break;
	case 0x39:
	case VK_NUMPAD9:
		TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('9'), TEXT('('));
		break;

	case VK_MULTIPLY:	TranslatedKeyCode = TEXT('*');										break;
	case VK_ADD:		TranslatedKeyCode = TEXT('+');										break;
	case VK_SUBTRACT:	TranslatedKeyCode = TEXT('-');										break;
	case VK_DECIMAL:	TranslatedKeyCode = TEXT('.');										break;
	case VK_DIVIDE:		TranslatedKeyCode = TEXT('/');										break;

	case VK_F1:
	case VK_F2:
	case VK_F3:
	case VK_F4:
	case VK_F5:
	case VK_F6:
	case VK_F7:
	case VK_F8:
	case VK_F9:
	case VK_F10:
	case VK_F11:
	case VK_F12:
	case VK_NUMLOCK:
	case VK_SCROLL:
	case VK_LSHIFT:
	case VK_RSHIFT:
	case VK_LCONTROL:
	case VK_RCONTROL:
	case VK_LMENU:
	case VK_RMENU:
		TranslatedKeyCode = 0;
		break;

	case 0xbb:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('='), TEXT('+'));			break;
	case 0xbc:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT(','), TEXT('<'));			break;
	case 0xbd:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('-'), TEXT('_'));			break;
	case 0xbe:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('.'), TEXT('>'));			break;
	case 0xbf:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('/'), TEXT('?'));			break;
	case 0xc0:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('`'), TEXT('~'));			break;
	case 0xdb:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('['), TEXT('{'));			break;
	case 0xdc:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT('\\'), TEXT('|'));			break;
	case 0xdd:	TranslatedKeyCode = WINRT_SHIFT_KEY_MAPPER(TEXT(']'), TEXT('}'));			break;
	}
	return TranslatedKeyCode;
}

void FWinRTInputInterface::SendControllerEvents()
{
	for ( int32 ControllerIndex=0; ControllerIndex < MAX_NUM_XINPUT_CONTROLLERS; ++ControllerIndex )
	{
		XINPUT_STATE XInputState;
		FMemory::Memzero( &XInputState, sizeof(XINPUT_STATE) );

		FControllerState& ControllerState = ControllerStates[ControllerIndex];
		if ( ControllerState.bIsConnected || ControllerState.bCheckConnection )
		{
			bool bWasConnected = ControllerState.bIsConnected;
			ControllerState.bIsConnected = ( XInputGetState( ControllerIndex, &XInputState ) == ERROR_SUCCESS ) ? true : false;
			bool bConnectionChanged = ControllerState.bIsConnected != bWasConnected;

			// send message
			//OutControllerEvents.AddItem( )
		}

		if( ControllerState.bIsConnected )
		{
			bool CurrentStates[MAX_NUM_CONTROLLER_BUTTONS] = {0};
		
			// Get the current state of all buttons
			CurrentStates[X360ToXboxControllerMapping[0]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_A);
			CurrentStates[X360ToXboxControllerMapping[1]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_B);
			CurrentStates[X360ToXboxControllerMapping[2]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_X);
			CurrentStates[X360ToXboxControllerMapping[3]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
			CurrentStates[X360ToXboxControllerMapping[4]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			CurrentStates[X360ToXboxControllerMapping[5]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
			CurrentStates[X360ToXboxControllerMapping[6]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
			CurrentStates[X360ToXboxControllerMapping[7]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_START);
			CurrentStates[X360ToXboxControllerMapping[8]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
			CurrentStates[X360ToXboxControllerMapping[9]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
			CurrentStates[X360ToXboxControllerMapping[10]] = !!(XInputState.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
			CurrentStates[X360ToXboxControllerMapping[11]] = !!(XInputState.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
			CurrentStates[X360ToXboxControllerMapping[12]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
			CurrentStates[X360ToXboxControllerMapping[13]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			CurrentStates[X360ToXboxControllerMapping[14]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			CurrentStates[X360ToXboxControllerMapping[15]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

			// Check Analog state

			// Axis, convert range -32768..+32767 set up to +/- 1.
			if( ControllerState.LeftXAnalog != XInputState.Gamepad.sThumbLX )
			{
				MessageHandler->OnControllerAnalog( FGamepadKeyNames::LeftAnalogX, ControllerState.ControllerId, XInputState.Gamepad.sThumbLX / 32768.f );
				ControllerState.LeftXAnalog = XInputState.Gamepad.sThumbLX;
			}

			if( ControllerState.LeftYAnalog != XInputState.Gamepad.sThumbLY )
			{
				MessageHandler->OnControllerAnalog( FGamepadKeyNames::LeftAnalogY, ControllerState.ControllerId, XInputState.Gamepad.sThumbLY / 32768.f );
				ControllerState.LeftYAnalog = XInputState.Gamepad.sThumbLY;
			}

			if( ControllerState.RightXAnalog != XInputState.Gamepad.sThumbRX )
			{
				MessageHandler->OnControllerAnalog( FGamepadKeyNames::RightAnalogX, ControllerState.ControllerId, XInputState.Gamepad.sThumbRX / 32768.f );
				ControllerState.RightXAnalog = XInputState.Gamepad.sThumbRX;
			}

			if( ControllerState.RightYAnalog != XInputState.Gamepad.sThumbRY )
			{
				MessageHandler->OnControllerAnalog( FGamepadKeyNames::RightAnalogY, ControllerState.ControllerId, XInputState.Gamepad.sThumbRY / 32768.f );
				ControllerState.RightYAnalog = XInputState.Gamepad.sThumbRY;
			}

			if( ControllerState.LeftTriggerAnalog != XInputState.Gamepad.bLeftTrigger )
			{
				MessageHandler->OnControllerAnalog( FGamepadKeyNames::LeftTriggerAnalog, ControllerState.ControllerId, XInputState.Gamepad.bLeftTrigger / 255.f );
				ControllerState.LeftTriggerAnalog = XInputState.Gamepad.bLeftTrigger;
			}

			if( ControllerState.RightTriggerAnalog != XInputState.Gamepad.bRightTrigger )
			{
				MessageHandler->OnControllerAnalog( FGamepadKeyNames::RightTriggerAnalog, ControllerState.ControllerId, XInputState.Gamepad.bRightTrigger / 255.f );
				ControllerState.RightTriggerAnalog = XInputState.Gamepad.bRightTrigger;
			}

			const double CurrentTime = FPlatformTime::Seconds();

			// For each button check against the previous state and send the correct message if any
			for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
			{
				if (CurrentStates[ButtonIndex] != ControllerState.ButtonStates[ButtonIndex])
				{
					if( CurrentStates[ButtonIndex] )
					{
						MessageHandler->OnControllerButtonPressed( Buttons[ButtonIndex], ControllerState.ControllerId, false );
					}
					else
					{
						MessageHandler->OnControllerButtonReleased( Buttons[ButtonIndex], ControllerState.ControllerId, false );
					}

					if ( CurrentStates[ButtonIndex] != 0 )
					{
						// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
					}
				}
				else if ( CurrentStates[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime )
				{
					MessageHandler->OnControllerButtonPressed( Buttons[ButtonIndex], ControllerState.ControllerId, true );

					// set the button's NextRepeatTime to the ButtonRepeatDelay
					ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}

				// Update the state for next time
				ControllerState.ButtonStates[ButtonIndex] = CurrentStates[ButtonIndex];
			}	
		}
	}
}