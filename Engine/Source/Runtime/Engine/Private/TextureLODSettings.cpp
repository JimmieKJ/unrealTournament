// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UTextureLODSettings::UTextureLODSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/**
 * Returns the texture group names, sorted like enum.
 *
 * @return array of texture group names
 */
TArray<FString> UTextureLODSettings::GetTextureGroupNames()
{
	TArray<FString> TextureGroupNames;

#define GROUPNAMES(g) new(TextureGroupNames) FString(TEXT(#g));
	FOREACH_ENUM_TEXTUREGROUP(GROUPNAMES)
#undef GROUPNAMES

	return TextureGroupNames;
}

void UTextureLODSettings::SetupLODGroup(int32 GroupId)
{
	TextureLODGroups[GroupId].MinLODMipCount = FMath::CeilLogTwo(TextureLODGroups[GroupId].MinLODSize);
	TextureLODGroups[GroupId].MaxLODMipCount = FMath::CeilLogTwo(TextureLODGroups[GroupId].MaxLODSize);

	// Convert into single filter enum. The code is layed out such that invalid input will 
	// map to the default state of highest quality filtering.

	// Linear filtering
	if (TextureLODGroups[GroupId].MinMagFilter == NAME_Linear)
	{
		if (TextureLODGroups[GroupId].MipFilter == NAME_Point)
		{
			TextureLODGroups[GroupId].Filter = ETextureSamplerFilter::Bilinear;
		}
		else
		{
			TextureLODGroups[GroupId].Filter = ETextureSamplerFilter::Trilinear;
		}
	}
	// Point. Don't even care about mip filter.
	else if (TextureLODGroups[GroupId].MinMagFilter == NAME_Point)
	{
		TextureLODGroups[GroupId].Filter = ETextureSamplerFilter::Point;
	}
	// Aniso or unknown.
	else
	{
		if (TextureLODGroups[GroupId].MipFilter == NAME_Point)
		{
			TextureLODGroups[GroupId].Filter = ETextureSamplerFilter::AnisotropicPoint;
		}
		else
		{
			TextureLODGroups[GroupId].Filter = ETextureSamplerFilter::AnisotropicLinear;
		}
	}
}

int32 UTextureLODSettings::CalculateLODBias(const UTexture* Texture, bool bIncTextureMips) const
{	
	check( Texture );
	TextureMipGenSettings MipGenSetting = TMGS_MAX;
#if WITH_EDITORONLY_DATA
	MipGenSetting = Texture->MipGenSettings;
#endif // #if WITH_EDITORONLY_DATA
	return CalculateLODBias(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight(), Texture->LODGroup, (bIncTextureMips ? Texture->LODBias : 0), (bIncTextureMips ? Texture->NumCinematicMipLevels : 0), MipGenSetting);
}

int32 UTextureLODSettings::CalculateLODBias(int32 Width, int32 Height, int32 LODGroup, int32 LODBias, int32 NumCinematicMipLevels, TextureMipGenSettings InMipGenSetting) const
{	
	// Find LOD group.
	const FTextureLODGroup& LODGroupInfo = TextureLODGroups[LODGroup];

	// Test to see if we have no mip generation as in which case the LOD bias will be ignored
	const TextureMipGenSettings FinalMipGenSetting = (InMipGenSetting == TMGS_FromTextureGroup) ? (TextureMipGenSettings)LODGroupInfo.MipGenSettings : InMipGenSetting;
	if ( FinalMipGenSetting == TMGS_NoMipmaps )
	{
		return 0;
	}

	// Calculate maximum number of miplevels.
	int32 TextureMaxLOD	= FMath::CeilLogTwo( FMath::TruncToInt( FMath::Max( Width, Height ) ) );

	// Calculate LOD bias.
	int32 UsedLODBias	= LODGroupInfo.LODBias + LODBias + NumCinematicMipLevels;
	int32 MinLOD		= LODGroupInfo.MinLODMipCount;
	int32 MaxLOD		= LODGroupInfo.MaxLODMipCount;
	int32 WantedMaxLOD	= FMath::Clamp( TextureMaxLOD - UsedLODBias, MinLOD, MaxLOD );
	WantedMaxLOD		= FMath::Clamp( WantedMaxLOD, 0, TextureMaxLOD );
	UsedLODBias			= TextureMaxLOD - WantedMaxLOD;

	return UsedLODBias;
}

