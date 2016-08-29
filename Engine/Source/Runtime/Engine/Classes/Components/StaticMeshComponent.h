// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTypes.h"
#include "Components/MeshComponent.h"
#include "Runtime/RenderCore/Public/PackedNormal.h"
#include "RawIndexBuffer.h"
#include "StaticMeshComponent.generated.h"

class FColorVertexBuffer;
class UStaticMesh;
class FStaticMeshStaticLightingMesh;
class ULightComponent;
struct FEngineShowFlags;
struct FConvexVolume;
struct FStreamingTextureBuildInfo;

/** Cached vertex information at the time the mesh was painted. */
USTRUCT()
struct FPaintedVertex
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FPackedNormal Normal;

	UPROPERTY()
	FColor Color;


	FPaintedVertex()
		: Position(ForceInit)
		, Color(ForceInit)
	{
	}


		FORCEINLINE friend FArchive& operator<<( FArchive& Ar, FPaintedVertex& PaintedVertex )
		{
			Ar << PaintedVertex.Position;
			Ar << PaintedVertex.Normal;
			Ar << PaintedVertex.Color;
			return Ar;
		}
	
};

struct FPreCulledStaticMeshSection
{
	/** Range of vertices and indices used when rendering this section. */
	uint32 FirstIndex;
	uint32 NumTriangles;
};

USTRUCT()
struct FStaticMeshComponentLODInfo
{
	GENERATED_USTRUCT_BODY()

	FLightMapRef LightMap;

	FShadowMapRef ShadowMap;

	/** Vertex data cached at the time this LOD was painted, if any */
	UPROPERTY()
	TArray<struct FPaintedVertex> PaintedVertices;

	/** Vertex colors to use for this mesh LOD */
	FColorVertexBuffer* OverrideVertexColors;

	/** Information for each section about what range of PreCulledIndexBuffer to use.  If no preculled index data is available, PreCulledSections will be empty. */
	TArray<FPreCulledStaticMeshSection> PreCulledSections;

	FRawStaticIndexBuffer PreCulledIndexBuffer;

#if WITH_EDITORONLY_DATA
	/** Owner of this FStaticMeshComponentLODInfo */
	class UStaticMeshComponent* Owner;
#endif

	/** Default constructor */
	FStaticMeshComponentLODInfo();
	/** Copy constructor */
	FStaticMeshComponentLODInfo( const FStaticMeshComponentLODInfo &rhs );
	/** Destructor */
	~FStaticMeshComponentLODInfo();

	/** Delete existing resources */
	void CleanUp();

	/**
	* Enqueues a rendering command to release the vertex colors.
	* The game thread must block until the rendering thread has processed the command before deleting OverrideVertexColors.
	*/
	ENGINE_API void BeginReleaseOverrideVertexColors();

	ENGINE_API void ReleaseOverrideVertexColorsAndBlock();

	void ReleaseResources();

	/** Methods for importing and exporting the painted vertex array to text */
	void ExportText(FString& ValueStr);
	void ImportText(const TCHAR** SourceText);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FStaticMeshComponentLODInfo& I);

private:
	/** Purposely hidden */
	FStaticMeshComponentLODInfo &operator=( const FStaticMeshComponentLODInfo &rhs ) { check(0); return *this; }
};

template<>
struct TStructOpsTypeTraits<FStaticMeshComponentLODInfo> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = false
	};
};

/**
 * StaticMeshComponent is used to create an instance of a UStaticMesh.
 * A static mesh is a piece of geometry that consists of a static set of polygons.
 *
 * @see https://docs.unrealengine.com/latest/INT/Engine/Content/Types/StaticMeshes/
 * @see UStaticMesh
 */
UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** If 0, auto-select LOD level. if >0, force to (ForcedLodModel-1). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	int32 ForcedLodModel;

	/** LOD that was desired for rendering this StaticMeshComponent last frame. */
	UPROPERTY()
	int32 PreviousLODLevel;

	/** Whether to override the MinLOD setting of the static mesh asset with the MinLOD of this component. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	bool bOverrideMinLOD;

	/** 
	 * Specifies the smallest LOD that will be used for this component.  
	 * This is ignored if ForcedLodModel is enabled.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD, meta=(editcondition = "bOverrideMinLOD"))
	int32 MinLOD;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=StaticMesh, ReplicatedUsing=OnRep_StaticMesh)
	class UStaticMesh* StaticMesh;

	UFUNCTION()
	void OnRep_StaticMesh(class UStaticMesh *OldStaticMesh);

	/** If true, WireframeColorOverride will be used. If false, color is determined based on mobility and physics simulation settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Rendering, meta=(InlineEditConditionToggle))
	bool bOverrideWireframeColor;

	/** Wireframe color to use if bOverrideWireframeColor is true */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering, meta=(editcondition = "bOverrideWireframeColor"))
	FColor WireframeColorOverride;

#if WITH_EDITORONLY_DATA
	/** The section currently selected in the Editor. Used for highlighting */
	UPROPERTY(transient)
	int32 SelectedEditorSection;
	/** Index of the section to preview. If set to INDEX_NONE, all section will be rendered. Used for isolating in Static Mesh Tool **/
	UPROPERTY(transient)
	int32 SectionIndexPreview;
#endif

	/** If true, bForceNavigationObstacle flag will take priority over navigation data stored in StaticMesh */
	UPROPERTY(transient)
	uint32 bOverrideNavigationExport : 1;

	/** Allows overriding navigation export behavior per component: full collisions or dynamic obstacle */
	UPROPERTY(transient)
	uint32 bForceNavigationObstacle : 1;

	/** If true, mesh painting is disallowed on this instance. Set if vertex colors are overridden in a construction script. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Rendering)
	uint32 bDisallowMeshPaintPerInstance : 1;

#if !(UE_BUILD_SHIPPING)
	/** Option to draw mesh collision in wireframe */
	uint32 bDrawMeshCollisionWireframe : 1;
#endif

	/**
	 *	Ignore this instance of this static mesh when calculating streaming information.
	 *	This can be useful when doing things like applying character textures to static geometry,
	 *	to avoid them using distance-based streaming.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=TextureStreaming)
	uint32 bIgnoreInstanceForTextureStreaming:1;

	/** Whether to override the lightmap resolution defined in the static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, meta=(InlineEditConditionToggle))
	uint32 bOverrideLightMapRes:1;

	/** Light map resolution to use on this component, used if bOverrideLightMapRes is true */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, meta=(editcondition="bOverrideLightMapRes") )
	int32 OverriddenLightMapRes;

	/**
	 * Allows adjusting the desired streaming distance of streaming textures that uses UV 0.
	 * 1.0 is the default, whereas a higher value makes the textures stream in sooner from far away.
	 * A lower value (0.0-1.0) makes the textures stream in later (you have to be closer).
	 * Value can be < 0 (from legcay content, or code changes)
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=TextureStreaming, meta=(ClampMin = 0, ToolTip="Allows adjusting the desired resolution of streaming textures that uses UV 0.  1.0 is the default, whereas a higher value increases the streamed-in resolution."))
	float StreamingDistanceMultiplier;

	/** Subdivision step size for static vertex lighting.				*/
	UPROPERTY()
	int32 SubDivisionStepSize;

	/** Whether to use subdivisions or just the triangle's vertices.	*/
	UPROPERTY()
	uint32 bUseSubDivisions:1;

	UPROPERTY()
	TArray<FGuid> IrrelevantLights;

	/** Static mesh LOD data.  Contains static lighting data along with instanced mesh vertex colors. */
	UPROPERTY(transient)
	TArray<struct FStaticMeshComponentLODInfo> LODData;

	/** The list of texture, bounds and scales. As computed in the texture streaming build process. */
	UPROPERTY(NonTransactional)
	TArray<FStreamingTextureBuildInfo> StreamingTextureData;

	/** Use the collision profile specified in the StaticMesh asset.*/
	UPROPERTY(EditAnywhere, Category = Collision)
	bool bUseDefaultCollision;

