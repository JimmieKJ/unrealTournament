// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HeadMountedDisplayFunctionLibrary.generated.h"

UENUM()
namespace EOrientPositionSelector
{
	enum Type
	{
		Orientation UMETA(DisplayName = "Orientation"),
		Position UMETA(DisplayName = "Position"),
		OrientationAndPosition UMETA(DisplayName = "Orientation and Position")
	};
}

/**
* For HMDs that support it, this specifies whether the origin of the tracking universe will be at the floor, or at the user's eye height
*/
UENUM()
namespace EHMDTrackingOrigin
{
	enum Type
	{
		Floor UMETA(DisplayName = "Floor Level"),
		Eye UMETA(DisplayName = "Eye Level"),
	};
}

/**
* Stores if the user is wearing the HMD or not. For HMDs without a sensor to detect the user wearing it, the state defaults to Unknown.
*/
UENUM()
namespace EHMDWornState
{
	enum Type
	{
		Unknown UMETA(DisplayName = "Unknown"),
		Worn UMETA(DisplayName = "Worn"),
		NotWorn UMETA(DisplayName = "Not Worn"),
	};
}

UCLASS()
class ENGINE_API UHeadMountedDisplayFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Returns whether or not we are currently using the head mounted display.
	 *
	 * @return (Boolean)  status of HMD
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static bool IsHeadMountedDisplayEnabled();

	/**
	 * Switches to/from using HMD and stereo rendering.
	 *
	 * @param bEnable			(in) 'true' to enable HMD / stereo; 'false' otherwise
	 * @return (Boolean)		True, if the request was successful.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static bool EnableHMD(bool bEnable);

	/**
	 * Returns the name of the device, so scripts can modify their behaviour appropriately
	 *
	 * @return	FName specific to the currently active HMD device type.  "None" implies no device, "Unknown" implies a device with no description.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static FName GetHMDDeviceName();

	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static void GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition);

	/**
	* If the HMD supports positional tracking, whether or not we are currently being tracked
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static bool HasValidTrackingPosition();

	/**
	* If the HMD has multiple positional tracking sensors, return a total number of them currently connected.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static int32 GetNumOfTrackingSensors();

	/**
	 * If the HMD has a positional sensor, this will return the game-world location of it, as well as the parameters for the bounding region of tracking.
	 * This allows an in-game representation of the legal positional tracking range.  All values will be zeroed if the sensor is not available or the HMD does not support it.
	 *
	 * @param Index				(in) Index of the tracking sensor to query
	 * @param Origin			(out) Origin, in world-space, of the sensor
	 * @param Rotation			(out) Rotation, in world-space, of the sensor
	 * @param HFOV				(out) Field-of-view, horizontal, in degrees, of the valid tracking zone of the sensor
	 * @param VFOV				(out) Field-of-view, vertical, in degrees, of the valid tracking zone of the sensor
	 * @param Distance			(out) Nominal distance to sensor, in world-space
	 * @param NearPlane			(out) Near plane distance of the tracking volume, in world-space
	 * @param FarPlane			(out) Far plane distance of the tracking volume, in world-space
	 * @param IsActive			(out) True, if the query for the specified sensor succeeded.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay", meta = (AdvancedDisplay = "HFOV,VFOV,Distance,NearPlane,FarPlane"))
	static void GetTrackingSensorParameters(FVector& Origin, FRotator& Rotation, float& HFOV, float& VFOV, float& Distance, float& NearPlane, float& FarPlane, bool& IsActive, int32 Index = 0);

	/**
	 * If the HMD has a positional sensor, this will return the game-world location of it, as well as the parameters for the bounding region of tracking.
	 * This allows an in-game representation of the legal positional tracking range.  All values will be zeroed if the sensor is not available or the HMD does not support it.
	 *
	 * @param Origin			(out) Origin, in world-space, of the sensor
	 * @param Rotation			(out) Rotation, in world-space, of the sensor
	 * @param HFOV				(out) Field-of-view, horizontal, in degrees, of the valid tracking zone of the sensor
	 * @param VFOV				(out) Field-of-view, vertical, in degrees, of the valid tracking zone of the sensor
	 * @param CameraDistance	(out) Nominal distance to sensor, in world-space
	 * @param NearPlane			(out) Near plane distance of the tracking volume, in world-space
	 * @param FarPlane			(out) Far plane distance of the tracking volume, in world-space
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay", meta=(DeprecatedFunction, DeprecationMessage = "Use new GetTrackingSensorParameters / GetNumOfTrackingSensors"))
	static void GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraRotation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float& FarPlane);

	/**
	 * Returns true, if HMD is in low persistence mode. 'false' otherwise.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static bool IsInLowPersistenceMode();

	/**
	 * Switches between low and full persistence modes.
	 *
	 * @param bEnable			(in) 'true' to enable low persistence mode; 'false' otherwise
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void EnableLowPersistenceMode(bool bEnable);

	/** 
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking). 
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 * @param Options			(in) specifies either position, orientation or both should be reset.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void ResetOrientationAndPosition(float Yaw = 0.f, EOrientPositionSelector::Type Options = EOrientPositionSelector::OrientationAndPosition);

	/** 
	 * Sets near and far clipping planes (NCP and FCP) for stereo rendering. Similar to 'stereo ncp= fcp' console command, but NCP and FCP set by this
	 * call won't be saved in .ini file.
	 *
	 * @param Near				(in) Near clipping plane, in centimeters
	 * @param Far				(in) Far clipping plane, in centimeters
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void SetClippingPlanes(float Near, float Far);

	/** 
	 * Sets screen percentage to be used in VR mode.
	 *
	 * @param ScreenPercentage	(in) Specifies the screen percentage to be used in VR mode. Use 0.0f value to reset to default value.
	 */
	static void SetScreenPercentage(float ScreenPercentage = 0.0f);

	/** 
	 * Returns screen percentage to be used in VR mode.
	 *
	 * @return (float)	The screen percentage to be used in VR mode.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static float GetScreenPercentage();


	/**
	* Sets the World to Meters scale, which changes the scale of the world as perceived by the player
	*
	* @param NewScale	Specifies how many Unreal units correspond to one meter in the real world
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay", meta = (WorldContext = "WorldContext"))
	static void SetWorldToMetersScale(UObject* WorldContext, float NewScale = 100.f);

	/**
	* Returns the World to Meters scale, which corresponds to the scale of the world as perceived by the player
	*
	* @return	How many Unreal units correspond to one meter in the real world
	*/
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay", meta = (WorldContext = "WorldContext"))
	static float GetWorldToMetersScale(UObject* WorldContext);

	/**
	 * Sets current tracking origin type (eye level or floor level).
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void SetTrackingOrigin(TEnumAsByte<EHMDTrackingOrigin::Type> Origin);

	/**
	 * Returns current tracking origin type (eye level or floor level).
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static TEnumAsByte<EHMDTrackingOrigin::Type> GetTrackingOrigin();

	/**
	 * Returns current state of VR focus.
	 *
	 * @param bUseFocus		(out) if set to true, then this App does use VR focus.
	 * @param bHasFocus		(out) if set to true, then this App currently has VR focus.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay", meta=(DisplayName="Get VR Focus State"))
	static void GetVRFocusState(bool& bUseFocus, bool& bHasFocus);
};