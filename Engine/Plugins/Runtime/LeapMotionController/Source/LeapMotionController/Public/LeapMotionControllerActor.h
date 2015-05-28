// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "LeapMotionTypes.h"
#include "LeapMotionControllerActor.generated.h"


/**
 * An Unreal Actor instance that allows you to drop a set of 3D hands anywhere in a level.
 *
 * Contains a LeapMotionControllerComponent instance which does all of the work.
 */
UCLASS(ClassGroup = LeapMotion, BlueprintType, Blueprintable)
class LEAPMOTIONCONTROLLER_API ALeapMotionControllerActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** An Unreal Component instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	class ULeapMotionControllerComponent* LeapMotionControllerComponent;

#if WITH_EDITOR
	/** Track changes to property applied in the editor */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
