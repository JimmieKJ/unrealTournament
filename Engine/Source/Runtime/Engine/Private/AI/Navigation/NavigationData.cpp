// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/NavDataGenerator.h"
#include "AI/Navigation/NavAreas/NavAreaMeta.h"
#include "AI/Navigation/NavigationData.h"

//----------------------------------------------------------------------//
// FPathFindingQuery
//----------------------------------------------------------------------//
FPathFindingQuery::FPathFindingQuery(const UObject* InOwner, const ANavigationData* InNavData, const FVector& Start, const FVector& End, TSharedPtr<const FNavigationQueryFilter> SourceQueryFilter, FNavPathSharedPtr InPathInstanceToFill)
: NavData(InNavData)
, Owner(InOwner)
, StartLocation(Start)
, EndLocation(End)
, QueryFilter(SourceQueryFilter)
, PathInstanceToFill(InPathInstanceToFill)
, NavDataFlags(0)
, bAllowPartialPaths(true)
{
	if (SourceQueryFilter.IsValid() == false && NavData.IsValid() == true)
	{
		QueryFilter = NavData->GetDefaultQueryFilter();
	}
}

FPathFindingQuery::FPathFindingQuery(const UObject* InOwner, const ANavigationData& InNavData, const FVector& Start, const FVector& End, TSharedPtr<const FNavigationQueryFilter> SourceQueryFilter, FNavPathSharedPtr InPathInstanceToFill)
: NavData(&InNavData)
, Owner(InOwner)
, StartLocation(Start)
, EndLocation(End)
, QueryFilter(SourceQueryFilter)
, PathInstanceToFill(InPathInstanceToFill)
, NavDataFlags(0)
, bAllowPartialPaths(true)
{
	if (SourceQueryFilter.IsValid() == false && NavData.IsValid() == true)
	{
		QueryFilter = NavData->GetDefaultQueryFilter();
	}
}

FPathFindingQuery::FPathFindingQuery(const FPathFindingQuery& Source)
: NavData(Source.NavData)
, Owner(Source.Owner)
, StartLocation(Source.StartLocation)
, EndLocation(Source.EndLocation)
, QueryFilter(Source.QueryFilter)
, PathInstanceToFill(Source.PathInstanceToFill)
, NavDataFlags(Source.NavDataFlags)
, bAllowPartialPaths(Source.bAllowPartialPaths)
{
	if (Source.QueryFilter.IsValid() == false && NavData.IsValid() == true)
	{
		QueryFilter = NavData->GetDefaultQueryFilter();
	}
}

FPathFindingQuery::FPathFindingQuery(FNavPathSharedRef PathToRecalculate, const ANavigationData* NavDataOverride)
: NavData(NavDataOverride != NULL ? NavDataOverride : PathToRecalculate->GetNavigationDataUsed())
, Owner(PathToRecalculate->GetQuerier())
, StartLocation(PathToRecalculate->GetPathFindingStartLocation())
, EndLocation(PathToRecalculate->GetGoalLocation())
, QueryFilter(PathToRecalculate->GetFilter())
, PathInstanceToFill(PathToRecalculate)
, NavDataFlags(0)
, bAllowPartialPaths(true)
{
	if (QueryFilter.IsValid() == false && NavData.IsValid() == true)
	{
		QueryFilter = NavData->GetDefaultQueryFilter();
	}
}

//----------------------------------------------------------------------//
// FAsyncPathFindingQuery
//----------------------------------------------------------------------//
uint32 FAsyncPathFindingQuery::LastPathFindingUniqueID = INVALID_NAVQUERYID;

FAsyncPathFindingQuery::FAsyncPathFindingQuery(const UObject* InOwner, const ANavigationData* InNavData, const FVector& Start, const FVector& End, const FNavPathQueryDelegate& Delegate, TSharedPtr<const FNavigationQueryFilter> SourceQueryFilter)
: FPathFindingQuery(InOwner, *InNavData, Start, End, SourceQueryFilter)
, QueryID(GetUniqueID())
, OnDoneDelegate(Delegate)
{
	if (InNavData == nullptr)
	{
		UE_LOG(LogNavigation, Error, TEXT("Trying to instantiate FAsyncPathFindingQuery while InNavData == null"));
	}
}

