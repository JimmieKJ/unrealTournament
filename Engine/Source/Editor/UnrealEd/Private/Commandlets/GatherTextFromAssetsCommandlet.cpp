// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "PackageTools.h"
#include "ARFilter.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "PackageHelperFunctions.h"
#include "InternationalizationMetadata.h"
#include "Sound/DialogueWave.h"
#include "Sound/DialogueVoice.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromAssetsCommandlet, Log, All);

#define LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
//UGatherTextFromAssetsCommandlet

const FString UGatherTextFromAssetsCommandlet::UsageText
	(
	TEXT("GatherTextFromAssetsCommandlet usage...\r\n")
	TEXT("    <GameName> UGatherTextFromAssetsCommandlet -root=<parsed code root folder> -exclude=<paths to exclude>\r\n")
	TEXT("    \r\n")
	TEXT("    <paths to include> Paths to include. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, everything will be included. \r\n")
	TEXT("    <paths to exclude> Paths to exclude. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, nothing will be excluded.\r\n")
	);

UGatherTextFromAssetsCommandlet::UGatherTextFromAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

void UGatherTextFromAssetsCommandlet::ProcessGatherableTextDataArray(const FString& PackageFilePath, const TArray<FGatherableTextData>& GatherableTextDataArray)
{
	for (const FGatherableTextData& GatherableTextData : GatherableTextDataArray)
	{
		for (const FTextSourceSiteContext& TextSourceSiteContext : GatherableTextData.SourceSiteContexts)
		{
			const FString KeyName = TextSourceSiteContext.KeyName;

			// Find existing entry from manifest or manifest dependencies.
			FContext Context;
			Context.Key = TextSourceSiteContext.KeyName;
			const FLocMetadataObject DefaultMetadataObject;
			Context.KeyMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.KeyMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.KeyMetaData)) : nullptr;
			Context.InfoMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.InfoMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.InfoMetaData)) : nullptr;
			Context.bIsOptional = TextSourceSiteContext.IsOptional;
			Context.SourceLocation = TextSourceSiteContext.SiteDescription;

			TSharedPtr< FManifestEntry > ExistingEntry = ManifestInfo->GetManifest()->FindEntryByContext( GatherableTextData.NamespaceName, Context );

			if (!ExistingEntry.IsValid())
			{
				FString FileInfo;
				ExistingEntry = ManifestInfo->FindDependencyEntryByContext( GatherableTextData.NamespaceName, Context, FileInfo );
			}

			FConflictTracker::FEntry NewEntry;
			NewEntry.PathToSourceString = TextSourceSiteContext.SiteDescription;
			NewEntry.SourceString = MakeShareable(new FString(GatherableTextData.SourceData.SourceString));
			NewEntry.Status = EAssetTextGatherStatus::None;

			if (TextSourceSiteContext.KeyName.IsEmpty())
			{
				NewEntry.Status = EAssetTextGatherStatus::MissingKey;
			}

			// Entry already exists, check for conflict.
			if (ExistingEntry.IsValid() && ExistingEntry->Source.Text != ( NewEntry.SourceString.IsValid() ? **(NewEntry.SourceString) : TEXT("") ))
			{
				NewEntry.Status = EAssetTextGatherStatus::IdentityConflict;
			}

			// Gather if it requires gathering and isn't in a bad state.
			if ((NewEntry.Status & EAssetTextGatherStatus::BadBitMask) == false)
			{
				if (!TextSourceSiteContext.IsEditorOnly || ShouldGatherFromEditorOnlyData)
				{
				ManifestInfo->AddEntry(TextSourceSiteContext.SiteDescription, GatherableTextData.NamespaceName, NewEntry.SourceString.Get() ? *(NewEntry.SourceString) : TEXT(""), Context );
			}
			}

			// Add to conflict tracker.
			FConflictTracker::FKeyTable& KeyTable = ConflictTracker.Namespaces.FindOrAdd(GatherableTextData.NamespaceName);
			FConflictTracker::FEntryArray& EntryArray = KeyTable.FindOrAdd(TextSourceSiteContext.KeyName);
			EntryArray.Add(NewEntry);
		}
	}
}

