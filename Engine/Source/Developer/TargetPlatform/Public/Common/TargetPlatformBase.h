// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlatformInfo.h"


/**
 * Base class for target platforms.
 */
class FTargetPlatformBase
	: public ITargetPlatform
{
public:

	// ITargetPlatform interface

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override
	{
		return false;
	}

	virtual FText DisplayName() const override
	{
		return PlatformInfo->DisplayName;
	}

	virtual const PlatformInfo::FPlatformInfo& GetPlatformInfo() const override
	{
		return *PlatformInfo;
	}

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override
	{
		OutFormats.Add(FName(TEXT("FullHDR")));
	}

#ifdef TEXTURERESOURCE_H_INCLUDED // defined in TextureResource.h, this way we know if UTexture is available, needed for Clang
	FName GetDefaultTextureFormatName( const UTexture* Texture, const FConfigFile& EngineSettings, bool bSupportDX11TextureFormats ) const
	{
		FName TextureFormatName = NAME_None;

#if WITH_EDITOR
		// Supported texture format names.
		static FName NameDXT1(TEXT("DXT1"));
		static FName NameDXT3(TEXT("DXT3"));
		static FName NameDXT5(TEXT("DXT5"));
		static FName NameDXT5n(TEXT("DXT5n"));
		static FName NameAutoDXT(TEXT("AutoDXT"));
		static FName NameBC4(TEXT("BC4"));
		static FName NameBC5(TEXT("BC5"));
		static FName NameBGRA8(TEXT("BGRA8"));
		static FName NameXGXR8(TEXT("XGXR8"));
		static FName NameG8(TEXT("G8"));
		static FName NameVU8(TEXT("VU8"));
		static FName NameRGBA16F(TEXT("RGBA16F"));
		static FName NameBC6H(TEXT("BC6H"));
		static FName NameBC7(TEXT("BC7"));

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

		bool bUseDXT5NormalMap = false;

		FString UseDXT5NormalMapsString;

		if (EngineSettings.GetString(TEXT("SystemSettings"), TEXT("Compat.UseDXT5NormalMaps"), UseDXT5NormalMapsString))
		{
			bUseDXT5NormalMap = FCString::ToBool(*UseDXT5NormalMapsString);
		}

		ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();

		// Determine the pixel format of the (un/)compressed texture
		if (bNoCompression)
		{
			if (Texture->HasHDRSource())
			{
				TextureFormatName = NameRGBA16F;
			}
			else if (SourceFormat == TSF_G8 || Texture->CompressionSettings == TC_Grayscale)
			{
				TextureFormatName = NameG8;
			}
			else if (Texture->CompressionSettings == TC_Normalmap && bUseDXT5NormalMap)
			{
				TextureFormatName = NameXGXR8;
			}
			else
			{
				TextureFormatName = NameBGRA8;
			}
		}
		else if (Texture->CompressionSettings == TC_HDR)
		{
			TextureFormatName = NameRGBA16F;
		}
		else if (Texture->CompressionSettings == TC_Normalmap)
		{
			TextureFormatName = bUseDXT5NormalMap ? NameDXT5n : NameBC5;
		}
		else if (Texture->CompressionSettings == TC_Displacementmap)
		{
			TextureFormatName = NameG8;
		}
		else if (Texture->CompressionSettings == TC_VectorDisplacementmap)
		{
			TextureFormatName = NameBGRA8;
		}
		else if (Texture->CompressionSettings == TC_Grayscale)
		{
			TextureFormatName = NameG8;
		}
		else if ( Texture->CompressionSettings == TC_Alpha)
		{
			TextureFormatName = NameBC4;
		}
		else if (Texture->CompressionSettings == TC_DistanceFieldFont)
		{
			TextureFormatName = NameG8;
		}
		else if ( Texture->CompressionSettings == TC_HDR_Compressed )
		{
			TextureFormatName = NameBC6H;
		}
		else if ( Texture->CompressionSettings == TC_BC7 )
		{
			TextureFormatName = NameBC7;
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

		// fallback to non-DX11 formats if one was chosen, but we can't use it
		if (!bSupportDX11TextureFormats)
		{
			if (TextureFormatName == NameBC6H)
			{
				TextureFormatName = NameRGBA16F;
			}
			else if (TextureFormatName == NameBC7)
			{
				TextureFormatName = NameAutoDXT;
			}
		}

#endif //WITH_EDITOR

		return TextureFormatName;
	}
#else //TEXTURERESOURCE_H_INCLUDED

	FName GetDefaultTextureFormatName( const UTexture* Texture, const FConfigFile& EngineSettings, bool bSupportDX11TextureFormats ) const
	{
		return NAME_None;
	}

#endif //TEXTURERESOURCE_H_INCLUDED
#endif //WITH_ENGINE

	virtual bool PackageBuild( const FString& InPackgeDirectory ) override
	{
		return true;
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		return true;
	}

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready; // @todo How do we check that the iOS SDK is installed when building from Windows? Is that even possible?
		if (!IsSdkInstalled(bProjectHasCode, OutDocumentationPath))
		{
			bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
		}
		return bReadyToBuild;
	}

	virtual bool SupportsVariants() const override
	{
		return false;
	}

	virtual FText GetVariantDisplayName() const override
	{
		return FText();
	}

	virtual FText GetVariantTitle() const override
	{
		return FText();
	}

	virtual float GetVariantPriority() const override
	{
		return 0.0f;
	}

	virtual bool SendLowerCaseFilePaths() const override
	{
		return false;
	}

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		// do nothing in the base class
	}

	virtual int32 GetCompressionBitWindow() const override
	{
		return DEFAULT_ZLIB_BIT_WINDOW;
	}

