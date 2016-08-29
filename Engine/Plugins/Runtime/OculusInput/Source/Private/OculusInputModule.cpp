// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusInput.h"
#include "IOculusInputPlugin.h"
#include "IOculusRiftPlugin.h"

#if OCULUS_INPUT_SUPPORTED_PLATFORMS

#define LOCTEXT_NAMESPACE "OculusInput"

class FOculusInputModule : public IOculusInputPlugin
{
	// IInputDeviceModule overrides
	virtual TSharedPtr< class IInputDevice > CreateInputDevice( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override
	{
		if (IOculusRiftPlugin::IsAvailable())
		{
			return TSharedPtr< class IInputDevice >(new FOculusInput(InMessageHandler));
		}
		else
		{
			UE_LOG(LogOcInput, Warning, TEXT("OculusInput plugin enabled, but OculusRift plugin is not available."));
			return nullptr;
		}
		
	}
};

#else	//	OCULUS_INPUT_SUPPORTED_PLATFORMS

class FOculusInputModule : public FDefaultModuleImpl
{
};

#undef LOCTEXT_NAMESPACE
#endif	// OCULUS_INPUT_SUPPORTED_PLATFORMS

IMPLEMENT_MODULE( FOculusInputModule, OculusInput )
