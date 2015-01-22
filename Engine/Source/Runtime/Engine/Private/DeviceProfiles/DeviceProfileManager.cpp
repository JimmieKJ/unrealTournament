// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "TargetPlatform.h"
#include "IDeviceProfileSelectorModule.h"
#include "IConsoleManager.h"

FString UDeviceProfileManager::DeviceProfileFileName;

void UDeviceProfileManager::InitializeCVarsForActiveDeviceProfile()
{
	// Find the device profile selector module used in this instance
	FString DeviceProfileSelectionModule;
	GConfig->GetString( TEXT("DeviceProfileManager"), TEXT("DeviceProfileSelectionModule"), DeviceProfileSelectionModule, GEngineIni );

	FString SelectedPlatformDeviceProfileName;
#if PLATFORM_HTML5
	SelectedPlatformDeviceProfileName = FPlatformProperties::PlatformName();
#else
	if ( !DeviceProfileSelectionModule.IsEmpty() )
	{
		// Load the module we had specified in the ini and Run our logic to select a device profile for this run
		IDeviceProfileSelectorModule& DPSelectorModule = FModuleManager::LoadModuleChecked<IDeviceProfileSelectorModule>(*DeviceProfileSelectionModule);
		SelectedPlatformDeviceProfileName = DPSelectorModule.GetRuntimeDeviceProfileName();
		UE_LOG(LogInit, Log, TEXT("Applying CVar settings loaded from the selected device profile: [%s]"), *SelectedPlatformDeviceProfileName);
	}
#endif

	// Load the device profile config
	FConfigCacheIni::LoadGlobalIniFile(DeviceProfileFileName, TEXT("DeviceProfiles"));

	TArray< FString > AvailableProfiles;
	GConfig->GetSectionNames( DeviceProfileFileName, AvailableProfiles );

	// Look up the ini for this tree as we are far too early to use the UObject system
	AvailableProfiles.Remove( TEXT( "DeviceProfiles" ) );

	// Next we need to create a hierarchy of CVars from the Selected Device Profile, to it's eldest parent
	TMap<FString, FString> CVarsAlreadySetList;
	
	// For each device profile, starting with the selected and working our way up the BaseProfileName tree,
	// Find all CVars and set them 
	FString BaseDeviceProfileName = SelectedPlatformDeviceProfileName;
	bool bReachedEndOfTree = BaseDeviceProfileName.IsEmpty();
	while( bReachedEndOfTree == false ) 
	{
		FString CurrentSectionName = FString::Printf( TEXT("%s %s"), *BaseDeviceProfileName, *UDeviceProfile::StaticClass()->GetName() );
		
		// Check the profile was available.
		bool bProfileExists = AvailableProfiles.Contains( CurrentSectionName );
		if( bProfileExists )
		{
			TArray< FString > CurrentProfilesCVars;
			GConfig->GetArray( *CurrentSectionName, TEXT("CVars"), CurrentProfilesCVars, DeviceProfileFileName );

			// Iterate over the profile and make sure we do not have duplicate CVars
			{
				TMap< FString, FString > ValidCVars;
				for( TArray< FString >::TConstIterator CVarIt(CurrentProfilesCVars); CVarIt; ++CVarIt )
				{
					FString CVarKey, CVarValue;
					if( (*CVarIt).Split( TEXT("="), &CVarKey, &CVarValue ) )
					{
						if( ValidCVars.Find( CVarKey ) )
						{
							ValidCVars.Remove( CVarKey );
						}

						ValidCVars.Add( CVarKey, CVarValue );
					}
				}
				
				// Empty the current list, and replace with the processed CVars. This removes duplicates
				CurrentProfilesCVars.Empty();

				for( TMap< FString, FString >::TConstIterator ProcessedCVarIt(ValidCVars); ProcessedCVarIt; ++ProcessedCVarIt )
				{
					CurrentProfilesCVars.Add( FString::Printf( TEXT("%s=%s"), *ProcessedCVarIt.Key(), *ProcessedCVarIt.Value() ) );
				}

			}

			// Iterate over this profiles cvars and set them if they haven't been already.
			for( TArray< FString >::TConstIterator CVarIt(CurrentProfilesCVars); CVarIt; ++CVarIt )
			{
				FString CVarKey, CVarValue;
				if( (*CVarIt).Split( TEXT("="), &CVarKey, &CVarValue ) )
				{
					if( !CVarsAlreadySetList.Find( CVarKey ) )
					{
						IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarKey);
						if( CVar )
						{
							UE_LOG(LogInit, Log, TEXT("Setting Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);
							CVar->Set( *CVarValue, ECVF_SetByDeviceProfile);
							CVarsAlreadySetList.Add( CVarKey, CVarValue );
						}
						else
						{
							UE_LOG(LogInit, Warning, TEXT("Failed to find a registered CVar that matches the key: [%s]"), *CVarKey);
						}
					}
				}
			}

			// Get the next device profile name, to look for CVars in, along the tree
			FString NextBaseDeviceProfileName;
			if( GConfig->GetString( *CurrentSectionName, TEXT("BaseProfileName"), NextBaseDeviceProfileName, DeviceProfileFileName ) )
			{
				BaseDeviceProfileName = NextBaseDeviceProfileName;
			}
			else
			{
				BaseDeviceProfileName.Empty();
			}
		}
		
		// Check if we have inevitably reached the end of the device profile tree.
		bReachedEndOfTree = !bProfileExists || BaseDeviceProfileName.IsEmpty();
	}
}


UDeviceProfileManager::UDeviceProfileManager( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	RenameIndex = 0;
#if WITH_EDITOR
	LoadProfiles();
#endif
}


UDeviceProfile* UDeviceProfileManager::CreateProfile( const FString& ProfileName )
{
	UDeviceProfile* NewProfile = ConstructObject<UDeviceProfile>( UDeviceProfile::StaticClass(), GetTransientPackage(), *ProfileName, RF_Transient|RF_Public);
	Profiles.Add( NewProfile );

	// Inform the UI that the device list has changed
	ManagerUpdatedDelegate.Broadcast();

	return NewProfile;
}


UDeviceProfile* UDeviceProfileManager::CreateProfile( const FString& ProfileName, const FString& ProfileType, const FString& ParentName )
{
	UDeviceProfile* DeviceProfile = FindObject<UDeviceProfile>( GetTransientPackage(), *ProfileName );
	if( DeviceProfile == NULL )
	{
		DeviceProfile = ConstructObject<UDeviceProfile>( UDeviceProfile::StaticClass(), GetTransientPackage(), *ProfileName, RF_Transient|RF_Public);
		DeviceProfile->LoadConfig( UDeviceProfile::StaticClass(), *DeviceProfileFileName );
		DeviceProfile->BaseProfileName = ParentName != TEXT("") ? ParentName : DeviceProfile->BaseProfileName;
		DeviceProfile->DeviceType = ProfileType;

		UDeviceProfile* ObjectTemplate = NULL;

		// Recursively build the parent tree
		if( DeviceProfile->BaseProfileName != TEXT("") )
		{
			DeviceProfile->Parent = FindObject<UDeviceProfile>( GetTransientPackage(), *DeviceProfile->BaseProfileName );
			if( DeviceProfile->Parent == NULL )
			{
				DeviceProfile->Parent = CreateProfile( DeviceProfile->BaseProfileName, ProfileType );
			}
			ObjectTemplate = CastChecked<UDeviceProfile>(DeviceProfile->Parent);
		}

		if( ObjectTemplate )
		{
			DeviceProfile->BaseProfileName = ObjectTemplate->GetName();
		}
		DeviceProfile->DeviceType = ProfileType;

		Profiles.Add( DeviceProfile );

		// Inform the UI that the device list has changed
		ManagerUpdatedDelegate.Broadcast(); 
	}

	return DeviceProfile;
}


void UDeviceProfileManager::DeleteProfile( UDeviceProfile* Profile )
{
	Profiles.Remove( Profile );
}


UDeviceProfile* UDeviceProfileManager::FindProfile( const FString& ProfileName )
{
	UDeviceProfile* FoundProfile = NULL;

	for( int32 Idx = 0; Idx < Profiles.Num(); Idx++ )
	{
		UDeviceProfile* CurrentDevice = CastChecked<UDeviceProfile>( Profiles[Idx] );
		if( CurrentDevice->GetName() == ProfileName )
		{
			FoundProfile = CurrentDevice;
			break;
		}
	}

	return FoundProfile;
}


const FString UDeviceProfileManager::GetDeviceProfileIniName()
{
	return DeviceProfileFileName;
}


FOnManagerUpdated& UDeviceProfileManager::OnManagerUpdated()
{
	return ManagerUpdatedDelegate;
}


void UDeviceProfileManager::LoadProfiles()
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		TArray< FString > DeviceProfileMapArray;
		GConfig->GetArray(TEXT("DeviceProfiles"), TEXT("DeviceProfileNameAndTypes"), DeviceProfileMapArray, DeviceProfileFileName);

		for (int32 DeviceProfileIndex = 0; DeviceProfileIndex < DeviceProfileMapArray.Num(); ++DeviceProfileIndex)
		{
			FString NewDeviceProfileSelectorPlatformName;
			FString NewDeviceProfileSelectorPlatformType;
			DeviceProfileMapArray[DeviceProfileIndex].Split(TEXT(","), &NewDeviceProfileSelectorPlatformName, &NewDeviceProfileSelectorPlatformType);

			if (FindObject<UDeviceProfile>(GetTransientPackage(), *NewDeviceProfileSelectorPlatformName) == NULL)
			{
				CreateProfile(NewDeviceProfileSelectorPlatformName, NewDeviceProfileSelectorPlatformType);
			}
		}
		ManagerUpdatedDelegate.Broadcast();
	}
}


