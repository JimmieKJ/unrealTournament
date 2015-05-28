// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Components/MeshComponent.h"
#include "SkinnedMeshComponent.generated.h"

//
// Forward declarations
//
class FSkeletalMeshResource;
struct FSkelMeshChunk;
class FSkeletalMeshVertexBuffer;
//
// Bone Visibility.
//

/** The valid BoneVisibilityStates values; A bone is only visible if it is *exactly* 1 */
UENUM()
enum EBoneVisibilityStatus
{
	BVS_HiddenByParent,    // Bone is hidden because it's parent is hidden
	BVS_Visible,    // Bone is visible
	BVS_ExplicitlyHidden,    // Bone is hidden directly
	BVS_MAX,
};

/** PhysicsBody options when bone is hidden */
UENUM()
enum EPhysBodyOp
{
	// don't do anything
	PBO_None,
	// terminate - if you terminate, you won't be able to re-init when unhidden
	PBO_Term,
	// disable collision - it will enable collision when unhidden
	PBO_Disable,
	PBO_MAX,
};

/** Skinned Mesh Update Flag based on rendered or not */
UENUM()
namespace EMeshComponentUpdateFlag
{
	enum Type
	{
		// Always Tick and Refresh BoneTransforms whether rendered or not
		AlwaysTickPoseAndRefreshBones,
		// Always Tick, but Refresh BoneTransforms only when rendered
		AlwaysTickPose,
		// Tick only when rendered, and it will only RefreshBoneTransforms when rendered
		OnlyTickPoseWhenRendered,
	};
}

/** Flag for specifying bone space */
UENUM()
namespace EBoneSpaces
{
	enum Type
	{
		/** Set absolute position of bone in world space */
		WorldSpace		UMETA(DisplayName = "World Space"),
		/** Set position of bone in components reference frame */
		ComponentSpace	UMETA(DisplayName = "Component Space"),
		/** Set position of bone relative to parent bone */
		//LocalSpace		UMETA( DisplayName = "Parent Bone Space" ),
	};
}

//
// Vertex Anim
//

/** Struct used to indicate one active vertex animation that should be applied to this USkeletalMesh when rendered. */
USTRUCT()
struct FActiveVertexAnim
{
	GENERATED_USTRUCT_BODY()

	/** The anim that we want to apply. */
	UPROPERTY()
	class UVertexAnimBase* VertAnim;

	/** Strength of the vertex animation, between 0.0 and 1.0 */
	UPROPERTY()
	float Weight;

	/** Time to evaluate the anim at */
	UPROPERTY()
	float Time;

	FActiveVertexAnim()
		: VertAnim(NULL)
		, Weight(0)
		, Time(0.f)
	{
	}

	FActiveVertexAnim(class UVertexAnimBase* InTarget, float InWeight)
	:	VertAnim(InTarget)
	,	Weight(InWeight)
	,	Time(0.f)
	{}

	FActiveVertexAnim(class UVertexAnimBase* InTarget, float InWeight, float InTime)
		:	VertAnim(InTarget)
		,	Weight(InWeight)
		,	Time(InTime)
	{}

	bool operator==(const FActiveVertexAnim& Other) const
	{
		// if target is same, we consider same 
		// any equal operator to check if it's same, 
		// we just check if this is same anim
		return VertAnim == Other.VertAnim;
	}
};

/** LOD specific setup for the skeletal mesh component */
USTRUCT()
struct FSkelMeshComponentLODInfo
{
	GENERATED_USTRUCT_BODY()

	/** Material corresponds to section. To show/hide each section, use this **/
	UPROPERTY()
	TArray<bool> HiddenMaterials;


		FSkelMeshComponentLODInfo()
		{
		}
	
};


/**
 *
 * Skinned mesh component that supports bone skinned mesh rendering.
 * This class does not support animation.
 *
 * @see USkeletalMeshComponent
*/

