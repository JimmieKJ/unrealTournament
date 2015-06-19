// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintGraphDefinitions.h"
#include "AnimGraphNode_ModifyBone.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BoneControllers/AnimNode_ModifyBone.h"
#include "AnimPreviewInstance.generated.h"

/** Enum to know how montage is being played */
UENUM()
enum EMontagePreviewType
{
	/** Playing montage in usual way. */
	EMPT_Normal, 
	/** Playing all sections. */
	EMPT_AllSections,
	EMPT_MAX,
};

/**
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

UCLASS(transient, NotBlueprintable, noteditinlinenew)
class ANIMGRAPH_API UAnimPreviewInstance : public UAnimSingleNodeInstance
{
	GENERATED_UCLASS_BODY()

	/** Controllers for individual bones */
	UPROPERTY(transient)
	TArray<FAnimNode_ModifyBone> BoneControllers;

	/** Curve modifiers */
	UPROPERTY(transient)
	TArray<FAnimNode_ModifyBone> CurveBoneControllers;

	/** Shared parameters for previewing blendspace or animsequence **/
	UPROPERTY(transient)
	float SkeletalControlAlpha;

	/** Shared parameters for previewing blendspace or animsequence **/
	UPROPERTY(transient)
	TEnumAsByte<enum EMontagePreviewType> MontagePreviewType;

	UPROPERTY(transient)
	int32 MontagePreviewStartSectionIdx;

	// Begin UAnimInstance interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaTimeX) override;
	virtual bool NativeEvaluateAnimation(FPoseContext& Output) override;
	// End UAnimInstance interface

	/** Set SkeletalControl Alpha**/
	void SetSkeletalControlAlpha(float SkeletalControlAlpha);

	UAnimSequence* GetAnimSequence();

	// Begin UAnimSingleNodeInstance interface
	virtual void RestartMontage(UAnimMontage* Montage, FName FromSection = FName()) override;
	virtual void SetAnimationAsset(UAnimationAsset* NewAsset, bool bIsLooping = true, float InPlayRate = 1.f) override;
	// End UAnimSingleNodeInstance interface

	/** Montage preview functions */
	void MontagePreview_JumpToStart();
	void MontagePreview_JumpToEnd();
	void MontagePreview_JumpToPreviewStart();
	void MontagePreview_Restart();
	void MontagePreview_PreviewNormal(int32 FromSectionIdx = INDEX_NONE);
	void MontagePreview_SetLoopNormal(bool bIsLooping, int32 PreferSectionIdx = INDEX_NONE);
	void MontagePreview_PreviewAllSections();
	void MontagePreview_SetLoopAllSections(bool bIsLooping);
	void MontagePreview_SetLoopAllSetupSections(bool bIsLooping);
	void MontagePreview_ResetSectionsOrder();
	void MontagePreview_SetLooping(bool bIsLooping);
	void MontagePreview_SetPlaying(bool bIsPlaying);
	void MontagePreview_SetReverse(bool bInReverse);
	void MontagePreview_StepForward();
	void MontagePreview_StepBackward();
	void MontagePreview_JumpToPosition(float NewPosition);
	int32 MontagePreview_FindFirstSectionAsInMontage(int32 AnySectionIdx);
	int32 MontagePreview_FindLastSection(int32 StartSectionIdx);
	float MontagePreview_CalculateStepLength();
	void MontagePreview_RemoveBlendOut();
	bool IsPlayingMontage() { return GetActiveMontageInstance() != NULL; }

	/** 
	 * Finds an already modified bone 
	 * @param	InBoneName	The name of the bone modification to find
	 * @return the bone modification or NULL if no current modification was found
	 */
	FAnimNode_ModifyBone* FindModifiedBone(const FName& InBoneName, bool bCurveController=false);

	/** 
	 * Modifies a single bone. Create a new FAnimNode_ModifyBone if one does not exist for the passed-in bone.
	 * @param	InBoneName	The name of the bone to modify
	 * @return the new or existing bone modification
	 */
	FAnimNode_ModifyBone& ModifyBone(const FName& InBoneName, bool bCurveController=false);

	/**
	 * Removes an existing bone modification
	 * @param	InBoneName	The name of the existing modification to remove
	 */
	void RemoveBoneModification(const FName& InBoneName, bool bCurveController=false);

	/**
	 * Reset all bone modified
	 */
	void ResetModifiedBone(bool bCurveController=false);

#if WITH_EDITORONLY_DATA
	bool bForceRetargetBasePose;
#endif

	/**
	 * Convert current modified bone transforms (BoneControllers) to transform curves (CurveControllers)
	 * it does based on CurrentTime. This function does not set key directly here. 
	 * It does wait until next update, and it gets the delta of transform before applying curves, and 
	 * creates curves from it, so you'll need delegate if you'd like to do something after
	 * 
	 * @param Delegate To be called once set key is completed
	 */
	void SetKey(FSimpleDelegate InOnSetKeyCompleteDelegate);

	/** 
	 * Refresh Curve Bone Controllers based on TransformCurves from Animation data
	 */
	void RefreshCurveBoneControllers();

	/**
	 * Apply all Transform Curves to the RawAnimationData of the animation
	 */
	void BakeAnimation();

	/** 
	 * Enable Controllers
	 * This is used by when editing, when controller has to be disabled
	 */
	void EnableControllers(bool bEnable);

private:
	/** 
	 * Apply Bone Controllers to the Outpose
	 *
	 * @param	Component	Component to apply bone controller to
	 * @param	BoneControllers	 List of Bone Controllers to apply
	 * @param 	OutMeshPose	Outpose in Mesh Space once applied
	 */
	void ApplyBoneControllers(USkeletalMeshComponent* Component, TArray<FAnimNode_ModifyBone> &BoneControllers, FA2CSPose& OutMeshPose);
	/** 
	 * Update CurveControllers based on TransformCurves of Animation
	 */
	void UpdateCurveController();

	/* 
	 * Set Key Implementation function
	 * It gets Pre Controller Local Space and gets Post Controller Local Space, and add the key to the curve 
	 */
	void SetKeyImplementation(const TArray<FTransform>& PreControllerInLocalSpace, const TArray<FTransform>& PostControllerInLocalSpace);
	/** 
	 * Add Key to the Sequence
	 * Now Additive Key is generated, add to the curves
	 */
	void AddKeyToSequence(UAnimSequence* Sequence, float Time, const FName& BoneName, const FTransform& AdditiveTransform);
	/* 
	 * When this flag is true, it sets key
	 */
	bool bSetKey;

	/*
	 * Used to determine if controller has to be applied or not
	 * Used to disable controller during editing
	 */
	bool bEnableControllers;
	
	/**
	 * Delegate to call after Key is set
	 */
	FSimpleDelegate OnSetKeyCompleteDelegate;
};



