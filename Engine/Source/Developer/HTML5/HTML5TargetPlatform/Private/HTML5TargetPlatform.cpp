// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetPlatform.cpp: Implements the FHTML5TargetPlatform class.
=============================================================================*/

#include "HTML5TargetPlatformPrivatePCH.h"

#if WITH_ENGINE
#include "DeviceProfiles/DeviceProfile.h"
#endif 

 /* FHTML5TargetPlatform structors
 *****************************************************************************/

FHTML5TargetPlatform::FHTML5TargetPlatform( )
{
	RefreshHTML5Setup();
#if WITH_ENGINE
	// load up texture settings from the config file
	HTML5LODSettings = nullptr;
	StaticMeshLODSettings.Initialize(HTML5EngineSettings);
#endif
}


 /* ITargetPlatform interface
 *****************************************************************************/

void FHTML5TargetPlatform::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();
	OutDevices.Append(LocalDevice);
}


ECompressionFlags FHTML5TargetPlatform::GetBaseCompressionMethod( ) const
{
	return COMPRESS_ZLIB;
}


ITargetDevicePtr FHTML5TargetPlatform::GetDefaultDevice( ) const
{
	if(LocalDevice.Num())
		return LocalDevice[0];
	else 
		return NULL; 
}


ITargetDevicePtr FHTML5TargetPlatform::GetDevice( const FTargetDeviceId& DeviceId )
{
	for ( auto Device : LocalDevice )
	{
		if ( Device.IsValid() && DeviceId == Device->GetId() )
			return Device; 
	}
	return NULL;
}

bool FHTML5TargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
#if PLATFORM_WINDOWS
		FString SDKPath = FPaths::EngineDir() / TEXT("Source") / TEXT("ThirdParty") / TEXT("HTML5") / TEXT("emsdk") / TEXT("Win64");
#elif PLATFORM_MAC
		FString SDKPath = FPaths::EngineDir() / TEXT("Source") / TEXT("ThirdParty") / TEXT("HTML5") / TEXT("emsdk") / TEXT("Mac");
#elif PLATFORM_LINUX
		FString SDKPath = FPaths::EngineDir() / TEXT("Source") / TEXT("ThirdParty") / TEXT("HTML5") / TEXT("emsdk") / TEXT("Linux");
#else 
		return; 
#endif 

	FString SDKDirectory = FPaths::ConvertRelativePathToFull(SDKPath);

	if (IFileManager::Get().DirectoryExists(*SDKDirectory))
	{
		return true; 	
	}
	return false; 
}


bool FHTML5TargetPlatform::IsRunningPlatform( ) const
{
	return false; // but this will never be called because this platform doesn't run the target platform framework
}


#if WITH_ENGINE

static FName NAME_GLSL_ES2_WEBGL(TEXT("GLSL_ES2_WEBGL"));

void FHTML5TargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	OutFormats.AddUnique(NAME_GLSL_ES2_WEBGL);
}


void FHTML5TargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}


const class FStaticMeshLODSettings& FHTML5TargetPlatform::GetStaticMeshLODSettings( ) const
{
	return StaticMeshLODSettings;
}


