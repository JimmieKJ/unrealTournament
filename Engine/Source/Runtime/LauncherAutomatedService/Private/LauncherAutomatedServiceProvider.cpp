// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LauncherAutomatedServicePrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC(LauncherAutomatedService, Log, All);
#define LOCTEXT_NAMESPACE "LauncherAutomatedService"


/* FLauncherAutomatedServiceProvider structors
 *****************************************************************************/

FLauncherAutomatedServiceProvider::FLauncherAutomatedServiceProvider()
	: bHasErrors( false )
	, bHasLaunchedAllInstances( false )
	, bIsReadyToShutdown( false )
	, bShouldDeleteProfileWhenComplete( false )
	, LastDevicePingTime( 0.0 )
	, SessionID( FGuid::NewGuid() )
	, TimeSinceLastShutdownRequest( 0.0 )
{ }


/* ILauncherAutomatedServiceProvider interface
 *****************************************************************************/

void FLauncherAutomatedServiceProvider::Setup( const TCHAR* Params )
{
	LastDevicePingTime = 0.0;
	TimeSinceLastShutdownRequest = 0.0;
	bHasLaunchedAllInstances = false;
	bIsReadyToShutdown = false;
	bRunTestsRequested = false;
	TimeSinceSessionLaunched = 0.0f;
	bHasErrors = false;

	// Create the test manager
	AutomatedTestManager = MakeShareable(new FAutomatedTestManager( SessionID ));

	SetupProfileAndGroupSettings( Params );
}


void FLauncherAutomatedServiceProvider::Tick( float DeltaTime )
{
	FTicker::GetCoreTicker().Tick(DeltaTime);

	// Update the test manager
	AutomatedTestManager->Tick( DeltaTime );

	// If we have not started tests after a minute, assume something has gone wrong with a game instance, and start the tests on any devices that are available
	static const float StartTestTimeoutLimit = 60.0f;
	if ( bRunTestsRequested == false )
	{
		TimeSinceSessionLaunched += DeltaTime;
		if ( TimeSinceSessionLaunched > StartTestTimeoutLimit )
		{
			StartAutomationTests();
		}
	}

	// Check if tests are complete
	if( AutomatedTestManager->IsTestingComplete() )
	{
		for( int32 DeviceIdx = 0; DeviceIdx < DeployedInstances.Num(); DeviceIdx++ )
		{
			ITargetDeviceProxyPtr Device = DeployedInstances[ DeviceIdx ];
			// @todo gmp: fix automated service provider
			//Device->CloseGame();
		}

		bIsReadyToShutdown = true;
	}
}


void FLauncherAutomatedServiceProvider::Shutdown()
{
	if( DeviceProxyManager.IsValid() )
	{
		DeviceProxyManager->OnProxyAdded().RemoveAll( this );
	}
	
	if( bShouldDeleteProfileWhenComplete && AutomatedProfile.IsValid() )
	{
		ProfileManager->RemoveProfile(AutomatedProfile.ToSharedRef());
	}
}


/* FLauncherAutomatedServiceProvider implementation
 *****************************************************************************/


