// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AllDesktopTargetPlatform.cpp: Implements the FDesktopTargetPlatform class.
=============================================================================*/

#include "AllDesktopTargetPlatformPrivatePCH.h"
#include "IProjectManager.h"

/* FAllDesktopTargetPlatform structors
 *****************************************************************************/

FAllDesktopTargetPlatform::FAllDesktopTargetPlatform()
{
#if WITH_ENGINE
	// use non-platform specific settings
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, NULL);
	StaticMeshLODSettings.Initialize(EngineSettings);
#endif // #if WITH_ENGINE

}


FAllDesktopTargetPlatform::~FAllDesktopTargetPlatform()
{
}



/* ITargetPlatform interface
 *****************************************************************************/

#if WITH_ENGINE


void FAllDesktopTargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
	static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
	static FName NAME_GLSL_150(TEXT("GLSL_150"));
	static FName NAME_GLSL_430(TEXT("GLSL_430"));
	static FName NAME_GLSL_150_MAC(TEXT("GLSL_150_MAC"));
	
#if PLATFORM_WINDOWS
	// right now, only windows can properly compile D3D shaders (this won't corrupt the DDC, but it will 
	// make it so that packages cooked on Mac/Linux will only run on Windows with -opengl)
	OutFormats.AddUnique(NAME_PCD3D_SM5);
	OutFormats.AddUnique(NAME_PCD3D_SM4);
#endif
	OutFormats.AddUnique(NAME_GLSL_150);
	OutFormats.AddUnique(NAME_GLSL_430);
	OutFormats.AddUnique(NAME_GLSL_150_MAC);
}


void FAllDesktopTargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}

void FAllDesktopTargetPlatform::GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const
{
	// just use the standard texture format name for this texture (without DX11 texture support)
	OutFormats.Add(GetDefaultTextureFormatName(Texture, EngineSettings, false));
}


FName FAllDesktopTargetPlatform::GetWaveFormat( const class USoundWave* Wave ) const
{
	static FName NAME_OGG(TEXT("OGG"));
	static FName NAME_OPUS(TEXT("OPUS"));
	
	if (Wave->IsStreaming())
	{
		// @todo desktop platform: Does Mac support OPUS?
		return NAME_OPUS;
	}
	
	return NAME_OGG;
}

#endif // WITH_ENGINE