#if WITH_EDITORONLY_DATA
	/** 
	 * Temporary section data used in the texture streaming build. 
	 * Stays persistent to allow texture streaming accuracy view mode to inspect it.
	 * The shared ptr is used to allow a safe way for the proxy to access it without duplicating it.
	 */
	TSharedPtr<TArray<FStreamingSectionBuildInfo>, ESPMode::NotThreadSafe> StreamingSectionData;

	/** Derived data key of the static mesh, used to determine if an update from the source static mesh is required. */
	UPROPERTY()
	FString StaticMeshDerivedDataKey;

	UPROPERTY()
	bool bStreamingTextureDataValid;
#endif

	/** The Lightmass settings for this object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Lighting)
	struct FLightmassPrimitiveSettings LightmassSettings;

	virtual ~UStaticMeshComponent();

	/** Change the StaticMesh used by this instance. */
	UFUNCTION(BlueprintCallable, Category="Components|StaticMesh")
	virtual bool SetStaticMesh(class UStaticMesh* NewMesh);

	UFUNCTION(BlueprintCallable, Category="Rendering|LOD")
	void SetForcedLodModel(int32 NewForcedLodModel);

	/** 
	 * Get Local bounds
	 */
	UFUNCTION(BlueprintCallable, Category="Components|StaticMesh")
	void GetLocalBounds(FVector& Min, FVector& Max) const;

	virtual void SetCollisionProfileName(FName InCollisionProfileName) override;

public:

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;	
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PreEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostLoad() override;
	virtual bool AreNativePropertiesIdenticalTo( UObject* Other ) const override;
	virtual FString GetDetailedInfoInternal() const override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool HasAnySockets() const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual bool ShouldCollideWhenPlacing() const override
	{
		// Current Method of collision does not work with non-capsule shapes, enable when it works with static meshes
		// return IsCollisionEnabled() && (StaticMesh != NULL);
		return false;
	}

	//~ End USceneComponent Interface

	//~ Begin UActorComponent Interface.
protected: 
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
public:
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) override;
	virtual UObject const* AdditionalStatObject() const override;
#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	//~ End UActorComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual int32 GetNumMaterials() const override;
#if WITH_EDITOR
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options) override;
#endif
	virtual float GetEmissiveBoost(int32 ElementIndex) const override;
	virtual float GetDiffuseBoost(int32 ElementIndex) const override;
	virtual bool GetShadowIndirectOnly() const override
	{
		return LightmassSettings.bShadowIndirectOnly;
	}
	virtual ELightMapInteractionType GetStaticLightingType() const override;

	/**
	* Return whether this primitive should have data for texture streaming. Used to optimize the texture streaming build.
	*
	* @return	true if a rebuild is required.
	*/
	virtual bool RequiresStreamingTextureData() const override;

	/**
	* Return whether this primitive has (good) section data for texture streaming. Used in the texture streaming build and accuracy viewmodes.
	*
	* @param bCheckTexCoordScales - If true, section data must contains texcoord scales to be valid.
	*
	* @return - true if streaming section data is valid.
	*/
	virtual bool HasStreamingSectionData(bool bCheckTexCoordScales) const override;
	/**
	* Return whether this primitive has (good) built data for texture streaming. Used for the "Texture Streaming Needs Rebuilt" check.
	*
	* @return	true if all texture streaming data is valid.
	*/
	virtual bool HasStreamingTextureData() const override;

	/**
	*	Update the precomputed streaming debug data of this component.
	*
	*	@param	TexCoordScales				The texcoord scales for each texture register of each relevant materials.
	*/
	virtual void UpdateStreamingSectionData(const FTexCoordScaleMap& TexCoordScales) override;

	/**
	 *	Update the precomputed streaming data of this component.
	 *
	 *	@param	LevelTextures	[in,out]	The list of textures referred by all component of a level. The array index maps to UTexture2D::LevelIndex.
	 *	@param	TexCoordScales	[in]		The texcoord scales for each texture register of each relevant materials.
	 *	@param	QualityLevel	[in]		The quality level being used in the texture streaming build.
	 *	@param	FeatureLevel	[in]		The feature level being used in the texture streaming build.
	 */
	virtual void UpdateStreamingTextureData(TArray<UTexture2D*>& LevelTextures, const FTexCoordScaleMap& TexCoordScales, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel) override;

	virtual bool GetStreamingTextureFactors(float& OutWorldTexelFactor, float& OutWorldLightmapFactor) const;
	virtual void GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const override;

	virtual class UBodySetup* GetBodySetup() override;
	virtual bool CanEditSimulatePhysics() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual bool UsesOnlyUnlitMaterials() const override;
	virtual bool GetLightMapResolution( int32& Width, int32& Height ) const override;
	virtual int32 GetStaticLightMapResolution() const override;
	/** Returns true if the component is static AND has the right static mesh setup to support lightmaps. */
	virtual bool HasValidSettingsForStaticLighting(bool bOverlookInvalidComponents) const override;

	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;

	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
