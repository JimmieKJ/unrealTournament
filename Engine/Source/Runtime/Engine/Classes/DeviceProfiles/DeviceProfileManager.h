// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeviceProfileManager.h: Declares the FDeviceProfileManager class.
=============================================================================*/

#pragma once

#include "DeviceProfileManager.generated.h"

class UDeviceProfile;

// Delegate used to refresh the UI when the profiles change
DECLARE_MULTICAST_DELEGATE( FOnManagerUpdated );

/**
 * Implements a helper class that manages all profiles in the Device
 */
 UCLASS( config=DeviceProfiles, transient )
class ENGINE_API UDeviceProfileManager : public UObject
{
public:

	GENERATED_UCLASS_BODY()

	/**
	 * Startup and select the active device profile
	 * Then Init the CVars from this profile and it's Device profile parent tree.
	 */
	static void InitializeCVarsForActiveDeviceProfile();

	/**
	 * Create a device profile.
	 *
	 * @param ProfileName - The profile name.
	 *
	 * @return the created profile.
	 */
	UDeviceProfile* CreateProfile( const FString& ProfileName );

	/**
	 * Create a copy of a device profile from a copy.
	 *
	 * @param ProfileName - The profile name.
 	 * @param ProfileToCopy - The profile to copy name.
	 *
	 * @return the created profile.
	 */
	UDeviceProfile* CreateProfile( const FString& ProfileName, const FString& ProfileType, const FString& ParentName = TEXT("") );

	/**
	 * Delete a profile.
	 *
	 * @param Profile - The profile to delete.
	 */
	void DeleteProfile( UDeviceProfile* Profile );

	/**
	 * Find a profile based on the name.
	 *
	 * @param ProfileName - The profile name to find.
	 * @return The found profile.
	 */
	UDeviceProfile* FindProfile( const FString& ProfileName );

	/**
	 * Get the device profile .ini name.
	 *
	 * @return the device profile .ini name.
	 */
	const FString GetDeviceProfileIniName();

	/**
	 * Load the device profiles from the config file.
	 */
	void LoadProfiles();

	/**
	 * Returns a delegate that is invoked when manager is updated.
	 *
	 * @return The delegate.
	 */
	FOnManagerUpdated& OnManagerUpdated();

	/**
	 * Save the device profiles.
	 */
	void SaveProfiles(bool bSaveToDefaults = false);

	/**
	 * Get the selected device profile. Set by the blueprint.
	 *
	 * @return The selected profile.
	 */
	UDeviceProfile* GetActiveProfile() const;

	/**
	 * Set the active device profile - set via the device profile blueprint.
	 *
	 * @param DeviceProfileName - The profile name.
	 */
	void SetActiveDeviceProfile( UDeviceProfile* DeviceProfile );

	/**
	 * Get a list of all possible parent profiles for a given device profile
	 *
	 * @param ChildProfile				- The profile we are looking for potential parents
	 * @param PossibleParentProfiles	- The list of profiles which would be suitable as a parent for the given profile
	 */
	void GetAllPossibleParentProfiles( const UDeviceProfile* ChildProfile, OUT TArray<UDeviceProfile*>& PossibleParentProfiles );


public:

	// Holds the collection of managed profiles.
	UPROPERTY( EditAnywhere, Category=Properties )
	TArray< UObject* > Profiles;

private:

	// Holds a delegate to be invoked profiles are updated.
	FOnManagerUpdated ManagerUpdatedDelegate;

	// Holds the selected device profile
	class UDeviceProfile* ActiveDeviceProfile;

	// Holds the device profile .ini location
	static FString DeviceProfileFileName;

	int32 RenameIndex;
};