protected:

	FTargetPlatformBase(const PlatformInfo::FPlatformInfo *const InPlatformInfo)
		: PlatformInfo(InPlatformInfo)
	{
		check(PlatformInfo);
	}

	/** Information about this platform */
	const PlatformInfo::FPlatformInfo *PlatformInfo;
};


/**
 * Template for target platforms.
 *
 * @param TPlatformProperties Type of platform properties.
 */
template<typename TPlatformProperties>
class TTargetPlatformBase
	: public FTargetPlatformBase
{
public:

	/** Default constructor. */
	TTargetPlatformBase()
		: FTargetPlatformBase( PlatformInfo::FindPlatformInfo(TPlatformProperties::PlatformName()) )
	{
		// HasEditorOnlyData and RequiresCookedData are mutually exclusive.
		check(TPlatformProperties::HasEditorOnlyData() != TPlatformProperties::RequiresCookedData());
	}

public:

	// ITargetPlatform interface

	virtual bool HasEditorOnlyData() const override
	{
		return TPlatformProperties::HasEditorOnlyData();
	}

	virtual bool IsLittleEndian() const override
	{
		return TPlatformProperties::IsLittleEndian();
	}

	virtual bool IsServerOnly() const override
	{
		return TPlatformProperties::IsServerOnly();
	}

	virtual bool IsClientOnly() const override
	{
		return TPlatformProperties::IsClientOnly();
	}

	virtual FString PlatformName() const override
	{
		return FString(TPlatformProperties::PlatformName());
	}

	virtual FString IniPlatformName() const override
	{
		return FString(TPlatformProperties::IniPlatformName());
	}

	virtual bool RequiresCookedData() const override
	{
		return TPlatformProperties::RequiresCookedData();
	}

	virtual bool RequiresUserCredentials() const override
	{
		return TPlatformProperties::RequiresUserCredentials();
	}

	virtual bool SupportsBuildTarget( EBuildTargets::Type BuildTarget ) const override
	{
		return TPlatformProperties::SupportsBuildTarget(BuildTarget);
	}

	virtual bool SupportsAutoSDK() const override
	{
		return TPlatformProperties::SupportsAutoSDK();
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetPlatformFeatures::AudioStreaming:
			return TPlatformProperties::SupportsAudioStreaming();

		case ETargetPlatformFeatures::DistanceFieldShadows:
			return TPlatformProperties::SupportsDistanceFieldShadows();

		case ETargetPlatformFeatures::GrayscaleSRGB:
			return TPlatformProperties::SupportsGrayscaleSRGB();

		case ETargetPlatformFeatures::HighQualityLightmaps:
			return TPlatformProperties::SupportsHighQualityLightmaps();

		case ETargetPlatformFeatures::LowQualityLightmaps:
			return TPlatformProperties::SupportsLowQualityLightmaps();

		case ETargetPlatformFeatures::MultipleGameInstances:
			return TPlatformProperties::SupportsMultipleGameInstances();

		case ETargetPlatformFeatures::Packaging:
			return false;

		case ETargetPlatformFeatures::Tessellation:
			return TPlatformProperties::SupportsTessellation();

		case ETargetPlatformFeatures::TextureStreaming:
			return TPlatformProperties::SupportsTextureStreaming();

		case ETargetPlatformFeatures::SdkConnectDisconnect:
		case ETargetPlatformFeatures::UserCredentials:
			break;

		case ETargetPlatformFeatures::MobileRendering:
			return false;
		case ETargetPlatformFeatures::DeferredRendering:
			return true;

		case ETargetPlatformFeatures::ShouldUseCompressedCookedPackages:
			return false;
		}

		return false;
	}
	

#if WITH_ENGINE
	virtual FName GetPhysicsFormat( class UBodySetup* Body ) const override
	{
		return FName(TPlatformProperties::GetPhysicsFormat());
	}
#endif // WITH_ENGINE
};
