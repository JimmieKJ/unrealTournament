// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericApplication.h"


/**
 * Raw input gesture that defines input that must be valid when           
 */
struct SLATE_API FInputGesture
{
	/** Key that must be pressed */
	FKey Key;

	EModifierKey::Type ModifierKeys;

#if PLATFORM_MAC
	bool NeedsControl() const { return (ModifierKeys & EModifierKey::Command) != 0; }
	bool NeedsCommand() const { return (ModifierKeys & EModifierKey::Control) != 0; }
#else
	bool NeedsControl() const { return (ModifierKeys & EModifierKey::Control) != 0; }
	bool NeedsCommand() const { return (ModifierKeys & EModifierKey::Command) != 0; }
#endif
	bool NeedsAlt() const { return (ModifierKeys & EModifierKey::Alt) != 0; }
	bool NeedsShift() const { return (ModifierKeys & EModifierKey::Shift) != 0; }

public:

	/** Default constructor. */
	FInputGesture()
		: ModifierKeys(EModifierKey::None)
	{
	}

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InModifierKeys
	 * @param InKey
	 */
	FInputGesture( const EModifierKey::Type InModifierKeys, const FKey InKey )
		: Key(InKey)
		, ModifierKeys(InModifierKeys)
	{
	}

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InKey
	 */
	FInputGesture( const FKey InKey )
		: Key(InKey)
		, ModifierKeys(EModifierKey::None)
	{
	}

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InKey
	 * @param InModifierKeys
	 */
	FInputGesture( const FKey InKey, const EModifierKey::Type InModifierKeys )
		: Key(InKey)
		, ModifierKeys(InModifierKeys)
	{
	}

	/**
	 * Copy constructor.
	 *
	 * @param Other
	 */
	FInputGesture( const FInputGesture& Other )
		: Key(Other.Key)
		, ModifierKeys(Other.ModifierKeys)
	{ }

public:

	/**
	 * Compares this input gesture with another for equality.
	 *
	 * @param Other The other gesture to compare with.
	 * @return true if the gestures are equal, false otherwise.
	 */
	bool operator!=( const FInputGesture& Other ) const
	{
		return !(*this == Other);
	}

	/**
	 * Compares this input gesture with another for inequality.
	 *
	 * @param Other The other gesture to compare with.
	 * @return true if the gestures are not equal, false otherwise.
	 */
	bool operator==( const FInputGesture& Other ) const
	{
		return (Key == Other.Key) && (ModifierKeys == Other.ModifierKeys);
	}

public:

	/** 
	 * Gets a localized string that represents the gesture.
	 *
	 * @return A localized string.
	 */
	FText GetInputText( ) const;
	
	/**
	 * Gets the key represented as a localized string.
	 *
	 * @return A localized string.
	 */
	FText GetKeyText( ) const;

	/**
	 * Checks whether this gesture requires an modifier keys to be pressed.
	 *
	 * @return true if modifier keys must be pressed, false otherwise.
	 */
	bool HasAnyModifierKeys( ) const
	{
		return (ModifierKeys != EModifierKey::None);
	}

	/**
	 * Determines if this gesture is valid.  A gesture is valid if it has a non modifier key that must be pressed
	 * and zero or more modifier keys that must be pressed
	 *
	 * @return true if the gesture is valid
	 */
	bool IsValidGesture( ) const
	{
		return (Key.IsValid() && !Key.IsModifierKey());
	}

	/**
	 * Sets this gesture to a new key and modifier state based on the provided template
	 * Should not be called directly.  Only used by the key binding editor to set user defined keys
	 */
	void Set( const FInputGesture& InTemplate )
	{
		*this = InTemplate;
	}

public:

	/**
	 * Gets a type hash value for the specified gesture.
	 *
	 * @param Gesture The input gesture to get the hash value for.
	 * @return The hash value.
	 */
	friend uint32 GetTypeHash( const FInputGesture& Gesture )
	{
		return GetTypeHash(Gesture.Key) ^ Gesture.ModifierKeys;
	}
};