FAsyncPathFindingQuery::FAsyncPathFindingQuery(const UObject* InOwner, const ANavigationData& InNavData, const FVector& Start, const FVector& End, const FNavPathQueryDelegate& Delegate, TSharedPtr<const FNavigationQueryFilter> SourceQueryFilter)
: FPathFindingQuery(InOwner, InNavData, Start, End, SourceQueryFilter)
, QueryID(GetUniqueID())
, OnDoneDelegate(Delegate)
{

}

FAsyncPathFindingQuery::FAsyncPathFindingQuery(const FPathFindingQuery& Query, const FNavPathQueryDelegate& Delegate, const EPathFindingMode::Type QueryMode)
: FPathFindingQuery(Query)
, QueryID(GetUniqueID())
, OnDoneDelegate(Delegate)
, Mode(QueryMode)
{

}
//----------------------------------------------------------------------//
// FSupportedAreaData
//----------------------------------------------------------------------//
FSupportedAreaData::FSupportedAreaData(TSubclassOf<UNavArea> NavAreaClass, int32 InAreaID)
	: AreaID(InAreaID), AreaClass(NavAreaClass)
{
	if (AreaClass != NULL)
	{
		AreaClassName = AreaClass->GetName();
	}
	else
	{
		AreaClassName = TEXT("Invalid");
	}
}

//----------------------------------------------------------------------//
// ANavigationData                                                                
//----------------------------------------------------------------------//
ANavigationData::ANavigationData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnableDrawing(false)
	, RuntimeGeneration(ERuntimeGenerationType::LegacyGeneration) //TODO: set to a valid value once bRebuildAtRuntime_DEPRECATED is removed
	, bForceRebuildOnLoad(false)
	, DataVersion(NAVMESHVER_LATEST)
	, FindPathImplementation(NULL)
	, FindHierarchicalPathImplementation(NULL)
	, bRegistered(false)
	, bWantsUpdate(true)
	, NavDataUniqueID(GetNextUniqueID())
{
	PrimaryActorTick.bCanEverTick = true;
	bNetLoadOnClient = false;
	bCanBeDamaged = false;
	DefaultQueryFilter = MakeShareable(new FNavigationQueryFilter());
	ObservedPathsTickInterval = 0.5;
}

uint16 ANavigationData::GetNextUniqueID()
{
	static FThreadSafeCounter StaticID(INVALID_NAVDATA);
	return StaticID.Increment();
}

void ANavigationData::PostInitProperties()
{
	Super::PostInitProperties();

	if (IsPendingKill() == true)
	{
		return;
	}

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		if (RuntimeGeneration == ERuntimeGenerationType::LegacyGeneration)
		{
			RuntimeGeneration = bRebuildAtRuntime_DEPRECATED ? ERuntimeGenerationType::Dynamic : ERuntimeGenerationType::Static;
		}
	}
	else
	{
		bNetLoadOnClient = (*GEngine->NavigationSystemClass != nullptr) && (GEngine->NavigationSystemClass->GetDefaultObject<UNavigationSystem>()->ShouldLoadNavigationOnClient(this));

		UWorld* WorldOuter = GetWorld();
		
		if (WorldOuter != NULL && WorldOuter->GetNavigationSystem() != NULL)
		{
			WorldOuter->GetNavigationSystem()->RequestRegistration(this);
		}

		RenderingComp = ConstructRenderingComponent();
	}
}

void ANavigationData::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	CachedWorld = GetWorld();

	if (CachedWorld == NULL || CachedWorld->GetNavigationSystem() == NULL)
	{
		CleanUpAndMarkPendingKill();
	}
	else
	{
		// note: this is not a final fix for world composition's issues with navmesh generation
		// but it's good for now, and navmesh creation is going to get a face-lift soon anyway
		bWantsUpdate |= CachedWorld->GetWorldSettings()->bEnableWorldComposition;
	}
}

