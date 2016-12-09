// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Framework/Application/NavigationConfig.h"
#include "Types/SlateEnums.h"
#include "Input/Events.h"

FNavigationConfig::FNavigationConfig()
{
	// left
	Left.Emplace( EKeys::Left );
	Left.Emplace( EKeys::Gamepad_DPad_Left );
	Left.Emplace( EKeys::Gamepad_LeftStick_Left );
		
	// right
	Right.Emplace( EKeys::Right );
	Right.Emplace( EKeys::Gamepad_DPad_Right );
	Right.Emplace( EKeys::Gamepad_LeftStick_Right  );

	// up
	Up.Emplace( EKeys::Up );
	Up.Emplace( EKeys::Gamepad_DPad_Up );
	Up.Emplace( EKeys::Gamepad_LeftStick_Up );

	// down
	Down.Emplace( EKeys::Down );
	Down.Emplace( EKeys::Gamepad_DPad_Down );
	Down.Emplace( EKeys::Gamepad_LeftStick_Down );
}

FNavigationConfig::~FNavigationConfig()
{
}

EUINavigation FNavigationConfig::GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const
{
	if ( Right.Contains(InKeyEvent.GetKey()) )
	{
		return EUINavigation::Right;
	}
	else if ( Left.Contains(InKeyEvent.GetKey()) )
	{
		return EUINavigation::Left;
	}
	else if ( Up.Contains(InKeyEvent.GetKey()) )
	{
		return EUINavigation::Up;
	}
	else if ( Down.Contains(InKeyEvent.GetKey()) )
	{
		return EUINavigation::Down;
	}
	else if ( InKeyEvent.GetKey() == EKeys::Tab )
	{
		//@TODO: Really these uses of input should be at a lower priority, only occurring if nothing else handled them
		// For now this code prevents consuming them when some modifiers are held down, allowing some limited binding
		const bool bAllowEatingKeyEvents = !InKeyEvent.IsControlDown() && !InKeyEvent.IsAltDown() && !InKeyEvent.IsCommandDown();

		if ( bAllowEatingKeyEvents )
		{
			return ( InKeyEvent.IsShiftDown() ) ? EUINavigation::Previous : EUINavigation::Next;
		}
	}

	return EUINavigation::Invalid;
}
