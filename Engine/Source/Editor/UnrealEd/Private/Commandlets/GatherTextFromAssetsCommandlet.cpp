// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
#include "EditorObjectVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromAssetsCommandlet, Log, All);

/** Special feedback context used to stop the commandlet to reporting failure due to a package load error */
class FLoadPackageLogOutputRedirector : public FFeedbackContext
{
public:
	struct FScopedCapture
	{
		FScopedCapture(FLoadPackageLogOutputRedirector* InLogOutputRedirector, const FString& InPackageContext)
			: LogOutputRedirector(InLogOutputRedirector)
		{
			LogOutputRedirector->BeginCapturingLogData(InPackageContext);
		}

		~FScopedCapture()
		{
			LogOutputRedirector->EndCapturingLogData();
		}

		FLoadPackageLogOutputRedirector* LogOutputRedirector;
	};

	FLoadPackageLogOutputRedirector()
		: ErrorCount(0)
		, WarningCount(0)
		, FormattedErrorsAndWarningsList()
		, PackageContext()
		, OriginalWarningContext(nullptr)
	{
	}

	virtual ~FLoadPackageLogOutputRedirector()
	{
	}

	void BeginCapturingLogData(const FString& InPackageContext)
	{
		// Override GWarn so that we can capture any log data
		check(!OriginalWarningContext);
		OriginalWarningContext = GWarn;
		GWarn = this;

		PackageContext = InPackageContext;

		// Reset the counts and previous log output
		ErrorCount = 0;
		WarningCount = 0;
		FormattedErrorsAndWarningsList.Reset();
	}

	void EndCapturingLogData()
	{
		// Restore the original GWarn now that we've finished capturing log data
		check(OriginalWarningContext);
		GWarn = OriginalWarningContext;
		OriginalWarningContext = nullptr;

		// Report any messages, and also report a warning if we silenced some warnings or errors when loading
		if (ErrorCount > 0 || WarningCount > 0)
		{
			static const FString LogIndentation = TEXT("    ");

			UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Package '%s' produced %d error(s) and %d warning(s) while loading. Please verify that your text has gathered correctly."), *PackageContext, ErrorCount, WarningCount);
			
			GWarn->Log(NAME_None, ELogVerbosity::Log, FString::Printf(TEXT("The following errors and warnings were reported while loading '%s':"), *PackageContext));
			for (const auto& FormattedOutput : FormattedErrorsAndWarningsList)
			{
				GWarn->Log(NAME_None, ELogVerbosity::Log, LogIndentation + FormattedOutput);
			}
		}
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Verbosity == ELogVerbosity::Error)
		{
			++ErrorCount;
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V));
		}
		else if (Verbosity == ELogVerbosity::Warning)
		{
			++WarningCount;
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V));
		}
		else
		{
			// Pass anything else on to GWarn so that it can handle them appropriately
			OriginalWarningContext->Serialize(V, Verbosity, Category);
		}
	}

private:
	int32 ErrorCount;
	int32 WarningCount;
	TArray<FString> FormattedErrorsAndWarningsList;

	FString PackageContext;
	FFeedbackContext* OriginalWarningContext;
};

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
			if (!TextSourceSiteContext.IsEditorOnly || ShouldGatherFromEditorOnlyData)
			{
				if (TextSourceSiteContext.KeyName.IsEmpty())
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Detected missing key on asset \"%s\"."), *TextSourceSiteContext.SiteDescription);
					continue;
				}

				static const FLocMetadataObject DefaultMetadataObject;

				FManifestContext Context;
				Context.Key = TextSourceSiteContext.KeyName;
				Context.KeyMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.KeyMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.KeyMetaData)) : nullptr;
				Context.InfoMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.InfoMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.InfoMetaData)) : nullptr;
				Context.bIsOptional = TextSourceSiteContext.IsOptional;
				Context.SourceLocation = TextSourceSiteContext.SiteDescription;

				FLocItem Source(GatherableTextData.SourceData.SourceString);

				GatherManifestHelper->AddSourceText(GatherableTextData.NamespaceName, Source, Context, &TextSourceSiteContext.SiteDescription);
			}
		}
	}
}

