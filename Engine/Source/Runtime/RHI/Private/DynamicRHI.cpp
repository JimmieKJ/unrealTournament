// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicRHI.cpp: Dynamically bound Render Hardware Interface implementation.
=============================================================================*/

#include "RHI.h"
#include "ModuleManager.h"

#ifndef PLATFORM_ALLOW_NULL_RHI
	#define PLATFORM_ALLOW_NULL_RHI		0
#endif

// Globals.
FDynamicRHI* GDynamicRHI = NULL;

void InitNullRHI()
{
	// Use the null RHI if it was specified on the command line, or if a commandlet is running.
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("NullDrv"));
	// Create the dynamic RHI.
	if ((DynamicRHIModule == 0) || !DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("DynamicRHI", "NullDrvFailure", "NullDrv failure?"));
		FPlatformMisc::RequestExit(1);
	}

	GDynamicRHI = DynamicRHIModule->CreateRHI();
	GDynamicRHI->Init();
	GRHICommandList.GetImmediateCommandList().SetContext(GDynamicRHI->RHIGetDefaultContext());
	GUsingNullRHI = true;
	GRHISupportsTextureStreaming = false;
}

void RHIInit(bool bHasEditorToken)
{
	if(!GDynamicRHI)
	{
		GRHICommandList.LatchBypass(); // read commandline for bypass flag

		if (USE_NULL_RHI || FParse::Param(FCommandLine::Get(),TEXT("nullrhi")) || !FApp::CanEverRender())
		{
			InitNullRHI();
		}
		else
		{
			GDynamicRHI = PlatformCreateDynamicRHI();
			if (GDynamicRHI)
			{
				GDynamicRHI->Init();
				GRHICommandList.GetImmediateCommandList().SetContext(GDynamicRHI->RHIGetDefaultContext());
			}
#if PLATFORM_ALLOW_NULL_RHI
			else
			{
				// If the platform supports doing so, fall back to the NULL RHI on failure
				InitNullRHI();
			}
#endif
		}

		check(GDynamicRHI);
	}
}

void RHIPostInit()
{
	check(GDynamicRHI);
	GDynamicRHI->PostInit();
}

void RHIExit()
{
	if ( !GUsingNullRHI && GDynamicRHI != NULL )
	{
		// Destruct the dynamic RHI.
		GDynamicRHI->Shutdown();
		delete GDynamicRHI;
		GDynamicRHI = NULL;
	}
}


