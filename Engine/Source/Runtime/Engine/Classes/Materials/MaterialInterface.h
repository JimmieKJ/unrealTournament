// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Engine/BlendableInterface.h"
#include "Runtime/RHI/Public/RHIDefinitions.h"
#include "Runtime/RenderCore/Public/RenderCommandFence.h"
#include "SceneTypes.h"
#include "MaterialInterface.generated.h"

class UMaterial;
class FMaterialResource;
class FMaterialCompiler;
struct FPrimitiveViewRelevance;
class UTexture;

UENUM()
enum EMaterialUsage
{
	MATUSAGE_SkeletalMesh,
	MATUSAGE_ParticleSprites,
	MATUSAGE_BeamTrails,
	MATUSAGE_MeshParticles,
	MATUSAGE_StaticLighting,
	MATUSAGE_MorphTargets,
	MATUSAGE_SplineMesh,
	MATUSAGE_Landscape,
	MATUSAGE_InstancedStaticMeshes,
	MATUSAGE_Clothing,
	MATUSAGE_UI,
	MATUSAGE_MAX,
};

USTRUCT()
struct ENGINE_API FMaterialRelevance
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint32 bOpaque : 1;

	UPROPERTY()
	uint32 bMasked : 1;

	UPROPERTY()
	uint32 bDistortion : 1;

	UPROPERTY()
	uint32 bSeparateTranslucency : 1;

	UPROPERTY()
	uint32 bNormalTranslucency : 1;

	UPROPERTY()
	uint32 bDisableDepthTest : 1;
	
	UPROPERTY()
	uint32 bOutputsVelocityInBasePass : 1;

	UPROPERTY()
	uint16 ShadingModelMask;

	/** Default constructor. */
	FMaterialRelevance()
		: bOpaque(false)
		, bMasked(false)
		, bDistortion(false)
		, bSeparateTranslucency(false)
		, bNormalTranslucency(false)
		, bDisableDepthTest(false)		
		, bOutputsVelocityInBasePass(true)
		, ShadingModelMask(0)
	{}

	/** Bitwise OR operator.  Sets any relevance bits which are present in either FMaterialRelevance. */
	FMaterialRelevance& operator|=(const FMaterialRelevance& B)
	{
		bOpaque |= B.bOpaque;
		bMasked |= B.bMasked;
		bDistortion |= B.bDistortion;
		bSeparateTranslucency |= B.bSeparateTranslucency;
		bNormalTranslucency |= B.bNormalTranslucency;
		bDisableDepthTest |= B.bDisableDepthTest;
		ShadingModelMask |= B.ShadingModelMask;
		bOutputsVelocityInBasePass |= B.bOutputsVelocityInBasePass;
		return *this;
	}

	/** Binary bitwise OR operator. */
	friend FMaterialRelevance operator|(const FMaterialRelevance& A, const FMaterialRelevance& B)
	{
		FMaterialRelevance Result(A);
		Result |= B;
		return Result;
	}

	/** Copies the material's relevance flags to a primitive's view relevance flags. */
	void SetPrimitiveViewRelevance(FPrimitiveViewRelevance& OutViewRelevance) const;
};

/** 
 *	UMaterial interface settings for Lightmass
 */
USTRUCT()
struct FLightmassMaterialInterfaceSettings
{
	GENERATED_USTRUCT_BODY()

	/** If true, forces translucency to cast static shadows as if the material were masked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Material)
	uint32 bCastShadowAsMasked:1;

	/** Scales the emissive contribution of this material to static lighting. */
	UPROPERTY()
	float EmissiveBoost;

	/** Scales the diffuse contribution of this material to static lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Material)
	float DiffuseBoost;

	/** 
	 * Scales the resolution that this material's attributes were exported at. 
	 * This is useful for increasing material resolution when details are needed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Material)
	float ExportResolutionScale;

	/***/
	UPROPERTY()
	float DistanceFieldPenumbraScale;

	/** Boolean override flags - only used in MaterialInstance* cases. */
	/** If true, override the bCastShadowAsMasked setting of the parent material. */
	UPROPERTY()
	uint32 bOverrideCastShadowAsMasked:1;

	/** If true, override the emissive boost setting of the parent material. */
	UPROPERTY()
	uint32 bOverrideEmissiveBoost:1;

