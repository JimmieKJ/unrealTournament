// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "AssetRegistryPCH.h"
#include "ModuleManager.h"
#include "GenericPlatformChunkInstall.h"

#if WITH_EDITOR
#include "DirectoryWatcherModule.h"
#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY(LogAssetRegistry);

/** Returns the appropriate ChunkProgressReportingType for the given Asset enum */
EChunkProgressReportingType::Type GetChunkAvailabilityProgressType(EAssetAvailabilityProgressReportingType::Type ReportType)
{
	EChunkProgressReportingType::Type ChunkReportType;
	switch (ReportType)
	{
	case EAssetAvailabilityProgressReportingType::ETA:
		ChunkReportType = EChunkProgressReportingType::ETA;
		break;
	case EAssetAvailabilityProgressReportingType::PercentageComplete:
		ChunkReportType = EChunkProgressReportingType::PercentageComplete;
		break;
	default:
		ChunkReportType = EChunkProgressReportingType::PercentageComplete;
		UE_LOG(LogAssetRegistry, Error, TEXT("Unsupported assetregistry report type: %i"), (int)ReportType);
		break;
	}
	return ChunkReportType;
}

FAssetRegistry::FAssetRegistry()
	: PathTreeRoot(FString())
	, PreallocatedAssetDataBuffer(NULL)
{
	const double StartupStartTime = FPlatformTime::Seconds();

	NumAssets = 0;
	NumDependsNodes = 0;
	bInitialSearchCompleted = true;
	AmortizeStartTime = 0;
	TotalAmortizeTime = 0;
	

	MaxSecondsPerFrame = 0.015;

	// Collect all code generator classes (currently BlueprintCore-derived ones)
	CollectCodeGeneratorClasses();

	// If in the editor, we scan all content right now
	// If in the game, we expect user to make explicit sync queries using ScanPathsSynchronous
	// If in a commandlet, we expect the commandlet to decide when to perform a synchronous scan
	if(GIsEditor && !IsRunningCommandlet())
	{
		bInitialSearchCompleted = false;
		SearchAllAssets(false);
	}
	// for platforms that require cooked data, we attempt to load a premade asset registry
	else if (FPlatformProperties::RequiresCookedData())
	{
		// load the cooked data
		FArrayReader SerializedAssetData;
		if (FFileHelper::LoadFileToArray(SerializedAssetData, *(FPaths::GameDir() / TEXT("AssetRegistry.bin"))))
		{
			// serialize the data with the memory reader (will convert FStrings to FNames, etc)
			Serialize(SerializedAssetData);
		}
	}

	// Report startup time. This does not include DirectoryWatcher startup time.
	UE_LOG(LogAssetRegistry, Log, TEXT( "FAssetRegistry took %0.4f seconds to start up" ), FPlatformTime::Seconds() - StartupStartTime );

#if WITH_EDITOR
	// Commandlets and in-game don't listen for directory changes
	if ( !IsRunningCommandlet() && GIsEditor)
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

		if (DirectoryWatcher)
		{
			TArray<FString> RootContentPaths;
			FPackageName::QueryRootContentPaths( RootContentPaths );
			for( TArray<FString>::TConstIterator RootPathIt( RootContentPaths ); RootPathIt; ++RootPathIt )
			{
				const FString& RootPath = *RootPathIt;
				const FString& ContentFolder = FPackageName::LongPackageNameToFilename( RootPath );
				FDelegateHandle NewHandle;
				DirectoryWatcher->RegisterDirectoryChangedCallback_Handle( ContentFolder, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FAssetRegistry::OnDirectoryChanged), NewHandle);
				OnDirectoryChangedDelegateHandles.Add(ContentFolder, NewHandle);
			}
		}
	}
#endif // WITH_EDITOR

	// Listen for new content paths being added or removed at runtime.  These are usually plugin-specific asset paths that
	// will be loaded a bit later on.
	FPackageName::OnContentPathMounted().AddRaw( this, &FAssetRegistry::OnContentPathMounted );
	FPackageName::OnContentPathDismounted().AddRaw( this, &FAssetRegistry::OnContentPathDismounted );
}

void FAssetRegistry::CollectCodeGeneratorClasses()
{
	// Work around the fact we don't reference Engine module directly
	UClass* BlueprintCoreClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, TEXT("BlueprintCore")));
	if (BlueprintCoreClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(BlueprintCoreClass))
			{
				ClassGeneratorNames.Add(It->GetFName());
			}
		}
	}
}

FAssetRegistry::~FAssetRegistry()
{
	// Make sure the asset search thread is closed
	if ( BackgroundAssetSearch.IsValid() )
	{
		BackgroundAssetSearch->EnsureCompletion();
		BackgroundAssetSearch.Reset();
	}

	// if we have preallocated all the FAssetData's in a single block, free it now, instead of one at a time
	if (PreallocatedAssetDataBuffer != NULL)
	{
		delete [] PreallocatedAssetDataBuffer;
		NumAssets = 0;
	}
	else
	{
		// Delete all assets in the cache
		for (TMap<FName, FAssetData*>::TConstIterator AssetDataIt(CachedAssetsByObjectPath); AssetDataIt; ++AssetDataIt)
		{
			if ( AssetDataIt.Value() )
			{
				delete AssetDataIt.Value();
				NumAssets--;
			}
		}
	}


	// Make sure we have deleted all our allocated FAssetData objects
	ensure(NumAssets == 0);

	// Delete all depends nodes in the cache
	for (TMap<FName, FDependsNode*>::TConstIterator DependsIt(CachedDependsNodes); DependsIt; ++DependsIt)
	{
		if ( DependsIt.Value() )
		{
			delete DependsIt.Value();
			NumDependsNodes--;
		}
	}

	// Make sure we have deleted all our allocated FDependsNode objects
	ensure(NumDependsNodes == 0);

	// Clear cache
	CachedAssetsByObjectPath.Empty();
	CachedAssetsByPackageName.Empty();
	CachedAssetsByPath.Empty();
	CachedAssetsByClass.Empty();
	CachedAssetsByTag.Empty();
	CachedDependsNodes.Empty();
#if WITH_EDITORONLY_DATA
	CachedAssetsBySourceFileName.Empty();
#endif

	// Stop listening for content mount point events
	FPackageName::OnContentPathMounted().RemoveAll( this );
	FPackageName::OnContentPathDismounted().RemoveAll( this );

	// Commandlets dont listen for directory changes
#if WITH_EDITOR
	if ( !IsRunningCommandlet() && GIsEditor )
	{
		// If the directory module is still loaded, unregister any delegates
		if ( FModuleManager::Get().IsModuleLoaded("DirectoryWatcher") )
		{
			FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
			IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

			if (DirectoryWatcher)
			{
				TArray<FString> RootContentPaths;
				FPackageName::QueryRootContentPaths( RootContentPaths );
				for( TArray<FString>::TConstIterator RootPathIt( RootContentPaths ); RootPathIt; ++RootPathIt )
				{
					const FString& RootPath = *RootPathIt;
					const FString& ContentFolder = FPackageName::LongPackageNameToFilename( RootPath );
					DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle( ContentFolder, OnDirectoryChangedDelegateHandles.FindRef(ContentFolder));
					OnDirectoryChangedDelegateHandles.Remove(ContentFolder);
				}
			}
		}
	}
#endif // WITH_EDITOR

	// Clear all listeners
	AssetAddedEvent.Clear();
	AssetRemovedEvent.Clear();
	AssetRenamedEvent.Clear();
	InMemoryAssetCreatedEvent.Clear();
	InMemoryAssetDeletedEvent.Clear();
	FileLoadedEvent.Clear();
	FileLoadProgressUpdatedEvent.Clear();
}

void FAssetRegistry::SearchAllAssets(bool bSynchronousSearch)
{
	// Mark the time before the first search started
	FullSearchStartTime = FPlatformTime::Seconds();

	// Figure out what all of the root asset directories are.  This will include Engine content, Game content, but also may include
	// mounted content directories for one or more plugins.	Also keep in mind that plugins may become loaded later on.  We'll listen
	// for that via a delegate, and add those directories to scan later as them come in.
	TArray<FString> PathsToSearch;
	FPackageName::QueryRootContentPaths( PathsToSearch );

	// Start the asset search (synchronous in commandlets)
	const bool bLoadAndSaveCache = !FParse::Param(FCommandLine::Get(), TEXT("NoAssetRegistryCache"));
	if ( bSynchronousSearch )
	{
		const bool bForceRescan = false;
		ScanPathsSynchronous_Internal(PathsToSearch, bForceRescan, bLoadAndSaveCache);
	}
	else if ( !BackgroundAssetSearch.IsValid() )
	{
		// if the BackgroundAssetSearch is already valid then we have already called it before
		BackgroundAssetSearch = MakeShareable( new FAssetDataGatherer(PathsToSearch, bSynchronousSearch, bLoadAndSaveCache) );
	}
}


bool FAssetRegistry::GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData) const
{
	FARFilter Filter;
	Filter.PackageNames.Add(PackageName);
	return GetAssets(Filter, OutAssetData);
}

bool FAssetRegistry::GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive) const
{
	FARFilter Filter;
	Filter.bRecursivePaths = bRecursive;
	Filter.PackagePaths.Add(PackagePath);
	return GetAssets(Filter, OutAssetData);
}

