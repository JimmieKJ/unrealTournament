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
	 * @return			True if controller is connected, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool IsConnected();

	/**
	 * Returns list of IDs of all hands seen by the device
	 * @param OutAllHandIds		Array of all visible hand IDs.
	 * @param UseReferenceFrame	Use saved reference frame, rather then the volatile newest one from the device.
	 * @return					True if the device is connected 
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetAllHandsIds(TArray<int32>& OutAllHandIds);

	/** 
	 * Returns the id of the oldest left or right hand, if one exists. 
	 * @param LeapSide	Query for left or right hands
	 * @param OutHandId	The Id of the oldest left/right hand, or -1 otherwise.
	 * @return			True if hand is found, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetOldestLeftOrRightHandId(ELeapSide LeapSide, int32& OutHandId);

	/**
	 * Gets the position and orientation of the specified hand from the device
	 * @param HandId			Which hand to query
	 * @param LeapBone			Which hand bone to get position & orientation for
	 * @param OutPosition		(out) The relative position of the hand, or zero if the hand wasn't detected this frame
	 * @param OutOrientation	(out) The relative orientation of the hand, or zero if the hand wasn't detected this frame
	 * @param UseReferenceFrame	Use saved reference frame, rather then the volatile newest one from the device. 
	 * @return					True if the hand was detected, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetBonePostionAndOrientation(int32 HandId, ELeapBone LeapBone, FVector& OutPosition, FRotator& OutOrientation);

	/**
	 * Gets the width and length of the specified bone of a hand from the device
	 * @param HandId			Which hand to query
	 * @param LeapBone			Which hand bone to get position & orientation for
	 * @param OutWidth			Bone's width (in centimeters)
	 * @param OutWidth			Bone's length (in centimeters)
	 * @param UseReferenceFrame	Use saved reference frame, rather then the volatile newest one from the device.
	 * @return					True if the hand was detected and bone was identified, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetBoneWidthAndLength(int32 HandId, ELeapBone LeapBone, float& OutWidth, float& OutLength);

	/**
	 * Queries grabbing strength of the hand
	 * @param HandId			Which hand to query
	 * @param OutGrabStrength	Hand's grabbing strength as reported by Leap API
	 * @param UseReferenceFrame	Use saved reference frame, rather then the volatile newest one from the device.
	 * @return					True if the hand was detected, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetGrabStrength(int32 HandId, float& GrabStrength);

	/**
	 * Queries pinching strength of the hand
	 * @param HandId			Which hand to query
	 * @param OutPinchStrength	Hand's pinch strength as reported by Leap API
	 * @param UseReferenceFrame	Use saved reference frame, rather then the volatile newest one from the device.
	 * @return					True if the hand was detected, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool GetPinchStrength(int32 HandId, float& PinchStrength);

	/**
	* Set LeapController policy that improves tracking for HMD-mounted controller
	* @param UseHmdPolicy		True for HMD-mounted mode, false otherwise
	* @return					True if the device is connected
	*/
	UFUNCTION(BlueprintPure, Category = LeapMotion)
	static bool SetHmdPolicy(bool UseHmdPolicy);
};

