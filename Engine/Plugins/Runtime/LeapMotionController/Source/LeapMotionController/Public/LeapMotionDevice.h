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
	 * Checks whether or not the device is connected and ready to be polled for data.
	 * @return			True if controller is connected, false otherwise.
	 */
	bool IsConnected() const;

	/**
	 * Returns list of IDs of all hands tracked by the device
	 * @param OutAllHandIds		An output array set to contain the ids of all tracked hands.
	 * @return					True, if the device is connected
	 */
	bool GetAllHandsIds(TArray<int32>& OutAllHandIds) const;

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
	bool GetOldestLeftOrRightHandId(ELeapSide LeapSide, int32& OutHandId) const;

	/**
	 * Reports whether the specified hand us identified as a left or a right hand.
	 * @param HandId			The id of the hand of interest.
	 * @param OutIsLeft			A bool set to true if the specified hand is a left hand.
	 * @returns			        True if a hand of the specified type exists, false otherwise.
	 */
	bool GetIsHandLeft(int32 HandId, bool& OutIsLeft) const;

	/**
	 * Gets the position and orientation of the specified hand from the device.
	 *
	 * Data returned by this function is relative to the Unreal Engine origin rather than
	 * a particular LeapMotionControllerComponent or LeapMotionHandActor instance.
	 * Get the properties of a LeapMotionBoneActor instance to get the data
	 * relative to that actor's parent hand actor and controller component. In this
	 * case, use the standard Unreal Actor location and rotation properties.
	 *
	 * @param HandId			The id of the hand of interest.
	 * @param LeapBone			Which hand bone to get position & orientation for
	 * @param OutPosition		An FVector set to the relative position of the hand, 
	 *							or zero if the hand wasn't detected this frame.
	 * @param OutOrientation	An FRotator set to the relative orientation of the hand, 
	 *							or zero if the hand wasn't detected this frame.
	 * @return					True if a hand with the specified id exists, false otherwise
	 */
	bool GetBonePostionAndOrientation(int32 HandId, ELeapBone LeapBone, FVector& OutPosition, FRotator& OutOrientation) const;

	/**
	 * Gets the width and length of the specified bone of a hand from the device.
	 * @param HandId			The id of the hand of interest.
	 * @param LeapBone			The bone of interest.
	 * @param OutWidth			A float set to the bone's width (in centimeters).
	 * @param OutLength			A float set to the bone's length (in centimeters).
	 * @return					True if the hand was detected and bone was identified, false otherwise
	 */
	bool GetBoneWidthAndLength(int32 HandId, ELeapBone LeapBone, float& OutWidth, float& OutLength) const;

	/**
	 * The grab strength rating of the specified hand.
	 *
	 * Grab strength is a rating of how the hand's posture resembles a fist.
	 * A strength of 0 is close to an open, flat hand; a strength of 1 is close 
	 * to a fist.
	 * @param HandId			The id of the hand of interest.
	 * @param OutGrabStrength	A float set to the hand's grabbing strength as reported by Leap API.
	 * @returns					True, if the hand with the specified id exists, false otherwise.
	 */
	bool GetGrabStrength(int32 HandId, float& OutGrabStrength) const;

	/**
	 * The pinch strength rating of the specified hand.
	 *
	 * Pinch strength is a rating of whether the hand is in a pinching posture.
	 * Pinch strength starts at 0 and increases to 1 as any finger tip approaches
	 * the tip of the thumb. Note that pinch and grab strength are not independent.
	 * A grabbing hand will generally have a non-zero pinch strength as well.
	 *
	 * @param HandId			The id of the hand of interest.
	 * @param OutPinchStrength	A float set to the hand's pinch strength as reported by Leap API
	 * @returns					True, if the hand with the specified id exists, false otherwise.
	 */
	bool GetPinchStrength(int32 HandId, float& OutPinchStrength) const;

	/**
	 * Enables or disables the Leap Motion Controller policy that improves tracking for an
	 * HMD-mounted controller.
	 * Note that calling this function does not change the transforms of any 
	 * LeapMotionControllerComponent instances that may exist.
	 *
	 * @param UseHmdPolicy		True to enable for HMD-mounted mode, false to disable.
	 * @returns					True if the device is connected.
	 */
	bool SetHmdPolicy(bool UseHmdPolicy);

	/** 
	 * Gets the cached reference frame. Call SetReferenceFrameOncePerTick() before
	 * the first access to this variable during a tick.
	 *
	 * Data in a Leap::Frame object has not been transformed into the Unreal Engine
	 * frame of reference or unit system.
	 */
	const Leap::Frame Frame() const;

	/** 
	 * Caches the current Leap::Frame as a reference. Call once per Unreal frame 
	 * tick, and before any other calls to LeapMotionDevice on that frame tick. 
	 * The Leap Motion software produces frames of data at a rate independent of
	 * the Unreal Engine. Cache the frame so that subsequent calls to get data 
	 * from the device use the same data.
	 *
	 * Calling the function a second time in the same tick has no affect, but it
	 * does need to be called at least once.
	 */
	void SetReferenceFrameOncePerTick();

protected:

	/** Local reference to the Leap::Controller */
	Leap::Controller LeapController;

	/** Reference frame of Leap Motion data. This is updated every Unreal frame tick & is kept constant throughout the frame. */
	Leap::Frame ReferenceLeapFrame;

	/** Last Unreal frame counter. Used to determine when ReferenceLeapFrame needs to be updated. */
	uint64 ReferenceFrameCounter;
};
