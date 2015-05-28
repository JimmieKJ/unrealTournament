// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxTargetPlatform.h: Declares the FLinuxTargetPlatform class.
=============================================================================*/

#pragma once

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE
#include "IProjectManager.h"

#define LOCTEXT_NAMESPACE "TLinuxTargetPlatform"

/**
 * Template for Linux target platforms
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
class TLinuxTargetPlatform
	: public TTargetPlatformBase<FLinuxPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> >
{
public:
	
	typedef FLinuxPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> TProperties;
	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TLinuxTargetPlatform( )
	{		
#if PLATFORM_LINUX
		// only add local device if actually running on Linux
		FTargetDeviceId UATFriendlyId(FTargetDeviceId(TSuper::PlatformName(), FPlatformProcess::ComputerName()));
		LocalDevice = MakeShareable(new FLinuxTargetDevice(*this, UATFriendlyId, FPlatformProcess::ComputerName()));
#endif
	
#if WITH_ENGINE
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *this->PlatformName());
		TextureLODSettings = nullptr;
		StaticMeshLODSettings.Initialize(EngineSettings);

		// Get the Target RHIs for this platform, we do not always want all those that are supported.
		GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

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
#endif // WITH_ENGINE
	}


public:

	// Begin ITargetPlatform interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice(const FString& DeviceName, bool bDefault) override
	{
		FLinuxTargetDevicePtr& Device = Devices.FindOrAdd(DeviceName);

		if (Device.IsValid())
		{
			// do not allow duplicates
			return false;
		}

		FTargetDeviceId UATFriendlyId(TEXT("Linux"), DeviceName);
		Device = MakeShareable(new FLinuxTargetDevice(*this, UATFriendlyId, DeviceName));
		DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		return true;
	}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		// TODO: ping all the machines in a local segment and/or try to connect to port 22 of those that respond
		OutDevices.Reset();
		if (LocalDevice.IsValid())
		{
			OutDevices.Add(LocalDevice);
		}

		for (const auto & DeviceIter : Devices)
		{
			OutDevices.Add(DeviceIter.Value);
		}
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
		if (LocalDevice.IsValid())
		{
			return LocalDevice;
		}

		return nullptr;
	}

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override
	{
		if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
		{
			return LocalDevice;
		}

		for (const auto & DeviceIter : Devices)
		{
			if (DeviceId == DeviceIter.Value->GetId())
			{
				return DeviceIter.Value;
			}
		}

		return nullptr;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Linux platform as editor for this to be considered a running platform
		return PLATFORM_LINUX && !UE_SERVER && !UE_GAME && WITH_EDITOR && HAS_EDITOR_DATA;
	}

	virtual bool SupportsFeature(ETargetPlatformFeatures Feature) const override
	{
		if (Feature == ETargetPlatformFeatures::UserCredentials || Feature == ETargetPlatformFeatures::Packaging)
		{
			return true;
		}

		return TTargetPlatformBase<FLinuxPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY>>::SupportsFeature(Feature);
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		if (!PLATFORM_LINUX)
		{
			// check for LINUX_ROOT when targeting Linux from Win/Mac
			TCHAR ToolchainRoot[32768] = { 0 };
			FPlatformMisc::GetEnvironmentVariable(TEXT("LINUX_ROOT"), ToolchainRoot, ARRAY_COUNT(ToolchainRoot));

			FString ToolchainCompiler = ToolchainRoot;
			if (PLATFORM_WINDOWS)
			{
				ToolchainCompiler += "/bin/clang++.exe";
			}
			else if (PLATFORM_MAC)
			{
				ToolchainCompiler += "/bin/clang++";
			}
			else
			{
				checkf(false, TEXT("Unable to target Linux on an unknown platform."));
				return false;
			}

			return FPaths::FileExists(ToolchainCompiler);
		}

		return true;
	}

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		int32 ReadyToBuild = TSuper::CheckRequirements(ProjectPath, bProjectHasCode, OutDocumentationPath);

		// do not support code/plugins in Rocket as the required libs aren't bundled (on Windows/Mac)
		if (!PLATFORM_LINUX && FRocketSupport::IsRocket())
		{
			if (bProjectHasCode)
			{
				ReadyToBuild |= ETargetPlatformReadyStatus::CodeUnsupported;
			}

			if (IProjectManager::Get().IsNonDefaultPluginEnabled())
			{
				ReadyToBuild |= ETargetPlatformReadyStatus::PluginsUnsupported;
			}
		}

		return ReadyToBuild;
	}


#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!IS_DEDICATED_SERVER)
		{
			static FName NAME_GLSL_150(TEXT("GLSL_150"));
			static FName NAME_GLSL_430(TEXT("GLSL_430"));

			OutFormats.AddUnique(NAME_GLSL_150);
			OutFormats.AddUnique(NAME_GLSL_430);
		}
	}

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override
	{
		for(const FString& ShaderFormat : TargetedShaderFormats)
		{
			OutFormats.AddUnique(FName(*ShaderFormat));
		}
	}

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}


	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture
			FName TextureFormatName = this->GetDefaultTextureFormatName(InTexture, EngineSettings, false);
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
			return LOCTEXT("LinuxServerVariantTitle", "Dedicated Server");
		}

		if (HAS_EDITOR_DATA)
		{
			return LOCTEXT("LinuxClientEditorDataVariantTitle", "Client with Editor Data");
		}

		if (IS_CLIENT_ONLY)
		{
			return LOCTEXT("LinuxClientOnlyVariantTitle", "Client only");
		}

		return LOCTEXT("LinuxClientVariantTitle", "Client");
	}

	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("LinuxVariantTitle", "Build Type");
	}

	virtual float GetVariantPriority() const override
	{
		return TProperties::GetVariantPriority();
	}

	DECLARE_DERIVED_EVENT(TLinuxTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}
	
	DECLARE_DERIVED_EVENT(TLinuxTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	// End ITargetPlatform interface

private:

	// Holds the local device.
	FLinuxTargetDevicePtr LocalDevice;
	// Holds a map of valid devices.
	TMap<FString, FLinuxTargetDevicePtr> Devices;


#if WITH_ENGINE
	// Holds the Engine INI settings for quick use.
	FConfigFile EngineSettings;

	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	// List of shader formats specified as targets
	TArray<FString> TargetedShaderFormats;
#endif // WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};

#undef LOCTEXT_NAMESPACE
