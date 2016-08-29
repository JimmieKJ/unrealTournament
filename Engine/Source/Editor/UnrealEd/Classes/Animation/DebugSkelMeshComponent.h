// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Components/SkeletalMeshComponent.h"
#include "DebugSkelMeshComponent.generated.h"

USTRUCT()
struct FSelectedSocketInfo
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FSelectedSocketInfo()
		: Socket( NULL )
		, bSocketIsOnSkeleton( false )
	{

	}

	/** Constructor */
	FSelectedSocketInfo( class USkeletalMeshSocket* InSocket, bool bInSocketIsOnSkeleton )
		: Socket( InSocket )
		, bSocketIsOnSkeleton( bInSocketIsOnSkeleton )
	{

	}

	/** The socket we have selected */
	class USkeletalMeshSocket* Socket;

	/** true if on skeleton, false if on mesh */
	bool bSocketIsOnSkeleton;


};

/** Different modes for Persona's Turn Table. */
namespace EPersonaTurnTableMode
{
	enum Type
	{
		Stopped,
		Playing,
		Paused
	};
};

UCLASS(transient, MinimalAPI)
class UDebugSkelMeshComponent : public USkeletalMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** If true, render a wireframe skeleton of the mesh animated with the raw (uncompressed) animation data. */
	UPROPERTY()
	uint32 bRenderRawSkeleton:1;

	/** Holds onto the bone color that will be used to render the bones of its skeletal mesh */
	//var Color		BoneColor;
	
	/** If true then the skeletal mesh associated with the component is drawn. */
	UPROPERTY()
	uint32 bDrawMesh:1;

	/** If true then the bone names associated with the skeletal mesh are displayed */
	UPROPERTY()
	uint32 bShowBoneNames:1;

	/** Bone influences viewing */
	UPROPERTY(transient)
	uint32 bDrawBoneInfluences:1;

	/** Vertex normal viewing */
	UPROPERTY(transient)
	uint32 bDrawNormals:1;

	/** Vertex tangent viewing */
	UPROPERTY(transient)
	uint32 bDrawTangents:1;

	/** Vertex binormal viewing */
	UPROPERTY(transient)
	uint32 bDrawBinormals:1;

	/** CPU skinning rendering - only for previewing in Persona */
	UPROPERTY(transient)
	uint32 bCPUSkinning : 1;

	/** Socket hit points viewing */
	UPROPERTY(transient)
	uint32 bDrawSockets:1;

	/** Skeleton sockets visible? */
	UPROPERTY(transient)
	uint32 bSkeletonSocketsVisible:1;

	/** Mesh sockets visible? */
	UPROPERTY(transient)
	uint32 bMeshSocketsVisible:1;

	/** Display raw animation bone transform */
	UPROPERTY(transient)
	uint32 bDisplayRawAnimation:1;

	/** Display non retargeted animation pose */
	UPROPERTY(Transient)
	uint32 bDisplayNonRetargetedPose:1;

	/** Display additive base bone transform */
	UPROPERTY(transient)
	uint32 bDisplayAdditiveBasePose:1;

	/** Display baked animation pose */
	UPROPERTY(Transient)
	uint32 bDisplayBakedAnimation:1;

	/** Display source animation pose */
	UPROPERTY(Transient)
	uint32 bDisplaySourceAnimation:1;

	/** Display Bound **/
	UPROPERTY(transient)
	uint32 bDisplayBound:1;

	UPROPERTY(transient)
	uint32 bPreviewRootMotion:1;

	/** Non Compressed SpaceBases for when bDisplayRawAnimation == true **/
	TArray<FTransform> UncompressedSpaceBases;

	/** Storage of Additive Base Pose for when bDisplayAdditiveBasePose == true, as they have to be calculated */
	TArray<FTransform> AdditiveBasePoses;

	/** Storage for non retargeted pose. */
	TArray<FTransform> NonRetargetedSpaceBases;

	/** Storage of Baked Animation Pose for when bDisplayBakedAnimation == true, as they have to be calculated */
	TArray<FTransform> BakedAnimationPoses;

	/** Storage of Source Animation Pose for when bDisplaySourceAnimation == true, as they have to be calculated */
	TArray<FTransform> SourceAnimationPoses;

	/** Color render mode enum value - 0 - none, 1 - tangent, 2 - normal, 3 - mirror, 4 - bone weighting */
	//var native transient int ColorRenderMode;
	
	/** Array of bones to render bone weights for */
	UPROPERTY(transient)
	TArray<int32> BonesOfInterest;
	
	/** Array of sockets to render manipulation widgets for
	/	Storing a pointer to the actual socket rather than a name, as we don't care here
	/	whether the socket is on the skeleton or the mesh! */
	UPROPERTY(transient)
	TArray< FSelectedSocketInfo > SocketsOfInterest;

	/** Array of materials to restore when not rendering blend weights */
	UPROPERTY(transient)
	TArray<class UMaterialInterface*> SkelMaterials;
	
	UPROPERTY(transient)
	class UAnimPreviewInstance* PreviewInstance;

	UPROPERTY(transient)
	class UAnimInstance* SavedAnimScriptInstance;

	/** Does this component use in game bounds or does it use bounds calculated from bones */
	UPROPERTY(transient)
	bool bIsUsingInGameBounds;
	
	/** true if wind effects on clothing is enabled */
	bool bEnableWind;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	
	// engine only draw bounds IF selected
	// @todo fix this properly
	// this isn't really the best way to do this, but for now
	// we'll just mark as selected
	virtual bool ShouldRenderSelected() const override
	{
		return bDisplayBound;
	}
	//~ End UPrimitiveComponent Interface.

	//~ Begin SkinnedMeshComponent Interface
	virtual bool ShouldCPUSkin() override;
	virtual void PostInitMeshObject(class FSkeletalMeshObject* MeshObject) override;
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = NULL) override;
	//~ End SkinnedMeshComponent Interface

	//~ Begin SkeletalMeshComponent Interface
	virtual void InitAnim(bool bForceReinit) override;
	virtual bool IsWindEnabled() const override;
	//~ End SkeletalMeshComponent Interface
	// Preview.
	// @todo document
	UNREALED_API bool IsPreviewOn() const;

	// @todo document
	UNREALED_API FString GetPreviewText() const;

	// @todo anim : you still need to give asset, so that we know which one to disable
	// we can disable per asset, so that if some other window disabled before me, I don't accidently turn it off
	UNREALED_API void EnablePreview(bool bEnable, class UAnimationAsset * PreviewAsset);

	/**
	 * Update material information depending on color render mode 
	 * Refresh/replace materials 
	 */
	UNREALED_API void SetShowBoneWeight(bool bNewShowBoneWeight);

	/**
	 * Does it use in-game bounds or bounds calculated from bones
	 */
	UNREALED_API bool IsUsingInGameBounds() const;

	/**
	 * Set to use in-game bounds or bounds calculated from bones
	 */
	UNREALED_API void UseInGameBounds(bool bUseInGameBounds);

	/**
	 * Test if in-game bounds are as big as preview bounds
	 */
	UNREALED_API bool CheckIfBoundsAreCorrrect();

	/** 
	 * Update components position based on animation root motion
	 */
	UNREALED_API void ConsumeRootMotion(const FVector& FloorMin, const FVector& FloorMax);