void ANavigationData::PostLoad() 
{
	Super::PostLoad();

	if ((GetLinkerUE4Version() < VER_UE4_ADD_MODIFIERS_RUNTIME_GENERATION) &&
		(RuntimeGeneration == ERuntimeGenerationType::LegacyGeneration))
	{
		RuntimeGeneration = bRebuildAtRuntime_DEPRECATED ? ERuntimeGenerationType::Dynamic : ERuntimeGenerationType::Static;
	}

	InstantiateAndRegisterRenderingComponent();

	CachedWorld = GetWorld();

	bNetLoadOnClient = (*GEngine->NavigationSystemClass != nullptr) && (GEngine->NavigationSystemClass->GetDefaultObject<UNavigationSystem>()->ShouldLoadNavigationOnClient(this));
}

void ANavigationData::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	PurgeUnusedPaths();
	if (NextObservedPathsTickInSeconds >= 0.f)
	{
		NextObservedPathsTickInSeconds -= DeltaTime;
		if (NextObservedPathsTickInSeconds <= 0.f)
		{
			RepathRequests.Reserve(ObservedPaths.Num());

			for (int32 PathIndex = ObservedPaths.Num() - 1; PathIndex >= 0; --PathIndex)
			{
				if (ObservedPaths[PathIndex].IsValid())
				{
					FNavPathSharedPtr SharedPath = ObservedPaths[PathIndex].Pin();
					FNavigationPath* Path = SharedPath.Get();
					EPathObservationResult::Type Result = Path->TickPathObservation();
					switch (Result)
					{
					case EPathObservationResult::NoLongerObserving:
						ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
						break;

					case EPathObservationResult::NoChange:
						// do nothing
						break;

					case EPathObservationResult::RequestRepath:
						RepathRequests.Add(FNavPathRecalculationRequest(SharedPath, ENavPathUpdateType::GoalMoved));
						break;
					
					default:
						check(false && "unhandled EPathObservationResult::Type in ANavigationData::TickActor");
						break;
					}
				}
				else
				{
					ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
				}
			}

			if (ObservedPaths.Num() > 0)
			{
				NextObservedPathsTickInSeconds = ObservedPathsTickInterval;
			}
		}
	}

	if (RepathRequests.Num() > 0)
	{
		float TimeStamp = GetWorldTimeStamp();
		TArray<FNavPathRecalculationRequest> PostponedRequests;
		const UWorld* World = GetWorld();

		// @todo batch-process it!
		for (auto RecalcRequest : RepathRequests)
		{
			// check if it can be updated right now
			FNavPathSharedPtr PinnedPath = RecalcRequest.Path.Pin();
			if (PinnedPath.IsValid() == false)
			{
				continue;
			}

			const UObject* PathQuerier = PinnedPath->GetQuerier();
			const INavAgentInterface* PathNavAgent = Cast<const INavAgentInterface>(PathQuerier);
			if (PathNavAgent && PathNavAgent->ShouldPostponePathUpdates())
			{
				PostponedRequests.Add(RecalcRequest);
				continue;
			}

			FPathFindingQuery Query(PinnedPath.ToSharedRef());
			// @todo consider supplying NavAgentPropertied from path's querier
			const FPathFindingResult Result = FindPath(FNavAgentProperties(), Query.SetPathInstanceToUpdate(PinnedPath));

			// update time stamp to give observers any means of telling if it has changed
			PinnedPath->SetTimeStamp(TimeStamp);

			// partial paths are still valid and can change to full path when moving goal gets back on navmesh
			if (Result.IsSuccessful() || Result.IsPartial())
			{
				PinnedPath->UpdateLastRepathGoalLocation();
				PinnedPath->DoneUpdating(RecalcRequest.Reason);
				if (RecalcRequest.Reason == ENavPathUpdateType::NavigationChanged)
				{
					RegisterActivePath(PinnedPath);
				}
			}
			else
			{
				PinnedPath->RePathFailed();
			}
		}

		RepathRequests.Reset();
		RepathRequests.Append(PostponedRequests);
	}
}

void ANavigationData::RerunConstructionScripts()
{
	Super::RerunConstructionScripts();

	InstantiateAndRegisterRenderingComponent();
}

