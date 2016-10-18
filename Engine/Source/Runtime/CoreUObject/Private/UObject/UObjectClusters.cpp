// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectClusters.cpp: Unreal UObject Cluster helper functions
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "Interface.h"
#include "ModuleManager.h"
#include "FastReferenceCollector.h"
#include "GarbageCollection.h"
#include "ReferenceChainSearch.h"

int32 GCreateGCClusters = 1;
static FAutoConsoleVariableRef CCreateGCClusters(
	TEXT("gc.CreateGCClusters"),
	GCreateGCClusters,
	TEXT("If true, the engine will attempt to create clusters of objects for better garbage collection performance."),
	ECVF_Default
	);

int32 GMergeGCClusters = 0;
static FAutoConsoleVariableRef CMergeGCClusters(
	TEXT("gc.MergeGCClusters"),
	GMergeGCClusters,
	TEXT("If true, when creating clusters, the clusters referenced from another cluster will get merged into one big cluster."),
	ECVF_Default
	);

#if !UE_BUILD_SHIPPING

// Dumps all clusters to log.
static void ListClusters(const TArray<FString>& Args)
{
	const bool bHierarchy = Args.Num() && Args[0] == TEXT("Hierarchy");
	int32 MaxInterClusterReferences = 0;
	int32 TotalInterClusterReferences = 0;
	int32 MaxClusterSize = 0;
	int32 TotalClusterObjects = 0;	
	for (TPair<int32, FUObjectCluster*>& Pair : GUObjectClusters)
	{
		FUObjectItem* RootItem = GUObjectArray.IndexToObjectUnsafeForGC(Pair.Key);
		UObject* RootObject = static_cast<UObject*>(RootItem->Object);
		UE_LOG(LogObj, Display, TEXT("%s (Index: %d), Size: %d, ReferencedClusters: %d"), *RootObject->GetFullName(), Pair.Key, Pair.Value->Objects.Num(), Pair.Value->ReferencedClusters.Num());
		MaxInterClusterReferences = FMath::Max(MaxInterClusterReferences, Pair.Value->ReferencedClusters.Num());
		TotalInterClusterReferences += Pair.Value->ReferencedClusters.Num();
		MaxClusterSize = FMath::Max(MaxClusterSize, Pair.Value->Objects.Num());
		TotalClusterObjects += Pair.Value->Objects.Num();
		if (bHierarchy)
		{
			int32 Index = 0;
			for (int32 ObjectIndex : Pair.Value->Objects)
			{
				FUObjectItem* ObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ObjectIndex);
				UObject* Object = static_cast<UObject*>(ObjectItem->Object);
				UE_LOG(LogObj, Display, TEXT("    [%.4d]: %s (Index: %d)"), Index++, *Object->GetFullName(), ObjectIndex);
			}
			for (int32 ClusterIndex : Pair.Value->ReferencedClusters)
			{
				FUObjectItem* ClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(ClusterIndex);
				UObject* ClusterRootObject = static_cast<UObject*>(ClusterRootItem->Object);
				UE_LOG(LogObj, Display, TEXT("    -> %s (Index: %d)"), *ClusterRootObject->GetFullName(), ClusterIndex);
			}
		}
	}
	UE_LOG(LogObj, Display, TEXT("Number of clusters: %d"), GUObjectClusters.Num());
	UE_LOG(LogObj, Display, TEXT("Maximum cluster size: %d"), MaxClusterSize);
	UE_LOG(LogObj, Display, TEXT("Average cluster size: %d"), GUObjectClusters.Num() ? (TotalClusterObjects / GUObjectClusters.Num()) : 0);
	UE_LOG(LogObj, Display, TEXT("Number of objects in GC clusters: %d"), TotalClusterObjects);
	UE_LOG(LogObj, Display, TEXT("Maximum number of custer-to-cluster references: %d"), MaxInterClusterReferences);
	UE_LOG(LogObj, Display, TEXT("Average number of custer-to-cluster references: %d"), GUObjectClusters.Num() ? (TotalInterClusterReferences / GUObjectClusters.Num()) : 0);
}

