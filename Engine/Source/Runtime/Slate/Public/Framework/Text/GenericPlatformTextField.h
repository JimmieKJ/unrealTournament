// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPlatformTextField.h"

class FGenericPlatformTextField : public IPlatformTextField
{
public:
	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override {};

};

typedef FGenericPlatformTextField FPlatformTextField;
