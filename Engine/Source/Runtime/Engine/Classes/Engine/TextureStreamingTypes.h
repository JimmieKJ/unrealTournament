// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Structs and defines used for texture streaming build

#pragma once 

#include "EngineTypes.h"
#include "TextureStreamingTypes.generated.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(TextureStreamingBuild, Log, All);

class UTexture;
class UTexture2D;
struct FStreamingTextureBuildInfo;

/** Information about a streaming texture that a primitive uses for rendering. */
USTRUCT()
struct FStreamingTexturePrimitiveInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UTexture2D* Texture;

	UPROPERTY()
	FBoxSphereBounds Bounds;

	UPROPERTY()
	float TexelFactor;

	FStreamingTexturePrimitiveInfo() : 
		Texture(nullptr), 
		Bounds(ForceInit), 
		TexelFactor(1.0f) 
	{
	}


	/**
	 *	Set this struct to match the packed params.
	 *
	 *	@param	InTexture			The texture as refered by the packed info.
	 * 	@param	ExtraScale			Extra scale to be applied to the texcoord world size.
	 *	@param	RefBounds			The reference bounds used to unpack the relative box.
	 *	@param	Info				The packed params.
	 *	@param	bUseRelativeBox		true if the relative box is relevant. Could be irrelevant if a level transform was applied after the texture streaming build.
	 */
	ENGINE_API void UnPackFrom(UTexture2D* InTexture, float ExtraScale, const FBoxSphereBounds& RefBounds, const FStreamingTextureBuildInfo& Info, bool bUseRelativeBox);
};

/** 
 * This struct holds the result of TextureStreaming Build for each component texture, as referred by its used materials.
 * It is possible that the entry referred by this data is not actually relevant in a given quality / target.
 * It is also possible that some texture are not referred, and will then fall on fallbacks computation.
 * Because each component holds its precomputed data for each texture, this struct is designed to be as compact as possible.
 */
USTRUCT()
struct FStreamingTextureBuildInfo
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * The relative bounding box for this entry. The relative bounds is a bound equal or smaller than the component bounds and represent
	 * the merged LOD section bounds of all LOD section referencing the given texture. When the level transform is modified following
	 * a call to ApplyLevelTransform, this relative bound becomes deprecated as it was computed from the transform at build time.
	 */
	UPROPERTY()
	uint32 PackedRelativeBox;

	/** 
	 * The level scope identifier of the texture. When building the texture streaming data, each level holds a list of all referred texture Guids.
	 * This is required to prevent loading textures on platforms which would not require the texture to be loaded, and is a consequence of the texture
	 * streaming build not being platform specific (the same streaming data is build for every platform target). Could also apply to quality level.
	 */
	UPROPERTY()
	int32 TextureLevelIndex;

	/** 
	 * The texel factor for this texture. This represent the world size a texture square holding with unit UVs.
	 * This value is a combination of the TexelFactor from the mesh and also the material scale.
	 * It does not take into consideration StreamingDistanceMultiplier, or texture group scale.
	 */
	UPROPERTY()
	float TexelFactor;

	FStreamingTextureBuildInfo() : 
		PackedRelativeBox(0), 
		TextureLevelIndex(0), 
		TexelFactor(0) 
	{
	}

	/**
	 *	Set this struct to match the unpacked params.
	 *
	 *	@param	LevelTextures	[in,out]	The list of textures referred by all component of a level. The array index maps to UTexture2D::LevelIndex.
	 *	@param	RefBounds		[in]		The reference bounds used to compute the packed relative box.
	 *	@param	Info			[in]		The unpacked params.
	 */
	ENGINE_API void PackFrom(TArray<UTexture2D*>& LevelTextures, const FBoxSphereBounds& RefBounds, const FStreamingTexturePrimitiveInfo& Info);

};

/** 
 * This struct holds data about the texture coordinate used to sample a texture within a material.
 */
struct FMaterialTexCoordBuildInfo
{
	enum 
	{
		MAX_NUM_TEX_COORD = 4,			// The max number of texcoords processed in the analysis.
		INITIAL_GPU_SCALE = 256,		// The initial texcoord scale (must be bigger than actual used values)
		TILE_RESOLUTION = 32,			// The tile size of the render used when analysis the shader scales.
		MAX_NUM_TEXTURE_REGISTER = 32	// The max number of texture indices processed in the analysis.
	};

	FMaterialTexCoordBuildInfo() : Scale(0), Index(INDEX_NONE) {}

	/** The coordinate scale */
	float Scale;

	/** The coordinate index */
	int32 Index;
};

/** 
 * This struct holds the transient data used in the texture streaming build and is also useful for debugging results.
 */
struct FStreamingSectionBuildInfo
{
	FStreamingSectionBuildInfo() : LODIndex(INDEX_NONE), ElementIndex(INDEX_NONE), MaterialIndex(INDEX_NONE), BoxOrigin(0, 0, 0), BoxExtent(0, 0, 0)  
	{
		FMemory::Memzero(TexelFactors);
	}

	/** LOD Index related to this data. */
	int32 LODIndex;

