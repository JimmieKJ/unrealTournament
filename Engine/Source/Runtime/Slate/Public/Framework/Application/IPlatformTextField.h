// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IVirtualKeyboardEntry;

class IPlatformTextField
{
public:
	virtual ~IPlatformTextField() {};

	virtual void ShowVirtualKeyboard(bool bShow, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) = 0;

private:

};