bool FAssetRegistry::GetAssetsByClass(FName ClassName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses) const
{
	FARFilter Filter;
	Filter.ClassNames.Add(ClassName);
	Filter.bRecursiveClasses = bSearchSubClasses;
	return GetAssets(Filter, OutAssetData);
}

bool FAssetRegistry::GetAssetsByTagValues(const TMultiMap<FName, FString>& AssetTagsAndValues, TArray<FAssetData>& OutAssetData) const
{
	FARFilter Filter;
	Filter.TagsAndValues = AssetTagsAndValues;
	return GetAssets(Filter, OutAssetData);
}

bool FAssetRegistry::GetAssets(const FARFilter& Filter, TArray<FAssetData>& OutAssetData) const
{
	double GetAssetsStartTime = FPlatformTime::Seconds();

	// Verify filter input. If all assets are needed, use GetAllAssets() instead.
	if ( !IsFilterValid(Filter) || Filter.IsEmpty() )
	{
		return false;
	}

	// Start with in memory assets
	TSet<FName> InMemoryObjectPaths;

	// Prepare a set of each filter component for fast searching
	TSet<FName> FilterPackageNames;
	TSet<FName> FilterPackagePaths;
	TSet<FName> FilterClassNames;
	TSet<FName> FilterObjectPaths;
	const int32 NumFilterPackageNames = Filter.PackageNames.Num();
	const int32 NumFilterPackagePaths = Filter.PackagePaths.Num();
	const int32 NumFilterClasses = Filter.ClassNames.Num();
	const int32 NumFilterObjectPaths = Filter.ObjectPaths.Num();

	for ( int32 NameIdx = 0; NameIdx < NumFilterPackageNames; ++NameIdx )
	{
		FilterPackageNames.Add(Filter.PackageNames[NameIdx]);
	}

	for ( int32 PathIdx = 0; PathIdx < NumFilterPackagePaths; ++PathIdx )
	{
		FilterPackagePaths.Add(Filter.PackagePaths[PathIdx]);
	}

	if ( Filter.bRecursivePaths )
	{
		// Add subpaths to all the input paths to the list
		for ( int32 PathIdx = 0; PathIdx < NumFilterPackagePaths; ++PathIdx )
		{
			PathTreeRoot.GetSubPaths(Filter.PackagePaths[PathIdx].ToString(), FilterPackagePaths);
		}
	}

	if ( Filter.bRecursiveClasses )
	{
		// GetSubClasses includes the base classes
		GetSubClasses(Filter.ClassNames, Filter.RecursiveClassesExclusionSet, FilterClassNames);
	}
	else
	{
		for ( int32 ClassIdx = 0; ClassIdx < NumFilterClasses; ++ClassIdx )
		{
			FilterClassNames.Add(Filter.ClassNames[ClassIdx]);
		}
	}

	if ( !Filter.bIncludeOnlyOnDiskAssets )
	{
		for (int32 ObjectPathIdx = 0; ObjectPathIdx < Filter.ObjectPaths.Num(); ++ObjectPathIdx)
		{
			FilterObjectPaths.Add(Filter.ObjectPaths[ObjectPathIdx]);
		}

		auto FilterInMemoryObjectLambda = [&](const UObject* Obj)
		{
			if ( Obj->IsAsset() )
			{
				UPackage* InMemoryPackage = Obj->GetOutermost();

				static const bool bUsingWorldAssets = FAssetRegistry::IsUsingWorldAssets();
				// Skip assets in map packages... unless we are showing world assets
				if ( InMemoryPackage->ContainsMap() && !bUsingWorldAssets )
				{
					return;
				}

				// Skip assets that were loaded for diffing
				if ( InMemoryPackage->PackageFlags & PKG_ForDiffing )
				{
					return;
				}

				// add it to in-memory object list for later merge
				const FName ObjectPath = FName(*Obj->GetPathName());
				InMemoryObjectPaths.Add(ObjectPath);

				// Package name
				const FName PackageName = InMemoryPackage->GetFName();
				if ( NumFilterPackageNames && !FilterPackageNames.Contains(PackageName) )
				{
					return;
				}

				// Object Path
				if ( NumFilterObjectPaths && !FilterObjectPaths.Contains(ObjectPath) )
				{
					return;
				}

				// Package path
				const FName PackagePath = FName(*FPackageName::GetLongPackagePath(InMemoryPackage->GetName()));
				if ( NumFilterPackagePaths && !FilterPackagePaths.Contains(PackagePath) )
				{
					return;
				}

				// Tags and values
				TArray<UObject::FAssetRegistryTag> ObjectTags;
				Obj->GetAssetRegistryTags(ObjectTags);
				if ( Filter.TagsAndValues.Num() )
				{
					bool bMatch = false;
					for (auto FilterTagIt = Filter.TagsAndValues.CreateConstIterator(); FilterTagIt; ++FilterTagIt)
					{
						const FName Tag = FilterTagIt.Key();
						const FString& Value = FilterTagIt.Value();

						for ( auto ObjectTagIt = ObjectTags.CreateConstIterator(); ObjectTagIt; ++ObjectTagIt )
						{
							if ( ObjectTagIt->Name == Tag )
							{
								if ( ObjectTagIt->Value == Value )
								{
									bMatch = true;
								}

								break;
							}
						}

						if ( bMatch )
						{
							break;
						}
					}

					if (!bMatch)
					{
						return;
					}
				}

				// Find the group names
				FString GroupNamesStr;
				FString AssetNameStr;
				Obj->GetPathName(InMemoryPackage).Split(TEXT("."), &GroupNamesStr, &AssetNameStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

				TMap<FName, FString> TagMap;
				for ( auto TagIt = ObjectTags.CreateConstIterator(); TagIt; ++TagIt )
				{
					TagMap.Add(TagIt->Name, TagIt->Value);
				}

				// This asset is in memory and passes all filters
				FAssetData* AssetData = new (OutAssetData) FAssetData(PackageName, PackagePath, FName(*GroupNamesStr), Obj->GetFName(), Obj->GetClass()->GetFName(), TagMap, InMemoryPackage->GetChunkIDs());
			}
		};

		// Iterate over all in-memory assets to find the ones that pass the filter components
		if(NumFilterClasses)
		{
			TArray<UObject*> InMemoryObjects;
			for (auto ClassNameIt = FilterClassNames.CreateConstIterator(); ClassNameIt; ++ClassNameIt)
			{
				UClass* Class = FindObjectFast<UClass>(nullptr, *ClassNameIt->ToString(), false, true, RF_NoFlags);
				if(Class != nullptr)
				{
					GetObjectsOfClass(Class, InMemoryObjects, false, RF_NoFlags);
				}
			}

			for (auto ObjIt = InMemoryObjects.CreateConstIterator(); ObjIt; ++ObjIt)
			{
				FilterInMemoryObjectLambda(*ObjIt);
			}
		}
		else
		{
			for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
			{
				FilterInMemoryObjectLambda(*ObjIt);
			}
		}
	}

	// Now add cached (unloaded) assets
	// Form a set of assets matched by each filter
	TArray< TArray<FAssetData*> > DiskFilterSets;

	// On disk package names
	if ( Filter.PackageNames.Num() )
	{
		auto PackageNameFilter = new(DiskFilterSets) TArray<FAssetData*>();

		for ( auto PackageIt = FilterPackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
		{
			auto PackageAssets = CachedAssetsByPackageName.Find(*PackageIt);
					
			if (PackageAssets != NULL)
			{
				PackageNameFilter->Append(*PackageAssets);
			}
		}
	}

	// On disk package paths
	if ( Filter.PackagePaths.Num() )
	{
		auto PathFilter = new(DiskFilterSets) TArray<FAssetData*>();

		for ( auto PathIt = FilterPackagePaths.CreateConstIterator(); PathIt; ++PathIt )
		{
			auto PathAssets = CachedAssetsByPath.Find(*PathIt);

			if (PathAssets != NULL)
			{
				PathFilter->Append(*PathAssets);
			}
		}
	}

	// On disk classes
	if ( Filter.ClassNames.Num() )
	{
		auto ClassFilter = new(DiskFilterSets) TArray<FAssetData*>();

		for ( auto ClassNameIt = FilterClassNames.CreateConstIterator(); ClassNameIt; ++ClassNameIt )
		{
			auto ClassAssets = CachedAssetsByClass.Find(*ClassNameIt);

			if (ClassAssets != NULL)
			{
				ClassFilter->Append(*ClassAssets);
			}
		}
	}

	// On disk object paths
	if ( Filter.ObjectPaths.Num() )
	{
		auto ObjectPathsFilter = new(DiskFilterSets) TArray<const FAssetData*>();

		for ( int32 ObjectPathIdx = 0; ObjectPathIdx < Filter.ObjectPaths.Num(); ++ObjectPathIdx )
		{
			const FAssetData*const* AssetDataPtr = CachedAssetsByObjectPath.Find(Filter.ObjectPaths[ObjectPathIdx]);

			if ( AssetDataPtr != NULL )
			{
				ObjectPathsFilter->Add(*AssetDataPtr);
			}
		}
	}

	// On disk tags and values
	if ( Filter.TagsAndValues.Num() )
	{
		auto TagAndValuesFilter = new(DiskFilterSets) TArray<const FAssetData*>();

		for (auto FilterTagIt = Filter.TagsAndValues.CreateConstIterator(); FilterTagIt; ++FilterTagIt)
		{
			const FName Tag = FilterTagIt.Key();
			const FString& Value = FilterTagIt.Value();
				
			auto TagAssets = CachedAssetsByTag.Find(Tag);

			if (TagAssets != NULL)
			{
				for (auto TagIt = (*TagAssets).CreateConstIterator(); TagIt; ++TagIt)
				{
					FAssetData* AssetData = *TagIt;

					if ( AssetData != NULL )
					{
						const FString* TagValue = AssetData->TagsAndValues.Find(Tag);
						if ( TagValue != NULL && *TagValue == Value )
						{
							TagAndValuesFilter->Add(AssetData);
						}
					}
				}
			}
		}
	}

#if WITH_EDITORONLY_DATA
	// Asset source file filter
	if ( GIsEditor && Filter.SourceFilenames.Num() )
	{
		for (const auto& Filename : Filter.SourceFilenames)
		{
			const auto* Assets = CachedAssetsBySourceFileName.Find(Filename);
			if (Assets)
			{
				DiskFilterSets.Add(*Assets);
			}
		}
	}
#endif

	// If we have any filter sets, add the assets which are contained in the sets to OutAssetData
	if ( DiskFilterSets.Num() > 0 )
	{
		// Initialize the combined filter set to the first set, in case we can skip combining.
		TArray<FAssetData*>* CombinedFilterSet = DiskFilterSets.GetData();
		TArray<FAssetData*> Intersection;

		// If we have more than one set, we must combine them. We take the intersection
		if ( DiskFilterSets.Num() > 1 )
		{
			// Sort each set for the intersection algorithm
			struct FCompareFAssetData
			{
				FORCEINLINE bool operator()( const FAssetData& A, const FAssetData& B ) const
				{
					return A.ObjectPath.Compare(B.ObjectPath) < 0;
				}
			};

			for ( auto SetIt = DiskFilterSets.CreateIterator(); SetIt; ++SetIt )
			{
				(*SetIt).Sort( FCompareFAssetData() );
			}

			// Set the "current" intersection set to the first filter set
			Intersection = DiskFilterSets[0];

			// Now iterate over every set beyond the first and intersect it with the current
			for ( int32 SetIdx = 1; SetIdx < DiskFilterSets.Num(); ++SetIdx )
			{
				TArray<FAssetData*> NewIntersection;
				const TArray<FAssetData*>& SetA = Intersection;
				const TArray<FAssetData*>& SetB = DiskFilterSets[SetIdx];
				int32 AIdx = 0;
				int32 BIdx = 0;

				// Do intersection
				while (AIdx < SetA.Num() && BIdx < SetB.Num())
				{
					if (SetA[AIdx]->ObjectPath.Compare(SetB[BIdx]->ObjectPath) < 0) ++AIdx;
					else if (SetB[BIdx]->ObjectPath.Compare(SetA[AIdx]->ObjectPath) < 0) ++BIdx;
					else { NewIntersection.Add(SetA[AIdx]); AIdx++; BIdx++; }
				}

				// Update the "current" intersection with the results
				Intersection = NewIntersection;
			}

			// Set the CombinedFilterSet pointer to the full intersection of all sets
			CombinedFilterSet = &Intersection;
		}

		// Iterate over the final combined filter set to add to OutAssetData
		for ( auto AssetIt = (*CombinedFilterSet).CreateConstIterator(); AssetIt; ++AssetIt )
		{
			if ( CachedEmptyPackages.Contains((*AssetIt)->PackageName) )
			{
				// Skip assets from empty packages
				continue;
			}

			if ( InMemoryObjectPaths.Contains((*AssetIt)->ObjectPath) )
			{
				// Make sure the asset is not in memory. It should have already been added.
				continue;
			}

			OutAssetData.Add(**AssetIt);
		}
	}

	UE_LOG(LogAssetRegistry, Verbose, TEXT("GetAssets completed in %0.4f seconds"), FPlatformTime::Seconds() - GetAssetsStartTime);

	return true;
}

FAssetData FAssetRegistry::GetAssetByObjectPath( const FName ObjectPath ) const 
{
	UObject* Asset = FindObject<UObject>( NULL, *ObjectPath.ToString() );

	if ( Asset != NULL )
	{
		return FAssetData( Asset );
	}

	const FAssetData* const * AssetData = CachedAssetsByObjectPath.Find( ObjectPath );
	if ( AssetData == NULL || CachedEmptyPackages.Contains( (*AssetData)->PackageName ) )
	{
		return FAssetData();
	}

	return **AssetData;
}

bool FAssetRegistry::GetAllAssets(TArray<FAssetData>& OutAssetData) const
{
	TSet<FName> InMemoryObjectPaths;
	double GetAllAssetsStartTime = FPlatformTime::Seconds();

	// All in memory assets
	for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
	{
		if (ObjIt->IsAsset())
		{
			FAssetData* AssetData = new (OutAssetData) FAssetData(*ObjIt);
			InMemoryObjectPaths.Add(AssetData->ObjectPath);
		}
	}

	// All unloaded disk assets
	for (TMap<FName, FAssetData*>::TConstIterator AssetDataIt(CachedAssetsByObjectPath); AssetDataIt; ++AssetDataIt)
	{
		const FAssetData* AssetData = AssetDataIt.Value();

		if (AssetData != NULL)
		{
			// Make sure the asset's package was not loaded then the object was deleted/renamed
			if ( !CachedEmptyPackages.Contains(AssetData->PackageName) )
			{
				// Make sure the asset is not in memory
				if ( !InMemoryObjectPaths.Contains(AssetData->ObjectPath) )
				{
					OutAssetData.Add(*AssetData);
				}
			}
		}
	}

	UE_LOG(LogAssetRegistry, VeryVerbose, TEXT("GetAllAssets completed in %0.4f seconds"), FPlatformTime::Seconds() - GetAllAssetsStartTime);

	return true;
}

bool FAssetRegistry::GetDependencies(FName PackageName, TArray<FName>& OutDependencies) const
{
	const FDependsNode*const* NodePtr = CachedDependsNodes.Find(PackageName);
	const FDependsNode* Node = NULL;
	if (NodePtr != NULL )
	{
		Node = *NodePtr;
	}

	if (Node != NULL)
	{
		TArray<FDependsNode*> DependencyNodes;
		Node->GetDependencies(DependencyNodes);

		for ( auto NodeIt = DependencyNodes.CreateConstIterator(); NodeIt; ++NodeIt )
		{
			OutDependencies.Add((*NodeIt)->PackageName);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistry::GetReferencers(FName PackageName, TArray<FName>& OutReferencers) const
{
	const FDependsNode*const* NodePtr = CachedDependsNodes.Find(PackageName);
	const FDependsNode* Node = NULL;
	if (NodePtr != NULL )
	{
		Node = *NodePtr;
	}

	if (Node != NULL)
	{
		TArray<FDependsNode*> DependencyNodes;
		Node->GetReferencers(DependencyNodes);

		for ( auto NodeIt = DependencyNodes.CreateConstIterator(); NodeIt; ++NodeIt )
		{
			OutReferencers.Add((*NodeIt)->PackageName);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistry::GetAncestorClassNames(FName ClassName, TArray<FName>& OutAncestorClassNames) const
{
	// Start with the cached inheritance map
	TMap<FName, FName> InheritanceMap = CachedInheritanceMap;

	// And add all in-memory classes at request time
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if ( !ClassIt->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) )
		{
			if ( ClassIt->GetSuperClass() )
			{
				InheritanceMap.Add(ClassIt->GetFName(), ClassIt->GetSuperClass()->GetFName());
			}
			else
			{
				InheritanceMap.Add(ClassIt->GetFName(), NAME_None);
			}
		}
	}

	// Make sure the requested class is in the inheritance map
	if ( !InheritanceMap.Contains(ClassName) )
	{
		return false;
	}

	// Now follow the map pairs until we cant find any more parents
	FName* CurrentClassName = &ClassName;
	const uint32 MaxInheritanceDepth = 65536;
	uint32 CurrentInheritanceDepth = 0;
	while ( CurrentInheritanceDepth < MaxInheritanceDepth && CurrentClassName != NULL )
	{
		CurrentClassName = InheritanceMap.Find(*CurrentClassName);

		if ( CurrentClassName )
		{
			if ( *CurrentClassName == NAME_None )
			{
				// No parent, we are at the root
				CurrentClassName = NULL;
			}
			else
			{
				OutAncestorClassNames.Add(*CurrentClassName);
			}
		}
		CurrentInheritanceDepth++;
	}

	if ( CurrentInheritanceDepth == MaxInheritanceDepth )
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("IsChildClass exceeded max inheritance depth. There is probably an infinite loop of parent classes."));
		return false;
	}
	else 
	{
		return true;
	}
}

void FAssetRegistry::GetDerivedClassNames(const TArray<FName>& ClassNames, const TSet<FName>& ExcludedClassNames, TSet<FName>& OutDerivedClassNames) const
{
	GetSubClasses(ClassNames, ExcludedClassNames, OutDerivedClassNames);
}

void FAssetRegistry::GetAllCachedPaths(TArray<FString>& OutPathList) const
{
	TSet<FName> PathList;
	PathTreeRoot.GetSubPaths(TEXT(""), PathList);
	
	OutPathList.Empty(PathList.Num());
	for ( auto PathIt = PathList.CreateConstIterator(); PathIt; ++PathIt )
	{
		OutPathList.Add((*PathIt).ToString());
	}
}

void FAssetRegistry::GetSubPaths(const FString& InBasePath, TArray<FString>& OutPathList, bool bInRecurse) const
{
	TSet<FName> PathList;
	PathTreeRoot.GetSubPaths(InBasePath, PathList, bInRecurse);
	
	OutPathList.Empty(PathList.Num());
	for ( auto PathIt = PathList.CreateConstIterator(); PathIt; ++PathIt )
	{
		OutPathList.Add((*PathIt).ToString());
	}
}

void FAssetRegistry::RunAssetsThroughFilter(TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const
{
	if ( !Filter.IsEmpty() )
	{
		TSet<FName> RequestedClassNames;
		if ( Filter.bRecursiveClasses && Filter.ClassNames.Num() > 0 )
		{
			// First assemble a full list of requested classes from the ClassTree
			// GetSubClasses includes the base classes
			GetSubClasses(Filter.ClassNames, Filter.RecursiveClassesExclusionSet, RequestedClassNames);
		}

		for ( int32 AssetDataIdx = AssetDataList.Num() - 1; AssetDataIdx >= 0; --AssetDataIdx )
		{
			const FAssetData& AssetData = AssetDataList[AssetDataIdx];

			// Package Names
			if ( Filter.PackageNames.Num() > 0 )
			{
				bool bPassesPackageNames = false;
				for ( int32 NameIdx = 0; NameIdx < Filter.PackageNames.Num(); ++NameIdx )
				{
					if (Filter.PackageNames[NameIdx] == AssetData.PackageName)
					{
						bPassesPackageNames = true;
						break;
					}
				}

				if ( !bPassesPackageNames )
				{
					AssetDataList.RemoveAt(AssetDataIdx);
					continue;
				}
			}

			// Package Paths
			if ( Filter.PackagePaths.Num() > 0 )
			{
				bool bPassesPackagePaths = false;
				if ( Filter.bRecursivePaths )
				{
					FString AssetPackagePath = AssetData.PackagePath.ToString();
					for ( int32 PathIdx = 0; PathIdx < Filter.PackagePaths.Num(); ++PathIdx )
					{
						const FString Path = Filter.PackagePaths[PathIdx].ToString();
						if ( AssetPackagePath.StartsWith(Path) )
						{
							// Only match the exact path or a path that starts with the target path followed by a slash
							if ( Path.Len() == AssetPackagePath.Len() || AssetPackagePath.Mid(Path.Len(), 1) == TEXT("/") )
							{
								bPassesPackagePaths = true;
								break;
							}
						}
					}
				}
				else
				{
					// Non-recursive. Just request data for each requested path.
					for ( int32 PathIdx = 0; PathIdx < Filter.PackagePaths.Num(); ++PathIdx )
					{
						if (Filter.PackagePaths[PathIdx] == AssetData.PackagePath)
						{
							bPassesPackagePaths = true;
							break;
						}
					}
				}

				if ( !bPassesPackagePaths )
				{
					AssetDataList.RemoveAt(AssetDataIdx);
					continue;
				}
			}

			// ObjectPaths
			if ( Filter.ObjectPaths.Num() > 0 )
			{
				bool bPassesObjectPaths = Filter.ObjectPaths.Contains(AssetData.ObjectPath);

				if ( !bPassesObjectPaths )
				{
					AssetDataList.RemoveAt(AssetDataIdx);
					continue;
				}
			}

			// Classes
			if ( Filter.ClassNames.Num() > 0 )
			{
				bool bPassesClasses = false;
				if ( Filter.bRecursiveClasses )
				{
					// Now check against each discovered class
					for (auto ClassIt = RequestedClassNames.CreateConstIterator(); ClassIt; ++ClassIt)
					{
						if (*ClassIt == AssetData.AssetClass)
						{
							bPassesClasses = true;
							break;
						}
					}
				}
				else
				{
					// Non-recursive. Just request data for each requested classes.
					for ( int32 ClassIdx = 0; ClassIdx < Filter.ClassNames.Num(); ++ClassIdx )
					{
						if (Filter.ClassNames[ClassIdx] == AssetData.AssetClass)
						{
							bPassesClasses = true;
							break;
						}
					}
				}

				if ( !bPassesClasses )
				{
					AssetDataList.RemoveAt(AssetDataIdx);
					continue;
				}
			}

			// Tags and values
			if ( Filter.TagsAndValues.Num() > 0 )
			{
				bool bPassesTags = false;
				for (auto FilterTagIt = Filter.TagsAndValues.CreateConstIterator(); FilterTagIt; ++FilterTagIt)
				{
					const FString* Value = AssetData.TagsAndValues.Find(FilterTagIt.Key());

					if ( Value != NULL && (*Value) == FilterTagIt.Value() )
					{
						bPassesTags = true;
						break;
					}
				}

				if ( !bPassesTags )
				{
					AssetDataList.RemoveAt(AssetDataIdx);
					continue;
				}
			}
		}
	}
}

EAssetAvailability::Type FAssetRegistry::GetAssetAvailability(const FAssetData& AssetData) const
{
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	EChunkLocation::Type BestLocation = EChunkLocation::DoesNotExist;

	// check all chunks to see which has the best locality
	for (auto ChunkIt = AssetData.ChunkIDs.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		EChunkLocation::Type ChunkLocation = ChunkInstall->GetChunkLocation(*ChunkIt);

		// if we find one in the best location, early out
		if (ChunkLocation == EChunkLocation::BestLocation)
		{
			BestLocation = ChunkLocation;
			break;
		}

		if (ChunkLocation > BestLocation)
		{
			BestLocation = ChunkLocation;
		}
	}

	switch (BestLocation)
	{
	case EChunkLocation::LocalFast:
		return EAssetAvailability::LocalFast;
	case EChunkLocation::LocalSlow:
		return EAssetAvailability::LocalSlow;
	case EChunkLocation::NotAvailable:
		return EAssetAvailability::NotAvailable;
	case EChunkLocation::DoesNotExist:
		return EAssetAvailability::DoesNotExist;
	default:
		check(0);
		return EAssetAvailability::LocalFast;
	}
}

float FAssetRegistry::GetAssetAvailabilityProgress(const FAssetData& AssetData, EAssetAvailabilityProgressReportingType::Type ReportType) const
{
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();
	EChunkProgressReportingType::Type ChunkReportType = GetChunkAvailabilityProgressType(ReportType);

	bool IsPercentageComplete = (ChunkReportType == EChunkProgressReportingType::PercentageComplete) ? true : false;
	check (ReportType == EAssetAvailabilityProgressReportingType::PercentageComplete || ReportType == EAssetAvailabilityProgressReportingType::ETA);

	float BestProgress = MAX_FLT;	

	// check all chunks to see which has the best time remaining
	for (auto ChunkIt = AssetData.ChunkIDs.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		float Progress = ChunkInstall->GetChunkProgress( *ChunkIt, ChunkReportType );

		// need to flip percentage completes for the comparison
		if (IsPercentageComplete)
		{
			Progress = 100.0f - Progress;
		}

		if( Progress <= 0.0f )
		{
			BestProgress = 0.0f;
			break;
		}

		if (Progress < BestProgress)
		{
			BestProgress = Progress;
		}
	}

	// unflip percentage completes
	if (IsPercentageComplete)
	{
		BestProgress = 100.0f - BestProgress;
	}	
	return BestProgress;
}

bool FAssetRegistry::GetAssetAvailabilityProgressTypeSupported(EAssetAvailabilityProgressReportingType::Type ReportType) const
{
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();	
	return ChunkInstall->GetProgressReportingTypeSupported(GetChunkAvailabilityProgressType(ReportType));
}

void FAssetRegistry::PrioritizeAssetInstall(const FAssetData& AssetData) const
{
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	if (AssetData.ChunkIDs.Num() == 0)
	{
		return;
	}

	ChunkInstall->PrioritizeChunk(AssetData.ChunkIDs[0], EChunkPriority::Immediate);
}

bool FAssetRegistry::AddPath(const FString& PathToAdd)
{
	return AddAssetPath(PathToAdd);
}

bool FAssetRegistry::RemovePath(const FString& PathToRemove)
{
	return RemoveAssetPath(PathToRemove);
}

void FAssetRegistry::ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan)
{
	const bool bUseCache = false;
	ScanPathsSynchronous_Internal(InPaths, bForceRescan, bUseCache);
}

void FAssetRegistry::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	// Prioritize the background search
	if (BackgroundAssetSearch.IsValid())
	{
		BackgroundAssetSearch->PrioritizeSearchPath(PathToPrioritize);
	}

	// Also prioritize the queue of background search results
	{
		// Swap all priority files to the top of the list
		int32 LowestNonPriorityFileIdx = 0;
		for (int32 ResultIdx = 0; ResultIdx < BackgroundAssetResults.Num(); ++ResultIdx)
		{
			if (BackgroundAssetResults[ResultIdx]->PackagePath.StartsWith(PathToPrioritize))
			{
				BackgroundAssetResults.Swap(ResultIdx, LowestNonPriorityFileIdx);
				LowestNonPriorityFileIdx++;
			}
		}
	}
}

void FAssetRegistry::AssetCreated(UObject* NewAsset)
{
	if ( ensure(NewAsset) && NewAsset->IsAsset() )
	{
		// Add the newly created object to the package file cache because its filename can already be
		// determined by its long package name.
		// @todo AssetRegistry We are assuming it will be saved in a single asset package.
		UPackage* NewPackage = NewAsset->GetOutermost();

		// Mark this package as newly created.
		NewPackage->PackageFlags |= PKG_NewlyCreated;

		const FString NewPackageName = NewPackage->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(NewPackageName, FPackageName::GetAssetPackageExtension());

		// This package not empty, in case it ever was
		RemoveEmptyPackage(NewPackage->GetFName());

		// Add the path to the Path Tree, in case it wasn't already there
		AddAssetPath( FPackageName::GetLongPackagePath(NewPackageName) );

		// Let subscribers know that the new asset was added to the registry
		AssetAddedEvent.Broadcast(FAssetData(NewAsset));

		// Notify listeners that an asset was just created
		InMemoryAssetCreatedEvent.Broadcast(NewAsset);
	}
}

void FAssetRegistry::AssetDeleted(UObject* DeletedAsset)
{
	if ( ensure(DeletedAsset) && DeletedAsset->IsAsset() )
	{
		UPackage* DeletedObjectPackage = DeletedAsset->GetOutermost();
		if ( DeletedObjectPackage != nullptr )
		{
			const FString PackageName = DeletedObjectPackage->GetName();

			// Deleting the last asset in a package causes the package to be garbage collected.
			// If the UPackage object is GCed, it will be considered 'Unloaded' which will cause it to
			// be fully loaded from disk when save is invoked.
			// We want to keep the package around so we can save it empty or delete the file.
			if ( UPackage::IsEmptyPackage( DeletedObjectPackage, DeletedAsset ) )
			{
				AddEmptyPackage( DeletedObjectPackage->GetFName() );

				// If there is a package metadata object, clear the standalone flag so the package can be truly emptied upon GC
				if ( DeletedObjectPackage->MetaData != nullptr )
				{
					DeletedObjectPackage->MetaData->ClearFlags( RF_Standalone );
				}
			}
		}

		// Let subscribers know that the asset was removed from the registry
		AssetRemovedEvent.Broadcast(FAssetData(DeletedAsset));

		// Notify listeners that an in-memory asset was just deleted
		InMemoryAssetDeletedEvent.Broadcast(DeletedAsset);
	}
}

void FAssetRegistry::AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath)
{
	if ( ensure(RenamedAsset) && RenamedAsset->IsAsset() )
	{
		// Add the renamed object to the package file cache because its filename can already be
		// determined by its long package name.
		// @todo AssetRegistry We are assuming it will be saved in a single asset package.
		UPackage* NewPackage = RenamedAsset->GetOutermost();
		const FString NewPackageName = NewPackage->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(NewPackageName, FPackageName::GetAssetPackageExtension());

		RemoveEmptyPackage(NewPackage->GetFName());

		// We want to keep track of empty packages so we can properly merge cached assets with in-memory assets
		FString OldPackageName;
		FString OldAssetName;
		if ( OldObjectPath.Split(TEXT("."), &OldPackageName, &OldAssetName) )
		{
			UPackage* OldPackage = FindPackage(NULL, *OldPackageName);

			if ( UPackage::IsEmptyPackage(OldPackage) )
			{
				AddEmptyPackage(OldPackage->GetFName());
			}
		}

		// Add the path to the Path Tree, in case it wasn't already there
		AddAssetPath( FPackageName::GetLongPackagePath(NewPackageName) );

		AssetRenamedEvent.Broadcast(FAssetData(RenamedAsset), OldObjectPath);
	}
}

bool FAssetRegistry::IsLoadingAssets() const
{
	return !bInitialSearchCompleted;
}

void FAssetRegistry::Tick(float DeltaTime)
{
	double TickStartTime = FPlatformTime::Seconds();

	// Gather results from the background search
	int32 NumFilesToSearch = 0;
	int32 NumPathsToSearch = 0;
	bool bIsSearching = false;
	TArray<double> SearchTimes;
	if ( BackgroundAssetSearch.IsValid() )
	{		
		bIsSearching = BackgroundAssetSearch->GetAndTrimSearchResults(BackgroundAssetResults, BackgroundPathResults, BackgroundDependencyResults, SearchTimes, NumFilesToSearch, NumPathsToSearch);
	}

	// Report the search times
	for (int32 SearchTimeIdx = 0; SearchTimeIdx < SearchTimes.Num(); ++SearchTimeIdx)
	{
		UE_LOG(LogAssetRegistry, Verbose, TEXT("### Background search completed in %0.4f seconds"), SearchTimes[SearchTimeIdx]);
	}

	// Add discovered paths
	if ( BackgroundPathResults.Num() )
	{
		PathDataGathered(TickStartTime, BackgroundPathResults);
	}

	// Process the asset results
	if ( BackgroundAssetResults.Num() )
	{
		// Mark the first amortize time
		if ( AmortizeStartTime == 0 )
		{
			AmortizeStartTime = FPlatformTime::Seconds();
		}

		AssetSearchDataGathered(TickStartTime, BackgroundAssetResults);

		if ( BackgroundAssetResults.Num() == 0 )
		{
			TotalAmortizeTime += FPlatformTime::Seconds() - AmortizeStartTime;
			AmortizeStartTime = 0;
		}

		// Notify the status change
		int32 NumDiscoveredAssets = CachedAssetsByObjectPath.Num();
		// Total assets may not be accurate if some files to search are not UAsset files. This number may fluctuate a little as packages are searched.
		int32 TotalAssets = NumDiscoveredAssets + BackgroundAssetResults.Num() + NumFilesToSearch;
		FileLoadProgressUpdatedEvent.Broadcast(NumDiscoveredAssets, TotalAssets);
	}

	// Add dependencies
	if ( BackgroundDependencyResults.Num() )
	{
		DependencyDataGathered(TickStartTime, BackgroundDependencyResults);
	}

	// If completing an initial search, refresh the content browser
	if ( NumFilesToSearch == 0 && NumPathsToSearch == 0 && !bIsSearching && BackgroundPathResults.Num() == 0 && BackgroundAssetResults.Num() == 0 && BackgroundDependencyResults.Num() == 0 )
	{
		if ( !bInitialSearchCompleted )
		{
			UE_LOG(LogAssetRegistry, Verbose, TEXT("### Time spent amortizing search results: %0.4f seconds"), TotalAmortizeTime);
			UE_LOG(LogAssetRegistry, Log, TEXT("Asset discovery search completed in %0.4f seconds"), FPlatformTime::Seconds() - FullSearchStartTime);

			bInitialSearchCompleted = true;

			FileLoadedEvent.Broadcast();
		}
	}
}


bool FAssetRegistry::IsUsingWorldAssets()
{
	return !FParse::Param(FCommandLine::Get(), TEXT("DisableWorldAssets"));
}

void FAssetRegistry::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		check(CachedAssetsByObjectPath.Num() == NumAssets);
		SaveRegistryData(Ar, CachedAssetsByObjectPath);
	}
	// load in by building the TMap
	else
	{
		// serialize number of objects
		int32 LocalNumAssets = 0;
		Ar << LocalNumAssets;

		// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
		PreallocatedAssetDataBuffer = new FAssetData[LocalNumAssets];

		for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; AssetIndex++)
		{
			// make a new asset data object
			FAssetData* NewAssetData = &PreallocatedAssetDataBuffer[AssetIndex];

			// load it
			Ar << *NewAssetData;

			AddAssetData(NewAssetData);

			AddAssetPath(NewAssetData->PackagePath.ToString());
		}
	}
}

