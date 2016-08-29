// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "TargetPlatform.h"
#include "IDeviceProfileSelectorModule.h"
#include "IConsoleManager.h"
#if WITH_EDITOR
#include "PlatformInfo.h"
#endif

FString UDeviceProfileManager::DeviceProfileFileName;

UDeviceProfileManager* UDeviceProfileManager::DeviceProfileManagerSingleton = nullptr;

UDeviceProfileManager& UDeviceProfileManager::Get()
{
	if (DeviceProfileManagerSingleton == nullptr)
	{
		DeviceProfileManagerSingleton = NewObject<UDeviceProfileManager>();

		DeviceProfileManagerSingleton->AddToRoot();
		if (!FPlatformProperties::RequiresCookedData())
		{
			DeviceProfileManagerSingleton->LoadProfiles();
		}

		UDeviceProfile* ActiveProfile = &DeviceProfileManagerSingleton->FindProfile(GetActiveProfileName());
		DeviceProfileManagerSingleton->SetActiveDeviceProfile(ActiveProfile);

		InitializeSharedSamplerStates();
	}
	return *DeviceProfileManagerSingleton;
}


void UDeviceProfileManager::InitializeCVarsForActiveDeviceProfile()
{
	// Find the device profile selector module used in this instance
	FString DeviceProfileSelectionModule;
	GConfig->GetString( TEXT("DeviceProfileManager"), TEXT("DeviceProfileSelectionModule"), DeviceProfileSelectionModule, GEngineIni );

	FString SelectedPlatformDeviceProfileName = GetActiveProfileName();
	UE_LOG(LogInit, Log, TEXT("Applying CVar settings loaded from the selected device profile: [%s]"), *SelectedPlatformDeviceProfileName);

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
						OnSetCVarFromIniEntry(*DeviceProfileFileName, *CVarKey, *CVarValue, ECVF_SetByDeviceProfile);
						IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarKey);
						if( CVar )
						{
							UE_LOG(LogInit, Log, TEXT("Setting Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);
						}
						else
						{
							UE_LOG(LogInit, Warning, TEXT("Creating unregistered Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);
						}
						CVarsAlreadySetList.Add(CVarKey, CVarValue);
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


UDeviceProfile& UDeviceProfileManager::CreateProfile( const FString& ProfileName, const FString& ProfileType, const FString& InSpecifyParentName )
{
	UDeviceProfile* DeviceProfile = FindObject<UDeviceProfile>( GetTransientPackage(), *ProfileName );
	if( DeviceProfile == NULL )
	{
		// Build Parent objects first. Important for setup
		FString ParentName = InSpecifyParentName;
		if (ParentName.Len() == 0)
		{
			const FString SectionName = FString::Printf(TEXT("%s %s"), *ProfileName, *UDeviceProfile::StaticClass()->GetName());
			GConfig->GetString(*SectionName, TEXT("BaseProfileName"), ParentName, GetDeviceProfileIniName());
		}

		UObject* ParentObject = nullptr;
		// Recursively build the parent tree
		if (ParentName.Len() > 0)
		{
			ParentObject = FindObject<UDeviceProfile>(GetTransientPackage(), *ParentName);
			if (ParentObject == nullptr)
			{
				ParentObject = &CreateProfile(ParentName, ProfileType);
			}
		}

		// Create the profile after it's parents have been created.
		DeviceProfile = NewObject<UDeviceProfile>(GetTransientPackage(), *ProfileName);
		DeviceProfile->DeviceType = DeviceProfile->DeviceType.Len() > 0 ? DeviceProfile->DeviceType : ProfileType;
		DeviceProfile->BaseProfileName = DeviceProfile->BaseProfileName.Len() > 0 ? DeviceProfile->BaseProfileName : ParentName;
		DeviceProfile->Parent = ParentObject;

		// Add the new profile to the accessible device profile list
		Profiles.Add( DeviceProfile );

		// Inform any listeners that the device list has changed
		ManagerUpdatedDelegate.Broadcast(); 
	}

	return *DeviceProfile;
}


void UDeviceProfileManager::DeleteProfile( UDeviceProfile* Profile )
{
	Profiles.Remove( Profile );
}


UDeviceProfile& UDeviceProfileManager::FindProfile( const FString& ProfileName )
{
	UDeviceProfile* FoundProfile = nullptr;

	for( int32 Idx = 0; Idx < Profiles.Num(); Idx++ )
	{
		UDeviceProfile* CurrentDevice = CastChecked<UDeviceProfile>( Profiles[Idx] );
		if( CurrentDevice->GetName() == ProfileName )
		{
			FoundProfile = CurrentDevice;
			break;
		}
	}

	return FoundProfile != nullptr ? *FoundProfile : CreateProfile(ProfileName, FPlatformProperties::PlatformName());
}


const FString UDeviceProfileManager::GetDeviceProfileIniName() const
{
	return DeviceProfileFileName;
}


FOnDeviceProfileManagerUpdated& UDeviceProfileManager::OnManagerUpdated()
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

#if WITH_EDITOR
		if (!FPlatformProperties::RequiresCookedData())
		{
			// Register Texture LOD settings with each Target Platform
			ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
			const TArray<ITargetPlatform*>& TargetPlatforms = TargetPlatformManager.GetTargetPlatforms();
			for (int32 PlatformIndex = 0; PlatformIndex < TargetPlatforms.Num(); ++PlatformIndex)
			{
				ITargetPlatform* Platform = TargetPlatforms[PlatformIndex];
				if (const UTextureLODSettings* TextureLODSettingsObj = (UTextureLODSettings*)&FindProfile(*Platform->GetPlatformInfo().VanillaPlatformName.ToString()))
				{
					// Set TextureLODSettings
					Platform->RegisterTextureLODSettings(TextureLODSettingsObj);
				}
			}
		}
#endif

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


const FString UDeviceProfileManager::GetActiveProfileName()
{
	FString ActiveProfileName = FPlatformProperties::PlatformName();

	FString DeviceProfileSelectionModule;
	if (GConfig->GetString(TEXT("DeviceProfileManager"), TEXT("DeviceProfileSelectionModule"), DeviceProfileSelectionModule, GEngineIni))
	{
		if (IDeviceProfileSelectorModule* DPSelectorModule = FModuleManager::LoadModulePtr<IDeviceProfileSelectorModule>(*DeviceProfileSelectionModule))
		{
			ActiveProfileName = DPSelectorModule->GetRuntimeDeviceProfileName();
		}
	}

	return ActiveProfileName;
}


void UDeviceProfileManager::SetActiveDeviceProfile( UDeviceProfile* DeviceProfile )
{
	ActiveDeviceProfile = DeviceProfile;
}


UDeviceProfile* UDeviceProfileManager::GetActiveProfile() const
{
	return ActiveDeviceProfile;
}


void UDeviceProfileManager::GetAllPossibleParentProfiles(const UDeviceProfile* ChildProfile, OUT TArray<UDeviceProfile*>& PossibleParentProfiles) const
{
	for(auto& NextProfile : Profiles)
	{
		UDeviceProfile* ParentProfile = CastChecked<UDeviceProfile>(NextProfile);
		if (ParentProfile->DeviceType == ChildProfile->DeviceType && ParentProfile != ChildProfile)
		{
			bool bIsValidPossibleParent = true;

			UDeviceProfile* CurrentAncestor = ParentProfile;
			do
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
			} while(CurrentAncestor && bIsValidPossibleParent);

			if(bIsValidPossibleParent)
			{
				PossibleParentProfiles.Add(ParentProfile);
			}
		}
	}
}