	/** If true, override the diffuse boost setting of the parent material. */
	UPROPERTY()
	uint32 bOverrideDiffuseBoost:1;

	/** If true, override the export resolution scale setting of the parent material. */
	UPROPERTY()
	uint32 bOverrideExportResolutionScale:1;

	/** If true, override the distance field penumbra scale setting of the parent material. */
	UPROPERTY()
	uint32 bOverrideDistanceFieldPenumbraScale:1;



		FLightmassMaterialInterfaceSettings()
		: bCastShadowAsMasked(false)
		, EmissiveBoost(1.0f)
		, DiffuseBoost(1.0f)
		, ExportResolutionScale(1.0f)
		, DistanceFieldPenumbraScale(1.0f)
		, bOverrideCastShadowAsMasked(false)
		, bOverrideEmissiveBoost(false)
		, bOverrideDiffuseBoost(false)
		, bOverrideExportResolutionScale(false)
		, bOverrideDistanceFieldPenumbraScale(false)
		{
		}
	
};

UCLASS(abstract, BlueprintType,MinimalAPI)
class UMaterialInterface : public UObject, public IBlendableInterface
{
	GENERATED_UCLASS_BODY()

	/** SubsurfaceProfile, for Screen Space Subsurface Scattering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	class USubsurfaceProfile* SubsurfaceProfile;

	/* -------------------------- */

	/** A fence to track when the primitive is no longer used as a parent */
	FRenderCommandFence ParentRefFence;

protected:
	/** The Lightmass settings for this object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Lightmass)
	struct FLightmassMaterialInterfaceSettings LightmassSettings;

public:

#if WITH_EDITORONLY_DATA
	/** The mesh used by the material editor to preview the material.*/
	UPROPERTY(EditAnywhere, Category=MaterialInterface, meta=(AllowedClasses="StaticMesh,SkeletalMesh", ExactClass="true"))
	FStringAssetReference PreviewMesh;

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

private:
	/** Unique ID for this material, used for caching during distributed lighting */
	UPROPERTY()
	FGuid LightingGuid;

#endif // WITH_EDITORONLY_DATA

private:
	/** Feature levels to force to compile. */
	uint32 FeatureLevelsToForceCompile;

	/** Feature level bitfield to compile for all materials */
	static uint32 FeatureLevelsForAllMaterials;
public:
	/** Set which feature levels this material instance should compile. GMaxRHIFeatureLevel is always compiled! */
	ENGINE_API void SetFeatureLevelToCompile(ERHIFeatureLevel::Type FeatureLevel, bool bShouldCompile);

	/** Set which feature levels _all_ materials should compile to. GMaxRHIFeatureLevel is always compiled. */
	ENGINE_API static void SetGlobalRequiredFeatureLevel(ERHIFeatureLevel::Type FeatureLevel, bool bShouldCompile);

	// Begin UObject interface.
	ENGINE_API virtual void BeginDestroy() override;
	ENGINE_API virtual bool IsReadyForFinishDestroy() override;
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface.

	// Begin interface IBlendableInterface
	ENGINE_API virtual void OverrideBlendableSettings(class FSceneView& View, float Weight) const override;
	// End interface IBlendableInterface

	/** Walks up parent chain and finds the base Material that this is an instance of. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	ENGINE_API UMaterial* GetBaseMaterial();

	/**
	 * Get the material which we are instancing.
	 */
	virtual class UMaterial* GetMaterial() PURE_VIRTUAL(UMaterialInterface::GetMaterial,return NULL;);
	/**
	 * Get the material which we are instancing.
	 */
	virtual const class UMaterial* GetMaterial() const PURE_VIRTUAL(UMaterialInterface::GetMaterial,return NULL;);

	/**
	 * Same as above, but can be called concurrently
	 */
	typedef TArray<class UMaterialInterface const*, TInlineAllocator<8> > TMicRecursionGuard;
	virtual const class UMaterial* GetMaterial_Concurrent(TMicRecursionGuard& RecursionGuard) const PURE_VIRTUAL(UMaterialInterface::GetMaterial_Concurrent,return NULL;);

