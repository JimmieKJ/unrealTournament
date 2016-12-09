// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"

// @todo - hook up keyboard types
enum EKeyboardType
{
	Keyboard_Default,
	Keyboard_Number,
	Keyboard_Web,
	Keyboard_Email,
	Keyboard_Password,
	Keyboard_AlphaNumeric,
};

enum class ESetTextType : uint8
{
	Changed,
	Commited
};

class SLATE_API IVirtualKeyboardEntry
{

public:
	virtual ~IVirtualKeyboardEntry() {}

	/**
	* Sets the text to that entered by the virtual keyboard
	*
	* @param  InNewText  The new text
	* @param SetTextType Set weather we want to send a TextChanged event after or a TextCommitted event
	* @param CommitType If we are sending a TextCommitted event, what commit type is it
	*/
	virtual void SetTextFromVirtualKeyboard(const FText& InNewText, ESetTextType SetTextType, ETextCommit::Type CommitType) = 0;
	
	/**
	* Returns the text.
	*
	* @return  Text
	*/
	virtual FText GetText() const = 0;

	/**
	* Returns the hint text.
	*
	* @return  HintText
	*/
	virtual FText GetHintText() const = 0;

	/**
	* Returns the virtual keyboard type.
	*
	* @return  VirtualKeyboardType
	*/
	virtual EKeyboardType GetVirtualKeyboardType() const = 0;

	/**
	* Returns whether the entry is multi-line
	*
	* @return Whether the entry is multi-line
	*/
	virtual bool IsMultilineEntry() const = 0;
};
