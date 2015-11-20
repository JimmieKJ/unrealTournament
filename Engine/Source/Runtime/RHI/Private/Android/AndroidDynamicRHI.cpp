// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ModuleManager.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;

	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = NULL;
	DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv")); //duplicate? memory redudancy ?

	if (!DynamicRHIModule->IsSupported()) 
	{

	//	FMessageDialog::Open(EAppMsgType::Ok, TEXT("OpenGL 3.2 is required to run the engine."));
		FPlatformMisc::RequestExit(1);
		DynamicRHIModule = NULL;
	}

	if (DynamicRHIModule)
	{
		// Create the dynamic RHI.
		DynamicRHI = DynamicRHIModule->CreateRHI();
	}

	return DynamicRHI;
}