void FLauncherAutomatedServiceProvider::SetupProfileAndGroupSettings( const TCHAR* Params )
{
	bool bHasValidProfile = false;
	bool bHasValidDeviceGroup = false;

	ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT( "LauncherServices"));

	ProfileManager = LauncherServicesModule.GetProfileManager();

	if( ProfileManager.IsValid() )
	{
		FString ProfileName;
		if( FParse::Value( Params, TEXT( "Profile=" ), ProfileName ) )
		{
			AutomatedProfile = ProfileManager->FindProfile(ProfileName);

			bHasValidProfile = AutomatedProfile.IsValid() && AutomatedProfile->IsValidForLaunch();
		}

		if( !AutomatedProfile.IsValid() )
		{
			AutomatedProfile = LauncherServicesModule.CreateProfile(TEXT("LauncherAutomatedServiceProviderProfile"));
			bShouldDeleteProfileWhenComplete = true;

			UE_LOG(LauncherAutomatedService, Display, TEXT("PROFILE:") );

			// Get the game name, this must exist here 
			FString ProfileGameName;
			
			bool bHasValidGameName = FParse::Value( Params, TEXT( "GameName=" ), ProfileGameName );

			if( bHasValidGameName )
			{
				// @todo gmp: fix automated service provider; must use project path here
				// game names are no longer supported
				//AutomatedProfile->SetLegacyGameName(ProfileGameName);

				UE_LOG(LauncherAutomatedService, Display, TEXT("---Game: %s"), *ProfileGameName);
			}
			else
			{
				UE_LOG(LauncherAutomatedService, Error, TEXT("A valid game name was not found on the commandline") );
			}


			// Get the game config, this must exist here
			FString ProfileGameConfig;

			bool bHasValidConfig = FParse::Value( Params, TEXT( "Config=" ), ProfileGameConfig );
			
			if( bHasValidConfig )
			{
				AutomatedProfile->SetBuildConfiguration(EBuildConfigurations::FromString(ProfileGameConfig));

				UE_LOG(LauncherAutomatedService, Display, TEXT("---Config: %s"), *ProfileGameConfig);
			}
			else
			{
				UE_LOG(LauncherAutomatedService, Error, TEXT("A valid game configuration was not found on the commandline") );
			}
			
			// Must have a valid game name and config to be able to launch
			bHasValidProfile = AutomatedProfile->IsValidForLaunch();

			if( bHasValidProfile )
			{
				// Get the map name, this is optional
				FString ProfileMapName;
				
				FParse::Value( Params, TEXT( "Map=" ), ProfileMapName );
				
				if( FParse::Value( Params, TEXT( "Map=" ), ProfileMapName ) )
				{
					AutomatedProfile->GetDefaultLaunchRole()->SetInitialMap(ProfileMapName);

					UE_LOG(LauncherAutomatedService, Display, TEXT("%s"), *FText::Format(FText::FromString("---Map: {0}"), FText::FromString(ProfileMapName)).ToString());
				}

				// Get the command line, this is optional
				FString ProfileCmdLine;
				
				FParse::Value( Params, TEXT( "CmdLine=" ), ProfileCmdLine );
				
				if( !ProfileMapName.IsEmpty() )
				{
					AutomatedProfile->GetDefaultLaunchRole()->SetCommandLine(ProfileCmdLine);

					UE_LOG(LauncherAutomatedService, Display, TEXT("%s"), *FText::Format(FText::FromString("---CmdLine: {0}"), FText::FromString(ProfileCmdLine)).ToString());
				}
			}
		}
	}

	if( bHasValidProfile )
	{
		// Get the device group we will be deploying to
		FString DeviceGroupName;

		if( FParse::Value( Params, TEXT( "DeviceGroup=" ), DeviceGroupName ) )
		{
			AutomatedDeviceGroup = NULL;
			TArray< ILauncherDeviceGroupPtr > DeviceGroups = ProfileManager->GetAllDeviceGroups();

			for( int32 DeviceGroupIdx = 0; DeviceGroupIdx < DeviceGroups.Num(); DeviceGroupIdx++ )
			{
				ILauncherDeviceGroupPtr CurrentGroup = DeviceGroups[ DeviceGroupIdx ];

				if( CurrentGroup->GetName() == DeviceGroupName )
				{
					AutomatedDeviceGroup = CurrentGroup;
					break;
				}
			}

			bHasValidDeviceGroup = AutomatedDeviceGroup.IsValid() && (AutomatedDeviceGroup->GetNumDevices() > 0);

			// A reference to the proxy manager responsible for device activity here.
			DeviceProxyManager = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices")).GetDeviceProxyManager();
			DeviceProxyManager->OnProxyAdded().AddRaw( this, &FLauncherAutomatedServiceProvider::HandleDeviceProxyManagerProxyAdded );

			LastDevicePingTime = FPlatformTime::Seconds();
		}
	}

	bHasErrors = ( bHasValidProfile == false || bHasValidDeviceGroup == false ); 
}


void FLauncherAutomatedServiceProvider::StartAutomationTests()
{
	if ( bRunTestsRequested == false )
	{
		// Start the automation tests
		AutomatedTestManager->RunTests();
		bRunTestsRequested = true;
	}
}


/* ILauncherAutomatedServiceProvider callbacks
 *****************************************************************************/

void FLauncherAutomatedServiceProvider::HandleDeviceProxyManagerProxyAdded( const ITargetDeviceProxyRef& AddedProxy )
{
	if (AddedProxy->IsConnected())
	{
		FString SessionName = FString::Printf(TEXT("%s - %s"), FPlatformProcess::ComputerName(), *AutomatedDeviceGroup->GetName());
		FString CommandLine = FString::Printf(TEXT("%s %s -SessionId=\"%s\" -SessionOwner=\"%s\" -SessionName=\"%s\""), *AutomatedProfile->GetDefaultLaunchRole()->GetInitialMap(), *AutomatedProfile->GetDefaultLaunchRole()->GetCommandLine(), *SessionID.ToString(), FPlatformProcess::UserName(true), *SessionName);

		// @todo gmp: fix automated service provider; must use ILauncher interface here
		//AddedProxy->Run(AutomatedProfile->GetBuildConfiguration(), AutomatedProfile->GetBuildGame(), CommandLine);

		UE_LOG(LauncherAutomatedService, Display, TEXT("%s"), *FText::Format(FText::FromString("Deploying to: {0}"), FText::FromString(AddedProxy->GetName())).ToString());

		DeployedInstances.Add( AddedProxy );

		if (AutomatedDeviceGroup->GetNumDevices() == DeployedInstances.Num())
		{
			bHasLaunchedAllInstances = true;
			// All devices have been found. Start the automation test process
			StartAutomationTests();
		}
	}
}


#undef LOCTEXT_NAMESPACE