void FHTML5TargetPlatform::GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const
{
	FName TextureFormatName = NAME_None;

#if WITH_EDITOR
	// Supported texture format names.
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT3(TEXT("DXT3"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameDXT5n(TEXT("DXT5n"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameG8(TEXT("G8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));
	static FName NameRGBA8(TEXT("RGBA8"));

	bool bNoCompression = Texture->CompressionNone				// Code wants the texture uncompressed.
		|| (HasEditorOnlyData() && Texture->DeferCompression)	// The user wishes to defer compression, this is ok for the Editor only.
		|| (Texture->CompressionSettings == TC_EditorIcon)
		|| (Texture->LODGroup == TEXTUREGROUP_ColorLookupTable)	// Textures in certain LOD groups should remain uncompressed.
		|| (Texture->LODGroup == TEXTUREGROUP_Bokeh)
		|| (Texture->LODGroup == TEXTUREGROUP_IESLightProfile)
		|| (Texture->Source.GetSizeX() < 4) // Don't compress textures smaller than the DXT block size.
		|| (Texture->Source.GetSizeY() < 4)
		|| (Texture->Source.GetSizeX() % 4 != 0)
		|| (Texture->Source.GetSizeY() % 4 != 0);


	ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();

	// Determine the pixel format of the (un/)compressed texture
	if (bNoCompression)
	{
			if (Texture->HasHDRSource())
			{
				TextureFormatName = NameBGRA8;
			}
			else if (SourceFormat == TSF_G8 || Texture->CompressionSettings == TC_Grayscale)
			{
				TextureFormatName = NameG8;
			}
			else if (Texture->LODGroup == TEXTUREGROUP_Shadowmap)
			{
				TextureFormatName = NameG8;
			}
			else 
			{
				TextureFormatName = NameRGBA8;
			}
	}
	else if (Texture->CompressionSettings == TC_HDR
		|| Texture->CompressionSettings == TC_HDR_Compressed)
	{
		TextureFormatName = NameRGBA16F;
	}
	else if (Texture->CompressionSettings == TC_Normalmap)
	{
		TextureFormatName =  NameDXT5;
	}
	else if (Texture->CompressionSettings == TC_Displacementmap)
	{
		TextureFormatName = NameG8;
	}
	else if (Texture->CompressionSettings == TC_VectorDisplacementmap)
	{
		TextureFormatName = NameRGBA8;
	}
	else if (Texture->CompressionSettings == TC_Grayscale)
	{
		TextureFormatName = NameG8;
	}
	else if( Texture->CompressionSettings == TC_Alpha)
	{
		TextureFormatName = NameDXT5;
	}
	else if (Texture->CompressionSettings == TC_DistanceFieldFont)
	{
		TextureFormatName = NameG8;
	}
	else if (Texture->CompressionNoAlpha)
	{
		TextureFormatName = NameDXT1;
	}
	else if (Texture->bDitherMipMapAlpha)
	{
		TextureFormatName = NameDXT5;
	}
	else
	{
		TextureFormatName = NameAutoDXT;
	}

	// Some PC GPUs don't support sRGB read from G8 textures (e.g. AMD DX10 cards on ShaderModel3.0)
	// This solution requires 4x more memory but a lot of PC HW emulate the format anyway
	if ((TextureFormatName == NameG8) && Texture->SRGB && !SupportsFeature(ETargetPlatformFeatures::GrayscaleSRGB))
	{
		TextureFormatName = NameBGRA8;
	}
#endif 

	OutFormats.Add( TextureFormatName); 
}


const UTextureLODSettings& FHTML5TargetPlatform::GetTextureLODSettings() const
{
	return *HTML5LODSettings;
}


FName FHTML5TargetPlatform::GetWaveFormat( const USoundWave* Wave ) const
{
	static FName NAME_OGG(TEXT("OGG"));
	return NAME_OGG;
}

#endif // WITH_ENGINE

void FHTML5TargetPlatform::RefreshHTML5Setup()
{
	FString Temp; 
	if (!FHTML5TargetPlatform::IsSdkInstalled(true, Temp))
	{
		// nothing to do. 
		return;
	}

	//New style detection of devices
	for (const auto& Device : LocalDevice)
	{
		DeviceLostEvent.Broadcast(Device.ToSharedRef());
	}
	LocalDevice.Reset();

	auto* Config = GConfig->FindConfigFile(GEngineIni);
	if (!Config)
	{
		Config = &HTML5EngineSettings;
	}
	TArray<FString> ValueArray;

	if (Config->Find("/Script/HTML5PlatformEditor.HTML5SDKSettings"))
	{
		FConfigSection AvaliableDevicesNewSection = (*Config)["/Script/HTML5PlatformEditor.HTML5SDKSettings"];
		for (auto It : AvaliableDevicesNewSection)
		{
			ValueArray.Reset();
			if (It.Key == TEXT("DeviceMap"))
			{
				FString DeviceName;
				FString DevicePath;
				It.Value.RemoveFromStart(TEXT("("));
				It.Value.RemoveFromEnd(TEXT(")"));
				It.Value.ParseIntoArray(ValueArray, TEXT(","), 1);
				for (auto& Value : ValueArray)
				{
					if (Value.StartsWith(TEXT("DeviceName=")))
					{
						DeviceName = Value.RightChop(11).TrimQuotes();
					}
					else if (Value.StartsWith(TEXT("DevicePath=(FilePath=")))
					{
						DevicePath = Value.RightChop(21);
						DevicePath.RemoveFromEnd(TEXT(")"));
						DevicePath = DevicePath.TrimQuotes();
					}
				}

				if (!DeviceName.IsEmpty() && !DevicePath.IsEmpty() &&
					(FPlatformFileManager::Get().GetPlatformFile().FileExists(*DevicePath) ||
                     FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*DevicePath)))
				{
					ITargetDevicePtr Device = MakeShareable(new FHTML5TargetDevice(*this, DeviceName, DevicePath));
					LocalDevice.Add(Device);
					DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
				}
			}
		}
	}

	struct FBrowserLocation
	{
		FString Name;
		FString Path;
	} PossibleLocations[] =
	{
#if PLATFORM_WINDOWS
		{ TEXT("Nightly(64bit)"), TEXT("C:/Program Files/Nightly/firefox.exe") },
		{ TEXT("Nightly"), TEXT("C:/Program Files (x86)/Nightly/firefox.exe") },
		{ TEXT("Firefox"), TEXT("C:/Program Files (x86)/Mozilla Firefox/firefox.exe") },
	//	{ TEXT("Chrome"), TEXT("C:/Program Files (x86)/Google/Chrome/Application/chrome.exe") },
#elif PLATFORM_MAC
		{ TEXT("Safari"), TEXT("/Applications/Safari.app") },
		{ TEXT("Firefox"), TEXT("/Applications/Firefox.app") },
		{ TEXT("Chrome"), TEXT("/Applications/Google Chrome.app") },
#elif PLATFORM_LINUX
		{ TEXT("Firefox"), TEXT("/usr/bin/firefox") },
#else
		{ TEXT("Firefox"), TEXT("") },
#endif
	};

	// Add default devices..
	for (const auto& Loc : PossibleLocations)
	{
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Loc.Path) || FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Loc.Path))
		{
			ITargetDevicePtr Device = MakeShareable(new FHTML5TargetDevice(*this, *Loc.Name, *Loc.Path));
			LocalDevice.Add(Device);
			DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		}
	}
}