/** 
* Useful for stats in the editor.
*/
void UTextureLODSettings::ComputeInGameMaxResolution(int32 LODBias, UTexture &Texture, uint32 &OutSizeX, uint32 &OutSizeY) const
{
	uint32 ImportedSizeX = FMath::TruncToInt(Texture.GetSurfaceWidth());
	uint32 ImportedSizeY = FMath::TruncToInt(Texture.GetSurfaceHeight());
	
	const FTextureLODGroup& LODGroup = GetTextureLODGroup((TextureGroup)Texture.LODGroup);

	uint32 SourceLOD = FMath::Max(FMath::CeilLogTwo(ImportedSizeX), FMath::CeilLogTwo(ImportedSizeY));
	uint32 MinLOD = FMath::Max(uint32(UTexture2D::GetMinTextureResidentMipCount() - 1), (uint32)LODGroup.MinLODMipCount);
	uint32 MaxLOD = FMath::Min(uint32(GMaxTextureMipCount - 1), (uint32)LODGroup.MaxLODMipCount);
	uint32 GameLOD = FMath::Min(SourceLOD, FMath::Clamp(SourceLOD - LODBias, MinLOD, MaxLOD));

	uint32 DeltaLOD = SourceLOD - GameLOD;

	OutSizeX = ImportedSizeX >> DeltaLOD;
	OutSizeY = ImportedSizeY >> DeltaLOD;
}

/**
* TextureLODGroups access with bounds check
*
* @param   GroupIndex      usually from Texture.LODGroup
* @return                  A handle to the indexed LOD group. 
*/
FTextureLODGroup& UTextureLODSettings::GetTextureLODGroup(TextureGroup GroupIndex)
{
	check(GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX);
	return TextureLODGroups[GroupIndex];
}

/**
* TextureLODGroups access with bounds check
*
* @param   GroupIndex      usually from Texture.LODGroup
* @return                  A handle to the indexed LOD group.
*/
const FTextureLODGroup& UTextureLODSettings::GetTextureLODGroup(TextureGroup GroupIndex) const
{
	check(GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX);
	return TextureLODGroups[GroupIndex];
}

#if WITH_EDITORONLY_DATA
void UTextureLODSettings::GetMipGenSettings(const UTexture& Texture, TextureMipGenSettings& OutMipGenSettings, float& OutSharpen, uint32& OutKernelSize, bool& bOutDownsampleWithAverage, bool& bOutSharpenWithoutColorShift, bool &bOutBorderColorBlack) const
{
	TextureMipGenSettings Setting = (TextureMipGenSettings)Texture.MipGenSettings;

	bOutBorderColorBlack = false;

	// avoiding the color shift assumes we deal with colors which is not true for normalmaps
	// or we blur where it's good to blur the color as well
	bOutSharpenWithoutColorShift = !Texture.IsNormalMap();

	bOutDownsampleWithAverage = true;

	// inherit from texture group
	if(Setting == TMGS_FromTextureGroup)
	{
		const FTextureLODGroup& LODGroup = TextureLODGroups[Texture.LODGroup];

		Setting = LODGroup.MipGenSettings;
	}
	OutMipGenSettings = Setting;

	// ------------

	// default:
	OutSharpen = 0;
	OutKernelSize = 2;

	if(Setting >= TMGS_Sharpen0 && Setting <= TMGS_Sharpen10)
	{
		// 0 .. 2.0f
		OutSharpen = ((int32)Setting - (int32)TMGS_Sharpen0) * 0.2f;
		OutKernelSize = 8;
	}
	else if(Setting >= TMGS_Blur1 && Setting <= TMGS_Blur5)
	{
		int32 BlurFactor = ((int32)Setting + 1 - (int32)TMGS_Blur1);
		OutSharpen = -BlurFactor * 2;
		OutKernelSize = 2 + 2 * BlurFactor;
		bOutDownsampleWithAverage = false;
		bOutSharpenWithoutColorShift = false;
		bOutBorderColorBlack = true;
	}
}
#endif // #if WITH_EDITORONLY_DATA