#if WITH_EDITOR
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
#endif
	//~ End UPrimitiveComponent Interface.

	//~ Begin INavRelevantInterface Interface.
	virtual bool IsNavigationRelevant() const override;
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	//~ End INavRelevantInterface Interface.
	/**
	 *	Returns true if the component uses texture lightmaps
	 *
	 *	@param	InWidth		[in]	The width of the light/shadow map
	 *	@param	InHeight	[in]	The width of the light/shadow map
	 *
	 *	@return	bool				true if texture lightmaps are used, false if not
	 */
	virtual bool UsesTextureLightmaps(int32 InWidth, int32 InHeight) const;

	/**
	 *	Returns true if the static mesh the component uses has valid lightmap texture coordinates
	 */
	virtual bool HasLightmapTextureCoordinates() const;

	/**
	 *	Get the memory used for texture-based light and shadow maps of the given width and height
	 *
	 *	@param	InWidth						The desired width of the light/shadow map
	 *	@param	InHeight					The desired height of the light/shadow map
	 *	@param	OutLightMapMemoryUsage		The resulting lightmap memory used
	 *	@param	OutShadowMapMemoryUsage		The resulting shadowmap memory used
	 */
	virtual void GetTextureLightAndShadowMapMemoryUsage(int32 InWidth, int32 InHeight, int32& OutLightMapMemoryUsage, int32& OutShadowMapMemoryUsage) const;

	/**
	 *	Returns the lightmap resolution used for this primitive instance in the case of it supporting texture light/ shadow maps.
	 *	This will return the value assuming the primitive will be automatically switched to use texture mapping.
	 *
	 *	@param Width	[out]	Width of light/shadow map
	 *	@param Height	[out]	Height of light/shadow map
	 */
	virtual void GetEstimatedLightMapResolution(int32& Width, int32& Height) const;


	/**
	 * Returns the light and shadow map memory for this primite in its out variables.
	 *
	 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
	 *
	 *	@param [out]	TextureLightMapMemoryUsage		Estimated memory usage in bytes for light map texel data
	 *	@param [out]	TextureShadowMapMemoryUsage		Estimated memory usage in bytes for shadow map texel data
	 *	@param [out]	VertexLightMapMemoryUsage		Estimated memory usage in bytes for light map vertex data
	 *	@param [out]	VertexShadowMapMemoryUsage		Estimated memory usage in bytes for shadow map vertex data
	 *	@param [out]	StaticLightingResolution		The StaticLightingResolution used for Texture estimates
	 *	@param [out]	bIsUsingTextureMapping			Set to true if the mesh is using texture mapping currently; false if vertex
	 *	@param [out]	bHasLightmapTexCoords			Set to true if the mesh has the proper UV channels
	 *
	 *	@return			bool							true if the mesh has static lighting; false if not
	 */
	virtual bool GetEstimatedLightAndShadowMapMemoryUsage(
		int32& TextureLightMapMemoryUsage, int32& TextureShadowMapMemoryUsage,
		int32& VertexLightMapMemoryUsage, int32& VertexShadowMapMemoryUsage,
		int32& StaticLightingResolution, bool& bIsUsingTextureMapping, bool& bHasLightmapTexCoords) const;

	/**
	 * Determines whether any of the component's LODs require override vertex color fixups
	 *
	 * @return	true if any LODs require override vertex color fixups
	 */
	bool RequiresOverrideVertexColorsFixup();

	/**
	 * Update the vertex override colors if necessary (i.e. vertices from source mesh have changed from override colors)
	 * @param bRebuildingStaticMesh	true if we are rebuilding the static mesh used by this component
	 * @returns true if any fixup was performed.
	 */
	bool FixupOverrideColorsIfNecessary( bool bRebuildingStaticMesh = false );

	/** Save off the data painted on to this mesh per LOD if necessary */
	void CachePaintedDataIfNecessary();

	/**
	 * Copies instance vertex colors from the SourceComponent into this component
	 * @param SourceComponent The component to copy vertex colors from
	 */
	void CopyInstanceVertexColorsIfCompatible( UStaticMeshComponent* SourceComponent );

	/**
	 * Removes instance vertex colors from the specified LOD
	 * @param LODToRemoveColorsFrom Index of the LOD to remove instance colors from
	 */
	void RemoveInstanceVertexColorsFromLOD( int32 LODToRemoveColorsFrom );

	/**
	 * Removes instance vertex colors from all LODs
	 */
	void RemoveInstanceVertexColors();

	void UpdatePreCulledData(int32 LODIndex, const TArray<uint32>& PreCulledData, const TArray<int32>& NumTrianglesPerSection);

	/**
	*	Sets the value of the SectionIndexPreview flag and reattaches the component as necessary.
	*	@param	InSectionIndexPreview		New value of SectionIndexPreview.
	*/
	void SetSectionPreview(int32 InSectionIndexPreview);
	
	/** Sets the BodyInstance to use the mesh's body setup for external collision information*/
	void UpdateCollisionFromStaticMesh();

	/** Whether or not the component supports default collision from its static mesh asset */
	virtual bool SupportsDefaultCollision();