	/**
	* Test this material for dependency on a given material.
	* @param	TestDependency - The material to test for dependency upon.
	* @return	True if the material is dependent on TestDependency.
	*/
	virtual bool IsDependent(UMaterialInterface* TestDependency) { return TestDependency == this; }

	/**
	* Return a pointer to the FMaterialRenderProxy used for rendering.
	* @param	Selected	specify true to return an alternate material used for rendering this material when part of a selection
	*						@note: only valid in the editor!
	* @return	The resource to use for rendering this material instance.
	*/
	virtual class FMaterialRenderProxy* GetRenderProxy(bool Selected, bool bHovered=false) const PURE_VIRTUAL(UMaterialInterface::GetRenderProxy,return NULL;);

	/**
	* Return a pointer to the physical material used by this material instance.
	* @return The physical material.
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Material")
	virtual UPhysicalMaterial* GetPhysicalMaterial() const PURE_VIRTUAL(UMaterialInterface::GetPhysicalMaterial,return NULL;);

	/** Return the textures used to render this material. */
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels, ERHIFeatureLevel::Type FeatureLevel, bool bAllFeatureLevels) const
		PURE_VIRTUAL(UMaterialInterface::GetUsedTextures,);

	/**
	 * Override a specific texture (transient)
	 *
	 * @param InTextureToOverride The texture to override
	 * @param OverrideTexture The new texture to use
	 */
	virtual void OverrideTexture(const UTexture* InTextureToOverride, UTexture* OverrideTexture, ERHIFeatureLevel::Type InFeatureLevel) PURE_VIRTUAL(UMaterialInterface::OverrideTexture, return;);

	/** 
	 * Overrides the default value of the given parameter (transient).  
	 * This is used to implement realtime previewing of parameter defaults. 
	 * Handles updating dependent MI's and cached uniform expressions.
	 */
	virtual void OverrideVectorParameterDefault(FName ParameterName, const FLinearColor& Value, bool bOverride, ERHIFeatureLevel::Type FeatureLevel) PURE_VIRTUAL(UMaterialInterface::OverrideTexture, return;);
	virtual void OverrideScalarParameterDefault(FName ParameterName, float Value, bool bOverride, ERHIFeatureLevel::Type FeatureLevel) PURE_VIRTUAL(UMaterialInterface::OverrideTexture, return;);

	/**
	 * Checks if the material can be used with the given usage flag.  
	 * If the flag isn't set in the editor, it will be set and the material will be recompiled with it.
	 * @param Usage - The usage flag to check
	 * @param bSkipPrim - Bypass the primitive type checks
	 * @return bool - true if the material can be used for rendering with the given type.
	 */
	virtual bool CheckMaterialUsage(const EMaterialUsage Usage, const bool bSkipPrim = false) PURE_VIRTUAL(UMaterialInterface::CheckMaterialUsage,return false;);
	/**
	 * Same as above but is valid to call from any thread. In the editor, this might spin and stall for a shader compile
	 */
	virtual bool CheckMaterialUsage_Concurrent(const EMaterialUsage Usage, const bool bSkipPrim = false) const PURE_VIRTUAL(UMaterialInterface::CheckMaterialUsage,return false;);

	/**
	 * Get the static permutation resource if the instance has one
	 * @return - the appropriate FMaterialResource if one exists, otherwise NULL
	 */
	virtual FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) { return NULL; }

	/**
	 * Get the static permutation resource if the instance has one
	 * @return - the appropriate FMaterialResource if one exists, otherwise NULL
	 */
	virtual const FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::Num) const { return NULL; }

	/**
	* Get the value of the given static switch parameter
	*
	* @param	ParameterName	The name of the static switch parameter
	* @param	OutValue		Will contain the value of the parameter if successful
	* @return					True if successful
	*/
	virtual bool GetStaticSwitchParameterValue(FName ParameterName,bool &OutValue,FGuid &OutExpressionGuid) const
		PURE_VIRTUAL(UMaterialInterface::GetStaticSwitchParameterValue,return false;);

	/**
	* Get the value of the given static component mask parameter
	*
	* @param	ParameterName	The name of the parameter
	* @param	R, G, B, A		Will contain the values of the parameter if successful
	* @return					True if successful
	*/
	virtual bool GetStaticComponentMaskParameterValue(FName ParameterName, bool &R, bool &G, bool &B, bool &A, FGuid &OutExpressionGuid) const
		PURE_VIRTUAL(UMaterialInterface::GetStaticComponentMaskParameterValue,return false;);

	/**
	* Get the weightmap index of the given terrain layer weight parameter
	*
	* @param	ParameterName	The name of the parameter
	* @param	OutWeightmapIndex	Will contain the values of the parameter if successful
	* @return					True if successful
	*/
	virtual bool GetTerrainLayerWeightParameterValue(FName ParameterName, int32& OutWeightmapIndex, FGuid &OutExpressionGuid) const
		PURE_VIRTUAL(UMaterialInterface::GetTerrainLayerWeightParameterValue,return false;);

	/** @return The material's relevance. */
	ENGINE_API FMaterialRelevance GetRelevance(ERHIFeatureLevel::Type InFeatureLevel) const;
	/** @return The material's relevance, from concurrent render thread updates. */
	ENGINE_API FMaterialRelevance GetRelevance_Concurrent(ERHIFeatureLevel::Type InFeatureLevel) const;