static void FindStaleClusters(const TArray<FString>& Args)
{
	// This is seriously slow.
	UE_LOG(LogObj, Display, TEXT("Searching for stale clusters. This may take a while...")); 
	int32 NumStaleClusters = 0;
	int32 TotalNumClusters = 0;
	for (FRawObjectIterator It(true); It; ++It)
	{
		FUObjectItem* ObjectItem = *It;
		if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
		{
			TotalNumClusters++;

			UObject* ClusterRootObject = static_cast<UObject*>(ObjectItem->Object);
			FReferenceChainSearch SearchRefs(ClusterRootObject, FReferenceChainSearch::ESearchMode::Shortest);
			
			bool bReferenced = false;
			if (SearchRefs.GetReferenceChains().Num() > 0)
			{
				for (const FReferenceChainSearch::FReferenceChain& ReferenceChain : SearchRefs.GetReferenceChains())
				{
					UObject* ReferencingObj = ReferenceChain.RefChain[0].ReferencedBy;
					// Ignore internal references
					if (!ReferencingObj->IsIn(ClusterRootObject) && ReferencingObj != ClusterRootObject)
					{
						bReferenced = true;
						break;
					}
				}
			}
			if (!bReferenced)
			{
				NumStaleClusters++;
				UE_LOG(LogObj, Display, TEXT("Cluster %s has no external references:"), *ClusterRootObject->GetFullName());
				SearchRefs.PrintResults();
			}
		}
	}
	UE_LOG(LogObj, Display, TEXT("Found %d clusters, including %d stale."), TotalNumClusters, NumStaleClusters);
}

static FAutoConsoleCommand ListClustersCommand(
	TEXT("gc.ListClusters"),
	TEXT("Dumps all clusters do output log. When 'Hiearchy' argument is specified lists all objects inside clusters."),
	FConsoleCommandWithArgsDelegate::CreateStatic(ListClusters)
	);

static FAutoConsoleCommand FindStaleClustersCommand(
	TEXT("gc.FindStaleClusters"),
	TEXT("Dumps all clusters do output log that are not referenced by anything."),
	FConsoleCommandWithArgsDelegate::CreateStatic(FindStaleClusters)
	);

#endif // !UE_BUILD_SHIPPING

/**
* Pool for reducing allocations when constructing clusters
*/
class FClusterArrayPool
{
public:

	/**
	* Gets the singleton instance of the FObjectArrayPool
	* @return Pool singleton.
	*/
	FORCEINLINE static FClusterArrayPool& Get()
	{
		static FClusterArrayPool Singleton;
		return Singleton;
	}

	/**
	* Gets an event from the pool or creates one if necessary.
	*
	* @return The array.
	* @see ReturnToPool
	*/
	FORCEINLINE TArray<UObject*>* GetArrayFromPool()
	{
		TArray<UObject*>* Result = Pool.Pop();
		if (!Result)
		{
			Result = new TArray<UObject*>();
		}
		check(Result);
#if UE_BUILD_DEBUG
		NumberOfUsedArrays.Increment();
#endif // UE_BUILD_DEBUG
		return Result;
	}

	/**
	* Returns an array to the pool.
	*
	* @param Array The array to return.
	* @see GetArrayFromPool
	*/
	FORCEINLINE void ReturnToPool(TArray<UObject*>* Array)
	{
#if UE_BUILD_DEBUG
		const int32 CheckUsedArrays = NumberOfUsedArrays.Decrement();
		checkSlow(CheckUsedArrays >= 0);
#endif // UE_BUILD_DEBUG
		check(Array);
		Array->Reset();
		Pool.Push(Array);
	}

	/** Performs memory cleanup */
	void Cleanup()
	{
#if UE_BUILD_DEBUG
		const int32 CheckUsedArrays = NumberOfUsedArrays.GetValue();
		checkSlow(CheckUsedArrays == 0);
#endif // UE_BUILD_DEBUG

		uint32 FreedMemory = 0;
		TArray< TArray<UObject*>* > AllArrays;
		Pool.PopAll(AllArrays);
		for (TArray<UObject*>* Array : AllArrays)
		{
			FreedMemory += Array->GetAllocatedSize();
			delete Array;
		}
		UE_LOG(LogObj, Log, TEXT("Freed %ub from %d cluster array pools."), FreedMemory, AllArrays.Num());
	}

#if UE_BUILD_DEBUG
	void CheckLeaks()
	{
		// This function is called after cluster has been created so at this point there should be no
		// arrays used by cluster creation code and all should be returned to the pool
		const int32 LeakedPoolArrays = NumberOfUsedArrays.GetValue();
		checkSlow(LeakedPoolArrays == 0);
	}
#endif

private:

