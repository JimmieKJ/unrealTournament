// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LauncherServicesPrivatePCH.h"


/* ILauncherProfileManager structors
 *****************************************************************************/

FLauncherProfileManager::FLauncherProfileManager()
{
}

/* ILauncherProfileManager interface
 *****************************************************************************/

void FLauncherProfileManager::Load()
{
	LoadDeviceGroups();
	LoadProfiles();
}

void FLauncherProfileManager::AddDeviceGroup( const ILauncherDeviceGroupRef& DeviceGroup )
{
	if (!DeviceGroups.Contains(DeviceGroup))
	{
		// replace the existing device group
		ILauncherDeviceGroupPtr ExistingGroup = GetDeviceGroup(DeviceGroup->GetId());

		if (ExistingGroup.IsValid())
		{
			RemoveDeviceGroup(ExistingGroup.ToSharedRef());
		}

		// add the new device group
		DeviceGroups.Add(DeviceGroup);
		SaveDeviceGroups();

		DeviceGroupAddedDelegate.Broadcast(DeviceGroup);
	}
}


ILauncherDeviceGroupRef FLauncherProfileManager::AddNewDeviceGroup( )
{
	ILauncherDeviceGroupRef  NewGroup = MakeShareable(new FLauncherDeviceGroup(FGuid::NewGuid(), FString::Printf(TEXT("New Group %d"), DeviceGroups.Num())));

	AddDeviceGroup(NewGroup);

	return NewGroup;
}


ILauncherDeviceGroupRef FLauncherProfileManager::CreateUnmanagedDeviceGroup()
{
	ILauncherDeviceGroupRef  NewGroup = MakeShareable(new FLauncherDeviceGroup(FGuid::NewGuid(), TEXT("Simple Group")));
	return NewGroup;
}


ILauncherSimpleProfilePtr FLauncherProfileManager::FindOrAddSimpleProfile(const FString& DeviceName)
{
	// replace the existing profile
	ILauncherSimpleProfilePtr SimpleProfile = FindSimpleProfile(DeviceName);
	if (!SimpleProfile.IsValid())
	{
		SimpleProfile = MakeShareable(new FLauncherSimpleProfile(DeviceName));
		SimpleProfiles.Add(SimpleProfile);
	}
	
	return SimpleProfile;
}


ILauncherSimpleProfilePtr FLauncherProfileManager::FindSimpleProfile(const FString& DeviceName)
{
	for (int32 ProfileIndex = 0; ProfileIndex < SimpleProfiles.Num(); ++ProfileIndex)
	{
		ILauncherSimpleProfilePtr SimpleProfile = SimpleProfiles[ProfileIndex];

		if (SimpleProfile->GetDeviceName() == DeviceName)
		{
			return SimpleProfile;
		}
	}

	return nullptr;
}

ILauncherProfileRef FLauncherProfileManager::AddNewProfile()
{
	// find a unique name for the profile.
	int32 ProfileIndex = SavedProfiles.Num();
	FString ProfileName = FString::Printf(TEXT("New Profile %d"), ProfileIndex);

	for (int32 Index = 0; Index < SavedProfiles.Num(); ++Index)
	{
		if (SavedProfiles[Index]->GetName() == ProfileName)
		{
			ProfileName = FString::Printf(TEXT("New Profile %d"), ++ProfileIndex);
			Index = -1;

			continue;
		}
	}

	// create and add the profile
	ILauncherProfileRef NewProfile = MakeShareable(new FLauncherProfile(AsShared(), FGuid::NewGuid(), ProfileName));

	AddProfile(NewProfile);

	SaveProfile(NewProfile);

	return NewProfile;
}

ILauncherProfileRef FLauncherProfileManager::CreateUnsavedProfile(FString ProfileName)
{
	// create and return the profile
	ILauncherProfileRef NewProfile = MakeShareable(new FLauncherProfile(AsShared(), FGuid(), ProfileName));
	
	AllProfiles.Add(NewProfile);
	
	return NewProfile;
}


void FLauncherProfileManager::AddProfile( const ILauncherProfileRef& Profile )
{
	if (!SavedProfiles.Contains(Profile))
	{
		// replace the existing profile
		ILauncherProfilePtr ExistingProfile = GetProfile(Profile->GetId());

		if (ExistingProfile.IsValid())
		{
			RemoveProfile(ExistingProfile.ToSharedRef());
		}

		if (!Profile->GetDeployedDeviceGroup().IsValid())
		{
			Profile->SetDeployedDeviceGroup(AddNewDeviceGroup());
		}

		// add the new profile
		SavedProfiles.Add(Profile);
		AllProfiles.Add(Profile);

		ProfileAddedDelegate.Broadcast(Profile);
	}
}