void ANavigationData::OnRegistered() 
{ 
	InstantiateAndRegisterRenderingComponent();

	bRegistered = true;
	ConditionalConstructGenerator();
}

void ANavigationData::OnUnregistered()
{
	bRegistered = false;
}

void ANavigationData::InstantiateAndRegisterRenderingComponent()
{
#if !UE_BUILD_SHIPPING
	if (!IsPendingKill() && (RenderingComp == NULL || RenderingComp->IsPendingKill()))
	{
		if (RenderingComp)
		{
			// rename the old rendering component out of the way
			RenderingComp->Rename(NULL, NULL, REN_DontCreateRedirectors | REN_ForceGlobalUnique | REN_DoNotDirty | REN_NonTransactional);
		}

		RenderingComp = ConstructRenderingComponent();
		if (RenderingComp != NULL)
		{
			RenderingComp->RegisterComponent();
		}
	}
#endif // !UE_BUILD_SHIPPING
}

void ANavigationData::PurgeUnusedPaths()
{
	check(IsInGameThread());

	const int32 Count = ActivePaths.Num();
	FNavPathWeakPtr* WeakPathPtr = (ActivePaths.GetData() + Count - 1);
	for (int32 i = Count - 1; i >= 0; --i, --WeakPathPtr)
	{
		if (WeakPathPtr->IsValid() == false)
		{
			ActivePaths.RemoveAtSwap(i, 1, /*bAllowShrinking=*/false);
		}
	}
}

#if WITH_EDITOR
void ANavigationData::PostEditUndo()
{
	Super::PostEditUndo();

	UWorld* WorldOuter = GetWorld();
	if (WorldOuter != NULL && WorldOuter->GetNavigationSystem() != NULL)
	{
		if (IsPendingKillPending())
		{
			WorldOuter->GetNavigationSystem()->UnregisterNavData(this);
		}
		else
		{
			WorldOuter->GetNavigationSystem()->RequestRegistration(this);
		}
	}
}
#endif // WITH_EDITOR

bool ANavigationData::DoesSupportAgent(const FNavAgentProperties& AgentProps) const
{
	return NavDataConfig.IsEquivalent(AgentProps);
}

void ANavigationData::Destroyed()
{
	UWorld* WorldOuter = GetWorld();

	bRegistered = false;
	if (WorldOuter != NULL && WorldOuter->GetNavigationSystem() != NULL)
	{
		WorldOuter->GetNavigationSystem()->UnregisterNavData(this);
	}

	CleanUp();

	Super::Destroyed();
}

void ANavigationData::CleanUp()
{
	bRegistered = false;
}

void ANavigationData::CleanUpAndMarkPendingKill()
{
	CleanUp();
	SetActorHiddenInGame(true);

	// do NOT destroy here! it can be called from PostLoad and will crash in DestroyActor()
	MarkPendingKill();
	MarkComponentsAsPendingKill();
}

bool ANavigationData::SupportsRuntimeGeneration() const
{
	return false;
}

bool ANavigationData::SupportsStreaming() const
{
	return false;
}

void ANavigationData::ConditionalConstructGenerator()
{
}

void ANavigationData::RebuildAll()
{
	ConditionalConstructGenerator(); //recreate generator
	
	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->RebuildAll();
	}
}

void ANavigationData::EnsureBuildCompletion()
{
	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->EnsureBuildCompletion();
	}
}

void ANavigationData::CancelBuild()
{
	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->CancelBuild();
	}
}

void ANavigationData::OnNavigationBoundsChanged()
{
	// Create generator if it wasn't yet
	if (NavDataGenerator.Get() == nullptr)
	{
		ConditionalConstructGenerator();
	}
	
	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->OnNavigationBoundsChanged();
	}
}

void ANavigationData::TickAsyncBuild(float DeltaSeconds)
{
	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->TickAsyncBuild(DeltaSeconds);
	}
}

