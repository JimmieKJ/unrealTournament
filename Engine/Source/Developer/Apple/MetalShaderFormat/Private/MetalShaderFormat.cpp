// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "MetalShaderFormat.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "TargetPlatform.h"   
#include "ShaderCore.h"
#include "hlslcc.h"

static FName NAME_SF_METAL(TEXT("SF_METAL"));
static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
static FName NAME_SF_METAL_SM4(TEXT("SF_METAL_SM4"));
static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));

class FMetalShaderFormat : public IShaderFormat
{
	enum
	{
		HEADER_VERSION = 34,
	};
	
	struct FVersion
	{
		uint16 HLSLCCMinor		: 8;
		uint16 Format			: 7;
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
		Version.Version.HLSLCCMinor = HLSLCC_VersionMinor;
		Version.Version.OfflineCompiled = METAL_OFFLINE_COMPILE;
		
		// Check that we didn't overwrite any bits
		check(Version.Version.Format == HEADER_VERSION);
		check(Version.Version.HLSLCCMinor == HLSLCC_VersionMinor);
		check(Version.Version.OfflineCompiled == METAL_OFFLINE_COMPILE);

		return Version.Raw;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_SF_METAL);
		OutFormats.Add(NAME_SF_METAL_MRT);
		OutFormats.Add(NAME_SF_METAL_SM4);
		OutFormats.Add(NAME_SF_METAL_SM5);
		OutFormats.Add(NAME_SF_METAL_MACES3_1);
		OutFormats.Add(NAME_SF_METAL_MACES2);
	}
	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		check(Format == NAME_SF_METAL || Format == NAME_SF_METAL_MRT || Format == NAME_SF_METAL_SM4 || Format == NAME_SF_METAL_SM5 || Format == NAME_SF_METAL_MACES3_1 || Format == NAME_SF_METAL_MACES2);
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