void UDeviceProfileManager::SaveProfiles(bool bSaveToDefaults)
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		if(bSaveToDefaults)
		{
			for (int32 DeviceProfileIndex = 0; DeviceProfileIndex < Profiles.Num(); ++DeviceProfileIndex)
			{
				UDeviceProfile* CurrentProfile = CastChecked<UDeviceProfile>(Profiles[DeviceProfileIndex]);
				CurrentProfile->UpdateDefaultConfigFile();
			}
		}
		else
		{
			TArray< FString > DeviceProfileMapArray;

			for (int32 DeviceProfileIndex = 0; DeviceProfileIndex < Profiles.Num(); ++DeviceProfileIndex)
			{
				UDeviceProfile* CurrentProfile = CastChecked<UDeviceProfile>(Profiles[DeviceProfileIndex]);
				FString DeviceProfileTypeNameCombo = FString::Printf(TEXT("%s,%s"), *CurrentProfile->GetName(), *CurrentProfile->DeviceType);
				DeviceProfileMapArray.Add(DeviceProfileTypeNameCombo);

				CurrentProfile->SaveConfig(CPF_Config, *DeviceProfileFileName);
			}

			GConfig->SetArray(TEXT("DeviceProfiles"), TEXT("DeviceProfileNameAndTypes"), DeviceProfileMapArray, DeviceProfileFileName);
			GConfig->Flush(false, DeviceProfileFileName);
		}

		ManagerUpdatedDelegate.Broadcast();
	}
}


