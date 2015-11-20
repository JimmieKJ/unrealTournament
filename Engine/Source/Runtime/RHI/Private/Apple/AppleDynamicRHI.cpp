// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ModuleManager.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;
	IDynamicRHIModule* DynamicRHIModule = NULL;

	// Load the dynamic RHI module.
	if (FPlatformMisc::HasPlatformFeature(TEXT("Metal")))
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("MetalRHI"));
	}
	else
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));
	}
	
	// Create the dynamic RHI.
	DynamicRHI = DynamicRHIModule->CreateRHI();
	return DynamicRHI;
}
