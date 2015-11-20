// Fill out your copyright notice in the Description page of Project Settings.

#include "LeapMotionControllerPrivatePCH.h"
#include "LeapMotionControllerActor.h"

#include "LeapMotionControllerComponent.h"
#include "LeapMotionControllerPlugin.h"
#include "LeapMotionDevice.h"
#include "LeapMotionFunctionLibrary.h"
#include "LeapMotionHandActor.h"
#include "LeapMotionImageComponent.h"
#include "LeapMotionTypes.h"


ALeapMotionControllerActor::ALeapMotionControllerActor(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Create our battery collection volume
	LeapMotionControllerComponent = ObjectInitializer.CreateDefaultSubobject<ULeapMotionControllerComponent>(this, TEXT("RootLeapMotionControllerComponent"));
	RootComponent = LeapMotionControllerComponent;

	LeapMotionImageComponent = ObjectInitializer.CreateDefaultSubobject<ULeapMotionImageComponent>(this, TEXT("LeapMotionImageComponent"));
	LeapMotionImageComponent->bIsPaused = true;
	LeapMotionImageComponent->bIsVisible = false;
	LeapMotionImageComponent->DisplaySurfaceComponent->SetVisibility(false);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

#if WITH_EDITOR
void ALeapMotionControllerActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	LeapMotionControllerComponent->PostEditChangeProperty(PropertyChangedEvent);
	LeapMotionImageComponent->PostEditChangeProperty(PropertyChangedEvent);
}
#endif
