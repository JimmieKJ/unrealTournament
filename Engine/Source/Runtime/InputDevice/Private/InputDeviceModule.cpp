// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "IInputDevice.h"
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
