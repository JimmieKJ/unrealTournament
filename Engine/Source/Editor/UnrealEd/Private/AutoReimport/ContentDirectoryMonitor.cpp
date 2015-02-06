// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "ContentDirectoryMonitor.h"

#include "AssetRegistryModule.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "ReimportFeedbackContext.h"

#define LOCTEXT_NAMESPACE "ContentDirectoryMonitor"

/** Generate a config from the specified options, to pass to FFileCache on construction */
FFileCacheConfig GenerateFileCacheConfig(const FString& InPath, const FString& InSupportedExtensions, const FString& InMountedContentPath)
{
	FString Directory = FPaths::ConvertRelativePathToFull(InPath);

	const FString& HashString = InMountedContentPath.IsEmpty() ? Directory : InMountedContentPath;
	const uint32 CRC = FCrc::MemCrc32(*HashString, HashString.Len()*sizeof(TCHAR));	
	FString CacheFilename = FPaths::ConvertRelativePathToFull(FPaths::GameIntermediateDir()) / TEXT("ReimportCache") / FString::Printf(TEXT("%u.bin"), CRC);

	FFileCacheConfig Config(MoveTemp(Directory), MoveTemp(CacheFilename));
	Config.IncludeExtensions = InSupportedExtensions;
	// We always store paths inside content folders relative to the folder
	Config.PathType = EPathType::Relative;

	return Config;
}

FContentDirectoryMonitor::FContentDirectoryMonitor(const FString& InDirectory, const FString& InSupportedExtensions, const FString& InMountedContentPath)
	: Cache(GenerateFileCacheConfig(InDirectory, InSupportedExtensions, InMountedContentPath))
	, MountedContentPath(InMountedContentPath)
	, LastSaveTime(0)
{
	
}

void FContentDirectoryMonitor::Destroy()
{
	Cache.Destroy();
}

void FContentDirectoryMonitor::Tick(const FTimeLimit& TimeLimit)
{
	Cache.Tick(TimeLimit);

	const double Now = FPlatformTime::Seconds();
	if (Now - LastSaveTime > ResaveIntervalS)
	{
		LastSaveTime = Now;
		Cache.WriteCache();
	}
}

void FContentDirectoryMonitor::StartProcessing()
{
	WorkProgress = TotalWork = 0;

	auto OutstandingChanges = Cache.GetOutstandingChanges();
	if (OutstandingChanges.Num() != 0)
	{
		const auto* Settings = GetDefault<UEditorLoadingSavingSettings>();

		for (auto& Transaction : OutstandingChanges)
		{
			switch(Transaction.Action)
			{
				case FFileChangeData::FCA_Added:
					if (Settings->bAutoCreateAssets && !MountedContentPath.IsEmpty())
					{
						AddedFiles.Emplace(MoveTemp(Transaction));
					}
					else
					{
						Cache.CompleteTransaction(MoveTemp(Transaction));
					}
					break;

				case FFileChangeData::FCA_Modified:
					ModifiedFiles.Emplace(MoveTemp(Transaction));
					break;

				case FFileChangeData::FCA_Removed:
					if (Settings->bAutoDeleteAssets && !MountedContentPath.IsEmpty())
					{
						DeletedFiles.Emplace(MoveTemp(Transaction));
					}
					else
					{
						Cache.CompleteTransaction(MoveTemp(Transaction));
					}
					break;
			}
		}

		TotalWork = AddedFiles.Num() + ModifiedFiles.Num() + DeletedFiles.Num();
	}
}

