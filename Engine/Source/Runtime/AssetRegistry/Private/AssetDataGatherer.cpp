// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryPCH.h"

#define MAX_FILES_TO_PROCESS_BEFORE_FLUSH 250
#define CACHE_SERIALIZATION_VERSION 4

FAssetDataGatherer::FAssetDataGatherer(const TArray<FString>& InPaths, bool bInIsSynchronous, bool bInLoadAndSaveCache)
	: StopTaskCounter( 0 )
	, bIsSynchronous( bInIsSynchronous )
	, bIsDiscoveringFiles( false )
	, SearchStartTime( 0 )
	, bLoadAndSaveCache( bInLoadAndSaveCache )
	, bSavedCacheAfterInitialDiscovery( false )
	, DiskCachedAssetDataBuffer( NULL )
	, Thread(NULL)
{
	const FString AllIllegalCharacters = INVALID_LONGPACKAGE_CHARACTERS;
	for ( int32 CharIdx = 0; CharIdx < AllIllegalCharacters.Len() ; ++CharIdx )
	{
		InvalidAssetFileCharacters.Add(AllIllegalCharacters[CharIdx]);
	}

	PathsToSearch = InPaths;

	bGatherDependsData = GIsEditor && !FParse::Param( FCommandLine::Get(), TEXT("NoDependsGathering") );

	CacheFilename = FPaths::GameIntermediateDir() / TEXT("CachedAssetRegistry.bin");

	if ( bIsSynchronous )
	{
		Run();
	}
	else
	{
		Thread = FRunnableThread::Create(this, TEXT("FAssetDataGatherer"), 0, TPri_BelowNormal);
	}
}

FAssetDataGatherer::~FAssetDataGatherer()
{
	NewCachedAssetDataMap.Empty();
	DiskCachedAssetDataMap.Empty();

	if ( DiskCachedAssetDataBuffer )
	{
		delete[] DiskCachedAssetDataBuffer;
		DiskCachedAssetDataBuffer = NULL;
	}

	for ( auto CacheIt = NewCachedAssetData.CreateConstIterator(); CacheIt; ++CacheIt )
	{
		delete (*CacheIt);
	}
	NewCachedAssetData.Empty();
}

bool FAssetDataGatherer::Init()
{
	return true;
}