#if WITH_EDITOR
	//TODO - This is a really poor way to post errors to the user. Work out a better way.
	struct FAnimNotifyErrors
	{
		FAnimNotifyErrors(UObject* InSourceNotify)
		: SourceNotify(InSourceNotify)
		{}
		UObject* SourceNotify;
		TArray<FString> Errors;
	};
	TArray<FAnimNotifyErrors> AnimNotifyErrors;
	virtual void ReportAnimNotifyError(const FText& Error, UObject* InSourceNotify) override;
	virtual void ClearAnimNotifyErrors(UObject* InSourceNotify) override;
#endif

#if WITH_APEX_CLOTHING

	enum ESectionDisplayMode
	{
		None = -1,
		ShowAll,
		ShowOnlyClothSections,
		HideOnlyClothSections,
		NumSectionDisplayMode
	};
	/** Draw All/ Draw only clothing sections/ Hide only clothing sections */
	int32 SectionsDisplayMode;

	/** 
	 * toggle visibility between cloth sections and non-cloth sections for all LODs
	 * if bShowOnlyClothSections is true, shows only cloth sections. On the other hand, 
	 * if bShowOnlyClothSections is false, hides only cloth sections.
	 */
	UNREALED_API void ToggleClothSectionsVisibility(bool bShowOnlyClothSections);
	/** Restore all section visibilities to original states for all LODs */
	UNREALED_API void RestoreClothSectionsVisibility();

	int32 FindCurrentSectionDisplayMode();

	/** to avoid clothing reset while modifying properties in Persona */
	virtual void CheckClothTeleport() override;

#endif //#if WITH_APEX_CLOTHING

private:

	// Helper function to generate space bases for current frame
	void GenSpaceBases(TArray<FTransform>& OutSpaceBases);

public:
	/** Current turn table mode */
	EPersonaTurnTableMode::Type TurnTableMode;
	/** Current turn table speed scaling */
	float TurnTableSpeedScaling;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
};