	/** Holds the collection of recycled arrays. */
	TLockFreePointerListLIFO< TArray<UObject*> > Pool;

#if UE_BUILD_DEBUG
	/** Number of arrays currently acquired from the pool by clusters */
	FThreadSafeCounter NumberOfUsedArrays;
#endif // UE_BUILD_DEBUG
};

/** Called on shutdown to free cluster memory */
void CleanupClusterArrayPools()
{
	FClusterArrayPool::Get().Cleanup();
}

/**
 * Handles UObject references found by TFastReferenceCollector
 */
class FClusterReferenceProcessor
{
	int32 ClusterRootIndex;
	FUObjectCluster& Cluster;
	volatile bool bIsRunningMultithreaded;
public:

	FClusterReferenceProcessor(int32 InClusterRootIndex, FUObjectCluster& InCluster)
		: ClusterRootIndex(InClusterRootIndex)
		, Cluster(InCluster)
		, bIsRunningMultithreaded(false)
	{}

	FORCEINLINE int32 GetMinDesiredObjectsPerSubTask() const
	{
		// We're not running the processor in parallel when creating clusters
		return 0;
	}

	FORCEINLINE volatile bool IsRunningMultithreaded() const
	{
		// This should always be false
		return bIsRunningMultithreaded;
	}

	FORCEINLINE void SetIsRunningMultithreaded(bool bIsParallel)
	{
		check(!bIsParallel);
		bIsRunningMultithreaded = bIsParallel;
	}

	void UpdateDetailedStats(UObject* CurrentObject, uint32 DeltaCycles)
	{
	}

	void LogDetailedStatsSummary()
	{
	}

