// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DeviceProfiles/DeviceProfileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "UObject/Package.h"
#include "SceneManagement.h"
#include "SystemSettings.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "IDeviceProfileSelectorModule.h"
#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "PlatformInfo.h"
#endif

static TAutoConsoleVariable<FString> CVarDeviceProfileOverride(
	TEXT("dp.Override"),
	TEXT(""),
	TEXT("DeviceProfile override - setting this will use the named DP as the active DP. In addition, it will restore any\n")
	TEXT(" previous overrides before setting (does a dp.OverridePop before setting after the first time).\n")
	TEXT(" The commandline -dp option will override this on startup, but not when setting this at runtime\n"),
	ECVF_Default);


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

		// always start with an active profile, even if we create it on the spot
		UDeviceProfile* ActiveProfile = DeviceProfileManagerSingleton->FindProfile(GetActiveProfileName());
		DeviceProfileManagerSingleton->SetActiveDeviceProfile(ActiveProfile);

		// now we allow the cvar changes to be acknowledged
		CVarDeviceProfileOverride.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			UDeviceProfileManager::Get().HandleDeviceProfileOverrideChange();
		}));

		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("dp.Override.Restore"),
			TEXT("Restores any cvars set by dp.Override to their previous value"),
			FConsoleCommandDelegate::CreateLambda([]()
			{
				UDeviceProfileManager::Get().HandleDeviceProfileOverridePop();
			}),
			ECVF_Default
		);

		InitializeSharedSamplerStates();
	}
	return *DeviceProfileManagerSingleton;
}


void UDeviceProfileManager::InitializeCVarsForActiveDeviceProfile(bool bPushSettings)
{
	FString ActiveProfileName = DeviceProfileManagerSingleton ? DeviceProfileManagerSingleton->ActiveDeviceProfile->GetName() : GetActiveProfileName();

	UE_LOG(LogInit, Log, TEXT("Applying CVar settings loaded from the selected device profile: [%s]"), *ActiveProfileName);

	// Load the device profile config
	FConfigCacheIni::LoadGlobalIniFile(DeviceProfileFileName, TEXT("DeviceProfiles"));

	TArray< FString > AvailableProfiles;
	GConfig->GetSectionNames( DeviceProfileFileName, AvailableProfiles );

	// Look up the ini for this tree as we are far too early to use the UObject system
	AvailableProfiles.Remove( TEXT( "DeviceProfiles" ) );

	// Next we need to create a hierarchy of CVars from the Selected Device Profile, to it's eldest parent
	TMap<FString, FString> CVarsAlreadySetList;
	
	// even if we aren't pushing new values, we should clear any old pushed values, as they are no longer valid after we run this loop
	if (DeviceProfileManagerSingleton)
	{
		DeviceProfileManagerSingleton->PushedSettings.Empty();
	}

	// For each device profile, starting with the selected and working our way up the BaseProfileName tree,
	// Find all CVars and set them 
	FString BaseDeviceProfileName = ActiveProfileName;
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
						if (CVar)
						{
							if (DeviceProfileManagerSingleton && bPushSettings)
							{
								// remember the previous value
								FString OldValue = CVar->GetString();
								DeviceProfileManagerSingleton->PushedSettings.Add(CVarKey, OldValue);

								// indicate we are pushing, not setting
								UE_LOG(LogInit, Log, TEXT("Pushing Device Profile CVar: [[%s:%s -> %s]]"), *CVarKey, *OldValue, *CVarValue);
							}
							else
							{
								UE_LOG(LogInit, Log, TEXT("Setting Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);
							}
						}
						else
						{
							UE_LOG(LogInit, Warning, TEXT("Creating unregistered Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);
						}

						OnSetCVarFromIniEntry(*DeviceProfileFileName, *CVarKey, *CVarValue, ECVF_SetByDeviceProfile);
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


UDeviceProfile* UDeviceProfileManager::CreateProfile( const FString& ProfileName, const FString& ProfileType, const FString& InSpecifyParentName )
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
				ParentObject = CreateProfile(ParentName, ProfileType);
			}
		}

		// Create the profile after it's parents have been created.
		DeviceProfile = NewObject<UDeviceProfile>(GetTransientPackage(), *ProfileName);
		DeviceProfile->DeviceType = DeviceProfile->DeviceType.Len() > 0 ? DeviceProfile->DeviceType : ProfileType;
		DeviceProfile->BaseProfileName = DeviceProfile->BaseProfileName.Len() > 0 ? DeviceProfile->BaseProfileName : ParentName;
		DeviceProfile->Parent = ParentObject;
		// the DP manager can be marked as Disregard for GC, so what it points to needs to be in the Root set
		DeviceProfile->AddToRoot();

		// Add the new profile to the accessible device profile list
		Profiles.Add( DeviceProfile );

		// Inform any listeners that the device list has changed
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

	return FoundProfile != nullptr ? FoundProfile : CreateProfile(ProfileName, FPlatformProperties::PlatformName());
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

				// Set TextureLODSettings
				const UTextureLODSettings* TextureLODSettingsObj = FindProfile(*Platform->GetPlatformInfo().VanillaPlatformName.ToString());
				Platform->RegisterTextureLODSettings(TextureLODSettingsObj);
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

void UDeviceProfileManager::HandleDeviceProfileOverrideChange()
{
	FString CVarValue = CVarDeviceProfileOverride.GetValueOnGameThread();
	// only handle when the value is different
	if (CVarValue.Len() > 0 && CVarValue != GetActiveProfile()->GetName())
	{
		// find the profile (note that if the name is bad, this will create one with that name)
		UDeviceProfile* NewActiveProfile = FindProfile(CVarValue);

		// pop any pushed settings
		HandleDeviceProfileOverridePop();

		// activate new one!
		DeviceProfileManagerSingleton->SetActiveDeviceProfile(NewActiveProfile);
		InitializeCVarsForActiveDeviceProfile(true);
	}
}

void UDeviceProfileManager::HandleDeviceProfileOverridePop()
{
	// restore pushed settings
	for (TMap<FString, FString>::TIterator It(PushedSettings); It; ++It)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*It.Key());
		if (CVar)
		{
			// restore it!
			CVar->Set(*It.Value(), ECVF_SetByDeviceProfile);
			UE_LOG(LogInit, Log, TEXT("Popping Device Profile CVar: [[%s:%s]]"), *It.Key(), *It.Value());
		}
	}
}

const FString UDeviceProfileManager::GetActiveProfileName()
{
	FString ActiveProfileName = FPlatformProperties::PlatformName();

	// look for a commandline override (never even calls into the selector plugin)
	FString OverrideProfileName;
	if (FParse::Value(FCommandLine::Get(), TEXT("DeviceProfile="), OverrideProfileName) || FParse::Value(FCommandLine::Get(), TEXT("DP="), OverrideProfileName))
	{
		return OverrideProfileName;
	}

	// look for cvar override
	OverrideProfileName = CVarDeviceProfileOverride.GetValueOnGameThread();
	if (OverrideProfileName.Len() > 0)
	{
		return OverrideProfileName;
	}


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
