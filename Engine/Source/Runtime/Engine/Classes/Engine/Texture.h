// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TextureDefines.h"
#include "TextureResource.h"
#include "MaterialShared.h"
#include "RenderResource.h"
#include "Texture.generated.h"

// This needs to be mirrored in EditorFactories.cpp.
UENUM()
enum TextureCompressionSettings
{
	TC_Default,
	TC_Normalmap,
	TC_Masks,
	TC_Grayscale,
	TC_Displacementmap,
	TC_VectorDisplacementmap,
	TC_HDR,
	TC_EditorIcon UMETA(DisplayName="TC_UserInterface2D"),
	TC_Alpha,
	TC_DistanceFieldFont,
	TC_HDR_Compressed,
	TC_BC7,
	TC_MAX,
};

UENUM()
enum TextureFilter
{
	TF_Nearest UMETA(DisplayName="Nearest"),
	TF_Bilinear UMETA(DisplayName="Bi-linear"),
	TF_Trilinear UMETA(DisplayName="Tri-linear"),
	/** use setting from the Texture Group */
	TF_Default UMETA(DisplayName="Default (from Texture Group)"),
	TF_MAX,
};

UENUM()
enum TextureAddress
{
	TA_Wrap UMETA(DisplayName="Wrap"),
	TA_Clamp UMETA(DisplayName="Clamp"),
	TA_Mirror UMETA(DisplayName="Mirror"),
	TA_MAX,
};

UENUM()
enum ECompositeTextureMode
{
	CTM_Disabled UMETA(DisplayName="Disabled"),
	// CompositingTexture needs to be a normal map with the same or larger size
	CTM_NormalRoughnessToRed UMETA(DisplayName="Add Normal Roughness To Red"),
	// CompositingTexture needs to be a normal map with the same or larger size
	CTM_NormalRoughnessToGreen UMETA(DisplayName="Add Normal Roughness To Green"),
	// CompositingTexture needs to be a normal map with the same or larger size
	CTM_NormalRoughnessToBlue UMETA(DisplayName="Add Normal Roughness To Blue"),
	// CompositingTexture needs to be a normal map with the same or larger size
	CTM_NormalRoughnessToAlpha UMETA(DisplayName="Add Normal Roughness To Alpha"),
	CTM_MAX,
};

UENUM()
enum ETextureMipCount
{
	TMC_ResidentMips,
	TMC_AllMips,
	TMC_AllMipsBiased,
	TMC_MAX,
};

UENUM()
enum ETextureSourceArtType
{
	// FColor Data[SrcWidth * SrcHeight]
	TSAT_Uncompressed,
	// PNG compresed version of FColor Data[SrcWidth * SrcHeight]
	TSAT_PNGCompressed,
	// DDS file with header
	TSAT_DDSFile,
	TSAT_MAX,
};

UENUM()
enum ETextureSourceFormat
{
	TSF_Invalid,
	TSF_G8,
	TSF_BGRA8,
	TSF_BGRE8,
	TSF_RGBA16,
	TSF_RGBA16F,

	//@todo: Deprecated!
	TSF_RGBA8,
	//@todo: Deprecated!
	TSF_RGBE8,

	TSF_MAX
};

/**
 * Texture source data management.
 */
USTRUCT()
struct FTextureSource
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor. */
	ENGINE_API FTextureSource();