	/**
	 * Adds an object to cluster (if possible)
	 *
	 * @param ObjectIndex UObject index in GUObjectArray
	 * @param ObjectItem UObject's entry in GUObjectArray
	 * @param Obj The object to add to cluster
	 * @param ObjectsToSerialize An array of remaining objects to serialize (Obj must be added to it if Obj can be added to cluster)
	 * @param bOuterAndClass If true, the Obj's Outer and Class will also be added to the cluster
	 */
	void AddObjectToCluster(int32 ObjectIndex, FUObjectItem* ObjectItem, UObject* Obj, TArray<UObject*>& ObjectsToSerialize, bool bOuterAndClass)
	{
		if (ObjectIndex != ClusterRootIndex && ObjectItem->GetOwnerIndex() == 0 && !ObjectItem->IsRootSet() && !GUObjectArray.IsDisregardForGC(Obj) && Obj->CanBeInCluster())
		{
			ObjectsToSerialize.Add(Obj);
			check(!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
			ObjectItem->SetOwnerIndex(ClusterRootIndex);
			Cluster.Objects.Add(ObjectIndex);

			if (bOuterAndClass)
			{
				UObject* ObjOuter = Obj->GetOuter();
				if (ObjOuter)
				{
					int32 OuterIndex = GUObjectArray.ObjectToIndex(ObjOuter);
					AddObjectToCluster(OuterIndex, GUObjectArray.IndexToObjectUnsafeForGC(OuterIndex), ObjOuter, ObjectsToSerialize, false);
				}
				if (!Obj->GetClass()->HasAllClassFlags(CLASS_Native))
				{
					UObject* ObjectClass = Obj->GetClass();
					HandleTokenStreamObjectReference(ObjectsToSerialize, Obj, ObjectClass, INDEX_NONE, true);
					UObject* ObjectClassOuter = Obj->GetClass()->GetOuter();
					HandleTokenStreamObjectReference(ObjectsToSerialize, Obj, ObjectClassOuter, INDEX_NONE, true);
				}
			}
		}		
	}

	/**
	* Merges an existing cluster with the currently constructed one
	*
	* @param ObjectItem UObject's entry in GUObjectArray. This is either the other cluster root or one if the cluster objects.
	* @param Obj The object to add to cluster
	* @param ObjectsToSerialize An array of remaining objects to serialize (Obj must be added to it if Obj can be added to cluster)
	*/
	void MergeCluster(FUObjectItem* ObjectItem, UObject* Object, TArray<UObject*>& ObjectsToSerialize)
	{
		FUObjectItem* OtherClusterRootItem = nullptr;
		int32 OtherClusterRootIndex = INDEX_NONE;
		// First find the other cluster root item and its index
		if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
		{
			OtherClusterRootIndex = GUObjectArray.ObjectToIndex(Object);
			OtherClusterRootItem = ObjectItem;
		}
		else
		{
			OtherClusterRootIndex = ObjectItem->GetOwnerIndex();
			OtherClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(OtherClusterRootIndex);
		}
		// This is another cluster, merge it with this one
		FUObjectCluster* ClusterToMerge = GUObjectClusters.FindChecked(OtherClusterRootIndex);
		for (int32 OtherClusterObjectIndex : ClusterToMerge->Objects)
		{
			FUObjectItem* OtherClusterObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(OtherClusterObjectIndex);
			OtherClusterObjectItem->SetOwnerIndex(0);
			AddObjectToCluster(OtherClusterObjectIndex, OtherClusterObjectItem, static_cast<UObject*>(OtherClusterObjectItem->Object), ObjectsToSerialize, true);
		}		
		GUObjectClusters.Remove(OtherClusterRootIndex);
		delete ClusterToMerge;

		// Make sure the root object is also added to the current cluster
		OtherClusterRootItem->ClearFlags(EInternalObjectFlags::ClusterRoot);
		OtherClusterRootItem->SetOwnerIndex(0);
		AddObjectToCluster(OtherClusterRootIndex, OtherClusterRootItem, static_cast<UObject*>(OtherClusterRootItem->Object), ObjectsToSerialize, true);

		// Sanity check so that we make sure the object was actually in the lister it said it belonged to
		check(ObjectItem->GetOwnerIndex() == ClusterRootIndex);
	}

	/**
	* Handles UObject reference from the token stream. Performance is critical here so we're FORCEINLINING this function.
	*
	* @param ObjectsToSerialize An array of remaining objects to serialize (Obj must be added to it if Obj can be added to cluster)
	* @param ReferencingObject Object referencing the object to process.
	* @param TokenIndex Index to the token stream where the reference was found.
	* @param bAllowReferenceElimination True if reference elimination is allowed (ignored when constructing clusters).
	*/
	FORCEINLINE void HandleTokenStreamObjectReference(TArray<UObject*>& ObjectsToSerialize, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, bool bAllowReferenceElimination)
	{
		if (Object)
		{
			FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);

			// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
			if (ObjectItem->GetOwnerIndex() != ClusterRootIndex)
			{
				if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot) || ObjectItem->GetOwnerIndex() != 0)
				{					
					if (GMergeGCClusters)
					{
						// This is an existing cluster, merge it with the current one.
						MergeCluster(ObjectItem, Object, ObjectsToSerialize);
					}
					else
					{
						// Simply reference this cluster and all clusters it's referencing
						const int32 OtherClusterRootIndex = ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot) ? GUObjectArray.ObjectToIndex(Object) : ObjectItem->GetOwnerIndex();
						FUObjectCluster* OtherCluster = GUObjectClusters.FindChecked(OtherClusterRootIndex);
						Cluster.ReferencedClusters.AddUnique(OtherClusterRootIndex);
						for (int32 OtherClusterReferencedCluster : OtherCluster->ReferencedClusters)
						{
							check(OtherClusterReferencedCluster != ClusterRootIndex);
							Cluster.ReferencedClusters.AddUnique(OtherClusterReferencedCluster);
						}
						for (int32 OtherClusterReferencedMutableObjectIndex : OtherCluster->MutableObjects)
						{
							Cluster.MutableObjects.AddUnique(OtherClusterReferencedMutableObjectIndex);
						}
					}
				}
				else if (ObjectItem->GetOwnerIndex() == 0 && !ObjectItem->IsRootSet() && !GUObjectArray.IsDisregardForGC(Object) &&
					!(Object->CanBeClusterRoot() && Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad))) // Objects that can create clusters themselves and haven't been postloaded yet should be excluded
				{
					// New object, add it to the cluster.
					if (Object->CanBeInCluster())
					{
						AddObjectToCluster(GUObjectArray.ObjectToIndex(Object), ObjectItem, Object, ObjectsToSerialize, true);
					}
					else
					{
						Cluster.MutableObjects.AddUnique(GUObjectArray.ObjectToIndex(Object));
					}
				}
			}
		}
	}
};