uint32 FAssetDataGatherer::Run()
{
	int32 CacheSerializationVersion = CACHE_SERIALIZATION_VERSION;
	
	static const bool bUsingWorldAssets = FAssetRegistry::IsUsingWorldAssets();
	if ( bUsingWorldAssets )
	{
		// Bump the serialization version to refresh the cache when switching between -WorldAssets and without.
		// This is a temporary hack just while WorldAssets are under development
		CacheSerializationVersion++;
	}

	if ( bLoadAndSaveCache )
	{
		// load the cached data
		FNameTableArchiveReader CachedAssetDataReader;
		if (CachedAssetDataReader.LoadFile(*CacheFilename, CacheSerializationVersion))
		{
			SerializeCache(CachedAssetDataReader);
		}
	}

	TArray<FString> LocalFilesToSearch;
	TArray<FBackgroundAssetData*> LocalAssetResults;
	TArray<FPackageDependencyData> LocalDependencyResults;

	while ( StopTaskCounter.GetValue() == 0 )
	{
		// Check to see if there are any paths that need scanning for files.  On the first iteration, there will always
		// be work to do here.  Later, if new paths are added on the fly, we'll also process those.
		DiscoverFilesToSearch();

		{
			FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
			if ( LocalAssetResults.Num() )
			{
				AssetResults.Append(LocalAssetResults);
			}

			if ( LocalDependencyResults.Num() )
			{
				DependencyResults.Append(LocalDependencyResults);
			}

			if ( FilesToSearch.Num() )
			{
				if (SearchStartTime == 0)
				{
					SearchStartTime = FPlatformTime::Seconds();
				}

				const int32 NumFilesToProcess = FMath::Min<int32>(MAX_FILES_TO_PROCESS_BEFORE_FLUSH, FilesToSearch.Num());

				for (int32 FileIdx = 0; FileIdx < NumFilesToProcess; ++FileIdx)
				{
					LocalFilesToSearch.Add(FilesToSearch[FileIdx]);
				}

				FilesToSearch.RemoveAt(0, NumFilesToProcess);
			}
			else if (SearchStartTime != 0)
			{
				SearchTimes.Add(FPlatformTime::Seconds() - SearchStartTime);
				SearchStartTime = 0;
			}
		}

		if ( LocalAssetResults.Num() )
		{
			LocalAssetResults.Empty();
		}

		if ( LocalDependencyResults.Num() )
		{
			LocalDependencyResults.Empty();
		}

		if ( LocalFilesToSearch.Num() )
		{
			for (int32 FileIdx = 0; FileIdx < LocalFilesToSearch.Num(); ++FileIdx)
			{
				const FString& AssetFile = LocalFilesToSearch[FileIdx];

				if ( StopTaskCounter.GetValue() != 0 )
				{
					// We have been asked to stop, so don't read any more files
					break;
				}

				bool bLoadedFromCache = false;
				if ( bLoadAndSaveCache )
				{
					const FName PackageName = FName(*FPackageName::FilenameToLongPackageName(AssetFile));
					const FDateTime& FileTimestamp = IFileManager::Get().GetTimeStamp(*AssetFile);
					FDiskCachedAssetData** DiskCachedAssetDataPtr = DiskCachedAssetDataMap.Find(PackageName);
					FDiskCachedAssetData* DiskCachedAssetData = NULL;
					if ( DiskCachedAssetDataPtr && *DiskCachedAssetDataPtr )
					{
						const FDateTime& CachedTimestamp = (*DiskCachedAssetDataPtr)->Timestamp;
						if ( FileTimestamp == CachedTimestamp )
						{
							DiskCachedAssetData = *DiskCachedAssetDataPtr;
						}
					}

					if ( DiskCachedAssetData )
					{
						for ( auto CacheIt = DiskCachedAssetData->AssetDataList.CreateConstIterator(); CacheIt; ++CacheIt )
						{
							LocalAssetResults.Add(new FBackgroundAssetData(*CacheIt));
						}

						LocalDependencyResults.Add(DiskCachedAssetData->DependencyData);

						NewCachedAssetDataMap.Add(PackageName, DiskCachedAssetData);
						bLoadedFromCache = true;
					}
				}

				if ( !bLoadedFromCache )
				{
					TArray<FBackgroundAssetData*> AssetDataFromFile;
					FPackageDependencyData DependencyData;
					if ( ReadAssetFile(AssetFile, AssetDataFromFile, DependencyData) )
					{
						LocalAssetResults.Append(AssetDataFromFile);
						LocalDependencyResults.Add(DependencyData);

						if ( bLoadAndSaveCache )
						{
							// Update the cache
							const FName PackageName = FName(*FPackageName::FilenameToLongPackageName(AssetFile));
							const FDateTime& FileTimestamp = IFileManager::Get().GetTimeStamp(*AssetFile);
							FDiskCachedAssetData* NewData = new FDiskCachedAssetData(PackageName, FileTimestamp);
							for ( auto AssetIt = AssetDataFromFile.CreateConstIterator(); AssetIt; ++AssetIt )
							{
								NewData->AssetDataList.Add((*AssetIt)->ToAssetData());
							}
							NewData->DependencyData = DependencyData;
							NewCachedAssetData.Add(NewData);
							NewCachedAssetDataMap.Add(PackageName, NewData);
						}
					}
				}
			}

			LocalFilesToSearch.Empty();
		}
		else
		{
			if (bIsSynchronous)
			{
				// This is synchronous. Since our work is done, we should safely exit
				Stop();
			}
			else
			{
				// If we are caching discovered assets and this is the first time we had no work to do, save off the cache now in case the user terminates unexpectedly
				if (bLoadAndSaveCache && !bSavedCacheAfterInitialDiscovery)
				{
					FNameTableArchiveWriter CachedAssetDataWriter(CacheSerializationVersion);
					SerializeCache(CachedAssetDataWriter);
					CachedAssetDataWriter.SaveToFile(*CacheFilename);

					bSavedCacheAfterInitialDiscovery = true;
				}

				// No work to do. Sleep for a little and try again later.
				FPlatformProcess::Sleep(0.1);
			}
		}
	}

	if ( bLoadAndSaveCache )
	{
		FNameTableArchiveWriter CachedAssetData(CacheSerializationVersion);
		SerializeCache(CachedAssetData);
		CachedAssetData.SaveToFile(*CacheFilename);
	}

	return 0;
}