void UGatherTextFromAssetsCommandlet::ProcessPackages( const TArray< UPackage* >& PackagesToProcess )
{
	TArray<FGatherableTextData> GatherableTextDataArray;

	for(const UPackage* const Package : PackagesToProcess)
	{
		GatherableTextDataArray.Reset();

		// Gathers from the given package
		EPropertyLocalizationGathererResultFlags GatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;
		FPropertyLocalizationDataGatherer(GatherableTextDataArray, Package, GatherableTextResultFlags);

		ProcessGatherableTextDataArray(Package->FileName.ToString(), GatherableTextDataArray);
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

	for (const FString& ManifestDependency : ManifestDependenciesList)
	{
		FText OutError;
		if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("The GatherTextFromAssets commandlet couldn't load the specified manifest dependency: '%'. %s"), *ManifestDependency, *OutError.ToString());
			return -1;
		}
	}

	//The main array of files to work from.
	TArray< FString > PackageFileNamesToProcess;

	TArray<FString> PackageFilesNotInIncludePath;
	TArray<FString> PackageFilesInExcludePath;
	TArray<FString> PackageFilesExcludedByClass;

	const FFuzzyPathMatcher FuzzyPathMatcher = FFuzzyPathMatcher(IncludePathFilters, ExcludePathFilters);

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

			//Check to see that this package is part of a valid gather path
			const FFuzzyPathMatcher::EPathMatch PathMatch = FuzzyPathMatcher.TestPath(PackageFile);
			switch (PathMatch)
			{
			case FFuzzyPathMatcher::Included:
				break;

			case FFuzzyPathMatcher::Excluded:
				bExclude = true;
				PackageFilesInExcludePath.Add(PackageFile);
				break;

			case FFuzzyPathMatcher::NoMatch:
				bExclude = true;
				PackageFilesNotInIncludePath.Add(PackageFile);
				break;

			default:
				break;
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

	bool bSkipGatherCache = FParse::Param(FCommandLine::Get(), TEXT("SkipGatherCache"));
	if (!bSkipGatherCache)
	{
		GetBoolFromConfig(*SectionName, TEXT("SkipGatherCache"), bSkipGatherCache, GatherTextConfigPath);
	}
	UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("SkipGatherCache: %s"), bSkipGatherCache ? TEXT("true") : TEXT("false"));

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

			// Have we been asked to skip the cache of text that exists in the header of newer packages?
			if (bSkipGatherCache && PackageFileSummary.GetFileVersionUE4() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
			{
				// Fallback on the old package flag check.
				if (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather)
				{
					MustLoadForGather = true;
				}
			}

			const FCustomVersion* const EditorVersion = PackageFileSummary.GetCustomVersionContainer().GetVersion(FEditorObjectVersion::GUID);

			// Packages not resaved since localization gathering flagging was added to packages must be loaded.
			if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_PACKAGE_REQUIRES_LOCALIZATION_GATHER_FLAGGING)
			{
				MustLoadForGather = true;
			}
			// Package not resaved since gatherable text data was added to package headers must be loaded, since their package header won't contain pregathered text data.
			else if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_SERIALIZE_TEXT_IN_PACKAGES || (!EditorVersion || EditorVersion->Version < FEditorObjectVersion::GatheredTextPackageCacheFixesV2))
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
			
			// If this package doesn't have any cached data, then we have to load it for gather
			if (PackageFileSummary.GetFileVersionUE4() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES && PackageFileSummary.GatherableTextDataOffset == 0 && (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather))
			{
				MustLoadForGather = true;
			}

			// Add package to list of packages to load fully and process.
			if (MustLoadForGather)
			{
				PackageFileNamesToLoad.Add(PackageFile);
			}
			// Process immediately packages that don't require loading to process.
			else if (PackageFileSummary.GatherableTextDataOffset > 0)
			{
				FileReader->Seek(PackageFileSummary.GatherableTextDataOffset);

				TArray<FGatherableTextData> GatherableTextDataArray;
				GatherableTextDataArray.SetNum(PackageFileSummary.GatherableTextDataCount);

				for (int32 GatherableTextDataIndex = 0; GatherableTextDataIndex < PackageFileSummary.GatherableTextDataCount; ++GatherableTextDataIndex)
				{
					(*FileReader) << GatherableTextDataArray[GatherableTextDataIndex];
				}

				ProcessGatherableTextDataArray(PackageFile, GatherableTextDataArray);
			}
		}
	}

	CollectGarbage(RF_NoFlags);

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

	FLoadPackageLogOutputRedirector LogOutputRedirector;

	//Load the packages in batches
	int32 PackageIndex = 0;
	for( int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex )
	{
		int32 PackagesInThisBatch = 0;
		int32 FailuresInThisBatch = 0;
		for( ; PackageIndex < PackageCount && PackagesInThisBatch < PackagesPerBatchCount; ++PackageIndex )
		{
			FString PackageFileName = PackageFileNamesToLoad[PackageIndex];

			UE_LOG(LogGatherTextFromAssetsCommandlet, Verbose, TEXT("Loading package: '%s'."), *PackageFileName);

			UPackage *Package = nullptr;
			{
				FString LongPackageName;
				if (!FPackageName::TryConvertFilenameToLongPackageName(PackageFileName, LongPackageName))
				{
					LongPackageName = FPaths::GetCleanFilename(PackageFileName);
				}

				FLoadPackageLogOutputRedirector::FScopedCapture ScopedCapture(&LogOutputRedirector, LongPackageName);
				Package = LoadPackage( NULL, *PackageFileName, LOAD_NoWarn | LOAD_Quiet );
			}

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

		CollectGarbage(RF_NoFlags);
		LoadedPackages.Empty(PackagesPerBatchCount);	
		LoadedPackageFileNames.Empty(PackagesPerBatchCount);
	}

	return 0;
}

#undef LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