UCLASS(hidecategories=Object, config=Engine, editinlinenew, abstract)
class ENGINE_API USkinnedMeshComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** The skeletal mesh used by this component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mesh")
	class USkeletalMesh* SkeletalMesh;

	//
	// MasterPoseComponent.
	//
	
	/**
	 *	If set, this SkeletalMeshComponent will not use its SpaceBase for bone transform, but will
	 *	use the SpaceBases array in the MasterPoseComponent. This is used when constructing a character using multiple skeletal meshes sharing the same
	 *	skeleton within the same Actor.
	 */
	TWeakObjectPtr< class USkinnedMeshComponent > MasterPoseComponent;

private:
	/** Temporary array of of component-space bone matrices, update each frame and used for rendering the mesh. */
	TArray<FTransform> SpaceBasesArray[2];

	/** The index for the space bases buffer we can currently write to */
	int32 CurrentEditableSpaceBases;

	/** The index for the space bases buffer we can currently read from */
	int32 CurrentReadSpaceBases;

protected:
	/** Are we using double buffered blend spaces */
	bool bDoubleBufferedBlendSpaces;

	/** 
	 * If set, this component has slave pose components that are associated with this 
	 * Note this is weak object ptr, so it will go away unless you have other strong reference
	 */
	TArray< TWeakObjectPtr<USkinnedMeshComponent> > SlavePoseComponents;
public:
	/**
	 *	Mapping between bone indices in this component and the parent one. Each element is the index of the bone in the MasterPoseComponent.
	 *	Size should be the same as USkeletalMesh.RefSkeleton size (ie number of bones in this skeleton).
	 */
	TArray<int32> MasterBoneMap;

	/** 
	 * When true, we will just using the bounds from our MasterPoseComponent.  This is useful for when we have a Mesh Parented
	 * to the main SkelMesh (e.g. outline mesh or a full body overdraw effect that is toggled) that is always going to be the same
	 * bounds as parent.  We want to do no calculations in that case.
	 */
	UPROPERTY()
	uint32 bUseBoundsFromMasterPoseComponent:1;

	/** Array indicating all active vertex animations. This array is updated inside RefreshBoneTransforms based on the Anim Blueprint. */
	UPROPERTY(transient)
	TArray<struct FActiveVertexAnim> ActiveVertexAnims;

#if WITH_EDITORONLY_DATA
	/** Index of the chunk to preview... If set to -1, all chunks will be rendered */
	UPROPERTY(transient)
	int32 ChunkIndexPreview;

	/** Index of the section to preview... If set to -1, all section will be rendered */
	UPROPERTY(transient)
	int32 SectionIndexPreview;