void FAssetRegistry::SaveRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data )
{
	// serialize number of objects
	int32 AssetCount = Data.Num();
	Ar << AssetCount;

	// save out by walking the TMap
	for (TMap<FName, FAssetData*>::TIterator It(Data); It; ++It)
	{
		Ar << *It.Value();
	}
}

void FAssetRegistry::LoadRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data )
{
	check(Ar.IsLoading());
	// serialize number of objects
	int AssetCount = 0;
	Ar << AssetCount;

	for (int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++)
	{
		// make a new asset data object
		FAssetData *NewAssetData = new FAssetData();

		// load it
		Ar << *NewAssetData;

		Data.Add(NewAssetData->ObjectPath, NewAssetData);
	}
}

void FAssetRegistry::ScanPathsSynchronous_Internal(const TArray<FString>& InPaths, bool bForceRescan, bool bUseCache)
{
	const double SearchStartTime = FPlatformTime::Seconds();

	// Only scan paths that were not previously synchronously scanned, unless we were asked to force rescan.
	TArray<FString> PathsToScan;
	for ( auto PathIt = InPaths.CreateConstIterator(); PathIt; ++PathIt )
	{
		if ( bForceRescan || !SynchronouslyScannedPaths.Contains(*PathIt) )
		{
			PathsToScan.Add(*PathIt);
			SynchronouslyScannedPaths.Add(*PathIt);
		}
	}

	if ( PathsToScan.Num() > 0 )
	{
		// Start the sync asset search
		FAssetDataGatherer AssetSearch(PathsToScan, /*bSynchronous=*/true, bUseCache);

		// Get the search results
		TArray<FBackgroundAssetData*> AssetResults;
		TArray<FString> PathResults;
		TArray<FPackageDependencyData> DependencyResults;
		TArray<double> SearchTimes;
		int32 NumFilesToSearch = 0;
		int32 NumPathsToSearch = 0;
		AssetSearch.GetAndTrimSearchResults(AssetResults, PathResults, DependencyResults, SearchTimes, NumFilesToSearch, NumPathsToSearch);

		// Cache the search results
		const int32 NumResults = AssetResults.Num();
		AssetSearchDataGathered(-1, AssetResults);
		PathDataGathered(-1, PathResults);
		DependencyDataGathered(-1, DependencyResults);

		// Log stats
		const FString& Path = PathsToScan[0];
		FString PathsString;
		if ( PathsToScan.Num() > 1 )
		{
			PathsString = FString::Printf(TEXT("'%s' and %d other paths"), *Path, PathsToScan.Num());
		}
		else
		{
			PathsString = FString::Printf(TEXT("'%s'"), *Path);
		}

		UE_LOG(LogAssetRegistry, Log, TEXT("ScanPathsSynchronous completed scanning %s to find %d assets in %0.4f seconds"), *PathsString, NumResults, FPlatformTime::Seconds() - SearchStartTime);
	}
}

