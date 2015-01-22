// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Structure containing all information related to an LOD group and providing helper functions to calculate
 * the LOD bias of a given group.
 */
struct FTextureLODSettings
{
	/**
	 * Initializes LOD settings by reading them from the passed in filename/ section.
	 *
	 * @param	IniFilename		Filename of ini to read from.
	 * @param	IniSection		Section in ini to look for settings
	 */
	ENGINE_API void Initialize( const FString& IniFilename, const TCHAR* IniSection );

	/**
	 * Initializes LOD settings by reading them from the passed in filename/ section.
	 *
	 * @param	IniFile			Preloaded ini file object to load from
	 * @param	IniSection		Section in ini to look for settings
	 */
	ENGINE_API void Initialize(const FConfigFile& IniFile, const TCHAR* IniSection);

	/**
	 * Calculates and returns the LOD bias based on texture LOD group, LOD bias and maximum size.
	 *
	 * @param	Texture			Texture object to calculate LOD bias for.
	 * @param	bIncTextureMips	If true, takes the textures LOD & Cinematic mips into consideration
	 * @return	LOD bias
	 */
	ENGINE_API int32 CalculateLODBias(const UTexture* Texture, bool bIncTextureMips = true) const;

	/**
	 * Calculates and returns the LOD bias based on the information provided.
	 *
	 * @param	Width						Width of the texture
	 * @param	Height						Height of the texture
	 * @param	LODGroup					Which LOD group the texture belongs to
	 * @param	LODBias						LOD bias to include in the calculation
	 * @param	NumCinematicMipLevels		The texture cinematic mip levels to include in the calculation
	 * @param	MipGenSetting				Mip generation setting
	 * @return	LOD bias
	 */
	ENGINE_API int32 CalculateLODBias( int32 Width, int32 Height, int32 LODGroup, int32 LODBias, int32 NumCinematicMipLevels, TextureMipGenSettings MipGenSetting ) const;

	/** 
	* Useful for stats in the editor.
	*
	* @param LODBias			Default LOD at which the texture renders. Platform dependent, call FTextureLODSettings::CalculateLODBias(Texture)
	*/
	ENGINE_API void ComputeInGameMaxResolution(int32 LODBias, UTexture& Texture, uint32& OutSizeX, uint32& OutSizeY) const;

#if WITH_EDITORONLY_DATA
	void GetMipGenSettings( const UTexture& Texture, TextureMipGenSettings& OutMipGenSettings, float& OutSharpen, uint32& OutKernelSize, bool& bOutDownsampleWithAverage, bool& bOutSharpenWithoutColorShift, bool &bOutBorderColorBlack ) const;
#endif // #if WITH_EDITORONLY_DATA

	/**
	 * Will return the LODBias for a passed in LODGroup
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	LODBias
	 */
	ENGINE_API int32 GetTextureLODGroupLODBias( int32 InLODGroup ) const;

	/**
	 * Returns the LODGroup setting for number of streaming mip-levels.
	 * -1 means that all mip-levels are allowed to stream.
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	Number of streaming mip-levels for textures in the specified LODGroup
	 */
	ENGINE_API int32 GetNumStreamedMips( int32 InLODGroup ) const;

	ENGINE_API int32 GetMinLODMipCount( int32 InLODGroup ) const;
	ENGINE_API int32 GetMaxLODMipCount( int32 InLODGroup ) const;

	/**
	 * Returns the filter state that should be used for the passed in texture, taking
	 * into account other system settings.
	 *
	 * @param	Texture		Texture to retrieve filter state for, must not be 0
	 * @return	Filter sampler state for passed in texture
	 */
	ENGINE_API ESamplerFilter GetSamplerFilter( const UTexture* Texture ) const;

	ENGINE_API ESamplerFilter GetSamplerFilter( int32 InLODGroup) const;

	/**
	 * Returns the LODGroup mip gen settings
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	TextureMipGenSettings for lod group
	 */
	ENGINE_API const TextureMipGenSettings& GetTextureMipGenSettings( int32 InLODGroup ) const; 


	/**
	 * Returns the texture group names, sorted like enum.
	 *
	 * @return array of texture group names
	 */
	ENGINE_API static TArray<FString> GetTextureGroupNames();

	/** LOD settings for a single texture group. */
	struct FTextureLODGroup 
	{
		FTextureLODGroup()
		:	MinLODMipCount(0)
		,	MaxLODMipCount(12)
		,	LODBias(0) 
		,	Filter(SF_AnisotropicPoint)
		,	NumStreamedMips(-1)
		,	MipGenSettings(TMGS_SimpleAverage)
		{}
		/** Minimum LOD mip count below which the code won't bias.						*/
		int32 MinLODMipCount;
		/** Maximum LOD mip count. Bias will be adjusted so texture won't go above.		*/
		int32 MaxLODMipCount;
		/** Group LOD bias.																*/
		int32 LODBias;
		/** Sampler filter state.														*/
		ESamplerFilter Filter;
		/** Number of mip-levels that can be streamed. -1 means all mips can stream.	*/
		int32 NumStreamedMips;
		/** Defines how the the mip-map generation works, e.g. sharpening				*/
		TextureMipGenSettings MipGenSettings;
	};

protected:
	/**
	 * Reads a single entry and parses it into the group array.
	 *
	 * @param	GroupId			Id/ enum of group to parse
	 * @param	GroupName		Name of group to look for in ini
	 * @param	IniFile			Config file to read from.
	 * @param	IniSection		Section in ini to look for settings
	 */
	void ReadEntry( int32 GroupId, const TCHAR* GroupName, const FConfigFile& IniFile, const TCHAR* IniSection );

	/**
	 * TextureLODGroups access with bounds check
	 *
	 * @param   GroupIndex      usually from Texture.LODGroup
	 * @return                  A handle to the indexed LOD group. 
	 */
	const FTextureLODGroup& GetTextureLODGroup(TextureGroup GroupIndex) const;

	/** Array of LOD settings with entries per group. */
	FTextureLODGroup TextureLODGroups[TEXTUREGROUP_MAX];
};
