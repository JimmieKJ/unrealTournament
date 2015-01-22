// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InputDevice.h"
#include "IInputDeviceModule.h"

class FInputDeviceModule : public IInputDeviceModule
{
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	{
		TSharedPtr<IInputDevice> DummyVal = NULL;
		return DummyVal;
	}
};

IMPLEMENT_MODULE( FInputDeviceModule, InputDevice );