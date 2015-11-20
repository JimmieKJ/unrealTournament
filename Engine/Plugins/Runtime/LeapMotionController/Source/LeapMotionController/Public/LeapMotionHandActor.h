// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "LeapMotionTypes.h"
#include "LeapMotionHandActor.generated.h"

class ALeapMotionBoneActor;
class ULeapMotionControllerComponent;

/**
 *  This actor is created dynamically by LeapMotionControllerComponent. It's location is same as the controller, 
 *  however you can offset it to control hands' positions independently. To query hand's palm location in the Unreal scene, 
 *  access this actor's GetHandBoneActor(ELeapBone::Palm).
 */
UCLASS(ClassGroup = LeapMotion, BlueprintType, Blueprintable)
class LEAPMOTIONCONTROLLER_API ALeapMotionHandActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Scale of the hand, relatively to real-world size */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	float Scale; 

	/** Visualize hand's colliding shape in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowCollider;

	/** Visualize hand's mesh in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowMesh;

	/** Show hand's arm. Don't change it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	bool bShowArm;

	/** Hand's id, as reported by Leap API. Don't change it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	int32 HandId;

	/** Is this a left or a right hand? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	ELeapSide HandSide;

	/** Hand's pinching strength, as reported by Leap API */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	float PinchStrength;

	/** Hand's grabbing strength, as reported by Leap API */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	float GrabStrength;

	/** Returns the parenting controller component that created this hand. This is based purely on actor/component attachment hierarchy */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ULeapMotionControllerComponent* GetParentingControllerComponent() const;

	/** Retrieves individual hand bone */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ALeapMotionBoneActor* GetBoneActor(ELeapBone LeapBone) const;

	/** Delegate triggered upon hand's creation */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandAddedDelegate OnHandAdded;

	/** Delegate triggered upon hand's destruction */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandRemovedDelegate OnHandRemoved;

	/** Delegate triggered once per frame update */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandUpdatedDelegate OnHandUpdated;

	/** Initializes hand, creates bone actors & triggers HandAdded delegate */
	virtual void Init(int32 HandIdParam, const TSubclassOf<class ALeapMotionBoneActor>& BoneBlueprint);

	/** Updates hand's motion  from Leap API & triggers HandUpdated delegate */
	virtual void Update(float DeltaSeconds);

	/** Updates visibility of hand's bones' mesh & colliding shape */
	void UpdateVisibility();

#if WITH_EDITOR
	/** Track changes to property applied in the editor */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/** Removes bone actors & triggers HadnRemoved delegate */
	virtual void Destroyed();

	/** Spawns actors for finger, palm, and arm bones, using BoneBlueprint, and sets their initial position */
	void CreateBones(const TSubclassOf<class ALeapMotionBoneActor>& BoneBlueprint);

	/** Removes finger bone actors */
	void DestroyBones();

	/** Drives velocities of the finger bones, to move them to current Leap Motion hand positions */
	void UpdateBones(float DeltaSeconds);

	/** Multiplies Scale parameter with scale from WorldSettings */
	float GetCombinedScale();

	/** Child finger-bone actors */
	TArray<ALeapMotionBoneActor*> BoneActors;
};
