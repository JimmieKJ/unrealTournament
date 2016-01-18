// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HotfixPrivatePCH.h"
#include "OnlineHotfixManager.h"
#include "Internationalization.h"
#include "OnlineSubsystemUtils.h"

DEFINE_LOG_CATEGORY(LogHotfixManager);

FName NAME_HotfixManager(TEXT("HotfixManager"));

struct FHotfixManagerExec :
	public FSelfRegisteringExec
{
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("HOTFIX")))
		{
			UOnlineHotfixManager* HotfixManager = UOnlineHotfixManager::Get(InWorld);
			if (HotfixManager != nullptr)
			{
				HotfixManager->StartHotfixProcess();
			}
			return true;
		}
		return false;
	}
};
static FHotfixManagerExec HotfixManagerExec;

UOnlineHotfixManager::UOnlineHotfixManager() :
	Super()
{
	OnEnumerateFilesCompleteDelegate = FOnEnumerateFilesCompleteDelegate::CreateUObject(this, &UOnlineHotfixManager::OnEnumerateFilesComplete);
	OnReadFileCompleteDelegate = FOnReadFileCompleteDelegate::CreateUObject(this, &UOnlineHotfixManager::OnReadFileComplete);
	// So we only try to apply files for this platform
	PlatformPrefix = ANSI_TO_TCHAR(FPlatformProperties::PlatformName());
	PlatformPrefix += TEXT("_");
}

UOnlineHotfixManager* UOnlineHotfixManager::Get(UWorld* World)
{
	UOnlineHotfixManager* DefaultObject = UOnlineHotfixManager::StaticClass()->GetDefaultObject<UOnlineHotfixManager>();
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World, DefaultObject->OSSName.Len() > 0 ? FName(*DefaultObject->OSSName) : NAME_None);
	if (OnlineSub != nullptr)
	{
		UOnlineHotfixManager* HotfixManager = Cast<UOnlineHotfixManager>(OnlineSub->GetNamedInterface(NAME_HotfixManager));
		if (HotfixManager == nullptr)
		{
			FString HotfixManagerClassName = DefaultObject->HotfixManagerClassName;
			UClass* HotfixManagerClass = LoadClass<UOnlineHotfixManager>(nullptr, *HotfixManagerClassName, nullptr, LOAD_None, nullptr);
			if (HotfixManagerClass == nullptr)
			{
				// Just use the default class if it couldn't load what was specified
				HotfixManagerClass = UOnlineHotfixManager::StaticClass();
			}
			// Create it and store it
			HotfixManager = NewObject<UOnlineHotfixManager>(GetTransientPackage(), HotfixManagerClass);
			OnlineSub->SetNamedInterface(NAME_HotfixManager, HotfixManager);
		}
		return HotfixManager;
	}
	return nullptr;
}

void UOnlineHotfixManager::Init()
{
	// Build the name of the loc file that we'll care about
	// It can change at runtime so build it just before fetching the data
	GameLocName = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() + TEXT("_Game.locres");
	OnlineTitleFile = Online::GetTitleFileInterface(OSSName.Len() ? FName(*OSSName, FNAME_Find) : NAME_None);
	if (OnlineTitleFile.IsValid())
	{
		OnEnumerateFilesCompleteDelegateHandle = OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesCompleteDelegate);
		OnReadFileCompleteDelegateHandle = OnlineTitleFile->AddOnReadFileCompleteDelegate_Handle(OnReadFileCompleteDelegate);
	}
}

void UOnlineHotfixManager::Cleanup()
{
	PendingHotfixFiles.Empty();
	if (OnlineTitleFile.IsValid())
	{
		// Make sure to give back the memory used when reading the hotfix files
		OnlineTitleFile->ClearFiles();
		OnlineTitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesCompleteDelegateHandle);
		OnlineTitleFile->ClearOnReadFileCompleteDelegate_Handle(OnReadFileCompleteDelegateHandle);
	}
	OnlineTitleFile = nullptr;
}

void UOnlineHotfixManager::StartHotfixProcess()
{
	// Patching the editor this way seems like a bad idea
	if (!IsRunningGame())
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing skipped due to IsRunningGame() == false"));
		TriggerHotfixComplete(EHotfixResult::SuccessNoChange);
		return;
	}

	Init();
	// Kick off an enumeration of the files that are available to download
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->EnumerateFiles();
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Failed to start the hotfixing process due to no OnlineTitleInterface present for OSS(%s)"), *OSSName);
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

