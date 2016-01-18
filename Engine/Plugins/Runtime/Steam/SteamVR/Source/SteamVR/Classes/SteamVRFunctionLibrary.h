// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IMotionController.h"
#include "SteamVRFunctionLibrary.generated.h"

/** Defines the class of tracked devices in SteamVR*/
UENUM(BlueprintType)
enum class ESteamVRTrackedDeviceType
{
	/** Represents a Steam VR Controller */
	Controller,

	/** Represents a static tracking reference device, such as a Lighthouse or tracking camera */
	TrackingReference,

	/** Misc. device types, for future expansion */
	Other,

	/** DeviceId is invalid */
	Invalid
};

/** Defines which calibration point to use (e.g. center of the room for standing, calibrated head position for seated) */
UENUM(BlueprintType)
enum class ESteamVRTrackingSpace
{
	/** Standing origin, where poses are relative to the safe room bounds provided by the user's calibration */
	Standing,

	/** Seated origin, where poses are relative to the user's calibrated seated head position */
	Seated
};

/**
 * SteamVR Extensions Function Library
 */
UCLASS()
class STEAMVR_API USteamVRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	
	/**
	 * Returns an array of the currently tracked device IDs
	 *
	 * @param	DeviceType	Which class of device (e.g. controller, tracking devices) to get Device Ids for
	 * @param	OutTrackedDeviceIds	(out) Array containing the ID of each device that's currently tracked
	 */
	UFUNCTION(BlueprintPure, Category="SteamVR")
	static void GetValidTrackedDeviceIds(TEnumAsByte<ESteamVRTrackedDeviceType> DeviceType, TArray<int32>& OutTrackedDeviceIds);

	/**
	 * Gets the orientation and position (in device space) of the device with the specified ID
	 *
	 * @param	DeviceId		Id of the device to get tracking info for
	 * @param	OutPosition		(out) Current position of the device
	 * @param	OutOrientation	(out) Current orientation of the device
	 * @return	True if the specified device id had a valid tracking pose this frame, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "SteamVR")
	static bool GetTrackedDevicePositionAndOrientation(int32 DeviceId, FVector& OutPosition, FRotator& OutOrientation);

	/**
	 * Given a controller index and a hand, returns the position and orientation of the controller
	 *
	 * @param	ControllerIndex	Index of the controller to get the tracked device ID for
	 * @param	Hand			Which hand's controller to get the position and orientation for
	 * @param	OutPosition		(out) Current position of the device
	 * @param	OutRotation		(out) Current rotation of the device
	 * @return	True if the specified controller index has a valid tracked device ID
	 */
	UFUNCTION(BlueprintPure, Category = "SteamVR")
	static bool GetHandPositionAndOrientation(int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FRotator& OutOrientation);

	/**
	 * Sets the tracking space (e.g. sitting or standing), which changes which space tracked positions are returned to
	 *
	 * NewSpace		The new space to consider all tracked positions in.  For instance, standing assumes the center of the safe zone as the origin.
	 */
	UFUNCTION(BlueprintCallable, Category = "SteamVR")
	static void SetTrackingSpace(TEnumAsByte<ESteamVRTrackingSpace> NewSpace);

	/**
	 * Gets the tracking space (e.g. sitting or standing), which determines the location of the origin.
	 */
	UFUNCTION(BlueprintPure, Category = "SteamVR")
	static TEnumAsByte<ESteamVRTrackingSpace> GetTrackingSpace();
};