void ANavigationData::RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& DirtyAreas)
{
	// the 'bWantsUpdate' mechanics allows us to skip first requested update after data is loaded
	// Can be also used to manually control navigation rebuilding, by for example forcing bWantsUpdate 
	//	to false in Tick function effectively suppressing rebuilding
	if (bWantsUpdate == true && NavDataGenerator.IsValid())
	{
		NavDataGenerator->RebuildDirtyAreas(DirtyAreas);
	}
	bWantsUpdate = true;
}

TArray<FBox> ANavigationData::GetNavigableBounds() const
{
	TArray<FBox> Result;
	const UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
	
	const auto& NavigationBounds = NavSys->GetNavigationBounds();
	Result.Reserve(NavigationBounds.Num());

	for (const auto& Bounds : NavigationBounds)
	{
		Result.Add(Bounds.AreaBox);
	}
	
	return Result;
}
	
TArray<FBox> ANavigationData::GetNavigableBoundsInLevel(ULevel* InLevel) const
{
	TArray<FBox> Result;
	const UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
	
	if (NavSys != nullptr)
	{
		const auto& NavigationBounds = NavSys->GetNavigationBounds();
		Result.Reserve(NavigationBounds.Num());

		for (const auto& Bounds : NavigationBounds)
		{
			if (Bounds.Level == InLevel)
			{
				Result.Add(Bounds.AreaBox);
			}
		}
	}
	
	return Result;
}

void ANavigationData::DrawDebugPath(FNavigationPath* Path, FColor PathColor, UCanvas* Canvas, bool bPersistent, const uint32 NextPathPointIndex) const
{
	Path->DebugDraw(this, PathColor, Canvas, bPersistent, NextPathPointIndex);
}

const UWorld* ANavigationData::GetCachedWorld() const
{
	return CachedWorld;
}

