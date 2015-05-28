
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Leap_NoPI.h"
#include "LeapMotionTypes.generated.h"



//---------------------------------------------------
// Common enumerations
//---------------------------------------------------

/** 
* \defgroup Enumerations Enumerations
*
* Leap Motion enumerations.
* @{
*/

/**
* The names for designating the hands.
*/
UENUM(BlueprintType)
enum class ELeapSide : uint8
{
	/** Port */
	Left,
	/** Starboard */
	Right
};

/** Enum to define a hand's bone. This includes finger bones, palm, and arm. */
UENUM(BlueprintType)
enum class ELeapBone : uint8
{
	/** The arm bone from elbow to wrist. */
	Forearm,
	/** The central portion of the hand, excluding fingers. */
	Palm,
	/** The anatomical thumb metacarpal. */
	ThumbBase,
	/** The anatomical thumb proximal phalanx. */
	ThumbMiddle,
	/** The anatomical thumb distal phalanx. */
	ThumbTip,
	/** The anatomical index finger proximal phalanx. */
	Finger1Base,
	/** The anatomical index finger intermediate phalanx. */
	Finger1Middle,
	/** The anatomical index finger distal phalanx. */
	Finger1Tip,
	/** The anatomical middle finger proximal phalanx. */
	Finger2Base,
	/** The anatomical middle finger intermediate phalanx. */
	Finger2Middle,
	/** The anatomical middle finger distal phalanx. */
	Finger2Tip,
	/** The anatomical ring finger proximal phalanx. */
	Finger3Base,
	/** The anatomical ring finger intermediate phalanx. */
	Finger3Middle,
	/** The anatomical ring finger distal phalanx. */
	Finger3Tip,
	/** The anatomical pinky finger proximal phalanx. */
	Finger4Base,
	/** The anatomical pink finger intermediate phalanx. */
	Finger4Middle,
	/** The anatomical pinky finger distal phalanx. */
	Finger4Tip
};
/**@}*/ //end doxygen group

//---------------------------------------------------
// Delegate types
//---------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLMHandAddedDelegate, int32, HandId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLMHandRemovedDelegate, int32, HandId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLMHandUpdatedDelegate, int32, HandId, float, DeltaSeconds);


/** ---------------------------------------------------
* \defgroup global Utility Functions
* Helper Functions
* @{
*/ //---------------------------------------------------

/**
* Helper function to convert from Leap space to Unreal space (right-handed to left-handed)
* and units (mm to cm).
*/
static FORCEINLINE FVector LEAPVECTOR_TO_FVECTOR(Leap::Vector InVector)
{
	return FVector(-InVector.z, InVector.x, InVector.y) * 0.1f;
}

/**
* Helper function to convert direction of the vector only.
*/
static FORCEINLINE FVector LEAPVECTOR_TO_FVECTOR_DIRECTION(Leap::Vector InVector)
{
	return FVector(-InVector.z, InVector.x, InVector.y);
}
/**@}*/ // End doxygen global group

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
