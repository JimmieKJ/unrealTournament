// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPlatformTextField.h"

class FAndroidPlatformTextField : public IPlatformTextField
{
public:
	virtual void ShowVirtualKeyboard(bool bShow, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;

private:
//	SlateTextField* TextField;
};

typedef FAndroidPlatformTextField FPlatformTextField;

