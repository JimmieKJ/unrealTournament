// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

#pragma once
#include "AnimInstance.h"
#include "AnimSingleNodeInstance.generated.h"

DECLARE_DYNAMIC_DELEGATE(FPostEvaluateAnimEvent);

UCLASS(transient, NotBlueprintable)
class ENGINE_API UAnimSingleNodeInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

	/** Current Asset being played **/
	UPROPERTY(transient)
	class UAnimationAsset* CurrentAsset;

	UPROPERTY(transient)
	class UVertexAnimation* CurrentVertexAnim;

	/** Random cached values to play each asset **/
	UPROPERTY(transient)
	FVector BlendSpaceInput;

	/** Random cached values to play each asset **/
	UPROPERTY(transient)
	TArray<FBlendSampleData> BlendSampleData;

	/** Random cached values to play each asset **/
	UPROPERTY(transient)
	FBlendFilter BlendFilter;

	/** Shared parameters for previewing blendspace or animsequence **/
	UPROPERTY(transient)
	float CurrentTime;

	/** Cache for data needed during marker sync */
	FMarkerTickRecord MarkerTickRecord;

	UPROPERTY(transient)
	float PlayRate;
	 
	UPROPERTY(Transient)
	FPostEvaluateAnimEvent PostEvaluateAnimEvent;

	UPROPERTY(transient)
	uint32 bLooping:1;

	UPROPERTY(transient)
	uint32 bPlaying:1;

	UPROPERTY(transient)
	uint32 bReverse:1;

	//~ Begin UAnimInstance Interface
	virtual void NativeInitializeAnimation() override;
	virtual bool NativeEvaluateAnimation(FPoseContext& Output) override;
	virtual void NativePostEvaluateAnimation() override;
	virtual void OnMontageInstanceStopped(FAnimMontageInstance& StoppedMontageInstance) override;

protected:
	virtual void Montage_Advance(float DeltaTime) override;
	void InternalBlendSpaceEvaluatePose(class UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, FPoseContext& OutContext);
	//~ End UAnimInstance Interface
public:

	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetLooping(bool bIsLooping);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetPlayRate(float InPlayRate);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetReverse(bool bInReverse);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetPosition(float InPosition, bool bFireNotifies=true);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetBlendSpaceInput(const FVector& InBlendInput);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetPlaying(bool bIsPlaying);
	UFUNCTION(BlueprintCallable, Category="Animation")
	float GetLength();
	/* For AnimSequence specific **/
	UFUNCTION(BlueprintCallable, Category="Animation")
	void PlayAnim(bool bIsLooping=false, float InPlayRate=1.f, float InStartPosition=0.f);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void StopAnim();
	/** Set New Asset - calls InitializeAnimation, for now we need MeshComponent **/
	UFUNCTION(BlueprintCallable, Category="Animation")
	virtual void SetAnimationAsset(UAnimationAsset* NewAsset, bool bIsLooping=true, float InPlayRate=1.f);
	/** Set new vertex animation */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetVertexAnimation(UVertexAnimation* NewVertexAnim, bool bIsLooping=true, float InPlayRate=1.f);

public:
	/** AnimSequence specific **/
	void StepForward();
	void StepBackward();

	/** custom evaluate pose **/
	virtual void RestartMontage(UAnimMontage * Montage, FName FromSection = FName());
	void SetMontageLoop(UAnimMontage* Montage, bool bIsLooping, FName StartingSection = FName());

	/** Updates montage weights based on a jump in time (as this wont be handled by SetPosition) */
	void UpdateMontageWeightForTimeSkip(float TimeDifference);

	/** Updates the blendspace samples list in the case of our asset being a blendspace */
	void UpdateBlendspaceSamples(FVector InBlendInput);

#if WITH_EDITORONLY_DATA
	float PreviewPoseCurrentTime;
#endif
private:
	/** Slot node weight transient data */
	FSlotNodeWeightInfo WeightInfo;

protected:
	// UAnimInstance interface to override animation node part
	virtual void UpdateAnimationNode(float DeltaSeconds) override;
};