ILauncherProfilePtr FLauncherProfileManager::FindProfile( const FString& ProfileName )
{
	for (int32 ProfileIndex = 0; ProfileIndex < SavedProfiles.Num(); ++ProfileIndex)
	{
		ILauncherProfilePtr Profile = SavedProfiles[ProfileIndex];

		if (Profile->GetName() == ProfileName)
		{
			return Profile;
		}
	}

	return nullptr;
}


const TArray<ILauncherDeviceGroupPtr>& FLauncherProfileManager::GetAllDeviceGroups( ) const
{
	return DeviceGroups;
}


const TArray<ILauncherProfilePtr>& FLauncherProfileManager::GetAllProfiles( ) const
{
	return SavedProfiles;
}


ILauncherDeviceGroupPtr FLauncherProfileManager::GetDeviceGroup( const FGuid& GroupId ) const
{
	for (int32 GroupIndex = 0; GroupIndex < DeviceGroups.Num(); ++GroupIndex)
	{
		const ILauncherDeviceGroupPtr& Group = DeviceGroups[GroupIndex];

		if (Group->GetId() == GroupId)
		{
			return Group;
		}
	}

	return nullptr;
}


ILauncherProfilePtr FLauncherProfileManager::GetProfile( const FGuid& ProfileId ) const
{
	for (int32 ProfileIndex = 0; ProfileIndex < SavedProfiles.Num(); ++ProfileIndex)
	{
		ILauncherProfilePtr Profile = SavedProfiles[ProfileIndex];

		if (Profile->GetId() == ProfileId)
		{
			return Profile;
		}
	}

	return nullptr;
}


ILauncherProfilePtr FLauncherProfileManager::LoadProfile( FArchive& Archive )
{
	FLauncherProfile* Profile = new FLauncherProfile(AsShared());

	if (Profile->Serialize(Archive))
	{
		ILauncherDeviceGroupPtr DeviceGroup = GetDeviceGroup(Profile->GetDeployedDeviceGroupId());
		if (!DeviceGroup.IsValid())
		{
			DeviceGroup = AddNewDeviceGroup();	
		}
		Profile->SetDeployedDeviceGroup(DeviceGroup);

		return MakeShareable(Profile);
	}

	return nullptr;
}


void FLauncherProfileManager::LoadSettings( )
{
	LoadDeviceGroups();
	LoadProfiles();
}


void FLauncherProfileManager::RemoveDeviceGroup( const ILauncherDeviceGroupRef& DeviceGroup )
{
	if (DeviceGroups.Remove(DeviceGroup) > 0)
	{
		SaveDeviceGroups();

		DeviceGroupRemovedDelegate.Broadcast(DeviceGroup);
	}
}


void FLauncherProfileManager::RemoveSimpleProfile(const ILauncherSimpleProfileRef& SimpleProfile)
{
	if (SimpleProfiles.Remove(SimpleProfile) > 0)
	{
		// delete the persisted simple profile on disk
		FString SimpleProfileFileName = GetProfileFolder() / SimpleProfile->GetDeviceName() + TEXT(".uslp");
		IFileManager::Get().Delete(*SimpleProfileFileName);
	}
}


void FLauncherProfileManager::RemoveProfile( const ILauncherProfileRef& Profile )
{
	AllProfiles.Remove(Profile);
	if (SavedProfiles.Remove(Profile) > 0)
	{
		if (Profile->GetId().IsValid())
		{
			// delete the persisted profile on disk
			FString ProfileFileName = GetProfileFolder() / Profile->GetId().ToString() + TEXT(".ulp");

			// delete the profile
			IFileManager::Get().Delete(*ProfileFileName);

			ProfileRemovedDelegate.Broadcast(Profile);
		}
	}
}


void FLauncherProfileManager::SaveProfile(const ILauncherProfileRef& Profile)
{
	if (Profile->GetId().IsValid())
	{
		FString ProfileFileName = GetProfileFolder() / Profile->GetId().ToString() + TEXT(".ulp");
		FArchive* ProfileFileWriter = IFileManager::Get().CreateFileWriter(*ProfileFileName);

		if (ProfileFileWriter != nullptr)
		{
			Profile->Serialize(*ProfileFileWriter);

			delete ProfileFileWriter;
		}
	}
}


void FLauncherProfileManager::SaveSettings( )
{
	SaveDeviceGroups();
	SaveSimpleProfiles();
	SaveProfiles();
}

FString FLauncherProfileManager::GetProjectName() const
{
	return FLauncherProjectPath::GetProjectName(ProjectPath);
}

FString FLauncherProfileManager::GetProjectBasePath() const
{
	return FLauncherProjectPath::GetProjectBasePath(ProjectPath);
}

FString FLauncherProfileManager::GetProjectPath() const
{
	return ProjectPath;
}

void FLauncherProfileManager::SetProjectPath(const FString& InProjectPath)
{
	if (ProjectPath != InProjectPath)
	{
		ProjectPath = InProjectPath;
		for (ILauncherProfilePtr Profile : AllProfiles)
		{
			if (Profile.IsValid())
			{
				Profile->FallbackProjectUpdated();
			}
		}
	}
}

