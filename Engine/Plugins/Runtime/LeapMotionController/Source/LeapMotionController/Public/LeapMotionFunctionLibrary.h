// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h" // needed for access to UBlueprintFunctionLibrary

#include "LeapMotionDevice.h"
#include "LeapMotionTypes.h"
#include "LeapMotionFunctionLibrary.generated.h"


class ULeapMotionControllerComponent;

/**
 * A C++ and Blueprint accessible library of utility functions for accessing Leap Motion device.
 * All positions & orientations are returned in Unreal reference frame & units, assuming the Leap device is located at the origin.
 */
UCLASS()
class LEAPMOTIONCONTROLLER_API ULeapMotionFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Checks whether a Leap Motion controller is connected.
	 * @returns			True if controller is connected, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool IsConnected();

	/**
	 * Returns list of IDs of all hands tracked by the device.
	 * @param OutAllHandIds		Output array which is filled with all the tracked hand IDs.
	 * @returns					True, if the device is connected.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetAllHandsIds(TArray<int32>& OutAllHandIds);

	/** 
	 * Returns the oldest left- or right-hand actor, if one exists, nullptr otherwise. 
	 *
	 * If more than one left or right hand is being tracked, this function returns 
	 * the one that has been tracked the longest.
	 * @param LeapSide	Look for a left or a right hand.
	 * @param OutHandId	An integer set to the Leap Motion id of the oldest left
	 *					or right hand. If no hand is found, this is set to -1.
	 * @returns			True if a hand of the specified type exists, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetOldestLeftOrRightHandId(ELeapSide LeapSide, int32& OutHandId);

	/**
	 * Gets the position and orientation of the specified bone of the specified hand.
	 * Data returned by this function is relative to the Unreal Engine origin rather than
	 * a particular LeapMotionControllerComponent or LeapMotionHandActor instance.
	 * Get the properties of a LeapMotionBoneActor instance to get the data
	 * relative to that actor's parent hand actor and controller component. In this
	 * case, use the standard Unreal Actor location and rotation properties.
	 *
	 * @param HandId			The id of the hand of interest.
	 * @param LeapBone			The bone of interest.
	 * @param OutPosition		An FVector set to the relative position of the hand, 
	 *							or a zero vector if a hand with the specified id does not exist.
	 * @param OutOrientation	An FRotator set to the relative orientation of 
	 *							the specified bone, or a zero rotation if a hand 
	 *							with the specified id does not exist.
	 * @returns					True, if the hand with the specified id exists, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetBonePostionAndOrientation(int32 HandId, ELeapBone LeapBone, FVector& OutPosition, FRotator& OutOrientation);

	/**
	 * Gets the width and length of the specified bone of the specified hand.
	 * @param HandId			The id of the hand of interest.
	 * @param LeapBone			The bone of interest.
	 * @param OutWidth			A float set to the bone's width (in centimeters).
	 * @param OutLength			A float set to the bone's length (in centimeters).
	 * @returns					True, if the hand with the specified id exists, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetBoneWidthAndLength(int32 HandId, ELeapBone LeapBone, float& OutWidth, float& OutLength);

	/**
	 * The grab strength rating of the specified hand.
	 *
	 * Grab strength is a rating of how the hand's posture resembles a fist.
	 * A strength of 0 is close to an open, flat hand; a strength of 1 is close 
	 * to a fist.
	 * @param HandId			The id of the hand of interest.
	 * @param GrabStrength		A float set to the hand's grabbing strength as reported by Leap API.
	 * @returns					True, if the hand with the specified id exists, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetGrabStrength(int32 HandId, float& GrabStrength);

	/**
	 * The pinch strength rating of the specified hand.
	 *
	 * Pinch strength is a rating of whether the hand is in a pinching posture.
	 * Pinch strength starts at 0 and increases to 1 as any finger tip approaches
	 * the tip of the thumb. Note that pinch and grab strength are not independent.
	 * A grabbing hand will generally have a non-zero pinch strength as well.
	 *
	 * @param HandId			The id of the hand of interest.
	 * @param PinchStrength		A float set to the hand's pinch strength as reported by Leap API
	 * @returns					True, if the hand with the specified id exists, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetPinchStrength(int32 HandId, float& PinchStrength);

	/**
	 * Enables or disables the Leap Motion Controller policy that improves tracking for an
	 * HMD-mounted controller.
	 * Note that calling this function does not change the transforms of any 
	 * LeapMotionControllerComponent instances that may exist.
	 *
	 * @param UseHmdPolicy		True to enable for HMD-mounted mode, false to disable.
	 * @returns					True if the device is connected.
	 */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	static bool SetHmdPolicy(bool UseHmdPolicy);

	/**
	 * Enables or disables the Leap Motion Controller policy that transmits image data.
	 * Note that calling this function does not change the transforms of any 
	 * LeapMotionControllerComponent instances that may exist.
	 *
	 * @param UseHmdPolicy		True to enable for image pass-through, false to disable.
	 * @returns					True if the device is connected.
	 */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	static bool SetImagePolicy(bool UseImagePolicy);
};