#endif // WITH_EDITORONLY_DATA
	//
	// Physics.
	//
	
	/**
	 *	PhysicsAsset is set in SkeletalMesh by default, but you can override with this value
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Physics)
	class UPhysicsAsset* PhysicsAssetOverride;

	//
	// Level of detail.
	//
	
	/** If 0, auto-select LOD level. if >0, force to (ForcedLodModel-1). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	int32 ForcedLodModel;

	/**
	 * This is the min LOD that this component will use.  (e.g. if set to 2 then only 2+ LOD Models will be used.) This is useful to set on
	 * meshes which are known to be a certain distance away and still want to have better LODs when zoomed in on them.
	 **/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	int32 MinLodModel;

	/** 
	 *	Best LOD that was 'predicted' by UpdateSkelPose. 
	 *	This is what bones were updated based on, so we do not allow rendering at a better LOD than this. 
	 */
	UPROPERTY()
	int32 PredictedLODLevel;

	/** LOD level from previous frame, so we can detect changes in LOD to recalc required bones. */
	UPROPERTY()
	int32 OldPredictedLODLevel;

	/**	High (best) DistanceFactor that was desired for rendering this USkeletalMesh last frame. Represents how big this mesh was in screen space   */
	UPROPERTY()
	float MaxDistanceFactor;

	/** LOD array info. Each index will correspond to the LOD index **/
	UPROPERTY(transient)
	TArray<struct FSkelMeshComponentLODInfo> LODInfo;

	//
	// Rendering options.
	//
	
	/**
	 * Allows adjusting the desired streaming distance of streaming textures that uses UV 0.
	 * 1.0 is the default, whereas a higher value makes the textures stream in sooner from far away.
	 * A lower value (0.0-1.0) makes the textures stream in later (you have to be closer).
	 * Value can be < 0 (from legcay content, or code changes)
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=SkeletalMesh)
	float StreamingDistanceMultiplier;

	/**
	 * Wireframe color
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=SkeletalMesh)
	FColor WireframeColor;

	/** Forces the mesh to draw in wireframe mode. */
	UPROPERTY()
	uint32 bForceWireframe:1;

	/** Draw the skeleton hierarchy for this skel mesh. */
	UPROPERTY()
	uint32 bDisplayBones:1;

	/** Disable Morphtarget for this component. */
	UPROPERTY()
	uint32 bDisableMorphTarget:1;

	/** Don't bother rendering the skin. */
	UPROPERTY()
	uint32 bHideSkin:1;
	/** Array of bone visibilities (containing one of the values in EBoneVisibilityStatus for each bone).  A bone is only visible if it is *exactly* 1 (BVS_Visible) */
	TArray<uint8> BoneVisibilityStates;

	/**
	 *	If true, use per-bone motion blur on this skeletal mesh (requires additional rendering, can be disabled to save performance).
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=SkeletalMesh)
	uint32 bPerBoneMotionBlur:1;

	//
	// Misc.
	//
	
	/** When true, skip using the physics asset etc. and always use the fixed bounds defined in the SkeletalMesh. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=SkeletalMesh)
	uint32 bComponentUseFixedSkelBounds:1;

	/** If true, when updating bounds from a PhysicsAsset, consider _all_ BodySetups, not just those flagged with bConsiderForBounds. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=SkeletalMesh)
	uint32 bConsiderAllBodiesForBounds:1;

	/** This is update frequency flag even when our Owner has not been rendered recently
	 * 
	 * SMU_AlwaysTickPoseAndRefreshBones,			// Always Tick and Refresh BoneTransforms whether rendered or not
	 * SMU_AlwaysTickPose,							// Always Tick, but Refresh BoneTransforms only when rendered
	 * SMU_OnlyTickPoseWhenRendered,				// Tick only when rendered, and it will only RefreshBoneTransforms when rendered
	 * 
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=SkeletalMesh)
	TEnumAsByte<EMeshComponentUpdateFlag::Type> MeshComponentUpdateFlag;

private:
	/** If true, UpdateTransform will always result in a call to MeshObject->Update. */
	UPROPERTY(transient)
	uint32 bForceMeshObjectUpdate:1;

