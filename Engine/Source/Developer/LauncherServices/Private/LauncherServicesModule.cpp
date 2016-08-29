// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LauncherServicesPrivatePCH.h"


/**
 * Implements the LauncherServices module.
 */
class FLauncherServicesModule
	: public ILauncherServicesModule
{
public:

	// Begin ILauncherServicesModule interface

	virtual ILauncherDeviceGroupRef CreateDeviceGroup( ) override
	{
		return MakeShareable(new FLauncherDeviceGroup());
	}

	virtual ILauncherDeviceGroupRef CreateDeviceGroup(const FGuid& Guid, const FString& Name ) override
	{
		return MakeShareable(new FLauncherDeviceGroup(Guid, Name));
	}

	virtual ILauncherRef CreateLauncher( ) override
	{
		return MakeShareable(new FLauncher());
	}

	virtual ILauncherProfileRef CreateProfile( const FString& ProfileName ) override
	{
		ILauncherProfileManagerRef ProfileManager = GetProfileManager();
		return MakeShareable(new FLauncherProfile(ProfileManager, FGuid(), ProfileName));
	}

	virtual ILauncherProfileManagerRef GetProfileManager( ) override
	{
		if (!ProfileManagerSingleton.IsValid())
		{
			TSharedPtr<FLauncherProfileManager> ProfileManager = MakeShareable(new FLauncherProfileManager());	
			ProfileManager->Load();
			ProfileManagerSingleton = ProfileManager;
			ProfileManagerInitializedDelegate.Broadcast(*ProfileManager);
		}

		return ProfileManagerSingleton.ToSharedRef();
	}
	
	DECLARE_DERIVED_EVENT(FLauncherServicesModule, ILauncherServicesModule::FLauncherServicesSDKNotInstalled, FLauncherServicesSDKNotInstalled);
	virtual FLauncherServicesSDKNotInstalled& OnLauncherServicesSDKNotInstalled( ) override
	{
		return LauncherServicesSDKNotInstalled;
	}
	void BroadcastLauncherServicesSDKNotInstalled(const FString& PlatformName, const FString& DocLink) override
	{
		return LauncherServicesSDKNotInstalled.Broadcast(PlatformName, DocLink);
	}

	// End ILauncherServicesModule interface

private:
	
	/// Event to be called when the editor tried to use a platform, but it wasn't installed
	FLauncherServicesSDKNotInstalled LauncherServicesSDKNotInstalled;

	// Holds the launcher profile manager singleton
	static ILauncherProfileManagerPtr ProfileManagerSingleton;
};


/* Static initialization
 *****************************************************************************/

FOnLauncherProfileManagerInitialized ILauncherServicesModule::ProfileManagerInitializedDelegate;
ILauncherProfileManagerPtr FLauncherServicesModule::ProfileManagerSingleton = NULL;


IMPLEMENT_MODULE(FLauncherServicesModule, LauncherServices);
