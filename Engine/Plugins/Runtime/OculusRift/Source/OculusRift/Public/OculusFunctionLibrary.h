// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusFunctionLibrary.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data Field"))
struct FHmdUserProfileField
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldName;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldValue;

	FHmdUserProfileField() {}
	FHmdUserProfileField(const FString& Name, const FString& Value) :
		FieldName(Name), FieldValue(Value) {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data"))
struct FHmdUserProfile
{
	GENERATED_USTRUCT_BODY()

	/** Name of the user's profile. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Name;

	/** Gender of the user ("male", "female", etc). */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Gender;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float PlayerHeight;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float EyeHeight;

	/** Interpupillary distance of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float IPD;

	/** Neck-to-eye distance, in meters. X - horizontal, Y - vertical. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FVector2D NeckToEyeDistance;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	TArray<FHmdUserProfileField> ExtraFields;

	FHmdUserProfile() :
		PlayerHeight(0.f), EyeHeight(0.f), IPD(0.f), NeckToEyeDistance(FVector2D::ZeroVector) {}
};

UCLASS(MinimalAPI)
class UOculusFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 * @param NeckPosition		(out) The estimated neck position, calculated using NeckToEye vector from User Profile. Same coordinate space as DevicePosition.
	 * @param bUseOrienationForPlayerCamera	(in) Should be set to 'true' if the orientation is going to be used to update orientation of the camera manually.
	 * @param bUsePositionForPlayerCamera	(in) Should be set to 'true' if the position is going to be used to update position of the camera manually.
	 * @param PositionScale		(in) The 3D scale that will be applied to position.
	 */
	UFUNCTION(BlueprintPure, Category="Input|OculusLibrary")
	static void GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector::ZeroVector);

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it will be set to zero.
	*
	* @param Accelerometer	(out) Acceleration reading in m/s^2.
	* @param Gyro			(out) Rotation rate in rad/s.
	* @param Magnetometer	(out) Magnetic field in Gauss.
	* @param Temperature	(out) Temperature of the sensor in degrees Celsius.
	* @param TimeInSeconds	(out) Time when the reported IMU reading took place, in seconds.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static void GetRawSensorData(FVector& Accelerometer, FVector& Gyro, FVector& Magnetometer, float& Temperature, float& TimeInSeconds);

	/**
	* Returns current user profile.
	*
	* @param Profile		(out) Structure to hold current user profile.
	* @return (boolean)	True, if user profile was acquired.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool GetUserProfile(FHmdUserProfile& Profile);

	/**
	* Sets 'base rotation' - the rotation that will be subtracted from
	* the actual HMD orientation.
	* Sets base position offset (in meters). The base position offset is the distance from the physical (0, 0, 0) position
	* to current HMD position (bringing the (0, 0, 0) point to the current HMD position)
	* Note, this vector is set by ResetPosition call; use this method with care.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param Rotation			(in) Rotator object with base rotation
	* @param BaseOffsetInMeters (in) the vector to be set as base offset, in meters.
	* @param Options			(in) specifies either position, orientation or both should be set.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options);

	/**
	* Returns current base rotation and base offset.
	* The base offset is currently used base position offset, previously set by the
	* ResetPosition or SetBasePositionOffset calls. It represents a vector that translates the HMD's position
	* into (0,0,0) point, in meters.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param OutRotation			(out) Rotator object with base rotation
	* @param OutBaseOffsetInMeters	(out) base position offset, vector, in meters.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static void GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters);

	/**
	 * Turns on/off default PlayerController's behavior to follow HMD orientation/position
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void EnablePlayerControllerFollowHmd(bool bEnable);

	/**
	 * Returns true if PlayerController follows HMD orientation/position. False, otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool IsPlayerControllerFollowHmdEnabled();

	/**
	 * Controls how PlayerCameraManager follows HMD. Note, this works for any active PlayerCameraManager
	 * with bFollowHmdOrientation property set to true. 
	 *
	 * @param bFollowHmdOrientation	(in) True if camera's orientation should be updated by most recent HMD orientation.
	 * @param bFollowHmdPosition	(in) Whether the camera's position should be updated by most recent HMD orientation or not.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void EnablePlayerCameraManagerFollowHmd(bool bFollowHmdOrientation, bool bFollowHmdPosition);

	/**
	 * Returns current settings for PlayerCameraManager's overrides for following HMD orientation and position.
	 *
	 * @param bFollowHmdOrientation	(out) True if camera's orientation should be updated by most recent HMD orientation.
	 * @param bFollowHmdPosition	(out) Whether the camera's position should be updated by most recent HMD orientation or not.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static void GetPlayerCameraManagerFollowHmd(bool& bFollowHmdOrientation, bool& bFollowHmdPosition);

protected:
	static class FOculusRiftHMD* GetOculusHMD();
};