public:
	/** If true, DistanceFactor for this SkinnedMeshComponent will be added to global chart. */
	UPROPERTY()
	uint32 bChartDistanceFactor:1;

	/** Whether or not we can highlight selected sections - this should really only be done in the editor */
	UPROPERTY(transient)
	uint32 bCanHighlightSelectedSections:1;

	/** true if mesh has been recently rendered, false otherwise */
	UPROPERTY(transient)
	uint32 bRecentlyRendered:1;

	/** Editor only. Used for visualizing drawing order in Animset Viewer. If < 1.0,
	  * only the specified fraction of triangles will be rendered
	  */
	UPROPERTY(transient)
	float ProgressiveDrawingFraction;

	/** Editor only. Used for manually selecting the alternate indices for
	  * TRISORT_CustomLeftRight sections.
	  */
	UPROPERTY(transient)
	uint8 CustomSortAlternateIndexMode;

	/** 
	 * Override the Physics Asset of the mesh. It uses SkeletalMesh.PhysicsAsset, but if you'd like to override use this function
	 * 
	 * @param	NewPhysicsAsset	New PhysicsAsset
	 * @param	bForceReInit	Force reinitialize
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	virtual void SetPhysicsAsset(class UPhysicsAsset* NewPhysicsAsset, bool bForceReInit = false);

	/**
	 * Find the index of bone by name. Looks in the current SkeletalMesh being used by this SkeletalMeshComponent.
	 * 
	 * @param BoneName Name of bone to look up
	 * 
	 * @return Index of the named bone in the current SkeletalMesh. Will return INDEX_NONE if bone not found.
	 *
	 * @see USkeletalMesh::GetBoneIndex.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	int32 GetBoneIndex( FName BoneName ) const;

	/** 
	 * Get Bone Name from index
	 * @param BoneIndex Index of the bone
	 *
	 * @return the name of the bone at the specified index 
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	FName GetBoneName(int32 BoneIndex) const;

	/**
	 * Returns bone name linked to a given named socket on the skeletal mesh component.
	 * If you're unsure to deal with sockets or bones names, you can use this function to filter through, and always return the bone name.
	 *
	 * @param	bone name or socket name
	 *
	 * @return	bone name
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	FName GetSocketBoneName(FName InSocketName) const;

	/** 
	 * Change the SkeletalMesh that is rendered for this Component. Will re-initialize the animation tree etc. 
	 *
	 * @param NewMesh New mesh to set for this component
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	virtual void SetSkeletalMesh(class USkeletalMesh* NewMesh);

	/** 
	 * Get Parent Bone of the input bone
	 * 
	 * @param BoneName Name of the bone
	 *
	 * @return the name of the parent bone for the specified bone. Returns 'None' if the bone does not exist or it is the root bone 
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	FName GetParentBone(FName BoneName) const;

public:
	/** Object responsible for sending bone transforms, vertex anim state etc. to render thread. */
	class FSkeletalMeshObject*	MeshObject;

	/** Gets the skeletal mesh resource used for rendering the component. */
	FSkeletalMeshResource* GetSkeletalMeshResource() const;

	// Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual FString GetDetailedInfoInternal() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface

protected:
	// Begin UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
#if 0
	/** return true if this component requires end of frame updates to happen from the game thread. */
	virtual bool RequiresGameThreadEndOfFrameUpdates()
	{
		//@todo there isn't a great reason for this, it has to do with activating morph targets triggering material usage flag changes and potentially shader compilation.
		// it seems unlikely this would cook right anyway
		return true;
	}
#endif
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual UObject const* AdditionalStatObject() const override;
	// End UActorComponent interface