private:
	// might get called from game or render thread
	FMaterialRelevance GetRelevance_Internal(const UMaterial* Material, ERHIFeatureLevel::Type InFeatureLevel) const;
public:

	int32 GetWidth() const;
	int32 GetHeight() const;

	const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid; 
#endif // WITH_EDITORONLY_DATA
	}

	void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = FGuid::NewGuid();
#endif // WITH_EDITORONLY_DATA
	}


	/**
	 *	Check if the textures have changed since the last time the material was
	 *	serialized for Lightmass... Update the lists while in here.
	 *	NOTE: This will mark the package dirty if they have changed.
	 *
	 *	@return	bool	true if the textures have changed.
	 *					false if they have not.
	 */
	virtual bool UpdateLightmassTextureTracking() 
	{ 
		return false; 
	}
	
	/** @return The override bOverrideCastShadowAsMasked setting of the material. */
	inline bool GetOverrideCastShadowAsMasked() const
	{
		return LightmassSettings.bOverrideCastShadowAsMasked;
	}

	/** @return The override emissive boost setting of the material. */
	inline bool GetOverrideEmissiveBoost() const
	{
		return LightmassSettings.bOverrideEmissiveBoost;
	}

	/** @return The override diffuse boost setting of the material. */
	inline bool GetOverrideDiffuseBoost() const
	{
		return LightmassSettings.bOverrideDiffuseBoost;
	}

	/** @return The override export resolution scale setting of the material. */
	inline bool GetOverrideExportResolutionScale() const
	{
		return LightmassSettings.bOverrideExportResolutionScale;
	}

	inline bool GetOverrideDistanceFieldPenumbraScale() const
	{
		return LightmassSettings.bOverrideDistanceFieldPenumbraScale;
	}

	/** @return	The bCastShadowAsMasked value for this material. */
	virtual bool GetCastShadowAsMasked() const
	{
		return LightmassSettings.bCastShadowAsMasked;
	}

	/** @return	The Emissive boost value for this material. */
	virtual float GetEmissiveBoost() const
	{
		return 
		LightmassSettings.EmissiveBoost;
	}

	/** @return	The Diffuse boost value for this material. */
	virtual float GetDiffuseBoost() const
	{
		return LightmassSettings.DiffuseBoost;
	}

	/** @return	The ExportResolutionScale value for this material. */
	virtual float GetExportResolutionScale() const
	{
		return FMath::Clamp(LightmassSettings.ExportResolutionScale, .1f, 10.0f);
	}

	virtual float GetDistanceFieldPenumbraScale() const
	{
		return LightmassSettings.DistanceFieldPenumbraScale;
	}

	/** @param	bInOverrideCastShadowAsMasked	The override CastShadowAsMasked setting to set. */
	inline void SetOverrideCastShadowAsMasked(bool bInOverrideCastShadowAsMasked)
	{
		LightmassSettings.bOverrideCastShadowAsMasked = bInOverrideCastShadowAsMasked;
	}

	/** @param	bInOverrideEmissiveBoost	The override emissive boost setting to set. */
	inline void SetOverrideEmissiveBoost(bool bInOverrideEmissiveBoost)
	{
		LightmassSettings.bOverrideEmissiveBoost = bInOverrideEmissiveBoost;
	}

	/** @param bInOverrideDiffuseBoost		The override diffuse boost setting of the parent material. */
	inline void SetOverrideDiffuseBoost(bool bInOverrideDiffuseBoost)
	{
		LightmassSettings.bOverrideDiffuseBoost = bInOverrideDiffuseBoost;
	}

	/** @param bInOverrideExportResolutionScale	The override export resolution scale setting of the parent material. */
	inline void SetOverrideExportResolutionScale(bool bInOverrideExportResolutionScale)
	{
		LightmassSettings.bOverrideExportResolutionScale = bInOverrideExportResolutionScale;
	}

	inline void SetOverrideDistanceFieldPenumbraScale(bool bInOverrideDistanceFieldPenumbraScale)
	{
		LightmassSettings.bOverrideDistanceFieldPenumbraScale = bInOverrideDistanceFieldPenumbraScale;
	}

	/** @param	InCastShadowAsMasked	The CastShadowAsMasked value for this material. */
	inline void SetCastShadowAsMasked(bool InCastShadowAsMasked)
	{
		LightmassSettings.bCastShadowAsMasked = InCastShadowAsMasked;
	}

	/** @param	InEmissiveBoost		The Emissive boost value for this material. */
	inline void SetEmissiveBoost(float InEmissiveBoost)
	{
		LightmassSettings.EmissiveBoost = InEmissiveBoost;
	}

	/** @param	InDiffuseBoost		The Diffuse boost value for this material. */
	inline void SetDiffuseBoost(float InDiffuseBoost)
	{
		LightmassSettings.DiffuseBoost = InDiffuseBoost;
	}

	/** @param	InExportResolutionScale		The ExportResolutionScale value for this material. */
	inline void SetExportResolutionScale(float InExportResolutionScale)
	{
		LightmassSettings.ExportResolutionScale = InExportResolutionScale;
	}

	inline void SetDistanceFieldPenumbraScale(float InDistanceFieldPenumbraScale)
	{
		LightmassSettings.DistanceFieldPenumbraScale = InDistanceFieldPenumbraScale;
	}

	/**
	 *	Get all of the textures in the expression chain for the given property (ie fill in the given array with all textures in the chain).
	 *
	 *	@param	InProperty				The material property chain to inspect, such as MP_BaseColor.
	 *	@param	OutTextures				The array to fill in all of the textures.
	 *	@param	OutTextureParamNames	Optional array to fill in with texture parameter names.
	 *	@param	InStaticParameterSet	Optional static parameter set - if specified only follow StaticSwitches according to its settings
	 *
	 *	@return	bool			true if successful, false if not.
	 */
	virtual bool GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,  TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet)
		PURE_VIRTUAL(UMaterialInterface::GetTexturesInPropertyChain,return false;);

	ENGINE_API virtual bool GetParameterDesc(FName ParameterName, FString& OutDesc) const;
	ENGINE_API virtual bool GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue, int32& OutFontPage) const;
	ENGINE_API virtual bool GetScalarParameterValue(FName ParameterName, float& OutValue) const;
	ENGINE_API virtual bool GetScalarCurveParameterValue(FName ParameterName, FInterpCurveFloat& OutValue) const;
	ENGINE_API virtual bool GetTextureParameterValue(FName ParameterName, class UTexture*& OutValue) const;
	ENGINE_API virtual bool GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue) const;
	ENGINE_API virtual bool GetVectorCurveParameterValue(FName ParameterName, FInterpCurveVector& OutValue) const;
	ENGINE_API virtual bool GetLinearColorParameterValue(FName ParameterName, FLinearColor& OutValue) const;
	ENGINE_API virtual bool GetLinearColorCurveParameterValue(FName ParameterName, FInterpCurveLinearColor& OutValue) const;
	ENGINE_API virtual bool GetRefractionSettings(float& OutBiasValue) const;
	ENGINE_API virtual bool GetGroupName(FName ParameterName, FName& GroupName) const;

	/**
		Access to overridable properties of the base material.
	*/
	ENGINE_API virtual float GetOpacityMaskClipValue(bool bIsGameThread = IsInGameThread()) const;
	ENGINE_API virtual EBlendMode GetBlendMode(bool bIsGameThread = IsInGameThread()) const;
	ENGINE_API virtual EMaterialShadingModel GetShadingModel(bool bIsGameThread = IsInGameThread()) const;
	ENGINE_API virtual bool IsTwoSided(bool bIsGameThread = IsInGameThread()) const;
	ENGINE_API virtual bool IsMasked(bool bIsGameThread = IsInGameThread()) const;

	ENGINE_API virtual USubsurfaceProfile* GetSubsurfaceProfile_Internal() const;

	/**
	 * Force the streaming system to disregard the normal logic for the specified duration and
	 * instead always load all mip-levels for all textures used by this material.
	 *
	 * @param OverrideForceMiplevelsToBeResident	- Whether to use (true) or ignore (false) the bForceMiplevelsToBeResidentValue parameter.
	 * @param bForceMiplevelsToBeResidentValue		- true forces all mips to stream in. false lets other factors decide what to do with the mips.
	 * @param ForceDuration							- Number of seconds to keep all mip-levels in memory, disregarding the normal priority logic. Negative value turns it off.
	 * @param CinematicTextureGroups				- Bitfield indicating texture groups that should use extra high-resolution mips
	 */
	ENGINE_API virtual void SetForceMipLevelsToBeResident( bool OverrideForceMiplevelsToBeResident, bool bForceMiplevelsToBeResidentValue, float ForceDuration, int32 CinematicTextureGroups = 0 );

	/**
	 * Re-caches uniform expressions for all material interfaces
	 */
	ENGINE_API static void RecacheAllMaterialUniformExpressions();

	/**
	 * Re-caches uniform expressions for this material interface                   
	 */
	virtual void RecacheUniformExpressions() const {}

	/** Clears the shader cache and recompiles the shader for rendering. */
	ENGINE_API virtual void ForceRecompileForRendering() {}

	/**
	 * Asserts if any default material does not exist.
	 */
	ENGINE_API static void AssertDefaultMaterialsExist();

	/**
	 * Asserts if any default material has not been post-loaded.
	 */
	ENGINE_API static void AssertDefaultMaterialsPostLoaded();

	/**
	 * Initializes all default materials.
	 */
	ENGINE_API static void InitDefaultMaterials();

	/** Checks to see if an input property should be active, based on the state of the material */
	ENGINE_API virtual bool IsPropertyActive(EMaterialProperty InProperty) const;

	/** Compiles a material property. */
	ENGINE_API int32 CompileProperty(FMaterialCompiler* Compiler, EMaterialProperty Property);

	/** Allows material properties to be compiled with the option of being overridden by the material attributes input. */
	ENGINE_API virtual int32 CompilePropertyEx( class FMaterialCompiler* Compiler, EMaterialProperty Property );

	/** Get bitfield indicating which feature levels should be compiled by default */
	ENGINE_API static uint32 GetFeatureLevelsToCompileForAllMaterials() { return FeatureLevelsForAllMaterials | (1 << GMaxRHIFeatureLevel); }

	/** Iterate over all feature levels currently marked as active */
	template <typename TFunction>
	static void IterateOverActiveFeatureLevels(TFunction InHandler) 
	{  
		uint32 FeatureLevels = GetFeatureLevelsToCompileForAllMaterials();
		while (FeatureLevels != 0)
		{
			InHandler((ERHIFeatureLevel::Type)FBitSet::GetAndClearNextBit(FeatureLevels));
		}
	}

protected:

	/** Returns a bitfield indicating which feature levels should be compiled for rendering. GMaxRHIFeatureLevel is always present */
	ENGINE_API uint32 GetFeatureLevelsToCompileForRendering() const;

	void UpdateMaterialRenderProxy(FMaterialRenderProxy& Proxy);

private:
	/**
	 * Post loads all default materials.
	 */
	static void PostLoadDefaultMaterials();

};

/** Helper function to serialize inline shader maps for the given material resources. */
extern void SerializeInlineShaderMaps(const TMap<const class ITargetPlatform*,TArray<FMaterialResource*>>* PlatformMaterialResourcesToSave, FArchive& Ar, FMaterialResource* OutMaterialResourcesLoaded[][ERHIFeatureLevel::Num]);