// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "LeapMotionTypes.h"
#include "LeapMotionBoneActor.generated.h"

class ALeapMotionHandActor;
class ULeapMotionControllerComponent;

/**
 * Represents a single part of a hand tracked by the Leap Motion Controller.
 *
 * Bone Actors are created automatically by a LeapMotionHandActor instance. You can
 * extended this class to replace or augment the default behavior.
 *
 * Bone actors contain the position and orientation information. These attributes 
 * are relative to the parent LeapMotionHandActor, but note that by default a
 * Hand actor has the same position and orientation as its parent
 * LeapMotionControllerComponent object.
 */
UCLASS(ClassGroup = LeapMotion, BlueprintType, Blueprintable)
class LEAPMOTIONCONTROLLER_API ALeapMotionBoneActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** The name of this bone. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	ELeapBone LeapBoneType;

	/** Whether to visualize hand bone's colliding shape in game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowCollider;

	/** Whether to render the hand bone's mesh in game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowMesh;

	/** The Unreal Primitive component used for collisions with the scene objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	class UPrimitiveComponent* PrimitiveComponent;

	/** The Static mesh for rendering the hand bone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	class UStaticMeshComponent* StaticMeshComponent;

	/** 
	 * Gets the controller component that created this bone and its parenting hand.
	 * @returns The parent LeapMotionControllerComponent.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ULeapMotionControllerComponent* GetParentingControllerComponent() const;

	/** 
	 * Gets the hand actor this bone belongs to. 
	 * @returns the parent LeapMotionHandActor instance.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ALeapMotionHandActor* GetHandActor() const;

	/** 
	 * Called to initialize the size and display properties of a bone based on 
	 * the current hand settings.
	 *
	 * Subclasses of LeapMotionBoneActor can implement this function to override 
	 * the default behavior.
	 *
	 * @param LeapBone The name of the current bone.
	 * @param Scale The relative scale for this bone.
	 * @param Width The width of this bone.
	 * @param Length The length of this bone.
	 * @param ShowCollider Whether to render the colliders.
	 * @param ShowMesh Whether to render the mesh for this bone.
	 */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void Init(ELeapBone LeapBone, float Scale, float Width, float Length, bool ShowCollider, bool ShowMesh);

	/** Updates visibility of hand bone's mesh and colliding shape. */
	void UpdateVisibility();

#if WITH_EDITOR
	/** Track changes to properties applied in the editor. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/** Uses the default finger mesh. */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void SetMeshForFinger();

	/** Uses the default palm mesh. */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void SetMeshForPalm();

	/** Uses the default arm mesh. */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void SetMeshForArm();

};
