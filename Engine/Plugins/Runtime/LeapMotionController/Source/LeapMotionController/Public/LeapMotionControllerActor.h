// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "LeapMotionTypes.h"
#include "LeapMotionControllerActor.generated.h"


/**
 *  Place this actor in the world to automatically create hand actors as tracked by Leap Motion 
 *  controller. You can also attach this Controller to a character or a camera.
 */
UCLASS(ClassGroup = LeapMotion, BlueprintType, Blueprintable)
class LEAPMOTIONCONTROLLER_API ALeapMotionControllerActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Scale of hands, relatively to real-world size. This ignores actor's transform's scale */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	class ULeapMotionControllerComponent* LeapMotionControllerComponent;

#if WITH_EDITOR
	/** Track changes to property applied in the editor */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
