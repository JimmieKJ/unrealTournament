// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** This structure is used to control which FKeys should move focus */
struct FNavigationConfig
{
	/** Keys that should move focus left */
	TSet<FKey> Left;

	/** Keys that should move focus right */
	TSet<FKey> Right;

	/** Keys that should move focus up*/
	TSet<FKey> Up;

	/** Keys that should move focus down*/
	TSet<FKey> Down;

	FNavigationConfig()
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
};