// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


/**
 * SplashTextType defines the types of text on the splash screen
 */
namespace SplashTextType
{
	enum Type
	{
		/** Startup progress text */
		StartupProgress	= 0,

		/** Version information text line 1 */
		VersionInfo1,

		/** Copyright information text */
		CopyrightInfo,

		/** Game Name */
		GameName,

		// ...

		/** Number of text types (must be final enum value) */
		NumTextTypes
	};
}


/**
 * Generic implementation for most platforms
 */
struct CORE_API FGenericPlatformSplash
{
	/** Show the splash screen. */
	FORCEINLINE static void Show() { }

	/** Hide the splash screen. */
	FORCEINLINE static void Hide() { }

	/**
	 * Sets the text displayed on the splash screen (for startup/loading progress)
	 *
	 * @param	InType		Type of text to change
	 * @param	InText		Text to display
	 */
	FORCEINLINE static void SetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
	{

	}

	/**
	 * Return whether the splash screen is being shown or not
	 */
	FORCEINLINE static bool IsShown()
	{
		return true;
	}
};