public:
	// Begin USceneComponent interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual bool HasAnySockets() const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;
	virtual void UpdateOverlaps(TArray<FOverlapInfo> const* PendingOverlaps=NULL, bool bDoNotifies=true, const TArray<FOverlapInfo>* OverlapsAtEndLocation=NULL) override;
	// End USceneComponent interface

	// Begin UPrimitiveComponent interface
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const override;
	virtual int32 GetNumMaterials() const override;
	// End UPrimitiveComponent interface

	/**
	 *	Sets the value of the bForceWireframe flag and reattaches the component as necessary.
	 *
	 *	@param	InForceWireframe		New value of bForceWireframe.
	 */
	void SetForceWireframe(bool InForceWireframe);

	/**
	 * Function returns whether or not CPU skinning should be applied
	 * Allows the editor to override the skinning state for editor tools
	 *
	 * @return true if should CPU skin. false otherwise
	 */
	virtual bool ShouldCPUSkin();

	/** 
	 * Function to operate on mesh object after its created, 
	 * but before it's attached.
	 * 
	 * @param MeshObject - Mesh Object owned by this component
	 */
	virtual void PostInitMeshObject(class FSkeletalMeshObject*) {}

	/** 
	 * Simple, CPU evaluation of a vertex's skinned position (returned in component space) 
	 *
	 * @param VertexIndex Vertex Index. If compressed, this will be slow. 
	 */
	virtual FVector GetSkinnedVertexPosition(int32 VertexIndex) const;

	/**
	* CPU evaluation of the positions of all vertices (returned in component space)
	*
	* @param OutPositions buffer to place positions into
	*/
	virtual void ComputeSkinnedPositions(TArray<FVector> & OutPositions) const;

	/**
	* Returns color of the vertex.
	*
	* @param VertexIndex Vertex Index. If compressed, this will be slow.
	*/
	FColor GetVertexColor(int32 VertexIndex) const;

	/**
	 * Update functions
	 */

	/** 
	 * Refresh Bone Transform (SpaceBases)
	 * Each class will need to implement this function
	 * Ideally this function should be atomic (not relying on Tick or any other update.) 
	 * 
	 * @param TickFunction Supplied as non null if we are running in a tick, allows us to create graph tasks for parallelism
	 * 
	 */
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = NULL) PURE_VIRTUAL(USkinnedMeshComponent::RefreshBoneTransforms, );

	/**
	 * Tick Pose, this function ticks and do whatever it needs to do in this frame, should be called before RefreshBoneTransforms
	 *
	 * @param DeltaTime DeltaTime
	 *
	 * @return	Return true if anything modified. Return false otherwise
	 * @param bNeedsValidRootMotion - Networked games care more about this, but if false we can do less calculations
	 */
	virtual void TickPose(float DeltaTime, bool bNeedsValidRootMotion) { TickUpdateRate(DeltaTime, bNeedsValidRootMotion); }

	/** 
	 * Update Slave Component. This gets called when MasterPoseComponent!=NULL
	 * 
	 */
	virtual void UpdateSlaveComponent();

	/** 
	 * Update the PredictedLODLevel and MaxDistanceFactor in the component from its MeshObject. 
	 * 
	 * @return true if LOD has been changed. false otherwise.
	 */
	virtual bool UpdateLODStatus();

	/** Initialize the LOD entries for the component */
	void InitLODInfos();

	/**
	 * Rebuild BoneVisibilityStates array. Mostly refresh information of bones for BVS_HiddenByParent 
	 */
	void RebuildVisibilityArray();

	/** Vertex anim */

	/**
	 * Checks/updates material usage on proxy based on current morph target usage
	 */
	void UpdateMorphMaterialUsageOnProxy();
	
	/** Access Space Bases for reading */
	const TArray<FTransform>& GetSpaceBases() const { return SpaceBasesArray[CurrentReadSpaceBases]; }

	/** Get Access to the current editable space bases */
	TArray<FTransform>& GetEditableSpaceBases() { return SpaceBasesArray[CurrentEditableSpaceBases]; }
	const TArray<FTransform>& GetEditableSpaceBases() const { return SpaceBasesArray[CurrentEditableSpaceBases]; }

	/** Get the number of space bases */
	int32 GetNumSpaceBases() const { return GetSpaceBases().Num(); }

	/** Flip the editable space base buffer */
	void FlipEditableSpaceBases();

	void SetSpaceBaseDoubleBuffering(bool bInDoubleBufferedBlendSpaces);

