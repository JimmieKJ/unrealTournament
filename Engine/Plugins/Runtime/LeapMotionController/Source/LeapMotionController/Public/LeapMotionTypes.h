
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Leap_NoPI.h"
#include "LeapMotionTypes.generated.h"

//---------------------------------------------------
// Common enumerations
//---------------------------------------------------

UENUM(BlueprintType)
enum class ELeapSide : uint8
{
	Left,
	Right
};

/** Enum to define a hand's bone. This includes finger bones, palm, and arm. */
UENUM(BlueprintType)
enum class ELeapBone : uint8
{
	Forearm,
	Palm,
	ThumbBase,
	ThumbMiddle,
	ThumbTip,
	Finger1Base,
	Finger1Middle,
	Finger1Tip,
	Finger2Base,
	Finger2Middle,
	Finger2Tip,
	Finger3Base,
	Finger3Middle,
	Finger3Tip,
	Finger4Base,
	Finger4Middle,
	Finger4Tip
};


//---------------------------------------------------
// Delegate types
//---------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLMHandAddedDelegate, int32, HandId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLMHandRemovedDelegate, int32, HandId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLMHandUpdatedDelegate, int32, HandId, float, DeltaSeconds);


//---------------------------------------------------
// Helper Functions
//---------------------------------------------------

/**
* Helper function to convert from Leap space (in mm) to Unreal space (in cm)
*/
static FORCEINLINE FVector LEAPVECTOR_TO_FVECTOR(Leap::Vector InVector)
{
	return FVector(-InVector.z, InVector.x, InVector.y) * 0.1f;
}

/**
* Helper function to convert direction of the vector only
*/
static FORCEINLINE FVector LEAPVECTOR_TO_FVECTOR_DIRECTION(Leap::Vector InVector)
{
	return FVector(-InVector.z, InVector.x, InVector.y);
}

#define LEAP_BONE_NAME_CASE(LeapBone) case ELeapBone::LeapBone: return #LeapBone

static const char* LEAP_GET_BONE_NAME(ELeapBone LeapBone)
{
	switch (LeapBone)
	{
	LEAP_BONE_NAME_CASE(Forearm);
	LEAP_BONE_NAME_CASE(Palm);
	LEAP_BONE_NAME_CASE(ThumbBase);
	LEAP_BONE_NAME_CASE(ThumbMiddle);
	LEAP_BONE_NAME_CASE(ThumbTip);
	LEAP_BONE_NAME_CASE(Finger1Base);
	LEAP_BONE_NAME_CASE(Finger1Middle);
	LEAP_BONE_NAME_CASE(Finger1Tip);
	LEAP_BONE_NAME_CASE(Finger2Base);
	LEAP_BONE_NAME_CASE(Finger2Middle);
	LEAP_BONE_NAME_CASE(Finger2Tip);
	LEAP_BONE_NAME_CASE(Finger3Base);
	LEAP_BONE_NAME_CASE(Finger3Middle);
	LEAP_BONE_NAME_CASE(Finger3Tip);
	LEAP_BONE_NAME_CASE(Finger4Base);
	LEAP_BONE_NAME_CASE(Finger4Middle);
	LEAP_BONE_NAME_CASE(Finger4Tip);
	default: return "None";
	}
}

#undef LEAP_BONE_NAME_CASE

//---------------------------------------------------
// Dummy class to get this file compiling, when delegates are declared above
//---------------------------------------------------

UCLASS()
class ULeapMotionTypes : public UObject
{
	GENERATED_BODY()
};