void FLauncherProfileManager::LoadDeviceGroups( )
{
	if (GConfig != nullptr)
	{
		FConfigSection* LoadedDeviceGroups = GConfig->GetSectionPrivate(TEXT("Launcher.DeviceGroups"), false, true, GEngineIni);

		if (LoadedDeviceGroups != nullptr)
		{
			// parse the configuration file entries into device groups
			for (FConfigSection::TIterator It(*LoadedDeviceGroups); It; ++It)
			{
				if (It.Key() == TEXT("DeviceGroup"))
				{
					DeviceGroups.Add(ParseDeviceGroup(*It.Value()));
				}
			}
		}
	}
}


void FLauncherProfileManager::LoadProfiles( )
{
	TArray<FString> ProfileFileNames;

	IFileManager::Get().FindFiles(ProfileFileNames, *(GetProfileFolder() / TEXT("*.ulp")), true, false);
	
	for (TArray<FString>::TConstIterator It(ProfileFileNames); It; ++It)
	{
		FString ProfileFilePath = GetProfileFolder() / *It;
		FArchive* ProfileFileReader = IFileManager::Get().CreateFileReader(*ProfileFilePath);

		if (ProfileFileReader != nullptr)
		{
			ILauncherProfilePtr LoadedProfile = LoadProfile(*ProfileFileReader);
			delete ProfileFileReader;

			if (LoadedProfile.IsValid())
			{
				AddProfile(LoadedProfile.ToSharedRef());
			}
			else
			{
				IFileManager::Get().Delete(*ProfileFilePath);
			}
		}
	}
}


ILauncherDeviceGroupPtr FLauncherProfileManager::ParseDeviceGroup( const FString& GroupString )
{
	TSharedPtr<FLauncherDeviceGroup> Result;

	FString GroupIdString;

	if (FParse::Value(*GroupString, TEXT("Id="), GroupIdString))
	{
		FGuid GroupId;

		if (!FGuid::Parse(GroupIdString, GroupId))
		{
			GroupId = FGuid::NewGuid();
		}

		FString GroupName;
		FParse::Value(*GroupString, TEXT("Name="), GroupName);

		FString DevicesString;
		FParse::Value(*GroupString, TEXT("Devices="), DevicesString);

		Result = MakeShareable(new FLauncherDeviceGroup(GroupId, GroupName));

		TArray<FString> DeviceList;
		DevicesString.ParseIntoArray(DeviceList, TEXT(", "), false);

		for (int32 Index = 0; Index < DeviceList.Num(); ++Index)
		{
			Result->AddDevice(DeviceList[Index]);
		}
	}

	return Result;
}


void FLauncherProfileManager::SaveDeviceGroups( )
{
	if (GConfig != nullptr)
	{
		GConfig->EmptySection(TEXT("Launcher.DeviceGroups"), GEngineIni);

		TArray<FString> DeviceGroupStrings;

		// create a string representation of all groups and their devices
		for (int32 GroupIndex = 0; GroupIndex < DeviceGroups.Num(); ++GroupIndex)
		{
			const ILauncherDeviceGroupPtr& Group = DeviceGroups[GroupIndex];
			const TArray<FString>& Devices = Group->GetDeviceIDs();

			FString DeviceListString;

			for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
			{
				if (DeviceIndex > 0)
				{
					DeviceListString += ", ";
				}

				DeviceListString += Devices[DeviceIndex];
			}

			FString DeviceGroupString = FString::Printf(TEXT("(Id=\"%s\", Name=\"%s\", Devices=\"%s\")" ), *Group->GetId().ToString(), *Group->GetName(), *DeviceListString);

			DeviceGroupStrings.Add(DeviceGroupString);
		}

		// save configuration
		GConfig->SetArray(TEXT("Launcher.DeviceGroups"), TEXT("DeviceGroup"), DeviceGroupStrings, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}
}


void FLauncherProfileManager::SaveSimpleProfiles()
{
	for (TArray<ILauncherSimpleProfilePtr>::TIterator It(SimpleProfiles); It; ++It)
	{
		FString SimpleProfileFileName = GetProfileFolder() / (*It)->GetDeviceName() + TEXT(".uslp");
		FArchive* ProfileFileWriter = IFileManager::Get().CreateFileWriter(*SimpleProfileFileName);

		if (ProfileFileWriter != nullptr)
		{
			(*It)->Serialize(*ProfileFileWriter);

			delete ProfileFileWriter;
		}
	}
}


void FLauncherProfileManager::SaveProfiles( )
{
	for (TArray<ILauncherProfilePtr>::TIterator It(SavedProfiles); It; ++It)
	{
		SaveProfile((*It).ToSharedRef());
	}
}