protected:

	/** Track whether we still need to flip to recently modified buffer */
	bool bNeedToFlipSpaceBaseBuffers;

	/**
	* Combine CurveKeys (that reference morph targets by name) and ActiveAnims (that reference vertex anims by reference) into the ActiveVertexAnims array.
	*/
	static TArray<struct FActiveVertexAnim> UpdateActiveVertexAnims(const USkeletalMesh* InSkeletalMesh, const TMap<FName, float>& InMorphCurveAnims, const TArray<FActiveVertexAnim>& InActiveAnims);

	/** 
	 * Should update transform in Tick
	 * 
	 * @param bLODHasChanged	: Has LOD been changed since last time?
	 * 
	 * @return : return true if need transform update. false otherwise.
	 */
	virtual bool ShouldUpdateTransform(bool bLODHasChanged) const;

	/** 
	 * Should tick  pose (by calling TickPose) in Tick
	 * 
	 * @return : return true if should Tick. false otherwise.
	 */
	virtual bool ShouldTickPose() const;

	/**
	 * Allocate Transform Data array including SpaceBases, BoneVisibilityStates 
	 *
	 */	
	virtual bool AllocateTransformData();
	virtual void DeallocateTransformData();

	/** LocalBounds cached, so they're computed just once. */
	UPROPERTY(Transient)
	FBoxSphereBounds CachedLocalBounds;

	/** true when CachedLocalBounds is up to date. */
	UPROPERTY(Transient)
	bool bCachedLocalBoundsUpToDate;

	/** Invalidate Cached Bounds, when Mesh Component has been updated. */
	void InvalidateCachedBounds();

	/** Update Mesh Bound information based on input
	 * 
	 * @param RootOffset	: Root Bone offset from mesh location
	 *						  If MasterPoseComponent exists, it will applied to MasterPoseComponent's bound
	 * @param UsePhysicsAsset	: Whether or not to use PhysicsAsset for calculating bound of mesh
	 */
	FBoxSphereBounds CalcMeshBound(const FVector& RootOffset, bool UsePhysicsAsset, const FTransform& Transform) const;

	/**
	 * return true if it needs update. Return false if not
	 */
	bool ShouldUpdateBoneVisibility() const;

	// Update Rate
public:
	/** if TRUE, Owner will determine how often animation will be updated and evaluated. See AnimUpdateRateTick() 
	 * This allows to skip frames for performance. (For example based on visibility and size on screen). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Optimization)
	bool bEnableUpdateRateOptimizations;

	/** Enable on screen debugging of update rate optimization. 
	 * Red = Skipping 0 frames, Green = skipping 1 frame, Blue = skipping 2 frames, black = skipping more than 2 frames. 
	 * @todo: turn this into a console command. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Optimization)
	bool bDisplayDebugUpdateRateOptimizations;

private:
	/** Update Rate Optimization ticking. */
	void TickUpdateRate(float DeltaTime, bool bNeedsValidRootMotion);
	
