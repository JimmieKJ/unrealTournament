// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"


/**
 * The public interface to this module
 */
class IOculusLibraryPlugin : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOculusLibraryPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IOculusLibraryPlugin >( "OculusLibrary" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "OculusLibrary" );
	}

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
	virtual void GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector::ZeroVector) = 0;

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it will be set to zero.
	*
	* @param Accelerometer	(out) Acceleration reading in m/s^2.
	* @param Gyro			(out) Rotation rate in rad/s.
	* @param Magnetometer	(out) Magnetic field in Gauss.
	* @param Temperature	(out) Temperature of the sensor in degrees Celsius.
	* @param TimeInSeconds	(out) Time when the reported IMU reading took place, in seconds.
	*/
	virtual void GetRawSensorData(FVector& Accelerometer, FVector& Gyro, FVector& Magnetometer, float& Temperature, float& TimeInSeconds) = 0;

	/**
	* Returns current user profile.
	*
	* @param Profile		(out) Structure to hold current user profile.
	* @return (boolean)	True, if user profile was acquired.
	*/
	virtual bool GetUserProfile(struct FHmdUserProfile& Profile)=0;

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
	virtual void SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options) = 0;

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
	virtual void GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters) = 0;

	/**
	* Turns on/off default PlayerController's behavior to follow HMD orientation/position
	*/
	virtual void EnablePlayerControllerFollowHmd(bool bEnable) = 0;

	/**
	* Returns true if PlayerController follows HMD orientation/position. False, otherwise.
	*/
	virtual bool IsPlayerControllerFollowHmdEnabled() = 0;

	/**
	* Controls how PlayerCameraManager follows HMD. Note, this works for any active PlayerCameraManager
	* with bFollowHmdOrientation property set to true.
	*
	* @param bFollowHmdOrientation	(in) True if camera's orientation should be updated by most recent HMD orientation.
	* @param bFollowHmdPosition	(in) Whether the camera's position should be updated by most recent HMD orientation or not.
	*/
	virtual void EnablePlayerCameraManagerFollowHmd(bool bFollowHmdOrientation, bool bFollowHmdPosition) = 0;

	/**
	* Returns current settings for PlayerCameraManager's overrides for following HMD orientation and position.
	*
	* @param bFollowHmdOrientation	(out) True if camera's orientation should be updated by most recent HMD orientation.
	* @param bFollowHmdPosition	(out) Whether the camera's position should be updated by most recent HMD orientation or not.
	*/
	virtual void GetPlayerCameraManagerFollowHmd(bool& bFollowHmdOrientation, bool& bFollowHmdPosition) = 0;

	/**
	* Sets 'base rotation' - the rotation that will be subtracted from
	* the actual HMD orientation.
	* The position offset might be added to current HMD position,
	* effectively moving the virtual camera by the specified offset. The addition
	* occurs after the HMD orientation and position are applied.
	*
	* @param BaseRot			(in) Rotator object with base rotation
	* @param PosOffset			(in) the vector to be added to HMD position.
	* @param Options			(in) specifies either position, orientation or both should be set.
	*/
	virtual void SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options) = 0;

	/**
	* Returns current base rotation and position offset.
	*
	* @param OutRot			(out) Rotator object with base rotation
	* @param OutPosOffset		(out) the vector with previously set position offset.
	*/
	virtual void GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset) = 0;
};

