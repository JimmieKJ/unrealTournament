// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MetalShaderFormat.h"
#include "Core.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "TargetPlatform.h"   
#include "ShaderCore.h"
#include "hlslcc.h"

static FName NAME_SF_METAL(TEXT("SF_METAL"));
static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));

class FMetalShaderFormat : public IShaderFormat
{
	enum
	{
		HEADER_VERSION = 14,
	};
	struct FVersion
	{
		uint16 HLSLCCMajor		: 3;
		uint16 HLSLCCMinor		: 7;
		uint16 Format			: 5;
		uint16 OfflineCompiled	: 1;
	};
public:
	virtual uint16 GetVersion(FName Format) const override
	{
		static_assert(sizeof(FVersion) == sizeof(uint16), "Out of bits!");
		union
		{
			FVersion Version;
			uint16 Raw;
		} Version;

		Version.Version.Format = HEADER_VERSION;
		Version.Version.HLSLCCMajor = HLSLCC_VersionMajor;
		Version.Version.HLSLCCMinor = HLSLCC_VersionMinor;
		Version.Version.OfflineCompiled = METAL_OFFLINE_COMPILE;
		
		// Check that we didn't overwrite any bits
		check(Version.Version.Format == HEADER_VERSION);
		check(Version.Version.HLSLCCMajor == HLSLCC_VersionMajor);
		check(Version.Version.HLSLCCMinor == HLSLCC_VersionMinor);
		check(Version.Version.OfflineCompiled == METAL_OFFLINE_COMPILE);

		return Version.Raw;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_SF_METAL);
		OutFormats.Add(NAME_SF_METAL_MRT);
	}
	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		check(Format == NAME_SF_METAL || Format == NAME_SF_METAL_MRT);
		CompileShader_Metal(Input, Output, WorkingDirectory);
	}
};

/**
 * Module for OpenGL shaders
 */

static IShaderFormat* Singleton = NULL;

class FMetalShaderFormatModule : public IShaderFormatModule
{
public:
	virtual ~FMetalShaderFormatModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FMetalShaderFormat();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FMetalShaderFormatModule, MetalShaderFormat);
