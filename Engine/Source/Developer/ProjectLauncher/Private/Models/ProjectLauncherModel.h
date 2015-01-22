// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


namespace ELauncherPanels
{
	/**
	 * Enumerates available launcher panels.
	 */
	enum Type
	{
		NoTask = 0,
		Launch,
		ProfileEditor,
		Progress,
		End,
	};
}


/** Type definition for shared pointers to instances of FProjectLauncherModel. */
typedef TSharedPtr<class FProjectLauncherModel> FProjectLauncherModelPtr;

/** Type definition for shared references to instances of FProjectLauncherModel. */
typedef TSharedRef<class FProjectLauncherModel> FProjectLauncherModelRef;


/**
 * Delegate type for launcher profile selection changes.
 *
 * The first parameter is the newly selected profile (or NULL if none was selected).
 * The second parameter is the old selected profile (or NULL if none was previous selected).
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSelectedSProjectLauncherProfileChanged, const ILauncherProfilePtr&, const ILauncherProfilePtr&)


/**
 * Implements the data model for the session launcher.
 */
class FProjectLauncherModel
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDeviceProxyManager - The device proxy manager to use.
	 * @param InLauncher - The launcher to use.
	 * @param InProfileManager - The profile manager to use.
	 */
	FProjectLauncherModel(const ITargetDeviceProxyManagerRef& InDeviceProxyManager, const ILauncherRef& InLauncher, const ILauncherProfileManagerRef& InProfileManager)
		: DeviceProxyManager(InDeviceProxyManager)
		, SProjectLauncher(InLauncher)
		, ProfileManager(InProfileManager)
	{
		ProfileManager->OnProfileAdded().AddRaw(this, &FProjectLauncherModel::HandleProfileManagerProfileAdded);
		ProfileManager->OnProfileRemoved().AddRaw(this, &FProjectLauncherModel::HandleProfileManagerProfileRemoved);

		LoadConfig();
	}

	/**
	 * Destructor.
	 */
	~FProjectLauncherModel()
	{
		ProfileManager->OnProfileAdded().RemoveAll(this);
		ProfileManager->OnProfileRemoved().RemoveAll(this);

		SaveConfig();
	}

public:

	/**
	 * Gets the model's device proxy manager.
	 *
	 * @return Device proxy manager.
	 */
	const ITargetDeviceProxyManagerRef& GetDeviceProxyManager() const
	{
		return DeviceProxyManager;
	}

	/**
	 * Gets the model's launcher.
	 *
	 * @return SProjectLauncher.
	 */
	const ILauncherRef& GetSProjectLauncher() const
	{
		return SProjectLauncher;
	}

	/**
	 * Get the model's profile manager.
	 *
	 * @return Profile manager.
	 */
	const ILauncherProfileManagerRef& GetProfileManager() const
	{
		return ProfileManager;
	}


	/**
	 * Gets the active profile.
	 *
	 * @return The active profile.
	 *
	 * @see GetProfiles
	 */
	const ILauncherProfilePtr& GetSelectedProfile() const
	{
		return SelectedProfile;
	}

	/**
	 * Sets the active profile.
	 *
	 * @param InProfile The profile to assign as active,, or NULL to deselect all.
	 */
	void SelectProfile(const ILauncherProfilePtr& Profile)
	{
		if (!Profile.IsValid() || ProfileManager->GetAllProfiles().Contains(Profile))
		{
			if (Profile != SelectedProfile)
			{
				ILauncherProfilePtr& PreviousProfile = SelectedProfile;
				SelectedProfile = Profile;

				ProfileSelectedDelegate.Broadcast(Profile, PreviousProfile);
			}
		}
	}

public:

	/**
	 * Returns a delegate to be invoked when the profile list has been modified.
	 *
	 * @return The delegate.
	 */
	FSimpleMulticastDelegate& OnProfileListChanged()
	{
		return ProfileListChangedDelegate;
	}

	/**
	 * Returns a delegate to be invoked when the selected profile changed.
	 *
	 * @return The delegate.
	 */
	FOnSelectedSProjectLauncherProfileChanged& OnProfileSelected()
	{
		return ProfileSelectedDelegate;
	}

protected:

	/*
	 * Load all profiles from disk.
	 */
	void LoadConfig()
	{
		// restore the previous project selection
		FString ProjectPath;
		if (FPaths::IsProjectFilePathSet())
		{
			ProjectPath = FPaths::GetProjectFilePath();
		}
		else if (FGameProjectHelper::IsGameAvailable(FApp::GetGameName()))
		{
			ProjectPath = FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
		}
		else if (GConfig != NULL)
		{
			GConfig->GetString(TEXT("FProjectLauncherModel"), TEXT("SelectedProjectPath"), ProjectPath, GEngineIni);
		}
		ProfileManager->SetProjectPath(ProjectPath);
	}

	/*
	 * Saves all profiles to disk.
	 */
	void SaveConfig()
	{
		if (GConfig != NULL && !FPaths::IsProjectFilePathSet() && !FGameProjectHelper::IsGameAvailable(FApp::GetGameName()))
		{
			FString ProjectPath = ProfileManager->GetProjectPath();
			GConfig->SetString(TEXT("FProjectLauncherModel"), TEXT("SelectedProjectPath"), *ProjectPath, GEngineIni);
		}
	}

private:

	// Callback for when a profile was added to the profile manager.
	void HandleProfileManagerProfileAdded(const ILauncherProfileRef& Profile)
	{
		ProfileListChangedDelegate.Broadcast();

		SelectProfile(Profile);
	}

	// Callback for when a profile was removed from the profile manager.
	void HandleProfileManagerProfileRemoved(const ILauncherProfileRef& Profile)
	{
		ProfileListChangedDelegate.Broadcast();

		if (Profile == SelectedProfile)
		{
			const TArray<ILauncherProfilePtr>& Profiles = ProfileManager->GetAllProfiles();

			if (Profiles.Num() > 0)
			{
				SelectProfile(Profiles[0]);
			}
			else
			{
				SelectProfile(NULL);
			}
		}
	}

private:

	// Holds a pointer to the device proxy manager.
	ITargetDeviceProxyManagerRef DeviceProxyManager;

	// Holds a pointer to the launcher.
	ILauncherRef SProjectLauncher;

	// Holds a pointer to the profile manager.
	ILauncherProfileManagerRef ProfileManager;

	// Holds a pointer to active profile.
	ILauncherProfilePtr SelectedProfile;

private:

	// Holds a delegate to be invoked when the project path has been modified.
	FSimpleMulticastDelegate ProjectPathChangedDelegate;

	// Holds a delegate to be invoked when the profile list has been modified.
	FSimpleMulticastDelegate ProfileListChangedDelegate;

	// Holds a delegate to be invoked when the selected profile changed.
	FOnSelectedSProjectLauncherProfileChanged ProfileSelectedDelegate;

};