void UOnlineHotfixManager::OnEnumerateFilesComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		check(OnlineTitleFile.IsValid());
		// Cache our current set so we can compare for differences
		LastHotfixFileList = HotfixFileList;
		HotfixFileList.Empty();
		// Get the new header data
		OnlineTitleFile->GetFileList(HotfixFileList);
		FilterHotfixFiles();
		// Sort after filtering so that the comparison below doesn't fail to different order from the server
		HotfixFileList.Sort();
		if (LastHotfixFileList.Num() == 0 || LastHotfixFileList != HotfixFileList)
		{
			UnmountHotfixFiles();
			ReadHotfixFiles();
		}
		else
		{
			UE_LOG(LogHotfixManager, Display, TEXT("Returned hotfix data is the same as last application, skipping the apply phase"));
			TriggerHotfixComplete(EHotfixResult::SuccessNoChange);
		}
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Enumeration of hotfix files failed"));
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

void UOnlineHotfixManager::FilterHotfixFiles()
{
	for (int32 Idx = 0; Idx < HotfixFileList.Num(); Idx++)
	{
		if (!WantsHotfixProcessing(HotfixFileList[Idx]))
		{
			HotfixFileList.RemoveAt(Idx, 1, false);
			Idx--;
		}
	}
}

void UOnlineHotfixManager::ReadHotfixFiles()
{
	if (HotfixFileList.Num())
	{
		check(OnlineTitleFile.IsValid());
		// Kick off a read for each file
		// Do this in two passes so already cached files don't trigger completion
		for (auto FileHeader : HotfixFileList)
		{
			PendingHotfixFiles.Add(FileHeader.DLName);
		}
		for (auto FileHeader : HotfixFileList)
		{
			OnlineTitleFile->ReadFile(FileHeader.DLName);
		}
	}
	else
	{
		UE_LOG(LogHotfixManager, Display, TEXT("No hotfix files need to be downloaded"));
		TriggerHotfixComplete(EHotfixResult::Success);
	}
}

void UOnlineHotfixManager::OnReadFileComplete(bool bWasSuccessful, const FString& FileName)
{
	if (PendingHotfixFiles.Contains(FileName))
	{
		if (bWasSuccessful)
		{
			UE_LOG(LogHotfixManager, Log, TEXT("Hotfix file (%s) downloaded"), *GetFriendlyNameFromDLName(FileName));
			PendingHotfixFiles.Remove(FileName);
			if (PendingHotfixFiles.Num() == 0)
			{
				// All files have been downloaded so now apply the files
				ApplyHotfix();
			}
		}
		else
		{
			UE_LOG(LogHotfixManager, Error, TEXT("Hotfix file (%s) failed to download"), *GetFriendlyNameFromDLName(FileName));
			TriggerHotfixComplete(EHotfixResult::Failed);
		}
	}
}

void UOnlineHotfixManager::ApplyHotfix()
{
	for (auto FileHeader : HotfixFileList)
	{
		if (!ApplyHotfixProcessing(FileHeader))
		{
			UE_LOG(LogHotfixManager, Error, TEXT("Couldn't apply hotfix file (%s)"), *FileHeader.FileName);
			TriggerHotfixComplete(EHotfixResult::Failed);
			return;
		}
	}
	UE_LOG(LogHotfixManager, Display, TEXT("Hotfix data has been successfully applied"));
	TriggerHotfixComplete(EHotfixResult::Success);
}

void UOnlineHotfixManager::TriggerHotfixComplete(EHotfixResult HotfixResult)
{
	TriggerOnHotfixCompleteDelegates(HotfixResult);
	if (HotfixResult == EHotfixResult::Failed)
	{
		HotfixFileList.Empty();
		UnmountHotfixFiles();
	}
	Cleanup();
}

bool UOnlineHotfixManager::WantsHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	const FString Extension = FPaths::GetExtension(FileHeader.FileName);
	if (Extension == TEXT("INI"))
	{
		return FileHeader.FileName.StartsWith(PlatformPrefix);
	}
	else if (Extension == TEXT("PAK"))
	{
		return FileHeader.FileName.StartsWith(PlatformPrefix);
	}
	return FileHeader.FileName == GameLocName;
}