public:
	/**
	 * Set MasterPoseComponent for this component
	 *
	 * @param NewMasterBoneComponent New MasterPoseComponent
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh", meta=(UnsafeDuringActorConstruction="true"))
	void SetMasterPoseComponent(USkinnedMeshComponent* NewMasterBoneComponent);

	/** 
	 * Refresh Slave Components if exists
	 * 
	 * This isn't necessary in any other case except in editor where you need to mark them as dirty for rendering
	 */
	void RefreshSlaveComponents();

	/**
	 * Update MasterBoneMap for MasterPoseComponent and this component
	 */
	void UpdateMasterBoneMap();

	/**
	 * @return SkeletalMeshSocket of named socket on the skeletal mesh component, or NULL if not found.
	 */
	class USkeletalMeshSocket const* GetSocketByName( FName InSocketName ) const;


	/** 
	 * Get Bone Matrix from index
	 *
	 * @param BoneIndex Index of the bone
	 * 
	 * @return the matrix of the bone at the specified index 
	 */
	FMatrix GetBoneMatrix( int32 BoneIndex ) const;

	/** 
	 * Get world space bone transform from bone index, also specifying the component transform to use
	 * 
	 * @param BoneIndex Index of the bone
	 *
	 * @return the transform of the bone at the specified index 
	 */
	FTransform GetBoneTransform( int32 BoneIndex, const FTransform& LocalToWorld ) const;

	/** 
	 * Get Bone Transform from index
	 * 
	 * @param BoneIndex Index of the bone
	 *
	 * @return the transform of the bone at the specified index 
	 */
	FTransform GetBoneTransform( int32 BoneIndex ) const;

	/** Get Bone Rotation in Quaternion
	 *
	 * @param BoneName Name of the bone
	 * @param Space	0 == World, 1 == Local (Component)
	 * 
	 * @return Quaternion of the bone
	 */
	FQuat GetBoneQuaternion(FName BoneName, EBoneSpaces::Type Space = EBoneSpaces::WorldSpace) const;

	/** Get Bone Location
	 *
	 * @param BoneName Name of the bone
	 * @param Space	0 == World, 1 == Local (Component)
	 * 
	 * @return Vector of the bone
	 */
	FVector GetBoneLocation( FName BoneName, EBoneSpaces::Type Space = EBoneSpaces::WorldSpace) const; 

	/** 
	 * Fills the given array with the names of all the bones in this component's current SkeletalMesh 
	 * 
	 * @param (out) Array to fill the names of the bones
	 */
	void GetBoneNames(TArray<FName>& BoneNames);

	/**
	 * Tests if BoneName is child of (or equal to) ParentBoneName.
	 *
	 * @param BoneName Name of the bone
	 * @param ParentBone Name to check
	 *
	 * @return true if child (strictly, not same). false otherwise
	 * Note - will return false if ChildBoneIndex is the same as ParentBoneIndex ie. must be strictly a child.
	 */
	bool BoneIsChildOf(FName BoneName, FName ParentBoneName) const;

	/** 
	 * Gets the local-space position of a bone in the reference pose. 
	 *
	 * @param BoneIndex Index of the bone
	 *
	 * @return Local space reference position 
	 */
	FVector GetRefPosePosition(int32 BoneIndex);

	/** finds a vector pointing along the given axis of the given bone
	 *
	 * @param BoneName the name of the bone to find
	 * @param Axis the axis of that bone to return
	 *
	 * @return the direction of the specified axis, or (0,0,0) if the specified bone was not found
	 */
	FVector GetBoneAxis(FName BoneName, EAxis::Type Axis) const;

	/**
	 *	Transform a location/rotation from world space to bone relative space.
	 *	This is handy if you know the location in world space for a bone attachment, as AttachComponent takes location/rotation in bone-relative space.
	 *
	 * @param BoneName Name of bone
	 * @param InPosition Input position
	 * @param InRotation Input rotation
	 * @param OutPosition (out) Transformed position
	 * @param OutRotation (out) Transformed rotation
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	void TransformToBoneSpace( FName BoneName, FVector InPosition, FRotator InRotation, FVector& OutPosition, FRotator& OutRotation ) const;

	/**
	 *	Transform a location/rotation in bone relative space to world space.
	 *
	 * @param BoneName Name of bone
	 * @param InPosition Input position
	 * @param InRotation Input rotation
	 * @param OutPosition (out) Transformed position
	 * @param OutRotation (out) Transformed rotation
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	void TransformFromBoneSpace( FName BoneName, FVector InPosition, FRotator InRotation, FVector& OutPosition, FRotator& OutRotation );

	/** finds the closest bone to the given location
	 *
	 * @param TestLocation the location to test against
	 * @param BoneLocation (optional, out) if specified, set to the world space location of the bone that was found, or (0,0,0) if no bone was found
	 * @param IgnoreScale (optional) if specified, only bones with scaling larger than the specified factor are considered
	 *
	 * @return the name of the bone that was found, or 'None' if no bone was found
	 */
	FName FindClosestBone(FVector TestLocation, FVector* BoneLocation = NULL, float IgnoreScale = 0.f) const;

	/**
	 * Find a named MorphTarget from the current SkeletalMesh
	 *
	 * @param MorphTargetName Name of MorphTarget to look for.
	 *
	 * @return Pointer to found MorphTarget. Returns NULL if could not find target with that name.
	 */
	virtual class UMorphTarget* FindMorphTarget( FName MorphTargetName ) const;

	/**	
	 * Find all bones by name within given radius 
	 * 
	 * @param Origin origin of the radius
	 * @param Radius radius of the sphere
	 * @param TraceFlags Trace flags
	 * @param (out) OutBones Bones within the sphere
	 *
	 * @return true if any bone is within the radius. false otherwise
	 */
	bool GetBonesWithinRadius( FVector Origin, float Radius, int32 TraceFlags, TArray<FName>& OutBones );

	/**
	 *	Hides the specified bone. You can also set option for physics body.
	 *
	 *	@param	BoneIndex			Index of the bone
	 *	@param	PhysBodyOption		Option for physics bodies that attach to the bones to be hidden
	 */
	virtual void HideBone( int32 BoneIndex, EPhysBodyOp PhysBodyOption );

	/**
	 *	Unhides the specified bone.  
	 *
	 *	@param	BoneIndex			Index of the bone
	 */
	virtual void UnHideBone( int32 BoneIndex );

	/** 
	 *	Determines if the specified bone is hidden. 
	 *
	 *	@param	BoneIndex			Index of the bone
	 *
	 *	@return true if hidden
	 */
	bool IsBoneHidden( int32 BoneIndex ) const;

	/**
	 *	Hides the specified bone with name.  Currently this just enforces a scale of 0 for the hidden bones.
	 *	Compoared to HideBone By Index - This keeps track of list of bones and update when LOD changes
	 *
	 *	@param  BoneName            Name of bone to hide
	 *	@param	PhysBodyOption		Option for physics bodies that attach to the bones to be hidden
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	void HideBoneByName( FName BoneName, EPhysBodyOp PhysBodyOption );

	/**
	 *	UnHide the specified bone with name.  Currently this just enforces a scale of 0 for the hidden bones.
	 *	Compoared to HideBone By Index - This keeps track of list of bones and update when LOD changes
	 *	@param  BoneName            Name of bone to unhide
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	void UnHideBoneByName( FName BoneName );

	/** 
	 *	Determines if the specified bone is hidden. 
	 *
	 *	@param  BoneName            Name of bone to check
	 *
	 *	@return true if hidden
	 */
	UFUNCTION(BlueprintCallable, Category="Components|SkinnedMesh")
	bool IsBoneHiddenByName( FName BoneName );

	/**
	 *  Show/Hide Material - technical correct name for this is Section, but seems Material is mostly used
	 *  This disable rendering of certain Material ID (Section)
	 *
	 * @param MaterialID - id of the material to match a section on and to show/hide
	 * @param bShow - true to show the section, otherwise hide it
	 * @param LODIndex - index of the lod entry since material mapping is unique to each LOD
	 */
	void ShowMaterialSection(int32 MaterialID, bool bShow, int32 LODIndex);

	/** 
	 * Return PhysicsAsset for this SkeletalMeshComponent
	 * It will return SkeletalMesh's PhysicsAsset unless PhysicsAssetOverride is set for this component
	 *
	 * @return : PhysicsAsset that's used by this component
	 */
	class UPhysicsAsset* GetPhysicsAsset() const;