/**
 * Specialized FReferenceCollector that uses FClusterReferenceProcessor to construct the cluster
 */
template <class TProcessor>
class TClusterCollector : public FReferenceCollector
{
	TProcessor& Processor;
	TArray<UObject*>& ObjectArray;

public:
	TClusterCollector(TProcessor& InProcessor, TArray<UObject*>& InObjectArray)
		: Processor(InProcessor)
		, ObjectArray(InObjectArray)
	{
	}
	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const UProperty* ReferencingProperty) override
	{
		Processor.HandleTokenStreamObjectReference(ObjectArray, const_cast<UObject*>(ReferencingObject), Object, INDEX_NONE, false);
	}
	virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* ReferencingObject, const UProperty* InReferencingProperty) override
	{
		for (int32 ObjectIndex = 0; ObjectIndex < ObjectNum; ++ObjectIndex)
		{
			UObject*& Object = InObjects[ObjectIndex];
			Processor.HandleTokenStreamObjectReference(ObjectArray, const_cast<UObject*>(ReferencingObject), Object, INDEX_NONE, false);
		}
	}
	virtual bool IsIgnoringArchetypeRef() const override
	{
		return false;
	}
	virtual bool IsIgnoringTransient() const override
	{
		return false;
	}
};

/** Looks through objects loaded with a package and creates clusters from them */
void CreateClustersFromPackage(FLinkerLoad* PackageLinker)
{	
	if (FPlatformProperties::RequiresCookedData() && !GIsInitialLoad && GCreateGCClusters && !GUObjectArray.IsOpenForDisregardForGC())
	{
		check(PackageLinker);

		for (FObjectExport& Export : PackageLinker->ExportMap)
		{
			if (Export.Object && Export.Object->CanBeClusterRoot())
			{
				Export.Object->CreateCluster();
			}
		}
	}
}

void UObjectBaseUtility::AddToCluster(UObjectBaseUtility* ClusterRootOrObjectFromCluster)
{
	check(ClusterRootOrObjectFromCluster);

	const int32 OuterIndex = GUObjectArray.ObjectToIndex(ClusterRootOrObjectFromCluster);
	FUObjectItem* OuterItem = GUObjectArray.IndexToObjectUnsafeForGC(OuterIndex);
	int32 ClusterRootIndex = 0;
	if (OuterItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
	{
		ClusterRootIndex = OuterIndex;
	}
	else
	{
		ClusterRootIndex = OuterItem->GetOwnerIndex();
	}
	if (ClusterRootIndex != 0)
	{
		FUObjectCluster* Cluster = GUObjectClusters.FindChecked(ClusterRootIndex);
		FClusterReferenceProcessor Processor(ClusterRootIndex, *Cluster);			
		TFastReferenceCollector<FClusterReferenceProcessor, TClusterCollector<FClusterReferenceProcessor>, FClusterArrayPool, true> ReferenceCollector(Processor, FClusterArrayPool::Get());
		TArray<UObject*> ObjectsToProcess;
		UObject* ThisObject = static_cast<UObject*>(this);
		Processor.HandleTokenStreamObjectReference(ObjectsToProcess, static_cast<UObject*>(ClusterRootOrObjectFromCluster), ThisObject, INDEX_NONE, true);
		if (ObjectsToProcess.Num())
		{
			ReferenceCollector.CollectReferences(ObjectsToProcess, true);
		}
	}
}

bool UObjectBaseUtility::CanBeInCluster() const
{
	return true;
}

void UObjectBaseUtility::CreateCluster()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UObjectBaseUtility::CreateCluster"), STAT_FArchiveRealtimeGC_CreateCluster, STATGROUP_GC);

	FUObjectItem* RootItem = GUObjectArray.IndexToObject(InternalIndex);
	if (RootItem->GetOwnerIndex() != 0 || RootItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
	{
		return;
	}

	// If we haven't finished loading, we can't be sure we know all the references
	check(!HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));

	// Create a new cluster, reserve an arbitrary amount of memory for it.
	FUObjectCluster* Cluster = new FUObjectCluster;
	Cluster->Objects.Reserve(64);

	// Collect all objects referenced by cluster root and by all objects it's referencing
	FClusterReferenceProcessor Processor(InternalIndex, *Cluster);
	TFastReferenceCollector<FClusterReferenceProcessor, TClusterCollector<FClusterReferenceProcessor>, FClusterArrayPool, true> ReferenceCollector(Processor, FClusterArrayPool::Get());
	TArray<UObject*> ObjectsToProcess;
	ObjectsToProcess.Add(static_cast<UObject*>(this));
	ReferenceCollector.CollectReferences(ObjectsToProcess, true);
#if UE_BUILD_DEBUG
	FClusterArrayPool::Get().CheckLeaks();
#endif

	if (Cluster->Objects.Num())
	{
		// Add new cluster to the global cluster map.
		GUObjectClusters.Add(InternalIndex, Cluster);
		check(RootItem->GetOwnerIndex() == 0);
		RootItem->SetFlags(EInternalObjectFlags::ClusterRoot);
	}
	else
	{
		delete Cluster;
	}
}


