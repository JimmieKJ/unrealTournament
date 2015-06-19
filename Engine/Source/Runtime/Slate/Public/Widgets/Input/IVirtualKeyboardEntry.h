// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

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

class SLATE_API IVirtualKeyboardEntry
{

public:
	/**
	* Sets the text to that entered by the virtual keyboard
	*
	* @param  InNewText  The new text
	*/
	virtual void SetTextFromVirtualKeyboard(const FText& InNewText) = 0;
	
	/**
	* Returns the text.
	*
	* @return  Text
	*/
	virtual const FText& GetText() const = 0;

	/**
	* Returns the hint text.
	*
	* @return  HintText
	*/
	virtual const FText GetHintText() const = 0;

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