bool UOnlineHotfixManager::ApplyHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	bool bSuccess = false;
	const FString Extension = FPaths::GetExtension(FileHeader.FileName);
	if (Extension == TEXT("INI"))
	{
		TArray<uint8> FileData;
		if (OnlineTitleFile->GetFileContents(FileHeader.DLName, FileData))
		{
			// Convert to a FString
			FileData.Add(0);
			const FString IniData = (ANSICHAR*)FileData.GetData();
			bSuccess = HotfixIniFile(FileHeader.FileName, IniData);
		}
	}
	else if (Extension == TEXT("LOCRES"))
	{
		HotfixLocFile(FileHeader);
	}
	else if (Extension == TEXT("PAK"))
	{
		bSuccess = HotfixPakFile(FileHeader);
	}
	OnlineTitleFile->ClearFile(FileHeader.FileName);
	return bSuccess;
}

FConfigFile* UOnlineHotfixManager::GetConfigFile(const FString& IniName)
{
	FString StrippedIniName;
	if (IniName.StartsWith(PlatformPrefix))
	{
		StrippedIniName = IniName.Right(IniName.Len() - PlatformPrefix.Len());
	}
	if (StrippedIniName.StartsWith(TEXT("Default")))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - 7);
	}
	FConfigFile* ConfigFile = nullptr;
	// Look for the first matching INI file entry
	for (TMap<FString, FConfigFile>::TIterator It(*GConfig); It; ++It)
	{
		if (It.Key().EndsWith(StrippedIniName))
		{
			ConfigFile = &It.Value();
			break;
		}
	}
	// If not found, add this file to the config cache
	if (ConfigFile == nullptr)
	{
		const FString IniNameWithPath = FPaths::GeneratedConfigDir() + StrippedIniName;
		ConfigFile = new FConfigFile();
		GConfig->SetFile(*IniNameWithPath, ConfigFile);
	}
	check(ConfigFile);
	// We never want to save these merged files
	ConfigFile->NoSave = true;
	return ConfigFile;
}