float ANavigationData::GetWorldTimeStamp() const
{
	const UWorld* World = GetCachedWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void ANavigationData::OnNavAreaAdded(const UClass* NavAreaClass, int32 AgentIndex)
{
	// check if area can be added
	const UNavArea* DefArea = NavAreaClass ? ((UClass*)NavAreaClass)->GetDefaultObject<UNavArea>() : NULL;
	const bool bIsMetaArea = NavAreaClass && NavAreaClass->IsChildOf(UNavAreaMeta::StaticClass());
	if (!DefArea || bIsMetaArea || !DefArea->IsSupportingAgent(AgentIndex))
	{
		UE_LOG(LogNavigation, Verbose, TEXT("%s discarded area %s (valid:%s meta:%s validAgent[%d]:%s)"),
			*GetName(), *GetNameSafe(NavAreaClass),
			DefArea ? TEXT("yes") : TEXT("NO"),
			bIsMetaArea ? TEXT("YES") : TEXT("no"),
			AgentIndex, DefArea->IsSupportingAgent(AgentIndex) ? TEXT("yes") : TEXT("NO"));
		return;
	}

	// check if area is already on supported list
	FString AreaClassName = NavAreaClass->GetName();
	for (int32 i = 0; i < SupportedAreas.Num(); i++)
	{
		if (SupportedAreas[i].AreaClassName == AreaClassName)
		{
			SupportedAreas[i].AreaClass = NavAreaClass;
			AreaClassToIdMap.Add(NavAreaClass, SupportedAreas[i].AreaID);
			UE_LOG(LogNavigation, Verbose, TEXT("%s updated area %s with ID %d"), *GetName(), *AreaClassName, SupportedAreas[i].AreaID);
			return;
		}
	}

	// try adding new one
	const int32 MaxSupported = GetMaxSupportedAreas();
	if (SupportedAreas.Num() >= MaxSupported)
	{
		UE_LOG(LogNavigation, Error, TEXT("%s can't support area %s - limit reached! (%d)"), *GetName(), *AreaClassName, MaxSupported);
		return;
	}

	FSupportedAreaData NewAgentData;
	NewAgentData.AreaClass = NavAreaClass;
	NewAgentData.AreaClassName = AreaClassName;
	NewAgentData.AreaID = GetNewAreaID(NavAreaClass);
	SupportedAreas.Add(NewAgentData);
	AreaClassToIdMap.Add(NavAreaClass, NewAgentData.AreaID);

	UE_LOG(LogNavigation, Verbose, TEXT("%s registered area %s with ID %d"), *GetName(), *AreaClassName, NewAgentData.AreaID);
}

void ANavigationData::OnNavAreaEvent(const UClass* NavAreaClass, ENavAreaEvent::Type Event)
{
	if (Event == ENavAreaEvent::Registered)
	{
		const UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
		const int32 AgentIndex = NavSys->GetSupportedAgentIndex(this);

		OnNavAreaAdded(NavAreaClass, AgentIndex);
	}
	else // Unregistered
	{
		OnNavAreaRemoved(NavAreaClass);
	}

	OnNavAreaChanged();
}

void ANavigationData::OnNavAreaRemoved(const UClass* NavAreaClass)
{
	for (int32 i = 0; i < SupportedAreas.Num(); i++)
	{
		if (SupportedAreas[i].AreaClass == NavAreaClass)
		{
			AreaClassToIdMap.Remove(NavAreaClass);
			SupportedAreas.RemoveAt(i);
			break;
		}
	}
}

void ANavigationData::OnNavAreaChanged()
{
	// empty in base class
}

void ANavigationData::ProcessNavAreas(const TArray<const UClass*>& AreaClasses, int32 AgentIndex)
{
	for (int32 i = 0; i < AreaClasses.Num(); i++)
	{
		OnNavAreaAdded(AreaClasses[i], AgentIndex);
	}

	OnNavAreaChanged();
}

int32 ANavigationData::GetNewAreaID(const UClass* AreaClass) const
{
	int TestId = 0;
	while (TestId < SupportedAreas.Num())
	{
		const bool bIsTaken = IsAreaAssigned(TestId);
		if (bIsTaken)
		{
			TestId++;
		}
		else
		{
			break;
		}
	}

	return TestId;
}

const UClass* ANavigationData::GetAreaClass(int32 AreaID) const
{
	for (int32 i = 0; i < SupportedAreas.Num(); i++)
	{
		if (SupportedAreas[i].AreaID == AreaID)
		{
			return SupportedAreas[i].AreaClass;
		}
	}

	return NULL;
}

bool ANavigationData::IsAreaAssigned(int32 AreaID) const
{
	for (int32 i = 0; i < SupportedAreas.Num(); i++)
	{
		if (SupportedAreas[i].AreaID == AreaID)
		{
			return true;
		}
	}

	return false;
}

int32 ANavigationData::GetAreaID(const UClass* AreaClass) const
{
	const int32* PtrId = AreaClassToIdMap.Find(AreaClass);
	return PtrId ? *PtrId : INDEX_NONE;
}

void ANavigationData::SetNavRenderingEnabled(bool bEnable)
{
	if (bEnableDrawing != bEnable)
	{
		bEnableDrawing = bEnable;
		MarkComponentsRenderStateDirty();
	}
}

void ANavigationData::UpdateCustomLink(const INavLinkCustomInterface* CustomLink)
{
	// no implementation for abstract class
}

TSharedPtr<const FNavigationQueryFilter> ANavigationData::GetQueryFilter(TSubclassOf<UNavigationQueryFilter> FilterClass) const
{
	return QueryFilters.FindRef(FilterClass);
}

void ANavigationData::StoreQueryFilter(TSubclassOf<UNavigationQueryFilter> FilterClass, TSharedPtr<const FNavigationQueryFilter> NavFilter)
{
	QueryFilters.Add(FilterClass, NavFilter);
}

void ANavigationData::RemoveQueryFilter(TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	QueryFilters.Remove(FilterClass);
}

uint32 ANavigationData::LogMemUsed() const
{
	const uint32 MemUsed = ActivePaths.GetAllocatedSize() + SupportedAreas.GetAllocatedSize() +
		QueryFilters.GetAllocatedSize() + AreaClassToIdMap.GetAllocatedSize();

	UE_LOG(LogNavigation, Display, TEXT("%s: ANavigationData: %u\n    self: %d"), *GetName(), MemUsed, sizeof(ANavigationData));	

	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->LogMemUsed();
	}

	return MemUsed;
}
