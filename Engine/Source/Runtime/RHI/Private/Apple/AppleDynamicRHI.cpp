// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RHIPrivatePCH.h"
#include "RHI.h"
#include "ModuleManager.h"

int32 GMacMetalEnabled = 1;
static FAutoConsoleVariableRef CVarMacMetalEnabled(
	TEXT("r.Mac.UseMetal"),
	GMacMetalEnabled,
	TEXT("If set to true uses Metal when available rather than OpenGL as the graphics API. (Default: True)"));

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;
	IDynamicRHIModule* DynamicRHIModule = NULL;

	// Load the dynamic RHI module.
	if ((GMacMetalEnabled || FParse::Param(FCommandLine::Get(),TEXT("metal")) || FParse::Param(FCommandLine::Get(),TEXT("metalsm5"))) && FPlatformMisc::HasPlatformFeature(TEXT("Metal")))
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