void UGatherTextFromAssetsCommandlet::ProcessPackages( const TArray< UPackage* >& PackagesToProcess )
{
	for(UPackage* const Package : PackagesToProcess)
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, true, RF_Transient | RF_PendingKill);
		for (UObject* const Object : ObjectsInPackage)
		{
			TArray<FGatherableTextData> GatherableTextDataArray;

			FPropertyLocalizationDataGatherer PropertyLocalizationDataGatherer(GatherableTextDataArray);
			PropertyLocalizationDataGatherer.GatherLocalizationDataFromPropertiesOfDataStructure(Object->GetClass(), Object);

				for(UClass* Class = Object->GetClass(); Class != nullptr; Class = Class->GetSuperClass())
				{
					UPackage::FLocalizationDataGatheringCallback* const CustomCallback = UPackage::GetTypeSpecificLocalizationDataGatheringCallbacks().Find(Class);
					if (CustomCallback)
					{
						(*CustomCallback)(Object, GatherableTextDataArray);
					}
				}

				ProcessGatherableTextDataArray(Package->FileName.ToString(), GatherableTextDataArray);
			}
		}
}

int32 UGatherTextFromAssetsCommandlet::Main(const FString& Params)
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	//Set config file
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	//Set config section
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	//Modules to Preload
	TArray<FString> ModulesToPreload;
	GetStringArrayFromConfig(*SectionName, TEXT("ModulesToPreload"), ModulesToPreload, GatherTextConfigPath);

	for (const FString& ModuleName : ModulesToPreload)
	{
		FModuleManager::Get().LoadModule(*ModuleName);
	}

	// IncludePathFilters
	TArray<FString> IncludePathFilters;
	GetPathArrayFromConfig(*SectionName, TEXT("IncludePathFilters"), IncludePathFilters, GatherTextConfigPath);

	// IncludePaths (DEPRECATED)
	{
		TArray<FString> IncludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);
		if (IncludePaths.Num())
		{
			IncludePathFilters.Append(IncludePaths);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("IncludePaths detected in section %s. IncludePaths is deprecated, please use IncludePathFilters."), *SectionName);
		}
	}

	if (IncludePathFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No include path filters in section %s."), *SectionName);
		return -1;
	}

	// ExcludePathFilters
	TArray<FString> ExcludePathFilters;
	GetPathArrayFromConfig(*SectionName, TEXT("ExcludePathFilters"), ExcludePathFilters, GatherTextConfigPath);

	// ExcludePaths (DEPRECATED)
	{
		TArray<FString> ExcludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);
		if (ExcludePaths.Num())
		{
			ExcludePathFilters.Append(ExcludePaths);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("ExcludePaths detected in section %s. ExcludePaths is deprecated, please use ExcludePathFilters."), *SectionName);
		}
	}

	// PackageNameFilters
	TArray<FString> PackageFileNameFilters;
	GetStringArrayFromConfig(*SectionName, TEXT("PackageFileNameFilters"), PackageFileNameFilters, GatherTextConfigPath);

	// PackageExtensions (DEPRECATED)
	{
		TArray<FString> PackageExtensions;
		GetStringArrayFromConfig(*SectionName, TEXT("PackageExtensions"), PackageExtensions, GatherTextConfigPath);
		if (PackageExtensions.Num())
		{
			PackageFileNameFilters.Append(PackageExtensions);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("PackageExtensions detected in section %s. PackageExtensions is deprecated, please use PackageFileNameFilters."), *SectionName);
		}
	}

	if (PackageFileNameFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No package file name filters in section %s."), *SectionName);
		return -1;
	}

	//asset class exclude
	TArray<FString> ExcludeClasses;
	GetStringArrayFromConfig(*SectionName, TEXT("ExcludeClasses"), ExcludeClasses, GatherTextConfigPath);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().SearchAllAssets( true );
	FARFilter Filter;

	for(const auto& ExcludeClass : ExcludeClasses)
	{
		UClass* FilterClass = FindObject<UClass>(ANY_PACKAGE, *ExcludeClass);
		if(FilterClass)
		{
			Filter.ClassNames.Add( FilterClass->GetFName() );
		}
		else
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Invalid exclude class %s"), *ExcludeClass);
		}
	}

	TArray<FAssetData> AssetDataArray;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);

	FString UAssetPackageExtension = FPackageName::GetAssetPackageExtension();
	TSet< FString > LongPackageNamesToExclude;
	for (int Index = 0; Index < AssetDataArray.Num(); Index++)
	{
		LongPackageNamesToExclude.Add( FPackageName::LongPackageNameToFilename( AssetDataArray[Index].PackageName.ToString(), UAssetPackageExtension ) );
	}

	//Get whether we should fix broken properties that we find.
	if (!GetBoolFromConfig(*SectionName, TEXT("bFixBroken"), bFixBroken, GatherTextConfigPath))
	{
		bFixBroken = false;
	}

	// Get whether we should gather editor-only data. Typically only useful for the localization of UE4 itself.
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldGatherFromEditorOnlyData"), ShouldGatherFromEditorOnlyData, GatherTextConfigPath))
	{
		ShouldGatherFromEditorOnlyData = false;
	}

	// Add any manifest dependencies if they were provided
	TArray<FString> ManifestDependenciesList;
	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);

	if( !ManifestInfo->AddManifestDependencies( ManifestDependenciesList ) )
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("The GatherTextFromAssets commandlet couldn't find all the specified manifest dependencies."));
		return -1;
	}

	//The main array of files to work from.
	TArray< FString > PackageFileNamesToProcess;

	TArray<FString> PackageFilesNotInIncludePath;
	TArray<FString> PackageFilesInExcludePath;
	TArray<FString> PackageFilesExcludedByClass;

	//Fill the list of packages to work from.
	uint8 PackageFilter = NORMALIZE_DefaultFlags;
	TArray<FString> Unused;	
	for ( int32 PackageFilenameWildcardIdx = 0; PackageFilenameWildcardIdx < PackageFileNameFilters.Num(); PackageFilenameWildcardIdx++ )
	{
		const bool IsAssetPackage = PackageFileNameFilters[PackageFilenameWildcardIdx] == ( FString( TEXT("*") )+ FPackageName::GetAssetPackageExtension() );

		TArray<FString> PackageFiles;
		if ( !NormalizePackageNames( Unused, PackageFiles, PackageFileNameFilters[PackageFilenameWildcardIdx], PackageFilter) )
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("No packages found with extension %i: '%s'"), PackageFilenameWildcardIdx, *PackageFileNameFilters[PackageFilenameWildcardIdx]);
			continue;
		}
		else
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Found %i packages with extension %i: '%s'"), PackageFiles.Num(), PackageFilenameWildcardIdx, *PackageFileNameFilters[PackageFilenameWildcardIdx]);
		}

		//Run through all the files found and add any that pass the include, exclude and filter constraints to OrderedPackageFilesToLoad
		for (FString& PackageFile : PackageFiles)
		{
			PackageFile = FPaths::ConvertRelativePathToFull(PackageFile);

			bool bExclude = false;
			//Ensure it matches the include paths if there are some.
			for (FString& IncludePath : IncludePathFilters)
			{
				bExclude = true;
				if( PackageFile.MatchesWildcard(IncludePath) )
				{
					bExclude = false;
					break;
				}
			}

			if ( bExclude )
			{
				PackageFilesNotInIncludePath.Add(PackageFile);
			}

			//Ensure it does not match the exclude paths if there are some.
			for (const FString& ExcludePath : ExcludePathFilters)
			{
				if (PackageFile.MatchesWildcard(ExcludePath))
				{
					bExclude = true;
					PackageFilesInExcludePath.Add(PackageFile);
					break;
				}
			}

			//Check that this is not on the list of packages that we don't care about e.g. textures.
			if ( !bExclude && IsAssetPackage && LongPackageNamesToExclude.Contains( PackageFile ) )
			{
				bExclude = true;
				PackageFilesExcludedByClass.Add(PackageFile);
			}

			//If we haven't failed one of the above checks, add it to the array of packages to process.
			if(!bExclude)
			{
				PackageFileNamesToProcess.Add(PackageFile);
			}
		}
	}

	if ( PackageFileNamesToProcess.Num() == 0 )
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("No files found or none passed the include/exclude criteria."));
	}

	TArray< FString > PackageFileNamesToLoad;
	for (FString& PackageFile : PackageFileNamesToProcess)
	{
		TScopedPointer< FArchive > FileReader( IFileManager::Get().CreateFileReader( *PackageFile ) );
		if( FileReader )
		{
			// Read package file summary from the file
			FPackageFileSummary PackageFileSummary;
			(*FileReader) << PackageFileSummary;

			bool MustLoadForGather = false;

			// Packages not resaved since localization gathering flagging was added to packages must be loaded.
			if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_PACKAGE_REQUIRES_LOCALIZATION_GATHER_FLAGGING)
			{
				MustLoadForGather = true;
			}
			// Package not resaved since gatherable text data was added to package headers must be loaded, since their package header won't contain pregathered text data.
			else if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
			{
				// Fallback on the old package flag check.
				if (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather)
				{
					MustLoadForGather = true;
				}
			}
			else if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_DIALOGUE_WAVE_NAMESPACE_AND_CONTEXT_CHANGES)
			{
				IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
				TArray<FAssetData> AssetDataInPackage;
				AssetRegistry.GetAssetsByPackageName(*FPackageName::FilenameToLongPackageName(PackageFile), AssetDataInPackage);
				for (const FAssetData& AssetData : AssetDataInPackage)
				{
					if (AssetData.AssetClass == UDialogueWave::StaticClass()->GetFName())
					{
						MustLoadForGather = true;
					}
				}
			}
				 
			// Add package to list of packages to load fully and process.
			if (MustLoadForGather)
			{
				PackageFileNamesToLoad.Add(PackageFile);
			}
			// Process immediately packages that don't require loading to process.
			else
			{
				TArray<FGatherableTextData> GatherableTextDataArray;

				if (PackageFileSummary.GatherableTextDataOffset > 0)
				{
					FileReader->Seek(PackageFileSummary.GatherableTextDataOffset);

					for(int32 i = 0; i < PackageFileSummary.GatherableTextDataCount; ++i)
					{
						FGatherableTextData* GatherableTextData = new(GatherableTextDataArray) FGatherableTextData;
						(*FileReader) << *GatherableTextData;
					}

					ProcessGatherableTextDataArray(PackageFile, GatherableTextDataArray);
				}
			}
		}
	}

	CollectGarbage( RF_Native );

	//Now go through the remaining packages in the main array and process them in batches.
	int32 PackagesPerBatchCount = 100;
	TArray< UPackage* > LoadedPackages;
	TArray< FString > LoadedPackageFileNames;
	TArray< FString > FailedPackageFileNames;
	TArray< UPackage* > LoadedPackagesToProcess;

	const int32 PackageCount = PackageFileNamesToLoad.Num();
	const int32 BatchCount = PackageCount / PackagesPerBatchCount + (PackageCount % PackagesPerBatchCount > 0 ? 1 : 0); // Add an extra batch for any remainder if necessary
	if(PackageCount > 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Loading %i packages in %i batches of %i."), PackageCount, BatchCount, PackagesPerBatchCount);
	}

	//Load the packages in batches
	int32 PackageIndex = 0;
	for( int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex )
	{
		int32 PackagesInThisBatch = 0;
		int32 FailuresInThisBatch = 0;
		for( ; PackageIndex < PackageCount && PackagesInThisBatch < PackagesPerBatchCount; ++PackageIndex )
		{
			FString PackageFileName = PackageFileNamesToLoad[PackageIndex];

			UPackage *Package = LoadPackage( NULL, *PackageFileName, LOAD_None );
			if( Package )
			{
				LoadedPackages.Add(Package);
				LoadedPackageFileNames.Add(PackageFileName);

				// Because packages may not have been resaved after this flagging was implemented, we may have added packages to load that weren't flagged - potential false positives.
				// The loading process should have reflagged said packages so that only true positives will have this flag.
				if( Package->RequiresLocalizationGather() )
				{
					LoadedPackagesToProcess.Add( Package );
				}
			}
			else
			{
				FailedPackageFileNames.Add( PackageFileName );
				++FailuresInThisBatch;
				continue;
			}

			++PackagesInThisBatch;
		}

		UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Loaded %i packages in batch %i of %i. %i failed."), PackagesInThisBatch, BatchIndex + 1, BatchCount, FailuresInThisBatch);

		ProcessPackages(LoadedPackagesToProcess);
		LoadedPackagesToProcess.Empty(PackagesPerBatchCount);

		if( bFixBroken )
		{
			for( int32 LoadedPackageIndex=0; LoadedPackageIndex < LoadedPackages.Num() ; ++LoadedPackageIndex )
			{
				UPackage *Package = LoadedPackages[LoadedPackageIndex];
				const FString PackageName = LoadedPackageFileNames[LoadedPackageIndex];

				//Todo - link with source control.
				if( Package )
				{
					if( Package->IsDirty() )
					{
						if( SavePackageHelper( Package, *PackageName ) )
						{
							UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Saved Package %s."),*PackageName);
						}
						else
						{
							//TODO - Work out how to integrate with source control. The code from the source gatherer doesn't work.
							UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Could not save package %s. Probably due to source control. "),*PackageName);
						}
					}
				}
				else
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Failed to find one of the loaded packages."));
				}
			}
		}

		CollectGarbage( RF_Native );
		LoadedPackages.Empty(PackagesPerBatchCount);	
		LoadedPackageFileNames.Empty(PackagesPerBatchCount);
	}

	for(auto i = ConflictTracker.Namespaces.CreateConstIterator(); i; ++i)
	{
		const FString& NamespaceName = i.Key();
		const FConflictTracker::FKeyTable& KeyTable = i.Value();
		for(auto j = KeyTable.CreateConstIterator(); j; ++j)
		{
			const FString& KeyName = j.Key();
			const FConflictTracker::FEntryArray& EntryArray = j.Value();

			for(int k = 0; k < EntryArray.Num(); ++k)
			{
				const FConflictTracker::FEntry& Entry = EntryArray[k];
				switch(Entry.Status)
				{
				case EAssetTextGatherStatus::MissingKey:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Detected missing key on asset \"%s\"."), *Entry.PathToSourceString);
					}
					break;
				case EAssetTextGatherStatus::MissingKey_Resolved:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Fixed missing key on asset \"%s\"."), *Entry.PathToSourceString);
					}
					break;
				case EAssetTextGatherStatus::IdentityConflict:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Detected duplicate identity with differing source on asset \"%s\"."), *Entry.PathToSourceString);
					}
					break;
				case EAssetTextGatherStatus::IdentityConflict_Resolved:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Fixed duplicate identity with differing source on asset \"%s\"."), *Entry.PathToSourceString);
					}
					break;
				}
			}
		}
	}

	return 0;
}

#undef LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