void FAssetDataGatherer::Stop()
{
	StopTaskCounter.Increment();
}

void FAssetDataGatherer::Exit()
{
    
}

void FAssetDataGatherer::EnsureCompletion()
{
	{
		FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
		FilesToSearch.Empty();
		PathsToSearch.Empty();
	}

	Stop();
	Thread->WaitForCompletion();
    delete Thread;
    Thread = NULL;
}

bool FAssetDataGatherer::GetAndTrimSearchResults(TArray<FBackgroundAssetData*>& OutAssetResults, TArray<FString>& OutPathResults, TArray<FPackageDependencyData>& OutDependencyResults, TArray<double>& OutSearchTimes, int32& OutNumFilesToSearch, int32& OutNumPathsToSearch)
{
	FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

	OutAssetResults.Append(AssetResults);
	AssetResults.Empty();

	OutPathResults.Append(DiscoveredPaths);
	DiscoveredPaths.Empty();

	OutDependencyResults.Append(DependencyResults);
	DependencyResults.Empty();

	OutSearchTimes.Append(SearchTimes);
	SearchTimes.Empty();

	OutNumFilesToSearch = FilesToSearch.Num();
	OutNumPathsToSearch = PathsToSearch.Num();

	return (SearchStartTime > 0 || bIsDiscoveringFiles);
}

void FAssetDataGatherer::AddPathToSearch(const FString& Path)
{
	FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
	PathsToSearch.Add(Path);
}

void FAssetDataGatherer::AddFilesToSearch(const TArray<FString>& Files)
{
	TArray<FString> FilesToAdd;
	for (int32 FilenameIdx = 0; FilenameIdx < Files.Num(); FilenameIdx++)
	{
		FString Filename(Files[FilenameIdx]);
		if ( IsValidPackageFileToRead(Filename) )
		{
			// Add the path to this asset into the list of discovered paths
			FilesToAdd.Add(Filename);
		}
	}

	{
		FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
		FilesToSearch.Append(FilesToAdd);
	}
}

void FAssetDataGatherer::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	const bool bIncludeReadOnlyRoots = true;
	if ( FPackageName::IsValidLongPackageName(PathToPrioritize, bIncludeReadOnlyRoots) )
	{
		const FString FilenamePathToPrioritize = FPackageName::LongPackageNameToFilename(PathToPrioritize);

		// Critical section. This code needs to be as fast as possible since it is in a critical section!
		// Swap all priority files to the top of the list
		{
			FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
			int32 LowestNonPriorityFileIdx = 0;
			for ( int32 FilenameIdx = 0; FilenameIdx < FilesToSearch.Num(); ++FilenameIdx )
			{
				if ( FilesToSearch[FilenameIdx].StartsWith(FilenamePathToPrioritize) )
				{
					FilesToSearch.Swap(FilenameIdx, LowestNonPriorityFileIdx);
					LowestNonPriorityFileIdx++;
				}
			}
		}
	}
}