#if WITH_EDITOR
	/**
	 * Initialize the source data with the given size, number of mips, and format.
	 * @param NewSizeX - Width of the texture source data.
	 * @param NewSizeY - Height of the texture source data.
	 * @param NewNumSlices - The number of slices in the texture source data.
	 * @param NewNumMips - The number of mips in the texture source data.
	 * @param NewFormat - The format in which source data is stored.
	 * @param NewData - [optional] The new source data.
	 */
	ENGINE_API void Init(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumSlices,
		int32 NewNumMips,
		ETextureSourceFormat NewFormat,
		const uint8* NewData = NULL
		);

	/**
	 * Initializes the source data for a 2D texture with a full mip chain.
	 * @param NewSizeX - Width of the texture source data.
	 * @param NewSizeY - Height of the texture source data.
	 * @param NewFormat - Format of the texture source data.
	 */
	ENGINE_API void Init2DWithMipChain(
		int32 NewSizeX,
		int32 NewSizeY,
		ETextureSourceFormat NewFormat
		);

	/**
	 * Initializes the source data for a cubemap with a full mip chain.
	 * @param NewSizeX - Width of each cube map face.
	 * @param NewSizeY - Height of each cube map face.
	 * @param NewFormat - Format of the cube map source data.
	 */
	ENGINE_API void InitCubeWithMipChain(
		int32 NewSizeX,
		int32 NewSizeY,
		ETextureSourceFormat NewFormat
		);

	/** PNG Compresses the source art if possible or tells the bulk data to zlib compress when it saves out to disk. */
	ENGINE_API void Compress();

	/** Force the GUID to change even if mip data has not been modified. */
	void ForceGenerateGuid();

	/** Lock a mip for editing. */
	ENGINE_API uint8* LockMip(int32 MipIndex);

	/** Unlock a mip. */
	ENGINE_API void UnlockMip(int32 MipIndex);

	/** Retrieve a copy of the data for a particular mip. */
	ENGINE_API bool GetMipData(TArray<uint8>& OutMipData, int32 MipIndex);

	/** Computes the size of a single mip. */
	ENGINE_API int32 CalcMipSize(int32 MipIndex) const;

	/** Computes the number of bytes per-pixel. */
	ENGINE_API int32 GetBytesPerPixel() const;

	/** Return true if the source data is power-of-2. */
	bool IsPowerOfTwo() const;

	/** Returns true if source art is available. */
	ENGINE_API bool IsValid() const;

	/** Returns the unique ID string for this source art. */
	FString GetIdString() const;

	/** Trivial accessors. */
	FORCEINLINE FGuid GetId() const { return Id; }
	FORCEINLINE int32 GetSizeX() const { return SizeX; }
	FORCEINLINE int32 GetSizeY() const { return SizeY; }
	FORCEINLINE int32 GetNumSlices() const { return NumSlices; }
	FORCEINLINE int32 GetNumMips() const { return NumMips; }
	FORCEINLINE ETextureSourceFormat GetFormat() const { return Format; }
	FORCEINLINE bool IsPNGCompressed() const { return bPNGCompressed; }
	FORCEINLINE int32 GetSizeOnDisk() const { return BulkData.GetBulkDataSize(); }
#endif

private:
	/** Allow UTexture access to internals. */
	friend class UTexture;
	friend class UTexture2D;
	friend class UTextureCube;

	/** The bulk source data. */
	FByteBulkData BulkData;
	/** Pointer to locked mip data, if any. */
	uint8* LockedMipData;
	/** Which mips are locked, if any. */
	uint32 LockedMips;

#if WITH_EDITOR
	/** Return true if the source art is not png compressed but could be. */
	bool CanPNGCompress() const;

	/** Removes source data. */
	void RemoveSourceData();

	/** Retrieve the size and offset for a source mip. The size includes all slices. */
	int32 CalcMipOffset(int32 MipIndex) const;

	/** Uses a hash as the GUID, useful to prevent creating new GUIDs on load for legacy assets. */
	void UseHashAsGuid();
#endif

#if WITH_EDITORONLY_DATA
	/** GUID used to track changes to the source data. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	FGuid Id;

	/** Width of the texture. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 SizeX;

	/** Height of the texture. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 SizeY;

	/** Depth (volume textures) or faces (cube maps). */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 NumSlices;

	/** Number of mips provided as source data for the texture. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 NumMips;

	/** RGBA8 source data is optionally compressed as PNG. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	bool bPNGCompressed;

	/** Legacy textures use a hash instead of a GUID. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	bool bGuidIsHash;

	/** Format in which the source data is stored. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	TEnumAsByte<enum ETextureSourceFormat> Format;
#endif // WITH_EDITORONLY_DATA
};

/**
 * Platform-specific data used by the texture resource at runtime.
 */
USTRUCT()
struct FTexturePlatformData
{
	GENERATED_USTRUCT_BODY()

	/** Width of the texture. */
	int32 SizeX;
	/** Height of the texture. */
	int32 SizeY;
	/** Number of texture slices. */
	int32 NumSlices;
	/** Format in which mip data is stored. */
	EPixelFormat PixelFormat;
	/** Mip data. */
	TIndirectArray<struct FTexture2DMipMap> Mips;

#if WITH_EDITORONLY_DATA
	/** The key associated with this derived data. */
	FString DerivedDataKey;
	/** Async cache task if one is outstanding. */
	struct FTextureAsyncCacheDerivedDataTask* AsyncTask;
#endif

	/** Default constructor. */
	ENGINE_API FTexturePlatformData();

	/** Destructor. */
	ENGINE_API ~FTexturePlatformData();

	/**
	 * Try to load mips from the derived data cache.
	 * @param FirstMipToLoad - The first mip index to load.
	 * @param OutMipData -	Must point to an array of pointers with at least
	 *						Texture.Mips.Num() - FirstMipToLoad + 1 entries. Upon
	 *						return those pointers will contain mip data.
	 * @returns true if all requested mips have been loaded.
	 */
	bool TryLoadMips(int32 FirstMipToLoad, void** OutMipData);