void FContentDirectoryMonitor::ProcessAdditions(TArray<UPackage*>& OutPackagesToSave, const FTimeLimit& TimeLimit, const TMap<FString, TArray<UFactory*>>& InFactoriesByExtension, FReimportFeedbackContext& Context)
{
	bool bCancelled = false;
	for (int32 Index = 0; Index < AddedFiles.Num(); ++Index)
	{
		auto& Addition = AddedFiles[Index];

		WorkProgress += 1;

		if (bCancelled)
		{
			// Just update the cache immediately if the user cancelled
			Cache.CompleteTransaction(MoveTemp(Addition));
			continue;
		}

		const FString FullFilename = Cache.GetDirectory() + Addition.Filename.Get();

		FString NewAssetName = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(FullFilename));
		FString PackagePath = PackageTools::SanitizePackageName(MountedContentPath / FPaths::GetPath(Addition.Filename.Get()) / NewAssetName);

		if (FEditorFileUtils::IsMapPackageAsset(PackagePath))
		{
			Context.AddMessage(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_ExistingMap", "Attempted to create asset with the same name as a map ({0})."), FText::FromString(PackagePath)));
		}
		else if (FindPackage(nullptr, *PackagePath))
		{
			Context.AddMessage(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_ExistingAsset", "Can't create a new asset at {0} - one already exists."), FText::FromString(PackagePath)));
		}
		else
		{
			UPackage* NewPackage = CreatePackage(nullptr, *PackagePath);
			if ( !ensure(NewPackage) )
			{
				Context.AddMessage(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_FailedToCreateAsset", "Failed to create new asset ({0}) for file ({1})."), FText::FromString(NewAssetName), FText::FromString(FullFilename)));
			}
			else
			{
				// Make sure the destination package is loaded
				NewPackage->FullyLoad();

				bool bSuccess = false;
				
				// Find a relevant factory for this file
				const FString Ext = FPaths::GetExtension(Addition.Filename.Get(), false);
				UFactory* FactoryType = nullptr;
				if (auto* Factories = InFactoriesByExtension.Find(Ext))
				{
					FactoryType = (*Factories)[0];

					UFactory* const* FoundCompatibleFactoryType = Factories->FindByPredicate([&](UFactory* InFactory){
						return InFactory->FactoryCanImport(FullFilename);
					});

					if (FoundCompatibleFactoryType)
					{
						FactoryType = *FoundCompatibleFactoryType;
					}
				}

				UObject* NewAsset = nullptr;
				UFactory* FactoryInstance = FactoryType ? ConstructObject<UFactory>(FactoryType->GetClass()) : nullptr;
				if (FactoryInstance)
				{
					FactoryInstance->AddToRoot();
					if (FactoryInstance->ConfigureProperties())
					{
						UClass* ImportAssetType = FactoryInstance->SupportedClass;
						NewAsset = UFactory::StaticImportObject(ImportAssetType, NewPackage, FName(*NewAssetName), RF_Public | RF_Standalone, bCancelled, *FullFilename, nullptr, FactoryInstance);
					}
					FactoryInstance->RemoveFromRoot();
				}

				if (!bCancelled)
				{
					if (!NewAsset)
					{
						TArray<UPackage*> Packages;
						Packages.Add(NewPackage);

						FText ErrorMessage;
						if (!PackageTools::UnloadPackages(Packages, ErrorMessage))
						{
							Context.AddMessage(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_UnloadingPackage", "There was an error unloading a package: {0}."), ErrorMessage));
						}						

						Context.AddMessage(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_FailedToImportAsset", "Failed to import file {0}."), FText::FromString(FullFilename)));
						continue;
					}

					Context.AddMessage(EMessageSeverity::Info, FText::Format(LOCTEXT("Success_CreatedNewAsset", "Created new asset {0}."), FText::FromString(PackagePath)));

					FAssetRegistryModule::AssetCreated(NewAsset);
					GEditor->BroadcastObjectReimported(NewAsset);

					OutPackagesToSave.Add(NewPackage);
				}

				// Refresh the supported class.  Some factories (e.g. FBX) only resolve their type after reading the file
				// ImportAssetType = Factory->ResolveSupportedClass();
				// @todo: analytics
			}
		}

		// Let the cache know that we've dealt with this change (it will be imported immediately)
		Cache.CompleteTransaction(MoveTemp(Addition));

		if (!bCancelled && TimeLimit.Exceeded())
		{
			// Remove the ones we've processed
			AddedFiles.RemoveAt(0, Index + 1);
			return;
		}
	}

	AddedFiles.Empty();
}

void FContentDirectoryMonitor::ProcessModifications(const IAssetRegistry& Registry, const FTimeLimit& TimeLimit, FReimportFeedbackContext& Context)
{
	auto* ReimportManager = FReimportManager::Instance();

	for (int32 Index = 0; Index < ModifiedFiles.Num(); ++Index)
	{
		auto& Change = ModifiedFiles[Index];

		const FString FullFilename = Cache.GetDirectory() + Change.Filename.Get();

		for (const auto& AssetData : Utils::FindAssetsPertainingToFile(Registry, FullFilename))
		{
			UObject* Asset = AssetData.GetAsset();
			if (!ReimportManager->Reimport(Asset, false /* Ask for new file */, false /* Show notification */))
			{
				Context.AddMessage(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_FailedToReimportAsset", "Failed to reimport asset {0}."), FText::FromString(Asset->GetName())));
			}
			else
			{
				Context.AddMessage(EMessageSeverity::Info, FText::Format(LOCTEXT("Success_CreatedNewAsset", "Reimported asset {0} from {1}."), FText::FromString(Asset->GetName()), FText::FromString(FullFilename)));
			}
		}

		// Let the cache know that we've dealt with this change
		Cache.CompleteTransaction(MoveTemp(Change));
		
		WorkProgress += 1;
		if (TimeLimit.Exceeded())
		{
			ModifiedFiles.RemoveAt(0, Index + 1);
			return;
		}
	}

	ModifiedFiles.Empty();
}

void FContentDirectoryMonitor::ExtractAssetsToDelete(const IAssetRegistry& Registry, TArray<FAssetData>& OutAssetsToDelete)
{
	for (auto& Deletion : DeletedFiles)
	{
		for (const auto& AssetData : Utils::FindAssetsPertainingToFile(Registry, Cache.GetDirectory() + Deletion.Filename.Get()))
		{
			OutAssetsToDelete.Add(AssetData);
		}

		// Let the cache know that we've dealt with this change (it will be imported in due course)
		Cache.CompleteTransaction(MoveTemp(Deletion));
	}

	WorkProgress += DeletedFiles.Num();
	DeletedFiles.Empty();
}

#undef LOCTEXT_NAMESPACE
