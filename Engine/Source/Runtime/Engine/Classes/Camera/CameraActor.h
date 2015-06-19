// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Scene.h"

#include "CameraActor.generated.h"

/** 
 * A CameraActor is a camera viewpoint that can be placed in a level.
 */
UCLASS(ClassGroup=Common, hideCategories=(Input, Rendering), showcategories=("Input|MouseInput", "Input|TouchInput"), Blueprintable)
class ENGINE_API ACameraActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:

	/** Specifies which player controller, if any, should automatically use this Camera when the controller is active. */
	UPROPERTY(Category="AutoPlayerActivation", EditAnywhere)
	TEnumAsByte<EAutoReceiveInput::Type> AutoActivateForPlayer;

private_subobject:

	/** The camera component for this camera */
	DEPRECATED_FORGAME(4.6, "CameraComponent should not be accessed directly, please use GetCameraComponent() function instead. CameraComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = CameraActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* CameraComponent;

public:

	/** If this CameraActor is being used to preview a CameraAnim in the editor, this is the anim being previewed. */
	TWeakObjectPtr<class UCameraAnim> PreviewedCameraAnim;

	/** Returns index of the player for whom we auto-activate, or INDEX_NONE (-1) if disabled. */
	UFUNCTION(BlueprintCallable, Category="AutoPlayerActivation")
	int32 GetAutoActivatePlayerIndex() const;

private:

	UPROPERTY()
	uint32 bConstrainAspectRatio_DEPRECATED:1;

	UPROPERTY()
	float AspectRatio_DEPRECATED;

	UPROPERTY()
	float FOVAngle_DEPRECATED;

	UPROPERTY()
	float PostProcessBlendWeight_DEPRECATED;

	UPROPERTY()
	struct FPostProcessSettings PostProcessSettings_DEPRECATED;

public:
	// Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End UObject interface
	
	// Begin AActor interface
	virtual void BeginPlay() override;
	// End AActor interface

	/** Returns CameraComponent subobject **/
	class UCameraComponent* GetCameraComponent() const;
};