/**
* Handles UObject references found by TFastReferenceCollector
*/
class FClusterVerifyReferenceProcessor
{	
	const UObject* const ClusterRootObject;
	const int32 ClusterRootIndex;
	const FUObjectCluster& Cluster;
	volatile bool bIsRunningMultithreaded;
	bool bFailed;
	TSet<UObject*> ProcessedObjects;

public:

	FClusterVerifyReferenceProcessor(UObject* InClusterRootObject)		
		: ClusterRootObject(InClusterRootObject)
		, ClusterRootIndex(GUObjectArray.ObjectToIndex(InClusterRootObject))
		, Cluster(*GUObjectClusters.FindChecked(ClusterRootIndex)) // This can't fail otherwise there's something wrong with cluster creation code
		, bIsRunningMultithreaded(false)
		, bFailed(false)
	{
	}

	bool NoExternalReferencesFound() const
	{
		return !bFailed;
	}

	FORCEINLINE int32 GetMinDesiredObjectsPerSubTask() const
	{
		// We're not running the processor in parallel when creating clusters
		return 0;
	}

	FORCEINLINE volatile bool IsRunningMultithreaded() const
	{
		// This should always be false
		return bIsRunningMultithreaded;
	}

	FORCEINLINE void SetIsRunningMultithreaded(bool bIsParallel)
	{
		check(!bIsParallel);
		bIsRunningMultithreaded = bIsParallel;
	}

	void UpdateDetailedStats(UObject* CurrentObject, uint32 DeltaCycles)
	{
	}

	void LogDetailedStatsSummary()
	{
	}

