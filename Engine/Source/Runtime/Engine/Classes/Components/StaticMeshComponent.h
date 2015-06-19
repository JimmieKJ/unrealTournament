// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTypes.h"
#include "Components/MeshComponent.h"
#include "Runtime/RenderCore/Public/PackedNormal.h"

#include "StaticMeshComponent.generated.h"

class FColorVertexBuffer;
class UStaticMesh;
class FStaticMeshStaticLightingMesh;
class ULightComponent;
struct FEngineShowFlags;
struct FConvexVolume;

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
	UPROPERTY(BlueprintReadOnly, Category=Rendering)
	bool bOverrideWireframeColor;

	/** Wireframe color to use if bOverrideWireframeColor is true */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering, meta=(editcondition = "bOverrideWireframeColor"))
	FColor WireframeColorOverride;

#if WITH_EDITORONLY_DATA
	/** The section currently selected in the Editor. */
	UPROPERTY(transient)
	int32 SelectedEditorSection;
#endif

	/**
	 *	Ignore this instance of this static mesh when calculating streaming information.
	 *	This can be useful when doing things like applying character textures to static geometry,
	 *	to avoid them using distance-based streaming.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=TextureStreaming)
	uint32 bIgnoreInstanceForTextureStreaming:1;

	/** Whether to override the lightmap resolution defined in the static mesh. */
	UPROPERTY(BlueprintReadOnly, Category=Lighting)
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

#if WITH_EDITORONLY_DATA
	/** Derived data key of the static mesh, used to determine if an update from the source static mesh is required. */
	UPROPERTY()
	FString StaticMeshDerivedDataKey;
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

public:

	// Begin UObject interface.
	virtual void BeginDestroy() override;
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;	
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PreEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PreSave() override;
	virtual void PostLoad() override;
	virtual bool AreNativePropertiesIdenticalTo( UObject* Other ) const override;
	virtual FString GetDetailedInfoInternal() const override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject interface.

	// Begin USceneComponent Interface
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
	// End USceneComponent Interface

	// Begin UActorComponent interface.
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
	virtual FName GetComponentInstanceDataType() const override;
	// End UActorComponent interface.

	// Begin UPrimitiveComponent interface.
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
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const override;
	virtual class UBodySetup* GetBodySetup() override;
	virtual bool CanEditSimulatePhysics() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual bool UsesOnlyUnlitMaterials() const override;
	virtual bool GetLightMapResolution( int32& Width, int32& Height ) const override;
	virtual int32 GetStaticLightMapResolution() const override;
	/** Returns true if the component is static AND has the right static mesh setup to support lightmaps. */
	virtual bool HasValidSettingsForStaticLighting() const override;

	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;

	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
#if WITH_EDITOR
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
#endif
	// End UPrimitiveComponent interface.

	// Begin INavRelevantInterface interface.
	virtual bool IsNavigationRelevant() const override;
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	// End INavRelevantInterface interface.
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
	 * @param	OutLODIndices	Indices of the LODs requiring fixup, if any
	 *
	 * @return	true if any LODs require override vertex color fixups
	 */
	bool RequiresOverrideVertexColorsFixup( TArray<int32>& OutLODIndices );

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

private:
	/** Initializes the resources used by the static mesh component. */
	void InitResources();

	/** Update the vertex override colors */
	void PrivateFixupOverrideColors( const TArray<int32>& LODsToUpdate );

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
};



