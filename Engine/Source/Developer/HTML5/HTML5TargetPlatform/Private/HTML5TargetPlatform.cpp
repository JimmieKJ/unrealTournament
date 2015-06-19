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
	RefreshAvailableDevices();

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
	FConfigSection SDKPaths;
	//New style detection of devices
	if (HTML5EngineSettings.Find("/Script/HTML5PlatformEditor.HTML5SDKSettings"))
	{

		SDKPaths = HTML5EngineSettings["/Script/HTML5PlatformEditor.HTML5SDKSettings"];
		for (auto It : SDKPaths)
		{
			const FString& Platform = It.Key.ToString();
			FString Path = It.Value;
			if (Platform == "EmscriptenRoot")
			{
				FString SDKPathString;
				FString VersionString;
				auto Index = Path.Find(TEXT("SDKPath="));
				if (Index != INDEX_NONE)
				{
					Index+=9;
					auto Index2 = Path.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Index);
					if (Index2 != INDEX_NONE)
					{
						SDKPathString = Path.Mid(Index, Index2-Index);
					}
				}
				Index = Path.Find(TEXT("EmscriptenVersion="));
				if (Index != INDEX_NONE)
				{
					Index += 19;
					auto Index2 = Path.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Index);
					if (Index2 != INDEX_NONE)
					{
						VersionString = Path.Mid(Index, Index2 - Index);
					}
				}
				if (!VersionString.IsEmpty() && !SDKPathString.IsEmpty())
				{
					bool bFoundSDK = false;
					FHTML5SDKVersionNumber RequiredVersion;
					RequiredVersion.VersionNumberFromString(*VersionString);
					TArray<FHTML5SDKVersionNumber> SDKVersions;
					GetInstalledSDKVersions(*SDKPathString, SDKVersions);

					if (SDKVersions.Num() > 0 &&
						RequiredVersion.VersionNumber.Major == -1 &&
						RequiredVersion.VersionNumber.Minor == -1 &&
						RequiredVersion.VersionNumber.Revision == -1)
					{
						bFoundSDK = true;
						SDKPathString = SDKVersions.Last().VersionPath;
					}
					else
					{
						for (const auto& i : SDKVersions)
						{
							if (i == RequiredVersion)
							{
								SDKPathString = i.VersionPath;
								bFoundSDK = true;
								break;
							}
						}
					}

					
					return bFoundSDK && IFileManager::Get().DirectoryExists(*SDKPathString);
				}
			}
		}
	}

	// Old fallbacks
	if (HTML5EngineSettings.Find("HTML5SDKPaths"))
	{
		FString SectionName = "HTML5SDKPaths";
		SDKPaths = HTML5EngineSettings[SectionName];
		for (auto It : SDKPaths)
		{
			const FString& Platform = It.Key.ToString();
			const FString& Path = It.Value;
			{
				if (Platform == "Emscripten" && IFileManager::Get().DirectoryExists(*Path))
				{
					return true;
				}
#if PLATFORM_WINDOWS
				if (Platform == "Windows" && IFileManager::Get().DirectoryExists(*Path))
				{
					return true;
				}
#endif 
#if PLATFORM_MAC
				if (Platform == "Mac" && IFileManager::Get().DirectoryExists(*Path))
				{
					return true;
				}
#endif 
			}
		}
	}

	TCHAR BaseSDKPath[512];
	FPlatformMisc::GetEnvironmentVariable(TEXT("EMSCRIPTEN"), BaseSDKPath, ARRAY_COUNT(BaseSDKPath));
	if (FString(BaseSDKPath).Len() > 0 && IFileManager::Get().DirectoryExists(BaseSDKPath) )
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

static FName NAME_OPENGL_ES2_WEBGL(TEXT("GLSL_ES2_WEBGL"));

void FHTML5TargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	OutFormats.AddUnique(NAME_OPENGL_ES2_WEBGL);
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

void FHTML5TargetPlatform::RefreshAvailableDevices()
{
	// load the final HTML5 engine settings for this game
	FConfigCacheIni::LoadLocalIniFile(HTML5EngineSettings, TEXT("Engine"), true, *PlatformName());

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
		FConfigSection AvaliableDevicesNewSection =  (*Config)["/Script/HTML5PlatformEditor.HTML5SDKSettings"];
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
					ITargetDevicePtr Device = MakeShareable(new FHTML5TargetDevice(*this, FString::Printf(TEXT("%s on %s"), *DeviceName, FPlatformProcess::ComputerName())));
					LocalDevice.Add(Device);
					DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
				}
			}
		}
	}

	// Old Fallback
	if (LocalDevice.Num() == 0)
	{
		FString DeviceSectionName;

#if PLATFORM_WINDOWS
		DeviceSectionName = "HTML5DevicesWindows";
#elif PLATFORM_MAC
		DeviceSectionName = "HTML5DevicesMac";
#else 
		DeviceSectionName = "HTML5DevicesLinux";
#endif 
		if (HTML5EngineSettings.Find(DeviceSectionName))
		{
			FConfigSection AvaliableDevicesSection = HTML5EngineSettings[DeviceSectionName];
			for (auto It : AvaliableDevicesSection)
			{
				const FString& BrowserName = It.Key.ToString();
				const FString& BrowserPath = It.Value;
				if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*It.Value))
				{
					ITargetDevicePtr Device = MakeShareable(new FHTML5TargetDevice(*this, FString::Printf(TEXT("%s on %s"), *It.Key.ToString(), FPlatformProcess::ComputerName())));
					LocalDevice.Add(Device);
					DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
				}
			}
		}
	}
}

void FHTML5TargetPlatform::GetInstalledSDKVersions(const TCHAR* SDKDirectory, TArray<FHTML5SDKVersionNumber>& OutSDKs)
{
	if (!SDKDirectory)
	{
		return;
	}

	struct FVersionSearch : public IPlatformFile::FDirectoryVisitor
	{
		IPlatformFile* PlatformFile;
		TArray<FHTML5SDKVersionNumber>& SDKs;
		FString VersionFilePath;

		FVersionSearch(IPlatformFile* InPlatformFile, TArray<FHTML5SDKVersionNumber>& InSDKs)
			: PlatformFile(InPlatformFile)
			, SDKs(InSDKs)
		{}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory == true)
			{
				VersionFilePath = FPaths::Combine(FilenameOrDirectory, TEXT("emscripten-version.txt"));
				if (PlatformFile->FileExists(*VersionFilePath))
				{
					FString VersionText;
					FFileHelper::LoadFileToString(VersionText, *VersionFilePath);
					VersionText.Trim();
					VersionText.TrimTrailing();
					FHTML5SDKVersionNumber Ver;
					Ver.VersionPath = FilenameOrDirectory;
					Ver.VersionNumberFromString(*VersionText);
					SDKs.Add(Ver);
				}
			}
			return true;
		}
	};

	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FVersionSearch VersionSearch(&PlatformFile, OutSDKs);
	FString EmscriptenSDkDir = FPaths::Combine(SDKDirectory, TEXT("emscripten"));
	PlatformFile.IterateDirectory(*EmscriptenSDkDir, VersionSearch);
	OutSDKs.Sort();
}