void FAssetRegistry::AssetSearchDataGathered(const double TickStartTime, TArray<FBackgroundAssetData*>& AssetResults)
{
	const bool bFlushFullBuffer = TickStartTime < 0;
	TSet<FName> ModifiedPaths;

	// Add the found assets
	int32 AssetIndex = 0;
	for (AssetIndex = 0; AssetIndex < AssetResults.Num(); ++AssetIndex)
	{
		FBackgroundAssetData*& BackgroundResult = AssetResults[AssetIndex];
		FAssetData Result = BackgroundResult->ToAssetData();

		// Try to update any asset data that may already exist
		FAssetData* AssetData = NULL;
		FAssetData** AssetDataPtr = CachedAssetsByObjectPath.Find(Result.ObjectPath);
		if (AssetDataPtr != NULL)
		{
			AssetData = *AssetDataPtr;
		}

		if ( AssetData != NULL )
		{
			// The asset exists in the cache, update it
			UpdateAssetData(AssetData, Result);
		}
		else
		{
			// The asset isn't in the cache yet, add it and notify subscribers
			AddAssetData(new FAssetData(Result));
		}

		// Populate the path tree
		AddAssetPath(BackgroundResult->PackagePath);

		// Delete the result that was originally created by an FPackageReader
		delete BackgroundResult;
		BackgroundResult = NULL;

		// Check to see if we have run out of time in this tick
		if ( !bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
		{
			// Increment the index to properly trim the buffer below
			++AssetIndex;
			break;
		}
	}

	// Trim the results array
	if (AssetIndex > 0)
	{
		AssetResults.RemoveAt(0, AssetIndex);
	}
}

void FAssetRegistry::PathDataGathered(const double TickStartTime, TArray<FString>& PathResults)
{
	const bool bFlushFullBuffer = TickStartTime < 0;
	int32 ResultIdx = 0;
	for (ResultIdx = 0; ResultIdx < PathResults.Num(); ++ResultIdx)
	{
		const FString& Path = PathResults[ResultIdx];
		AddAssetPath(Path);
		
		// Check to see if we have run out of time in this tick
		if ( !bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
		{
			// Increment the index to properly trim the buffer below
			++ResultIdx;
			break;
		}
	}

	// Trim the results array
	PathResults.RemoveAt(0, ResultIdx);
}

void FAssetRegistry::DependencyDataGathered(const double TickStartTime, TArray<FPackageDependencyData>& DependsResults)
{
	const bool bFlushFullBuffer = TickStartTime < 0;

	int32 ResultIdx = 0;
	for (ResultIdx = 0; ResultIdx < DependsResults.Num(); ++ResultIdx)
	{
		FPackageDependencyData& Result = DependsResults[ResultIdx];

		FDependsNode* Node = CreateOrFindDependsNode(Result.PackageName);

		// We will populate the node dependencies below. Empty the set here in case this file was already read
		// Also remove references to all existing dependencies, those will be also repopulated below
		for (auto OldDependsIt = Node->Dependencies.CreateConstIterator(); OldDependsIt; ++OldDependsIt)
		{
			(*OldDependsIt)->Referencers.Remove(Node);
		}
		Node->Dependencies.Empty();

		// Determine the new package dependencies
		TSet<FName> PackageDependencies;
		for (int32 ImportIdx = 0; ImportIdx < Result.ImportMap.Num(); ++ImportIdx)
		{
			PackageDependencies.Add( Result.GetImportPackageName(ImportIdx) );
		}
		for (int32 StringAssetRefIdx = 0; StringAssetRefIdx < Result.StringAssetReferencesMap.Num(); ++StringAssetRefIdx)
		{
			FString PackageName, ObjName;
			Result.StringAssetReferencesMap[StringAssetRefIdx].Split(".", &PackageName, &ObjName, ESearchCase::CaseSensitive);
			PackageDependencies.Add(*PackageName);
		}


		// Doubly-link all new dependencies for this package
		for (auto NewDependsIt = PackageDependencies.CreateConstIterator(); NewDependsIt; ++NewDependsIt)
		{
			FDependsNode* DependsNode = CreateOrFindDependsNode(*NewDependsIt);
			
			if (DependsNode != NULL)
			{
				Node->Dependencies.Add(DependsNode);
				DependsNode->Referencers.Add(Node);
			}
		}

		// Check to see if we have run out of time in this tick
		if ( !bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
		{
			// Increment the index to properly trim the buffer below
			++ResultIdx;
			break;
		}
	}

	// Trim the results array
	DependsResults.RemoveAt(0, ResultIdx);
}

FDependsNode* FAssetRegistry::CreateOrFindDependsNode(FName ObjectPath)
{
	FDependsNode** FoundNode = CachedDependsNodes.Find(ObjectPath);
	if ( FoundNode )
	{
		return *FoundNode;
	}

	FDependsNode* NewNode = new FDependsNode(ObjectPath);
	NumDependsNodes++;
	CachedDependsNodes.Add(ObjectPath, NewNode);

	return NewNode;
}

bool FAssetRegistry::RemoveDependsNode( FName PackageName )
{
	FDependsNode** NodePtr = CachedDependsNodes.Find( PackageName );

	if ( NodePtr != nullptr )
	{
		FDependsNode* Node = *NodePtr;
		if ( Node != nullptr )
		{
			TArray<FDependsNode*> DependencyNodes;
			Node->GetDependencies( DependencyNodes );

			// Remove the reference to this node from all dependencies
			for ( FDependsNode* DependencyNode : DependencyNodes )
			{
				DependencyNode->Referencers.Remove( Node );
			}

			TArray<FDependsNode*> ReferencerNodes;
			Node->GetReferencers( ReferencerNodes );

			// Remove the reference to this node from all referencers
			for ( FDependsNode* ReferencerNode : ReferencerNodes )
			{
				ReferencerNode->Dependencies.Remove( Node );
			}

			// Remove the node and delete it
			CachedDependsNodes.Remove( PackageName );
			NumDependsNodes--;
			delete Node;

			return true;
		}
	}
	
	return false;
}

void FAssetRegistry::AddEmptyPackage(FName PackageName)
{
	CachedEmptyPackages.Add(PackageName);
}

bool FAssetRegistry::RemoveEmptyPackage(FName PackageName)
{
	return CachedEmptyPackages.Remove( PackageName ) > 0;
}

bool FAssetRegistry::AddAssetPath(const FString& PathToAdd)
{
	if ( PathTreeRoot.CachePath(PathToAdd) )
	{		
		PathAddedEvent.Broadcast(PathToAdd);
		return true;
	}

	return false;
}

bool FAssetRegistry::RemoveAssetPath(const FString& PathToRemove, bool bEvenIfAssetsStillExist)
{
	if ( !bEvenIfAssetsStillExist )
	{
		// Check if there were assets in the specified folder. You can not remove paths that still contain assets
		TArray<FAssetData> AssetsInPath;
		GetAssetsByPath(FName(*PathToRemove), AssetsInPath, true);
		if ( AssetsInPath.Num() > 0 )
		{
			// At least one asset still exists in the path. Fail the remove.
			return false;
		}
	}

	if ( PathTreeRoot.RemoveFolder(PathToRemove) )
	{
		PathRemovedEvent.Broadcast(PathToRemove);
		return true;
	}
	else
	{
		// The folder did not exist in the tree, fail the remove
		return false;
	}
}

FString FAssetRegistry::ExportTextPathToObjectName(const FString& InExportTextPath) const
{
	const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(InExportTextPath);
	return FPackageName::ObjectPathToObjectName(ObjectPath);
}

void FAssetRegistry::AddAssetData(FAssetData* AssetData)
{
	NumAssets++;

	auto& PackageAssets = CachedAssetsByPackageName.FindOrAdd(AssetData->PackageName);
	auto& PathAssets = CachedAssetsByPath.FindOrAdd(AssetData->PackagePath);
	auto& ClassAssets = CachedAssetsByClass.FindOrAdd(AssetData->AssetClass);

	CachedAssetsByObjectPath.Add(AssetData->ObjectPath, AssetData);
	PackageAssets.Add(AssetData);
	PathAssets.Add(AssetData);
	ClassAssets.Add(AssetData);

	for (TMap<FName, FString>::TConstIterator TagIt(AssetData->TagsAndValues); TagIt; ++TagIt)
	{
		FName Key = TagIt.Key();

		auto& TagAssets = CachedAssetsByTag.FindOrAdd(Key);
		TagAssets.Add(AssetData);

#if WITH_EDITORONLY_DATA
		// Don't check FName number so we still accumulate them even if there are multiple
		if (GIsEditor && TagIt.Key().IsEqual(UObject::SourceFileTagName(), ENameCase::IgnoreCase, false))
		{
			auto& Assets = CachedAssetsBySourceFileName.FindOrAdd(*FPaths::GetCleanFilename(TagIt.Value()));
			Assets.Add(AssetData);
		}
#endif
	}

	// Notify subscribers
	AssetAddedEvent.Broadcast(*AssetData);

	// Populate the class map if adding blueprint
	if (ClassGeneratorNames.Contains(AssetData->AssetClass))
	{
		FString* GeneratedClassPtr = AssetData->TagsAndValues.Find("GeneratedClass");
		FString* ParentClassPtr = AssetData->TagsAndValues.Find("ParentClass");
		if ( GeneratedClassPtr && ParentClassPtr )
		{
			const FName GeneratedClassFName = FName(*ExportTextPathToObjectName(*GeneratedClassPtr));
			const FName ParentClassFName = FName(*ExportTextPathToObjectName(*ParentClassPtr));
			CachedInheritanceMap.Add(GeneratedClassFName, ParentClassFName);
		}
	}
}

void FAssetRegistry::UpdateAssetData(FAssetData* AssetData, const FAssetData& NewAssetData)
{
	// Update ObjectPath
	if ( AssetData->PackageName != NewAssetData.PackageName || AssetData->AssetName != NewAssetData.AssetName)
	{
		CachedAssetsByObjectPath.Remove(AssetData->ObjectPath);
		CachedAssetsByObjectPath.Add(NewAssetData.ObjectPath, AssetData);
	}

	// Update PackageName
	if ( AssetData->PackageName != NewAssetData.PackageName )
	{
		auto OldPackageAssets = CachedAssetsByPackageName.Find(AssetData->PackageName);
		auto& NewPackageAssets = CachedAssetsByPackageName.FindOrAdd(NewAssetData.PackageName);
		
		OldPackageAssets->Remove(AssetData);
		NewPackageAssets.Add(AssetData);
	}
	
	// Update PackagePath
	if ( AssetData->PackagePath != NewAssetData.PackagePath )
	{
		auto OldPathAssets = CachedAssetsByPath.Find(AssetData->PackagePath);
		auto& NewPathAssets = CachedAssetsByPath.FindOrAdd(NewAssetData.PackagePath);

		OldPathAssets->Remove(AssetData);
		NewPathAssets.Add(AssetData);
	}

	// Update AssetClass
	if ( AssetData->AssetClass != NewAssetData.AssetClass )
	{
		auto OldClassAssets = CachedAssetsByClass.Find(AssetData->AssetClass);
		auto& NewClassAssets = CachedAssetsByClass.FindOrAdd(NewAssetData.AssetClass);
		
		OldClassAssets->Remove(AssetData);
		NewClassAssets.Add(AssetData);
	}

	// Update Tags
	{
		const FName& SourceFileTagName = UObject::SourceFileTagName();
		TArray<FString> OldSourcePaths, NewSourcePaths;

		// Remove any tags that no longer exist
		for (const auto& Pair : AssetData->TagsAndValues)
		{
#if WITH_EDITORONLY_DATA
			// Accumulate a list of old source filenames
			if (GIsEditor && Pair.Key.IsEqual(SourceFileTagName, ENameCase::IgnoreCase, false))
			{
				OldSourcePaths.Add(Pair.Value);
			}
#endif
			if (!NewAssetData.TagsAndValues.Contains(Pair.Key))
			{
				auto OldTagAssets = CachedAssetsByTag.Find(Pair.Key);
				OldTagAssets->Remove(AssetData);
			}
		}

		// Add new tags
		for (const auto& Pair : NewAssetData.TagsAndValues)
		{
#if WITH_EDITORONLY_DATA
			// Accumulate a list of old source filenames
			if (GIsEditor && Pair.Key.IsEqual(SourceFileTagName, ENameCase::IgnoreCase, false))
			{
				NewSourcePaths.Add(Pair.Value);
			}
#endif
			if (!AssetData->TagsAndValues.Contains(Pair.Key))
			{
				auto& NewTagAssets = CachedAssetsByTag.FindOrAdd(Pair.Key);
				NewTagAssets.Add(AssetData);
			}
		}

#if WITH_EDITORONLY_DATA
		// Fixup source file -> asset mappings
		for (const auto& SourcePath : OldSourcePaths)
		{
			if (!NewSourcePaths.Contains(SourcePath))
			{
				FName CleanFilename = *FPaths::GetCleanFilename(SourcePath);
				auto* Assets = CachedAssetsBySourceFileName.Find(CleanFilename);
				if (Assets)
				{
					Assets->Remove(AssetData);
					if (Assets->Num() == 0)
					{
						CachedAssetsBySourceFileName.Remove(CleanFilename);
					}
				}
			}
		}
		for (const auto& SourcePath : NewSourcePaths)
		{
			if (!OldSourcePaths.Contains(SourcePath))
			{
				auto& Assets = CachedAssetsBySourceFileName.FindOrAdd(*FPaths::GetCleanFilename(SourcePath));
				Assets.Add(AssetData);
			}
		}
#endif
	}

	// Update the class map if updating a blueprint
	if (ClassGeneratorNames.Contains(AssetData->AssetClass))
	{
		FString* OldGeneratedClassPtr = AssetData->TagsAndValues.Find("GeneratedClass");

		if ( OldGeneratedClassPtr )
		{
			const FName OldGeneratedClassFName = FName(*ExportTextPathToObjectName(*OldGeneratedClassPtr));
			CachedInheritanceMap.Remove(OldGeneratedClassFName);
		}

		const FString* NewGeneratedClassPtr = NewAssetData.TagsAndValues.Find("GeneratedClass");
		const FString* NewParentClassPtr = NewAssetData.TagsAndValues.Find("ParentClass");
		if ( NewGeneratedClassPtr && NewParentClassPtr )
		{
			const FName NewGeneratedClassFName = FName(*ExportTextPathToObjectName(*NewGeneratedClassPtr));
			const FName NewParentClassFName = FName(*ExportTextPathToObjectName(*NewParentClassPtr));
			CachedInheritanceMap.Add(NewGeneratedClassFName, NewParentClassFName);
		}
	}

	// Copy in new values
	*AssetData = NewAssetData;
}

bool FAssetRegistry::RemoveAssetData(FAssetData* AssetData)
{
	bool bRemoved = false;

	if ( ensure(AssetData) )
	{
		// Notify subscribers
		AssetRemovedEvent.Broadcast(*AssetData);

		// Remove from the class map if removing a blueprint
		if (ClassGeneratorNames.Contains(AssetData->AssetClass))
		{
			FString* OldGeneratedClassPtr = AssetData->TagsAndValues.Find("GeneratedClass");

			if ( OldGeneratedClassPtr )
			{
				const FName OldGeneratedClassFName = FName(*ExportTextPathToObjectName(*OldGeneratedClassPtr));
				CachedInheritanceMap.Remove(OldGeneratedClassFName);
			}
		}

		auto OldPackageAssets = CachedAssetsByPackageName.Find(AssetData->PackageName);
		auto OldPathAssets = CachedAssetsByPath.Find(AssetData->PackagePath);
		auto OldClassAssets = CachedAssetsByClass.Find(AssetData->AssetClass);

		CachedAssetsByObjectPath.Remove(AssetData->ObjectPath);
		OldPackageAssets->Remove(AssetData);
		OldPathAssets->Remove(AssetData);
		OldClassAssets->Remove(AssetData);

		const FName& SourceFileTagName = UObject::SourceFileTagName();
		for (TMap<FName, FString>::TConstIterator TagIt(AssetData->TagsAndValues); TagIt; ++TagIt)
		{
			auto OldTagAssets = CachedAssetsByTag.Find(TagIt.Key());
			OldTagAssets->Remove(AssetData);


#if WITH_EDITORONLY_DATA
			// Don't check FName number so we still remove them even if there are multiple
			if (GIsEditor && TagIt.Key().IsEqual(SourceFileTagName, ENameCase::IgnoreCase, false))
			{
				FName CleanFilename = *FPaths::GetCleanFilename(TagIt.Value());
				auto* Assets = CachedAssetsBySourceFileName.Find(CleanFilename);
				if (Assets)
				{
					Assets->Remove(AssetData);
					if (Assets->Num() == 0)
					{
						CachedAssetsBySourceFileName.Remove(CleanFilename);
					}
				}
			}
#endif
		}

		// We need to update the cached dependencies references cache so that they know we no
		// longer exist and so don't reference them.
		RemoveDependsNode( AssetData->PackageName );

		// if the assets were preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
		if (PreallocatedAssetDataBuffer == NULL)
		{
			delete AssetData;
		}
		NumAssets--;
	}

	return bRemoved;
}

void FAssetRegistry::AddPathToSearch(const FString& Path)
{
	if ( BackgroundAssetSearch.IsValid() )
	{
		BackgroundAssetSearch->AddPathToSearch(Path);
	}
}

void FAssetRegistry::AddFilesToSearch (const TArray<FString>& Files)
{
	if ( BackgroundAssetSearch.IsValid() )
	{
		BackgroundAssetSearch->AddFilesToSearch(Files);
	}
}

#if WITH_EDITOR

void FAssetRegistry::OnDirectoryChanged (const TArray<FFileChangeData>& FileChanges)
{
	// Take local copy of FileChanges array as we wish to collapse pairs of 'Removed then Added' FileChangeData
	// entries into a single 'Modified' entry.
	TArray<FFileChangeData> FileChangesProcessed(FileChanges);

	for (int32 FileEntryIndex = 0; FileEntryIndex < FileChangesProcessed.Num(); FileEntryIndex++)
	{
		if (FileChangesProcessed[FileEntryIndex].Action == FFileChangeData::FCA_Added)
		{
			// Search back through previous entries to see if this Added can be paired with a previous Removed
			const FString& FilenameToCompare = FileChangesProcessed[FileEntryIndex].Filename;
			for (int32 SearchIndex = FileEntryIndex - 1; SearchIndex >= 0; SearchIndex--)
			{
				if (FileChangesProcessed[SearchIndex].Action == FFileChangeData::FCA_Removed &&
					FileChangesProcessed[SearchIndex].Filename == FilenameToCompare)
				{
					// Found a Removed which matches the Added - change the Added file entry to be a Modified...
					FileChangesProcessed[FileEntryIndex].Action = FFileChangeData::FCA_Modified;

					// ...and remove the Removed entry
					FileChangesProcessed.RemoveAt(SearchIndex);
					FileEntryIndex--;
					break;
				}
			}
		}
	}

	TArray<FString> FilteredFiles;
	for (int32 FileIdx = 0; FileIdx < FileChangesProcessed.Num(); ++FileIdx)
	{
		FString LongPackageName;
		const FString File = FString(FileChangesProcessed[FileIdx].Filename);
		const bool bIsPackageFile = FPackageName::IsPackageExtension(*FPaths::GetExtension(File, true));
		const bool bIsValidPackageName = FPackageName::TryConvertFilenameToLongPackageName(File, LongPackageName);
		const bool bIsValidPackage = bIsPackageFile && bIsValidPackageName;

		if ( bIsValidPackage )
		{
			switch( FileChangesProcessed[FileIdx].Action )
			{
				case FFileChangeData::FCA_Added:
					// This is a package file that was created on disk. Mark it to be scanned for asset data.
					FilteredFiles.AddUnique(File);

					// Make sure this file is added to the package file cache since it is new.
					UE_LOG(LogAssetRegistry, Verbose, TEXT("File was added to content directory: %s"), *File);
					break;

				case FFileChangeData::FCA_Modified:
					// This is a package file that changed on disk. Mark it to be scanned for asset data.
					FilteredFiles.AddUnique(File);
					UE_LOG(LogAssetRegistry, Verbose, TEXT("File changed in content directory: %s"), *File);
					break;

				case FFileChangeData::FCA_Removed:
					{
						// This file was deleted. Remove all assets in the package from the registry.
						const FName PackageName = FName(*LongPackageName);
						TArray<FAssetData*>* PackageAssetsPtr = CachedAssetsByPackageName.Find(PackageName);

						if (PackageAssetsPtr)
						{
							TArray<FAssetData*>& PackageAssets = *PackageAssetsPtr;
							for ( int32 AssetIdx = PackageAssets.Num() - 1; AssetIdx >= 0; --AssetIdx )
							{
								RemoveAssetData(PackageAssets[AssetIdx]);
							}
						}

						UE_LOG(LogAssetRegistry, Verbose, TEXT("File was removed from content directory: %s"), *File);
					}
					break;
			}
		}
	}

	if ( FilteredFiles.Num() )
	{
		AddFilesToSearch(FilteredFiles);
	}
}

#endif // WITH_EDITOR


void FAssetRegistry::OnContentPathMounted( const FString& InAssetPath, const FString& FileSystemPath )
{
	// Sanitize
	FString AssetPath = InAssetPath;
	if (AssetPath.EndsWith(TEXT("/")) == false)
	{
		// We actually want a trailing slash here so the path can be properly converted while searching for assets
		AssetPath = AssetPath + TEXT("/");
	}

	// Add this to our list of root paths to process
	AddPathToSearch( AssetPath );

	// Listen for directory changes in this content path
#if WITH_EDITOR
	// Commandlets and in-game don't listen for directory changes
	if ( !IsRunningCommandlet() && GIsEditor)
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			// If the path doesn't exist on disk, make it so the watcher will work.
			IFileManager::Get().MakeDirectory(*FileSystemPath);
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle( FileSystemPath, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FAssetRegistry::OnDirectoryChanged), OnContentPathMountedOnDirectoryChangedDelegateHandle);
		}
	}
#endif // WITH_EDITOR

}

