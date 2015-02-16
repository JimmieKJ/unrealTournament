// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Leap_NoPI.h"

#include "LeapMotionTypes.h"


// TODO: hide this from interface
namespace Leap { class Controller; class Frame; }
class LeapHandListener;

/**
 * Implementation of the Leap Motion device. It bridges access to Leap API.
 * You shouldn't use this in your code. Use LeapMotionFunctionLibrary, or LeapMotionControllerActor instead.
 */
class LEAPMOTIONCONTROLLER_API FLeapMotionDevice
{
public:
	FLeapMotionDevice();
	~FLeapMotionDevice();

	/** Startup the device, and do any initialization that may be needed */
	bool StartupDevice();

	/** Tear down the device */
	void ShutdownDevice();

	/**
	 * Checks whether or not the device is connected and ready to be polled for data
	 * @return			True if controller is connected, false otherwise.
	 */
	bool IsConnected() const;

	/**
	 * Returns list of IDs of all hands seen by the device
	 * @param OutAllHandIds		Array of all visible hand IDs.
	 * @return					True if the device is connected 
	 */
	bool GetAllHandsIds(TArray<int32>& OutAllHandIds) const;

	/** 
	 * Returns the id of the oldest left or right hand, if one exists. 
	 * @param LeapSide	Query for left or right hands
	 * @param OutHandId	The Id of the oldest left/right hand, or -1 otherwise.
	 * @return			True if hand is found, false otherwise.
	 */
	bool GetOldestLeftOrRightHandId(ELeapSide LeapSide, int32& OutHandId) const;

	/**
	 * Tells if this is a left or a right hand
	 * @param HandId			Which hand to query
	 * @param OutIsLeft			Tells if this is a left hand
	 * @return					True if the hand was detected , false otherwise
	 */
	bool GetIsHandLeft(int32 HandId, bool& OutIsLeft) const;

	/**
	 * Gets the position and orientation of the specified hand from the device
	 * @param HandId			Which hand to query
	 * @param LeapBone			Which hand bone to get position & orientation for
	 * @param OutPosition		(out) The relative position of the hand, or zero if the hand wasn't detected this frame
	 * @param OutOrientation	(out) The relative orientation of the hand, or zero if the hand wasn't detected this frame
	 * @return					True if the hand was detected, false otherwise
	 */
	bool GetBonePostionAndOrientation(int32 HandId, ELeapBone LeapBone, FVector& OutPosition, FRotator& OutOrientation) const;

	/**
	 * Gets the width and length of the specified bone of a hand from the device
	 * @param HandId			Which hand to query
	 * @param LeapBone			Which hand bone to get position & orientation for
	 * @param OutWidth			Bone's width (in centimeters)
	 * @param OutWidth			Bone's length (in centimeters)
	 * @return					True if the hand was detected and bone was identified, false otherwise
	 */
	bool GetBoneWidthAndLength(int32 HandId, ELeapBone LeapBone, float& OutWidth, float& OutLength) const;

	/**
	 * Queries grabbing strength of the hand
	 * @param HandId			Which hand to query
	 * @param OutGrabStrength	Hand's grabbing strength as reported by Leap API
	 * @return					True if the hand was detected, false otherwise
	 */
	bool GetGrabStrength(int32 HandId, float& OutGrabStrength) const;

	/**
	 * Queries pinching strength of the hand
	 * @param HandId			Which hand to query
	 * @param OutPinchStrength	Hand's pinch strength as reported by Leap API
	 * @return					True if the hand was detected, false otherwise
	 */
	bool GetPinchStrength(int32 HandId, float& OutPinchStrength) const;

	/**
	 * Set LeapController policy that improves tracking for HMD-mounted controller
	 * @param UseHmdPolicy		True for HMD-mounted mode, false otherwise
	 * @return					True if the device is connected 
	 */
	bool SetHmdPolicy(bool UseHmdPolicy);

	/** Gets reference frame */
	const Leap::Frame Frame() const;

	/** Caches the current Leap::Frame as a reference. Call once per unreal frame tick, and before any other calls to LeapMotionDevice on that frame tick. */
	void SetReferenceFrameOncePerTick();

protected:

	/** Local reference to the Leap::Controller */
	Leap::Controller LeapController;

	/** Reference frame of Leap Motion data. This is updated every Unreal frame tick & is kept constant throughout the frame. */
	Leap::Frame ReferenceLeapFrame;

	/** Last Unreal frame counter. Used to determine when ReferenceLeapFrame needs to be updated. */
	uint64 ReferenceFrameCounter;
};