private:
	/** Initializes the resources used by the static mesh component. */
	void InitResources();

	/** Update the vertex override colors */
	void PrivateFixupOverrideColors();

protected:

	/** Whether the component type supports static lighting. */
	virtual bool SupportsStaticLighting() const override
	{
		return true;
	}

public:

	void ReleaseResources();

	/** Allocates an implementation of FStaticLightingMesh that will handle static lighting for this component */
	virtual FStaticMeshStaticLightingMesh* AllocateStaticLightingMesh(int32 LODIndex, const TArray<ULightComponent*>& InRelevantLights);

	/** Add or remove elements to have the size in the specified range. Reconstructs elements if MaxSize<MinSize */
	void SetLODDataCount( const uint32 MinSize, const uint32 MaxSize );

	/**
	 *	Switches the static mesh component to use either Texture or Vertex static lighting.
	 *
	 *	@param	bTextureMapping		If true, set the component to use texture light mapping.
	 *								If false, set it to use vertex light mapping.
	 *	@param	ResolutionToUse		If != 0, set the resolution to the given value.
	 *
	 *	@return	bool				true if successfully set; false if not
	 *								If false, set it to use vertex light mapping.
	 */
	virtual bool SetStaticLightingMapping(bool bTextureMapping, int32 ResolutionToUse);

	/**
	 * Returns the named socket on the static mesh component.
	 *
	 * @return UStaticMeshSocket of named socket on the static mesh component. None if not found.
	 */
	class UStaticMeshSocket const* GetSocketByName( FName InSocketName ) const;

	/** Returns the wireframe color to use for this component. */
	FColor GetWireframeColor() const;

	/** Get this components index in its parents blueprint created components array (used for matching instance data) */
	int32 GetBlueprintCreatedComponentIndex() const;

	void ApplyComponentInstanceData(class FStaticMeshComponentInstanceData* ComponentInstanceData);

	/** Register this component's render data with the scene for SpeedTree wind */
	void AddSpeedTreeWind();

	/** Unregister this component's render data with the scene for SpeedTree wind */
	void RemoveSpeedTreeWind();

#if WITH_EDITOR
	/** Called when the static mesh changes  */
	DECLARE_EVENT_OneParam(UStaticMeshComponent, FOnStaticMeshChanged, UStaticMeshComponent*);
	virtual FOnStaticMeshChanged& OnStaticMeshChanged() { return OnStaticMeshChangedEvent; }
private:
	FOnStaticMeshChanged OnStaticMeshChangedEvent;
#endif
};