void FAssetRegistry::OnContentPathDismounted(const FString& InAssetPath, const FString& FileSystemPath)
{
	// Sanitize
	FString AssetPath = InAssetPath;
	if ( AssetPath.EndsWith(TEXT("/")) )
	{
		// We don't want a trailing slash here as it could interfere with RemoveAssetPath
		AssetPath = AssetPath.LeftChop(1);
	}

	// Remove all cached assets found at this location
	{
		TArray<FAssetData*> AllAssetDataToRemove;
		TArray<FString> PathList;
		const bool bRecurse = true;
		GetSubPaths(AssetPath, PathList, bRecurse);
		PathList.Add(AssetPath);
		for ( const FString& Path : PathList )
		{
			TArray<FAssetData*>* AssetsInPath = CachedAssetsByPath.Find(FName(*Path));
			if ( AssetsInPath )
			{
				AllAssetDataToRemove.Append(*AssetsInPath);
			}
		}

		for ( FAssetData* AssetData : AllAssetDataToRemove )
		{
			RemoveAssetData(AssetData);
		}
	}

	// Remove the root path
	{
		const bool bEvenIfAssetsStillExist = true;
		RemoveAssetPath(AssetPath, bEvenIfAssetsStillExist);
	}

	// Stop listening for directory changes in this content path
#if WITH_EDITOR
	// Commandlets and in-game don't listen for directory changes
	if (!IsRunningCommandlet() && GIsEditor)
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(FileSystemPath, OnContentPathMountedOnDirectoryChangedDelegateHandle);
		}
	}