/**
 * Will return the LODBias for a passed in LODGroup
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	LODBias
 */
int32 UTextureLODSettings::GetTextureLODGroupLODBias(int32 InLODGroup) const
{
	int32 Retval = 0;

	const FTextureLODGroup& LODGroup = TextureLODGroups[InLODGroup]; 

	Retval = LODGroup.LODBias;

	return Retval;
}



/**
 * Returns the LODGroup setting for number of streaming mip-levels.
 * -1 means that all mip-levels are allowed to stream.
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	Number of streaming mip-levels for textures in the specified LODGroup
 */
int32 UTextureLODSettings::GetMinLODMipCount(int32 InLODGroup) const
{
	int32 Retval = 0;

	const FTextureLODGroup& LODGroup = TextureLODGroups[InLODGroup]; 

	Retval = LODGroup.MinLODMipCount;

	return Retval;
}




/**
 * Returns the LODGroup setting for number of streaming mip-levels.
 * -1 means that all mip-levels are allowed to stream.
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	Number of streaming mip-levels for textures in the specified LODGroup
 */
int32 UTextureLODSettings::GetMaxLODMipCount(int32 InLODGroup) const
{
	int32 Retval = 0;

	const FTextureLODGroup& LODGroup = TextureLODGroups[InLODGroup]; 

	Retval = LODGroup.MaxLODMipCount;

	return Retval;
}


/**
 * Returns the LODGroup setting for number of streaming mip-levels.
 * -1 means that all mip-levels are allowed to stream.
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	Number of streaming mip-levels for textures in the specified LODGroup
 */
int32 UTextureLODSettings::GetNumStreamedMips(int32 InLODGroup) const
{
	int32 Retval = 0;

	const FTextureLODGroup& LODGroup = TextureLODGroups[InLODGroup]; 

	Retval = LODGroup.NumStreamedMips;

	return Retval;
}

/**
 * Returns the LODGroup mip gen settings
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	TextureMipGenSettings for lod group
 */
const TextureMipGenSettings UTextureLODSettings::GetTextureMipGenSettings(int32 InLODGroup) const
{
	return TextureLODGroups[InLODGroup].MipGenSettings; 
}



/**
 * Returns the filter state that should be used for the passed in texture, taking
 * into account other system settings.
 *
 * @param	Texture		Texture to retrieve filter state for, must not be 0
 * @return	Filter sampler state for passed in texture
 */
ETextureSamplerFilter UTextureLODSettings::GetSamplerFilter(const UTexture* Texture) const
{
	// Default to point filtering.
	ETextureSamplerFilter Filter = ETextureSamplerFilter::Point;

	switch(Texture->Filter)
	{
		case TF_Nearest: Filter = ETextureSamplerFilter::Point; break;
		case TF_Bilinear: Filter = ETextureSamplerFilter::Bilinear; break;
		case TF_Trilinear: Filter = ETextureSamplerFilter::Trilinear; break;

		// TF_Default
		default:
			// Use LOD group value to find proper filter setting.
			Filter = TextureLODGroups[Texture->LODGroup].Filter;
	}

	return Filter;
}

ETextureSamplerFilter UTextureLODSettings::GetSamplerFilter(int32 InLODGroup) const
{
	return TextureLODGroups[InLODGroup].Filter;
}