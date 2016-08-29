// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RHIPrivatePCH.h"
#include "RHI.h"
#include "ModuleManager.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;
	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));

	if (!DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
		FPlatformMisc::RequestExit(1);
		DynamicRHIModule = NULL;
	}

	// Create the dynamic RHI.
	if (DynamicRHIModule)
	{
		DynamicRHI = DynamicRHIModule->CreateRHI();
	}

	return DynamicRHI;
}