#endif // WITH_EDITOR

}


bool FAssetRegistry::IsFilterValid(const FARFilter& Filter) const
{
	for ( int32 NameIdx = 0; NameIdx < Filter.PackageNames.Num(); ++NameIdx )
	{
		if (Filter.PackageNames[NameIdx] == NAME_None)
		{
			return false;
		}
	}

	for ( int32 PathIdx = 0; PathIdx < Filter.PackagePaths.Num(); ++PathIdx )
	{
		if (Filter.PackagePaths[PathIdx] == NAME_None)
		{
			return false;
		}
	}

	for ( int32 ObjectPathIdx = 0; ObjectPathIdx < Filter.ObjectPaths.Num(); ++ObjectPathIdx )
	{
		if (Filter.ObjectPaths[ObjectPathIdx] == NAME_None)
		{
			return false;
		}
	}

	for ( int32 ClassIdx = 0; ClassIdx < Filter.ClassNames.Num(); ++ClassIdx )
	{
		if (Filter.ClassNames[ClassIdx] == NAME_None)
		{
			return false;
		}
	}

	for (auto FilterTagIt = Filter.TagsAndValues.CreateConstIterator(); FilterTagIt; ++FilterTagIt)
	{
		if (FilterTagIt.Key() == NAME_None)
		{
			return false;
		}
	}

	return true;
}