private:
	/** 
	 * Simple, CPU evaluation of a vertex's skinned position helper function
	 */
	template <bool bExtraBoneInfluencesT, bool bCachedMatrices>
	FVector GetTypedSkinnedVertexPosition(const FSkelMeshChunk& Chunk, const FSkeletalMeshVertexBuffer& VertexBufferGPUSkin, int32 VertIndex, bool bSoftVertex, const TArray<FMatrix> & RefToLocals = TArray<FMatrix>()) const;

	/**
	 * Gets called when register, verifies all ActiveVertexAnimation is still valid 
	 */
	virtual void RefreshActiveVertexAnims() {};

	// Animation update rate control.
public:
	/** Animation Update Rate optimization parameters. */
	struct FAnimUpdateRateParameters* AnimUpdateRateParams;

	/** Updates AnimUpdateRateParams, used by SkinnedMeshComponents.
	* 
	* @param bRecentlyRendered : true if at least one SkinnedMeshComponent on this Actor has been rendered in the last second.
	* @param MaxDistanceFactor : Largest SkinnedMeshComponent of this Actor drawn on screen. */
	void AnimUpdateRateSetParams(uint8 UpdateRateShift, float DeltaTime, const bool & bRecentlyRendered, const float& MaxDistanceFactor, const bool & bPlayingRootMotion);

	virtual bool IsPlayingRootMotion(){ return false; }

	virtual bool IsPlayingRootMotionFromEverything(){ return false; }
};