void FAssetDataGatherer::DiscoverFilesToSearch()
{
	if( PathsToSearch.Num() > 0 )
	{
		TArray<FString> DiscoveredFilesToSearch;
		TSet<FString> LocalDiscoveredPathsSet;

		TArray<FString> CopyOfPathsToSearch;
		{
			FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
			CopyOfPathsToSearch = PathsToSearch;

			// Remove all of the existing paths from the list, since we'll process them all below.  New paths may be
			// added to the original list on a different thread as we go along, but those new paths won't be processed
			// during this call of DisoverFilesToSearch().  But we'll get them on the next call!
			PathsToSearch.Empty();
			bIsDiscoveringFiles = true;
		}

		// Iterate over any paths that we have remaining to scan
		for ( int32 PathIdx=0; PathIdx < CopyOfPathsToSearch.Num(); ++PathIdx )
		{
			const FString& Path = CopyOfPathsToSearch[PathIdx];

			// Convert the package path to a filename with no extension (directory)
			const FString FilePath = FPackageName::LongPackageNameToFilename(Path);

			// Gather the package files in that directory and subdirectories
			TArray<FString> Filenames;
			FPackageName::FindPackagesInDirectory(Filenames, FilePath);

			for (int32 FilenameIdx = 0; FilenameIdx < Filenames.Num(); FilenameIdx++)
			{
				FString Filename(Filenames[FilenameIdx]);
				if ( IsValidPackageFileToRead(Filename) )
				{
					// Add the path to this asset into the list of discovered paths
					const FString LongPackageName = FPackageName::FilenameToLongPackageName(Filename);
					LocalDiscoveredPathsSet.Add( FPackageName::GetLongPackagePath(LongPackageName) );
					DiscoveredFilesToSearch.Add(Filename);
				}
			}
		}

		// Turn the set into an array here before the critical section below
		TArray<FString> LocalDiscoveredPathsArray = LocalDiscoveredPathsSet.Array();

		{
			// Place all the discovered files into the files to search list
			FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
			FilesToSearch.Append(DiscoveredFilesToSearch);
			DiscoveredPaths.Append(LocalDiscoveredPathsArray);
			bIsDiscoveringFiles = false;
		}
	}
}

bool FAssetDataGatherer::IsValidPackageFileToRead(const FString& Filename) const
{
	FString LongPackageName;
	if ( FPackageName::TryConvertFilenameToLongPackageName(Filename, LongPackageName) )
	{
		// Make sure the path does not contain invalid characters. These packages will not be successfully loaded or read later.
		for ( int32 CharIdx = 0; CharIdx < LongPackageName.Len() ; ++CharIdx )
		{
			if ( InvalidAssetFileCharacters.Contains(LongPackageName[CharIdx]) )
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool FAssetDataGatherer::ReadAssetFile(const FString& AssetFilename, TArray<FBackgroundAssetData*>& AssetDataList, FPackageDependencyData& DependencyData) const
{
	FPackageReader PackageReader;

	if ( !PackageReader.OpenPackageFile(AssetFilename) )
	{
		return false;
	}

	if ( !PackageReader.ReadAssetRegistryData(AssetDataList) )
	{
		if ( !PackageReader.ReadAssetDataFromThumbnailCache(AssetDataList) )
		{
			// It's ok to keep reading even if the asset registry data doesn't exist yet
			//return false;
		}
	}

	if ( bGatherDependsData )
	{
		if ( !PackageReader.ReadDependencyData(DependencyData) )
		{
			return false;
		}
	}

	return true;
}

void FAssetDataGatherer::SerializeCache(FArchive& Ar)
{
	double SerializeStartTime = FPlatformTime::Seconds();

	// serialize number of objects
	int32 LocalNumAssets = NewCachedAssetDataMap.Num();
	Ar << LocalNumAssets;

	if (Ar.IsSaving())
	{
		// save out by walking the TMap
		for (auto CacheIt = NewCachedAssetDataMap.CreateConstIterator(); CacheIt; ++CacheIt)
		{
			Ar << *CacheIt.Value();
		}
	}
	else
	{
		// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
		DiskCachedAssetDataMap.Empty(LocalNumAssets);
		DiskCachedAssetDataBuffer = new FDiskCachedAssetData[LocalNumAssets];

		for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; ++AssetIndex)
		{
			// make a new asset data object
			FDiskCachedAssetData* NewCachedAssetDataPtr = &DiskCachedAssetDataBuffer[AssetIndex];

			// load it
			Ar << *NewCachedAssetDataPtr;

			// hash it
			DiskCachedAssetDataMap.Add(NewCachedAssetDataPtr->PackageName, NewCachedAssetDataPtr);
		}
	}

	UE_LOG(LogAssetRegistry, Verbose, TEXT("Asset data gatherer serialized in %0.6f seconds"), FPlatformTime::Seconds() - SerializeStartTime);
}