void UDeviceProfileManager::SetActiveDeviceProfile( UDeviceProfile* DeviceProfile )
{
	ActiveDeviceProfile = DeviceProfile;
}


UDeviceProfile* UDeviceProfileManager::GetActiveProfile() const
{
	return ActiveDeviceProfile;
}


void UDeviceProfileManager::GetAllPossibleParentProfiles(const UDeviceProfile* ChildProfile, OUT TArray<UDeviceProfile*>& PossibleParentProfiles)
{
	for(auto& NextProfile : Profiles)
	{
		UDeviceProfile* ParentProfile = CastChecked<UDeviceProfile>(NextProfile);
		if (ParentProfile->DeviceType == ChildProfile->DeviceType && ParentProfile != ChildProfile)
		{
			bool bIsValidPossibleParent = true;

			UDeviceProfile* CurrentAncestor = ParentProfile;
			while(CurrentAncestor && bIsValidPossibleParent)
			{
				if(CurrentAncestor->BaseProfileName == ChildProfile->GetName())
				{
					bIsValidPossibleParent = false;
					break;
				}
				else
				{
					CurrentAncestor = CurrentAncestor->Parent != nullptr ? CastChecked<UDeviceProfile>(CurrentAncestor->Parent) : NULL;
				}
			}

			if(bIsValidPossibleParent)
			{
				PossibleParentProfiles.Add(ParentProfile);
			}
		}
	}
}