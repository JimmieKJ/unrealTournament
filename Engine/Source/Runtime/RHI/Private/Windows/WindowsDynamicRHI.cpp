// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ModuleManager.h"

#if USE_DYNAMIC_RHI

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;

	// command line overrides
	const bool bForceD3D11 = FParse::Param(FCommandLine::Get(),TEXT("d3d11")) || FParse::Param(FCommandLine::Get(),TEXT("sm5")) || FParse::Param(FCommandLine::Get(),TEXT("dx11"));
	const bool bForceD3D10 = FParse::Param(FCommandLine::Get(),TEXT("d3d10")) || FParse::Param(FCommandLine::Get(),TEXT("sm4")) || FParse::Param(FCommandLine::Get(),TEXT("dx10"));

	const bool bForceOpenGL = FWindowsPlatformMisc::VerifyWindowsVersion(6, 0) == false || FParse::Param(FCommandLine::Get(), TEXT("opengl")) || FParse::Param(FCommandLine::Get(), TEXT("opengl3")) || FParse::Param(FCommandLine::Get(), TEXT("opengl4"));

	if (((bForceD3D11 ? 1 : 0) + (bForceD3D10 ? 1 : 0) + (bForceOpenGL ? 1 : 0)) > 1)
	{
		UE_LOG(LogRHI, Fatal,TEXT("-d3d11, -d3d10, and -opengl[3|4] mutually exclusive options, but more than one was specified on the command-line."));
	}

	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = NULL;
	if(bForceOpenGL)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));

		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
	}
	else
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("D3D11RHI"));

		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX11Feature", "DX11 feature level 10.0 is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
		else if( FPlatformProcess::IsApplicationRunning( TEXT("fraps.exe") ) )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "UseExpressionEncoder", "Fraps has been known to crash D3D11. Please use Microsoft Expression Encoder instead for capturing."));
		}
	}

	if (DynamicRHIModule)
	{
		// Create the dynamic RHI.
		DynamicRHI = DynamicRHIModule->CreateRHI();
	}

	return DynamicRHI;
}

#endif // USE_DYNAMIC_RHI
