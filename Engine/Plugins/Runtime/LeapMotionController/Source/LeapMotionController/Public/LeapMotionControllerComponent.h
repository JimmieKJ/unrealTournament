// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneComponent.h"

#include "LeapMotionTypes.h"
#include "LeapMotionControllerComponent.generated.h"


class ALeapMotionHandActor;

/**
 *  Add this component to an actor and place the actor in the world to automatically create hand actors
 *  as tracked by Leap Motion controller. Move the location of the controller, or attach it to another actor, 
 *  and the hands will follow the controller's transform. Set the scale parameter, rather then transforms's scale,
 *  to affect the scale of the created hand objects.
 */
UCLASS(ClassGroup = LeapMotion, BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class LEAPMOTIONCONTROLLER_API ULeapMotionControllerComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** Scale of hands, relatively to real-world size. This ignores actor's transform's scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	float Scale;

	/** Visualize hand's colliding shape in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowCollider;

	/** Visualize hand's mesh in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowMesh;

	/** Show hand's arm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowArm;

	/** Run the controller as when attached to an HMD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool IsInHmdMode;

	/** Optional blueprint used to spawn each hand. When none is specified ALeapMotionHandActor is created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	TSubclassOf<class ALeapMotionHandActor> HandBlueprint;

	/** Optional blueprint used to spawn each individual finger bone. When none is specified ALeapMotionHandBoneActor is created */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	TSubclassOf<class ALeapMotionBoneActor> BoneBlueprint;

	/** 
	 * Gets all active hands ids 
	 * @param OutHandIds	Array of ids of existing hand actors.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	void GetAllHandIds(TArray<int32>& OutHandIds) const;

	/** 
	 * Gets all active hands actors 
	 * @param OutHandActors		Array of existing hand actors.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	void GetAllHandActors(TArray<ALeapMotionHandActor*>& OutHandActors) const;

	/** 
	 * Returns hand actor 
	 * @param HandId	Which hand to query
	 * @return			Actor pointer if hand is found, nullptr otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ALeapMotionHandActor* GetHandActor(int32 HandId) const;

	/**
	 * Returns the oldest left- or right-hand actor, if one exists, nullptr otherwise. 
	 * @param LeapSide	Are we looking for left or right-sided hands?
	 * @return			Pointer to oldest hand actor if one is found, nullptr otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ALeapMotionHandActor* GetOldestLeftOrRightHandActor(ELeapSide LeapSide) const;

	/** Delegate triggered when a new hand is spotted by the Leap Motion device */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandAddedDelegate OnHandAdded;

	/** Delegate triggered when a hand lost by the Leap Motion device */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandRemovedDelegate OnHandRemoved;

	/** Delegate triggered once per frame update for each hand */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandUpdatedDelegate OnHandUpdated;

	/** Sets the controller as when attached to an HMD, or in the standard desktop mode */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	void UseHmdMode(bool EnableOrDisable);

	/**
	 * Updates all hands. 
	 * @param DeltaTime - The time since the last tick.
	 * @param TickType - The kind of tick this is
	 * @param ThisTickFunction - Tick function that caused this to run
	 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction);

	/** Finger model mesh used by the default hand implementation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UStaticMesh* FingerMesh;

	/** Palm model mesh used by the default hand implementation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UStaticMesh* PalmMesh;

	/** Arm model mesh used by the default hand implementation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UStaticMesh* ArmMesh;

	/** Material used by the default hand implementation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UMaterialInterface* Material;

#if WITH_EDITOR
	/** Track changes to property applied in the editor */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/** Created & destroys hand actors & triggers corresponding delegates */
	void AddAndRemoveHands();

	/** Updates the hands & calls the update delegate */
	void UdpateHandsPositions(float DeltaSeconds);

	/** Creates a hand */
	virtual void OnHandAddedImpl(int32 HandId);
	
	/** Destroys a hand */
	virtual void OnHandRemovedImpl(int32 HandId);

	/** Updates a hand */
	virtual void OnHandUpdatedImpl(int32 HandId, float DeltaSeconds);

	/** List of ids in the last frame */
	TArray<int32> LastFrameHandIds;

	/** All actors for active hand ids */
	TMap<int32, class ALeapMotionHandActor*> HandActors;
};
