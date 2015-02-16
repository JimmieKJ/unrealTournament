// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "LeapMotionTypes.h"
#include "LeapMotionBoneActor.generated.h"

class ALeapMotionHandActor;
class ULeapMotionControllerComponent;

/**
 * This actor is dynamically created by LeapMotionHandActor. It represents a single hand bone tracked by Leap Motion Controller.
 */
UCLASS(ClassGroup = LeapMotion, BlueprintType, Blueprintable)
class LEAPMOTIONCONTROLLER_API ALeapMotionBoneActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	ELeapBone LeapBoneType;

	/** Visualize hand bone's colliding shape in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowCollider;

	/** Visualize hand bone's mesh in game */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bShowMesh;

	/** Primitive component used for collisions with the scene objects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	class UPrimitiveComponent* PrimitiveComponent;

	/** Static mesh for visualizing the hand bone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	class UStaticMeshComponent* StaticMeshComponent;

	/** Gets the controller component that created this bone & it's parenting hand. */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ULeapMotionControllerComponent* GetParentingControllerComponent() const;

	/** Gets the hand actor this bone belongs to. */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	ALeapMotionHandActor* GetHandActor() const;

	/** Sizes the mesh & colliding shap eof this hand bone */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void Init(ELeapBone LeapBone, float Scale, float Width, float Length, bool ShowCollider, bool ShowMesh);

	/** Updates visibility of hand bone's mesh & colliding shape */
	void UpdateVisibility();

#if WITH_EDITOR
	/** Track changes to property applied in the editor */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/** Uses the default finger mesh */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void SetMeshForFinger();

	/** Uses the default palm mesh */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void SetMeshForPalm();

	/** Uses the default arm mesh */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	virtual void SetMeshForArm();

};