	/** Serialization. */
	void Serialize(FArchive& Ar, class UTexture* Owner);

	/** Serialization for cooked builds. */
	void SerializeCooked(FArchive& Ar, class UTexture* Owner);

#if WITH_EDITOR
	void Cache(
		class UTexture& InTexture,
		const struct FTextureBuildSettings& InSettings,
		uint32 InFlags
		);
	void FinishCache();
	ENGINE_API bool TryInlineMipData();
	bool AreDerivedMipsAvailable() const;
#endif

};

UCLASS(abstract, MinimalAPI, BlueprintType)
class UTexture : public UObject
{
	GENERATED_UCLASS_BODY()

	/*--------------------------------------------------------------------------
		Editor only properties used to build the runtime texture data.
	--------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FTextureSource Source;

private:
	/** Unique ID for this material, used for caching during distributed lighting */
	UPROPERTY()
	FGuid LightingGuid;

public:
	/** Path to the resource used to construct this texture. Relative to the object's package, BaseDir() or absolute */
	UPROPERTY(Category=Texture, VisibleAnywhere, BlueprintReadWrite)
	FString SourceFilePath;

	/** Date/Time-stamp of the file from the last import. */
	UPROPERTY(Category=Texture, VisibleAnywhere, BlueprintReadWrite)
	FString SourceFileTimestamp;

	/** Static texture brightness adjustment (scales HSV value.)  (Non-destructive; Requires texture source art to be available.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Brightness"))
	float AdjustBrightness;

	/** Static texture curve adjustment (raises HSV value to the specified power.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Brightness Curve"))
	float AdjustBrightnessCurve;

	/** Static texture "vibrance" adjustment (0 - 1) (HSV saturation algorithm adjustment.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Vibrance", ClampMin = "0.0", ClampMax = "1.0"))
	float AdjustVibrance;

	/** Static texture saturation adjustment (scales HSV saturation.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Saturation"))
	float AdjustSaturation;

	/** Static texture RGB curve adjustment (raises linear-space RGB color to the specified power.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "RGBCurve"))
	float AdjustRGBCurve;

	/** Static texture hue adjustment (0 - 360) (offsets HSV hue by value in degrees.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Hue", ClampMin = "0.0", ClampMax = "360.0"))
	float AdjustHue;

	/** Remaps the alpha to the specified min/max range, defines the new value of 0 (Non-destructive; Requires texture source art to be available.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Min Alpha"))
	float AdjustMinAlpha;

	/** Remaps the alpha to the specified min/max range, defines the new value of 1 (Non-destructive; Requires texture source art to be available.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Max Alpha"))
	float AdjustMaxAlpha;

	/** If enabled, the texture's alpha channel will be discarded during compression */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compression, meta=(DisplayName="Compress Without Alpha"))
	uint32 CompressionNoAlpha:1;

	UPROPERTY()
	uint32 CompressionNone:1;

	/** If enabled, defer compression of the texture until save. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compression)
	uint32 DeferCompression:1;

	/** The maximum resolution for generated textures. A value of 0 means the maximum size for the format on each platform, except HDR long/lat cubemaps, which default to a resolution of 512. */ 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Compression, meta=(DisplayName="Maximum Texture Size", ClampMin = "0.0"), AdvancedDisplay)
	int32 MaxTextureSize;

	/** When true, the alpha channel of mip-maps and the base image are dithered for smooth LOD transitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, AdvancedDisplay)
	uint32 bDitherMipMapAlpha:1;

	/** When true the texture's border will be preserved during mipmap generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail, AdvancedDisplay)
	uint32 bPreserveBorder:1;

	/** When true the texture's green channel will be inverted. This is useful for some normal maps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, AdvancedDisplay)
	uint32 bFlipGreenChannel:1;

	/** For DXT1 textures, setting this will cause the texture to be twice the size, but better looking, on iPhone */
	UPROPERTY()
	uint32 bForcePVRTC4:1;

	/** Per asset specific setting to define the mip-map generation properties like sharpening and kernel size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail)
	TEnumAsByte<enum TextureMipGenSettings> MipGenSettings;
	
	/**
	 * Can be defined to modify the roughness based on the normal map variation (mostly from mip maps).
	 * MaxAlpha comes in handy to define a base roughness if no source alpha was there.
	 * Make sure the normal map has at least as many mips as this texture.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compositing)
	class UTexture* CompositeTexture;

	/* defines how the CompositeTexture is applied, e.g. CTM_RoughnessFromNormalAlpha */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compositing, AdvancedDisplay)
	TEnumAsByte<enum ECompositeTextureMode> CompositeTextureMode;

	/**
	 * default 1, high values result in a stronger effect e.g 1, 2, 4, 8
	 * this is no slider because the texture update would not be fast enough
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compositing, AdvancedDisplay)
	float CompositePower;

#endif // WITH_EDITORONLY_DATA

	/*--------------------------------------------------------------------------
		Properties needed at runtime below.
	--------------------------------------------------------------------------*/

	/** A bias to the index of the top mip level to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail, meta=(DisplayName="LOD Bias"), AssetRegistrySearchable)
	int32 LODBias;

	/** Number of mip-levels to use for cinematic quality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail, AdvancedDisplay)
	int32 NumCinematicMipLevels;

	/** This should be unchecked if using alpha channels individually as masks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName="sRGB"), AssetRegistrySearchable)
	uint32 SRGB:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, AssetRegistrySearchable, AdvancedDisplay)
	uint32 NeverStream:1;

	/** If true, the RHI texture will be created using TexCreate_NoTiling */
	UPROPERTY()
	uint32 bNoTiling:1;

	/** Whether to use the extra cinematic quality mip-levels, when we're forcing mip-levels to be resident. */
	UPROPERTY(transient)
	uint32 bUseCinematicMipLevels:1;