	/**
	* Handles UObject reference from the token stream. Performance is critical here so we're FORCEINLINING this function.
	*
	* @param ObjectsToSerialize An array of remaining objects to serialize (Obj must be added to it if Obj can be added to cluster)
	* @param ReferencingObject Object referencing the object to process.
	* @param TokenIndex Index to the token stream where the reference was found.
	* @param bAllowReferenceElimination True if reference elimination is allowed (ignored when constructing clusters).
	*/
	FORCEINLINE void HandleTokenStreamObjectReference(TArray<UObject*>& ObjectsToSerialize, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, bool bAllowReferenceElimination)
	{
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		if (Object && !Object->IsValidLowLevelFast())
		{
			FString TokenDebugInfo;
			if (UClass *Class = (ReferencingObject ? ReferencingObject->GetClass() : nullptr))
			{
				const FTokenInfo& TokenInfo = Class->DebugTokenMap.GetTokenInfo(TokenIndex);
				TokenDebugInfo = FString::Printf(TEXT("ReferencingObjectClass: %s, Property Name: %s, Offset: %d"),
					*Class->GetFullName(), *TokenInfo.Name.GetPlainNameString(), TokenInfo.Offset);
			}
			else
			{
				// This means this objects is most likely being referenced by AddReferencedObjects
				TokenDebugInfo = TEXT("Native Reference");
			}

			UE_LOG(LogObj, Fatal, TEXT("Invalid object while verifying cluster assumptions: 0x%016llx, ReferencingObject: %s, %s, TokenIndex: %d"),
				(int64)(PTRINT)Object,
				ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
				*TokenDebugInfo, TokenIndex);
		}
#endif
		if (Object && !ProcessedObjects.Contains(Object))
		{
			ProcessedObjects.Add(Object);
			FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
			if (ObjectItem->GetOwnerIndex() == 0)
			{
				// We are allowed to reference other clusters, root set objects and objects from diregard for GC pool
				if (!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot|EInternalObjectFlags::RootSet) && !GUObjectArray.IsDisregardForGC(Object) && Object->CanBeInCluster())
				{
					UE_LOG(LogObj, Warning, TEXT("Object %s from cluster %s is referencing 0x%016llx %s which is not part of root set or cluster."),
						*ReferencingObject->GetFullName(),
						*ClusterRootObject->GetFullName(),
						(int64)(PTRINT)Object,
						*Object->GetFullName());
					bFailed = true;
#if UE_BUILD_DEBUG
					FReferenceChainSearch RefChainSearch(Object, FReferenceChainSearch::ESearchMode::Shortest | FReferenceChainSearch::ESearchMode::PrintResults);
#endif
				}
				else if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
				{
					// However, clusters need to be referenced by the current cluster otherwise they can also get GC'd too early.
					const int32 OtherClusterRootIndex = GUObjectArray.ObjectToIndex(Object);
					const FUObjectItem* OtherClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(OtherClusterRootIndex);
					check(OtherClusterRootItem && OtherClusterRootItem->Object);
					UObject* OtherClusterRootObject = static_cast<UObject*>(OtherClusterRootItem->Object);
					UE_CLOG(OtherClusterRootIndex != ClusterRootIndex && !Cluster.ReferencedClusters.Contains(OtherClusterRootIndex), LogObj, Fatal,
						TEXT("Object %s from source cluster %s is referencing object %s (0x%016llx) from cluster %s which is not referenced by the source cluster."),
						*ReferencingObject->GetFullName(),
						*ClusterRootObject->GetFullName(),
						*Object->GetFullName(),
						(int64)(PTRINT)Object,
						*OtherClusterRootObject->GetFullName());
				}
			}
			else if (ObjectItem->GetOwnerIndex() == ClusterRootIndex)
			{
				// If this object belongs to the current cluster, keep processing its references. Otherwise ignore it as it will be processed by its cluster
				ObjectsToSerialize.Add(Object);
			}
			else
			{
				// If we're referencing an object from another cluster, make sure the other cluster is actually referenced by this cluster
				const int32 OtherClusterRootIndex = ObjectItem->GetOwnerIndex();
				const FUObjectItem* OtherClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(OtherClusterRootIndex);				
				check(OtherClusterRootItem && OtherClusterRootItem->Object);
				UObject* OtherClusterRootObject = static_cast<UObject*>(OtherClusterRootItem->Object);
				UE_CLOG(OtherClusterRootIndex != ClusterRootIndex && !Cluster.ReferencedClusters.Contains(OtherClusterRootIndex), LogObj, Fatal,
					TEXT("Object %s from source cluster %s is referencing object %s (0x%016llx) from cluster %s which is not referenced by the source cluster."),
					*ReferencingObject->GetFullName(),
					*ClusterRootObject->GetFullName(),
					*Object->GetFullName(),
					(int64)(PTRINT)Object,
					*OtherClusterRootObject->GetFullName());
			}
		}
	}
};

bool VerifyClusterAssumptions(UObject* ClusterRootObject)
{
	// Collect all objects referenced by cluster root and by all objects it's referencing
	FClusterVerifyReferenceProcessor Processor(ClusterRootObject);
	TFastReferenceCollector<FClusterVerifyReferenceProcessor, TClusterCollector<FClusterVerifyReferenceProcessor>, FClusterArrayPool> ReferenceCollector(Processor, FClusterArrayPool::Get());
	TArray<UObject*> ObjectsToProcess;
	ObjectsToProcess.Add(ClusterRootObject);
	ReferenceCollector.CollectReferences(ObjectsToProcess, true);
	return Processor.NoExternalReferencesFound();
}
