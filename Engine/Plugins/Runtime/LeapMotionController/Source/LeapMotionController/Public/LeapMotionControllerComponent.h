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

	/** The scale of hands, relative to real-world size. This ignores actor's transform's scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	float Scale;

	/** The scale of hands, when in HMD mode, relative to real-world size. This ignores actor's transform's scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	float ScaleForHmdMode;

	/** Whether to render the hand's colliding shape in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowCollider;

	/** Whether to render the hand's mesh in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowMesh;

	/** Whether to render the hand's arm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowArm;

	/** 
	 * Whether to transform the tracking data as appropriate for a Leap Motion device
	 * attached to an HMD. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bHmdMode;

	/** 
	 * Whether to automatically attach this controller to player camera, both in desktop & VR modes. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bAutoAttachToPlayerCamera;

	/** Placement of Leap Device in relation to HMD position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	FVector OffsetFromHMDToLeapDevice;

	/** Optional blueprint used to spawn each hand. When none is specified ALeapMotionHandActor is created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	TSubclassOf<class ALeapMotionHandActor> HandBlueprint;

	/** Optional blueprint used to spawn each individual finger bone. When none is specified ALeapMotionHandBoneActor is created */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	TSubclassOf<class ALeapMotionBoneActor> BoneBlueprint;

	/** 
	 * Gets all active hands ids.
	 * @param OutHandIds Output array which is filled with the ids of existing hands.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	void GetAllHandIds(TArray<int32>& OutHandIds) const;

	/** 
	 * Gets all active hands actors.
	 * @param OutHandActors	Output array which is filled with existing hand actors.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	void GetAllHandActors(TArray<ALeapMotionHandActor*>& OutHandActors) const;

	/** 
	 * Gets the hand actor instance for the specified hand ID.
	 * @param HandId The Leap Motion ID for the hand of interest.
	 * @returns	A pointer to the Actor object if the hand ID is active, nullptr otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ALeapMotionHandActor* GetHandActor(int32 HandId) const;

	/**
	 * Returns the oldest left- or right-hand actor, if one exists, nullptr otherwise. 
	 *
	 * If more than one left or right hand is being tracked, this function returns 
	 * the one that has been tracked the longest.
	 * @param LeapSide	Are we looking for left or right-sided hands?
	 * @return			Pointer to oldest hand actor if one is found, nullptr otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ALeapMotionHandActor* GetOldestLeftOrRightHandActor(ELeapSide LeapSide) const;

	/** Delegate triggered when a new hand is spotted by the Leap Motion device. */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandAddedDelegate OnHandAdded;

	/** Delegate triggered when a hand lost by the Leap Motion device. */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandRemovedDelegate OnHandRemoved;

	/** Delegate triggered once per frame update for each hand. */
	UPROPERTY(BlueprintAssignable, Category = LeapMotion)
	FLMHandUpdatedDelegate OnHandUpdated;

	/** Sets the controller as when attached to an HMD, or in the standard desktop mode 
	 * Puts the Leap Motion controller and this component into (or out of) HMD mode.
	 * This sets the Leap Motion software to use the Optimize for HMD policy.
	 * It also changes the transform of Leap Motion to Unreal coordinates so that the
	 * hands are projected in front of the component position rather than above it.
	 *
	 * @param EnableOrDisable Set to true to enable HMD mode.
	 */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	void UseHmdMode(bool EnableOrDisable);

	/**
	 * Updates all hand actors of this component.
	 * @param DeltaTime - The time since the last tick.
	 * @param TickType - The kind of tick this is.
	 * @param ThisTickFunction - Tick function that caused this to run.
	 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction);

	/** Finger model mesh used by the default hand implementation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UStaticMesh* FingerMesh;

	/** Palm model mesh used by the default hand implementation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UStaticMesh* PalmMesh;

	/** Arm model mesh used by the default hand implementation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UStaticMesh* ArmMesh;

	/** Material used by the default hand implementation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UMaterialInterface* Material;

	/** Called after the C++ constructor and after the properties have been initialized */
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	/** Track changes to property applied in the editor. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/** Creates and destroys hand actors and triggers the corresponding delegates. */
	void AddAndRemoveHands();

	/** Updates the hands and calls the update delegate. */
	void UdpateHandsPositions(float DeltaSeconds);

	/** Attaches the Controller Component to player's camera. Works both for desktop & HMD-mounted use. */
	void AttachControllerToPlayerCamera(int PlayerIndex);

	/** Creates a hand. */
	virtual void OnHandAddedImpl(int32 HandId);
	
	/** Destroys a hand. */
	virtual void OnHandRemovedImpl(int32 HandId);

	/** Updates a hand. */
	virtual void OnHandUpdatedImpl(int32 HandId, float DeltaSeconds);

	/** List of Leap Motion hand ids in the last frame. */
	TArray<int32> LastFrameHandIds;

	/** All actors for active hand ids. */
	TMap<int32, class ALeapMotionHandActor*> HandActors;
};