private:
	/** Cached combined group and texture LOD bias to use.	*/
	UPROPERTY(transient)
	int32 CachedCombinedLODBias;

	/** Whether the async resource release process has already been kicked off or not */
	UPROPERTY(transient)
	uint32 bAsyncResourceReleaseHasBeenStarted:1;

public:
	/** Compression settings to use when building the texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compression, AssetRegistrySearchable)
	TEnumAsByte<enum TextureCompressionSettings> CompressionSettings;

	/** The texture filtering mode to use when sampling this texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureFilter> Filter;

	/** Texture group this texture belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail, meta=(DisplayName="Texture Group"), AssetRegistrySearchable)
	TEnumAsByte<enum TextureGroup> LODGroup;

public:
	/** The texture's resource, can be NULL */
	class FTextureResource*	Resource;

	/** Stable RHI texture reference that refers to the current RHI texture. Note this is manually refcounted! */
	FTextureReference TextureReference;

	/** Release fence to know when resources have been freed on the rendering thread. */
	FRenderCommandFence ReleaseFence;

	/** delegate type for texture save events ( Params: UTexture* TextureToSave ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTextureSaved, class UTexture*);
	/** triggered before a texture is being saved */
	ENGINE_API static FOnTextureSaved PreSaveEvent;

	/**
	 * Resets the resource for the texture.
	 */
	ENGINE_API void ReleaseResource();

	/**
	 * Creates a new resource for the texture, and updates any cached references to the resource.
	 */
	ENGINE_API virtual void UpdateResource();

	/**
	 * Implemented by subclasses to create a new resource for the texture.
	 */
	virtual class FTextureResource* CreateResource() PURE_VIRTUAL(UTexture::CreateResource,return NULL;);

	/**
	 * Returns the cached combined LOD bias based on texture LOD group and LOD bias.
	 *
	 * @return	LOD bias
	 */
	ENGINE_API int32 GetCachedLODBias() const;

	/**
	 * Cache the combined LOD bias based on texture LOD group and LOD bias.
	 */
	ENGINE_API void UpdateCachedLODBias( bool bIncTextureMips = true );

	/**
	 * @return The material value type of this texture.
	 */
	virtual EMaterialValueType GetMaterialType() PURE_VIRTUAL(UTexture::GetMaterialType,return MCT_Texture;);

	/**
	 * Waits until all streaming requests for this texture has been fully processed.
	 */
	virtual void WaitForStreaming()
	{
	}
	
	/**
	 * Updates the streaming status of the texture and performs finalization when appropriate. The function returns
	 * true while there are pending requests in flight and updating needs to continue.
	 *
	 * @param bWaitForMipFading	Whether to wait for Mip Fading to complete before finalizing.
	 * @return					true if there are requests in flight, false otherwise
	 */
	virtual bool UpdateStreamingStatus( bool bWaitForMipFading = false )
	{
		return false;
	}

	/**
	 * Textures that use the derived data cache must override this function and
	 * provide a pointer to the linked list of platform data.
	 */
	virtual FTexturePlatformData** GetRunningPlatformData() { return NULL; }
	virtual TMap<FString, FTexturePlatformData*>* GetCookedPlatformData() { return NULL; }

	void CleanupCachedRunningPlatformData();
	void CleanupCachedCookedPlatformData();

	/**
	 * Serializes cooked platform data.
	 */
	ENGINE_API void SerializeCookedPlatformData(class FArchive& Ar);

