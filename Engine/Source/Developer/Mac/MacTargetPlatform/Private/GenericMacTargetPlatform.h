// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericMacTargetPlatform.h: Declares the TGenericMacTargetPlatform class template.
=============================================================================*/

#pragma once

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "TGenericMacTargetPlatform"

/**
 * Template for Mac target platforms
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
class TGenericMacTargetPlatform
	: public TTargetPlatformBase<FMacPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> >
{
public:

	typedef FMacPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> TProperties;
	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TGenericMacTargetPlatform( )
	{
		LocalDevice = MakeShareable(new FLocalMacTargetDevice(*this));

		#if WITH_ENGINE
			FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *this->PlatformName());
			TextureLODSettings = nullptr;
			StaticMeshLODSettings.Initialize(EngineSettings);
		
			// Get the Target RHIs for this platform, we do not always want all those that are supported.
			GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
		
			// Get the cached shader formats for this platform, we do not always want all those that are supported.
			GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("CachedShaderFormats"), CachedShaderFormats, GEngineIni);
		
			// Gather the list of Target RHIs and filter out any that may be invalid.
			TArray<FName> PossibleShaderFormats;
			GetAllPossibleShaderFormats(PossibleShaderFormats);
			
			for(int32 ShaderFormatIdx = TargetedShaderFormats.Num()-1; ShaderFormatIdx >= 0; ShaderFormatIdx--)
			{
				FString ShaderFormat = TargetedShaderFormats[ShaderFormatIdx];
				if(PossibleShaderFormats.Contains(FName(*ShaderFormat)) == false)
				{
					TargetedShaderFormats.RemoveAt(ShaderFormatIdx);
				}
			}
		
			for(int32 ShaderFormatIdx = CachedShaderFormats.Num()-1; ShaderFormatIdx >= 0; ShaderFormatIdx--)
			{
				FString ShaderFormat = CachedShaderFormats[ShaderFormatIdx];
				if(PossibleShaderFormats.Contains(FName(*ShaderFormat)) == false)
				{
					CachedShaderFormats.RemoveAt(ShaderFormatIdx);
				}
			}
		#endif
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		OutDevices.Reset();
		OutDevices.Add(LocalDevice);
	}

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override
	{
		return COMPRESS_ZLIB;
	}

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override
	{
		return LocalDevice;
	}

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId )
	{
		if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
		{
			return LocalDevice;
		}

		return NULL;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Mac platform as editor for this to be considered a running platform
		return PLATFORM_MAC && !UE_SERVER && !UE_GAME && WITH_EDITOR && HAS_EDITOR_DATA;
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		// we currently do not have a build target for MacServer
		if (Feature == ETargetPlatformFeatures::Packaging)
		{
			return (HAS_EDITOR_DATA || !IS_DEDICATED_SERVER);
		}

return TSuper::SupportsFeature(Feature);
	}

#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!IS_DEDICATED_SERVER)
		{
			static FName NAME_GLSL_150_MAC(TEXT("GLSL_150_MAC"));
			OutFormats.AddUnique(NAME_GLSL_150_MAC);

#if PLATFORM_MAC // @todo: Enable this check on Windows and Linux to handle remote shader compiling, it would need to be done via rpc when compilation starts 
			if (FPlatformMisc::MacOSXVersionCompare(10, 11, 0) >= 0)
#endif
			{
				static FName NAME_SF_METAL_SM4(TEXT("SF_METAL_SM4"));
				OutFormats.AddUnique(NAME_SF_METAL_SM4);
				static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
				OutFormats.AddUnique(NAME_SF_METAL_SM5);
				static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
				OutFormats.AddUnique(NAME_SF_METAL_MACES3_1);
				static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));
				OutFormats.AddUnique(NAME_SF_METAL_MACES2);
			}
		}
	}

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override
	{
		for(const FString& ShaderFormat : TargetedShaderFormats)
		{
			OutFormats.AddUnique(FName(*ShaderFormat));
		}
	}
	
	virtual void GetAllCachedShaderFormats( TArray<FName>& OutFormats ) const override
	{
		for(const FString& ShaderFormat : CachedShaderFormats)
		{
			OutFormats.AddUnique(FName(*ShaderFormat));
		}
	}

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}

	virtual void GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture (with no DX11 support)
			FName TextureFormatName = this->GetDefaultTextureFormatName(Texture, EngineSettings, false);
			OutFormats.Add(TextureFormatName);
		}
	}


	virtual const UTextureLODSettings& GetTextureLODSettings() const override
	{
		return *TextureLODSettings;
	}

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}


	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override
	{
		static FName NAME_OGG(TEXT("OGG"));
		static FName NAME_OPUS(TEXT("OPUS"));

		if (Wave->IsStreaming())
		{
			return NAME_OPUS;
		}

		return NAME_OGG;
	}
#endif //WITH_ENGINE


	virtual bool SupportsVariants() const override
	{
		return true;
	}


	virtual FText GetVariantDisplayName() const override
	{
		if (IS_DEDICATED_SERVER)
		{
			return LOCTEXT("MacServerVariantTitle", "Dedicated Server");
		}

		if (HAS_EDITOR_DATA)
		{
			return LOCTEXT("MacClientEditorDataVariantTitle", "Client with Editor Data");
		}

		if (IS_CLIENT_ONLY)
		{
			return LOCTEXT("MacClientOnlyVariantTitle", "Client only");
		}

		return LOCTEXT("MacClientVariantTitle", "Client");
	}


	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("MacVariantTitle", "Build Type");
	}


	virtual float GetVariantPriority() const override
	{
		return TProperties::GetVariantPriority();
	}


	DECLARE_DERIVED_EVENT(TGenericMacTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(TGenericMacTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

private:

	// Holds the local device.
	ITargetDevicePtr LocalDevice;

#if WITH_ENGINE
	// Holds the Engine INI settings for quick use.
	FConfigFile EngineSettings;

	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
	
	// List of shader formats specified as targets
	TArray<FString> TargetedShaderFormats;
	
	// List of shader formats specified to cache
	TArray<FString> CachedShaderFormats;
#endif // WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};

#undef LOCTEXT_NAMESPACE