	/** Element (Section) Index related to this data. */
	int32 ElementIndex;

	/** The material index used for this LOD Section. */
	int32 MaterialIndex;

	/** The world space box origin of the lod section.*/
	FVector	BoxOrigin;

	/** The world space box extent of the lod section.*/
	FVector BoxExtent;

	/** The texture world size associated to the mesh UV mapping.*/
	TArray<FMaterialTexCoordBuildInfo> TexCoordData;

	/** The texture world size computed from the mesh UV mapping.*/
	float TexelFactors[FMaterialTexCoordBuildInfo::MAX_NUM_TEX_COORD];
};

/** 
 * Context used to resolve FStreamingTextureBuildInfo to FStreamingTexturePrimitiveInfo
 * The context make sure that build data and each texture is only processed once per component (with constant time).
 * It manage internally structures used to accelerate the binding between precomputed data and textures,
 * so that there is only one map lookup per texture per level. 
 * There is some complexity here because the build data does not reference directly texture objects to avoid hard references
 * which would load texture when the component is loaded, which could be wrong since the build data is built for a specific
 * feature level and quality level. The current feature and quality could reference more or less textures.
 * This requires the logic to not submit a streaming entry for precomputed data, as well as submit fallback data for 
 * texture that were referenced in the texture streaming build.
 */
class FStreamingTextureLevelContext
{
	/** Reversed lookup for ULevel::StreamingTextureGuids. */
	TMap<FGuid, int32> TextureGuidToLevelIndex;

	/** List of texture for which LevelIndex was set. Used to reset the value to INDEX_NONE after the level was processed. */
	TArray<UTexture2D*> ProcessedTextures;

	/*
	 * Whether the precomputed relative bounds should be used or not. 
	 * Will be false if the transform level was rotated  since the last texture streaming build.
	 */
	bool bUseRelativeBoxes;

	/** An id used to identify the component. */
	int32 ComponentTimestamp;

	/** The last bound component texture streaming build data. */
	const TArray<FStreamingTextureBuildInfo>* ComponentBuildData;
	/** The last bound component bounds. */
	FBoxSphereBounds ComponentBounds;
	/** The last bound component precomputed data scale. */
	float ComponentPrecomputedDataScale;
	/** The last bound component streaming fallback scale. */
	float ComponentFallbackScale;

	struct FTextureBoundState
	{
		/** The component timestamp that last component referring this texture. */
		int32 Timestamp;
		/** The component timestamp that last component having build data for this texture. */
		int32 BuildDataTimestamp;
		/** The ComponentBuildData Index referring this texture. */
		int32 BuildDataIndex;
	};

	/*
	 * The component state of the each texture. Used to prevent processing each texture several time.
	 * Also used to find quickly the build data relating to each texture. 
	 */
	TArray<FTextureBoundState> BoundStates;

public:

	FStreamingTextureLevelContext(const ULevel* InLevel = nullptr);
	~FStreamingTextureLevelContext();

	void BindComponent(const TArray<FStreamingTextureBuildInfo>* BuildData, const FBoxSphereBounds& Bounds, float PrecomputedDataScale, float FallbackScale);
	void Process(const TArray<UTexture*>& InTextures, TArray<FStreamingTexturePrimitiveInfo>& OutInfos);
};


/** A Map that gives the (smallest) texture coordinate scale used when sampling each texture register of a shader.
 * The array index is the register index, and the value, is the coordinate scale.
 * Since a texture resource can be bound to several texture registers, it can related to different indices.
 * This is reflected in UMaterialInterface::GetUsedTexturesAndIndices where each texture is bound to 
 * an array of texture register indices.
 */
typedef TMap<UMaterialInterface*, TArray<FMaterialTexCoordBuildInfo> > FTexCoordScaleMap;

/** A mapping between used material and levels for refering primitives. */
typedef TMap<UMaterialInterface*, TArray<ULevel*> > FMaterialToLevelsMap;

/** Build the shaders required for the texture streaming build. Returns whether or not the action was successful. */
ENGINE_API bool BuildTextureStreamingShaders(UWorld* InWorld, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, OUT FTexCoordScaleMap& TexCoordScales, OUT FMaterialToLevelsMap& MaterialToLevels, bool bIncremental, FSlowTask& BuildTextureStreamingTask);
ENGINE_API bool UpdateComponentStreamingSectionData(UWorld* InWorld, const FTexCoordScaleMap& InTexCoordScales, bool bIncremental, FSlowTask& BuildTextureStreamingTask);
ENGINE_API bool BuildTextureStreamingData(UWorld* InWorld, const FTexCoordScaleMap& InTexCoordScales, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, FSlowTask& BuildTextureStreamingTask);

/** Check if the lighting build is dirty. Updates the needs rebuild status of the level. */
ENGINE_API void CheckTextureStreamingBuild(ULevel* InLevel);
/** Check if the lighting build is dirty. Updates the needs rebuild status of the level and world. */
ENGINE_API void CheckTextureStreamingBuild(UWorld* InWorld);

extern ENGINE_API TAutoConsoleVariable<int32> CVarStreamingUseNewMetrics;