bool UOnlineHotfixManager::HotfixIniFile(const FString& FileName, const FString& IniData)
{
	FConfigFile* ConfigFile = GetConfigFile(FileName);
	// Merge the string into the config file
	ConfigFile->CombineFromBuffer(*IniData);
	TArray<UClass*> Classes;
	TArray<UObject*> PerObjectConfigObjects;
	int32 StartIndex = 0;
	int32 EndIndex = 0;
	// Find the set of object classes that were affected
	while (StartIndex >= 0 && StartIndex < IniData.Len() && EndIndex >= StartIndex)
	{
		// Find the next section header
		StartIndex = IniData.Find(TEXT("["), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
		if (StartIndex > -1)
		{
			// Find the ending section identifier
			EndIndex = IniData.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
			if (EndIndex > StartIndex)
			{
				int32 PerObjectNameIndex = IniData.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
				// Per object config entries will have a space in the name, but classes won't
				if (PerObjectNameIndex == -1)
				{
					if (IniData.StartsWith(TEXT("[/Script/"), ESearchCase::IgnoreCase))
					{
						const int32 ScriptSectionTag = 9;
						// Snip the text out and try to find the class for that
						const FString PackageClassName = IniData.Mid(StartIndex + ScriptSectionTag, EndIndex - StartIndex - ScriptSectionTag);
						// Find the class for this so we know what to update
						UClass* Class = FindObject<UClass>(nullptr, *PackageClassName, true);
						if (Class)
						{
							// Add this to the list to check against
							Classes.Add(Class);
						}
					}
				}
				// Handle the per object config case by finding the object for reload
				else
				{
					const FString PerObjectName = IniData.Mid(StartIndex + 1, PerObjectNameIndex - 1);
					// Explicitly search the transient package (won't update non-transient objects)
					UObject* PerObject = FindObject<UObject>(ANY_PACKAGE, *PerObjectName, false);
					if (PerObject != nullptr)
					{
						PerObjectConfigObjects.Add(PerObject);
					}
				}
				StartIndex = EndIndex;
			}
		}
	}

	int32 NumObjectsReloaded = 0;
	const double StartTime = FPlatformTime::Seconds();
	if (Classes.Num())
	{
		// Now that we have a list of classes to update, we can iterate objects and reload
		for (FObjectIterator It; It; ++It)
		{
			UClass* Class = It->GetClass();
			if (Class->HasAnyClassFlags(CLASS_Config))
			{
				// Check to see if this class is in our list (yes, potentially n^2, but not in practice)
				for (int32 ClassIndex = 0; ClassIndex < Classes.Num(); ClassIndex++)
				{
					if (It->IsA(Classes[ClassIndex]))
					{
						// Force a reload of the config vars
						It->ReloadConfig();
						NumObjectsReloaded++;
						break;
					}
				}
			}
		}
	}
	// Reload any PerObjectConfig objects that were affected
	for (auto ReloadObject : PerObjectConfigObjects)
	{
		ReloadObject->ReloadConfig();
		NumObjectsReloaded++;
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Updating config from %s took %f seconds and reloaded %d objects"),
		*FileName, FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
	return true;
}

void UOnlineHotfixManager::HotfixLocFile(const FCloudFileHeader& FileHeader)
{
	const double StartTime = FPlatformTime::Seconds();
	FString LocFilePath = FString::Printf(TEXT("%s/%s"), *GetCachedDirectory(), *FileHeader.DLName);
	FTextLocalizationManager::Get().UpdateFromLocalizationResource(LocFilePath);
	UE_LOG(LogHotfixManager, Log, TEXT("Updating loc from %s took %f seconds"), *FileHeader.FileName, FPlatformTime::Seconds() - StartTime);
}

bool UOnlineHotfixManager::HotfixPakFile(const FCloudFileHeader& FileHeader)
{
	if (!FCoreDelegates::OnMountPak.IsBound())
	{
		UE_LOG(LogHotfixManager, Error, TEXT("PAK file (%s) could not be mounted because OnMountPak is not bound"), *FileHeader.FileName);
		return false;
	}
	FString PakLocation = FString::Printf(TEXT("%s/%s"), *GetCachedDirectory(), *FileHeader.DLName);
	if (FCoreDelegates::OnMountPak.Execute(PakLocation, 0))
	{
		MountedPakFiles.Add(FileHeader.DLName);
		UE_LOG(LogHotfixManager, Log, TEXT("Hotfix mounted PAK file (%s)"), *FileHeader.FileName);
		// @todo joeg - Merge any INI files that are part of this PAK file into the config cache
		// Do we want/need this? Is it clear whether this will give us the right info?
		return true;
	}
	return false;
}

bool UOnlineHotfixManager::HotfixPakIniFile(const FString& FileName)
{
	FConfigFile* ConfigFile = GetConfigFile(FileName);
	ConfigFile->Combine(FileName);
	UE_LOG(LogHotfixManager, Log, TEXT("Hotfix merged INI (%s) found in a PAK file"), *FileName);

	FName IniFileName(*FileName, FNAME_Find);
	int32 NumObjectsReloaded = 0;
	const double StartTime = FPlatformTime::Seconds();
	// Now that we have a list of classes to update, we can iterate objects and
	// reload if they match the INI file that was changed
	for (FObjectIterator It; It; ++It)
	{
		UClass* Class = It->GetClass();
		if (Class->HasAnyClassFlags(CLASS_Config) &&
			Class->ClassConfigName == IniFileName)
		{
			// Force a reload of the config vars
			It->ReloadConfig();
			NumObjectsReloaded++;
		}
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Updating config from %s took %f seconds reloading %d objects"),
		*FileName, FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
	return true;
}

const FString UOnlineHotfixManager::GetFriendlyNameFromDLName(const FString& DLName) const
{
	for (auto Header : HotfixFileList)
	{
		if (Header.DLName == DLName)
		{
			return Header.FileName;
		}
	}
	return FString();
}

void UOnlineHotfixManager::UnmountHotfixFiles()
{
	// Unmount any hotfix files since we need to download them again
	for (auto PakFile : MountedPakFiles)
	{
		FCoreDelegates::OnUnmountPak.Execute(PakFile);
	}
	MountedPakFiles.Empty();
}