void FAssetRegistry::GetSubClasses(const TArray<FName>& InClassNames, const TSet<FName>& ExcludedClassNames, TSet<FName>& SubClassNames) const
{
	// Build a reverse map of classes to their children for quick lookup
	TMap<FName, TSet<FName>> ReverseInheritanceMap;

	// And add all in-memory classes at request time
	TSet<FName> InMemoryClassNames;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if ( !ClassIt->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) )
		{
			if ( ClassIt->GetSuperClass() )
			{
				TSet<FName>& ChildClasses = ReverseInheritanceMap.FindOrAdd( ClassIt->GetSuperClass()->GetFName() );
				ChildClasses.Add( ClassIt->GetFName() );
			}

			InMemoryClassNames.Add(ClassIt->GetFName());
		}
	}

	// Form a child list for all cached classes
	for (auto ClassNameIt = CachedInheritanceMap.CreateConstIterator(); ClassNameIt; ++ClassNameIt)
	{
		const FName ClassName = ClassNameIt.Key();
		if ( !InMemoryClassNames.Contains(ClassName) )
		{
			const FName ParentClassName = ClassNameIt.Value();
			if ( ParentClassName != NAME_None )
			{
				TSet<FName>& ChildClasses = ReverseInheritanceMap.FindOrAdd( ParentClassName );
				ChildClasses.Add( ClassName );
			}
		}
	}

	for (auto ClassNameIt = InClassNames.CreateConstIterator(); ClassNameIt; ++ClassNameIt)
	{
		// Now find all subclass names
		GetSubClasses_Recursive(*ClassNameIt, SubClassNames, ReverseInheritanceMap, ExcludedClassNames);
	}
}

void FAssetRegistry::GetSubClasses_Recursive(FName InClassName, TSet<FName>& SubClassNames, const TMap<FName, TSet<FName>>& ReverseInheritanceMap, const TSet<FName>& ExcludedClassNames) const
{
	if ( ExcludedClassNames.Contains(InClassName) )
	{
		// This class is in the exclusion list. Exclude it.
	}
	else
	{
		SubClassNames.Add(InClassName);

		const TSet<FName>* FoundSubClassNames = ReverseInheritanceMap.Find(InClassName);
		if ( FoundSubClassNames )
		{
			for (auto ClassNameIt = (*FoundSubClassNames).CreateConstIterator(); ClassNameIt; ++ClassNameIt)
			{
				GetSubClasses_Recursive(*ClassNameIt, SubClassNames, ReverseInheritanceMap, ExcludedClassNames);
			}
		}
	}
}