#if WITH_EDITOR
	/**
	 * Caches platform data for the texture.
	 */
	void CachePlatformData(bool bAsyncCache = false);

	/**
	 * Begins caching platform data in the background for the platform requested
	 */
	ENGINE_API virtual void BeginCacheForCookedPlatformData(  const ITargetPlatform* TargetPlatform ) override;

	/**
	 * Have we finished loading all the cooked platform data for the target platforms requested in BeginCacheForCookedPlatformData
	 * 
	 * @param	TargetPlatform target platform to check for cooked platform data
	 */
	ENGINE_API virtual bool IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) override;

	/**
	 * Begins caching platform data in the background.
	 */
	void BeginCachePlatformData();

	/**
	 * Returns true if all async caching has completed.
	 */
	bool IsAsyncCacheComplete();

	/**
	 * Blocks on async cache tasks and prepares platform data for use.
	 */
	ENGINE_API void FinishCachePlatformData();

	/**
	 * Forces platform data to be rebuilt.
	 */
	ENGINE_API void ForceRebuildPlatformData();

	/**
	 * Marks platform data as transient. This optionally removes persistent or cached data associated with the platform.
	 */
	ENGINE_API void MarkPlatformDataTransient();

	/**
	* Return maximum dimension for this texture type.
	*/
	ENGINE_API virtual uint32 GetMaximumDimension() const;
#endif

	/** @return the width of the surface represented by the texture. */
	virtual float GetSurfaceWidth() const PURE_VIRTUAL(UTexture::GetSurfaceWidth,return 0;);

	/** @return the height of the surface represented by the texture. */
	virtual float GetSurfaceHeight() const PURE_VIRTUAL(UTexture::GetSurfaceHeight,return 0;);

	// Begin UObject interface.
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual void PreSave() override;
	ENGINE_API virtual void BeginDestroy() override;
	ENGINE_API virtual bool IsReadyForFinishDestroy() override;
	ENGINE_API virtual void FinishDestroy() override;
	// End UObject interface.

	/**
	 *	Gets the average brightness of the texture (in linear space)
	 *
	 *	@param	bIgnoreTrueBlack		If true, then pixels w/ 0,0,0 rgb values do not contribute.
	 *	@param	bUseGrayscale			If true, use gray scale else use the max color component.
	 *
	 *	@return	float					The average brightness of the texture
	 */
	ENGINE_API virtual float GetAverageBrightness(bool bIgnoreTrueBlack, bool bUseGrayscale);
	
	// @todo document
	ENGINE_API static const TCHAR* GetTextureGroupString(TextureGroup InGroup);

	// @todo document
	ENGINE_API static const TCHAR* GetMipGenSettingsString(TextureMipGenSettings InEnum);

	// @param	bTextureGroup	true=TexturGroup, false=Texture otherwise
	ENGINE_API static TextureMipGenSettings GetMipGenSettingsFromString(const TCHAR* InStr, bool bTextureGroup);

	/**
	 * Forces textures to recompute LOD settings and stream as needed.
	 * @returns true if the settings were applied, false if they couldn't be applied immediately.
	 */
	ENGINE_API static bool ForceUpdateTextureStreaming();

	/**
	 * Checks whether this texture has a high dynamic range (HDR) source.
	 *
	 * @return true if the texture has an HDR source, false otherwise.
	 */
	bool HasHDRSource() const
	{
#if WITH_EDITOR
		return ((Source.GetFormat() == TSF_BGRE8) || (Source.GetFormat() == TSF_RGBA16F));
#else
		return false;
#endif // WITH_EDITOR
	}


	/** @return true if the compression type is a normal map compression type */
	bool IsNormalMap() const
	{
		return (CompressionSettings == TC_Normalmap);
	}

	/**
	 * Calculates the size of this texture if it had MipCount miplevels streamed in.
	 *
	 * @param	Enum	Which mips to calculate size for.
	 * @return	Total size of all specified mips, in bytes
	 */
	virtual uint32 CalcTextureMemorySizeEnum( ETextureMipCount Enum ) const
	{
		return 0;
	}

	// @todo document
	const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid; 
#endif // WITH_EDITORONLY_DATA
	}

	// @todo document
	void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = FGuid::NewGuid();
#endif // WITH_EDITORONLY_DATA
	}

	/**
	 * Retrieves the pixel format enum for enum <-> string conversions.
	 */
	static class UEnum* GetPixelFormatEnum();

};



