// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EQSTestingPawn.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#if WITH_EDITOR
#include "UnrealEd.h"
#include "Engine/Brush.h"
#include "EngineUtils.h"

extern UNREALED_API UEditorEngine* GEditor;
#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY(LogEQS);

DEFINE_STAT(STAT_AI_EQS_Tick);
DEFINE_STAT(STAT_AI_EQS_TickWork);
DEFINE_STAT(STAT_AI_EQS_TickNotifies);
DEFINE_STAT(STAT_AI_EQS_LoadTime);
DEFINE_STAT(STAT_AI_EQS_GeneratorTime);
DEFINE_STAT(STAT_AI_EQS_TestTime);
DEFINE_STAT(STAT_AI_EQS_NumInstances);
DEFINE_STAT(STAT_AI_EQS_NumItems);
DEFINE_STAT(STAT_AI_EQS_InstanceMemory);

//////////////////////////////////////////////////////////////////////////
// FEnvQueryRequest

FEnvQueryRequest& FEnvQueryRequest::SetNamedParams(const TArray<FEnvNamedValue>& Params)
{
	for (int32 ParamIndex = 0; ParamIndex < Params.Num(); ParamIndex++)
	{
		NamedParams.Add(Params[ParamIndex].ParamName, Params[ParamIndex].Value);
	}

	return *this;
}

int32 FEnvQueryRequest::Execute(EEnvQueryRunMode::Type RunMode, FQueryFinishedSignature const& FinishDelegate)
{
	if (Owner == NULL)
	{
		Owner = FinishDelegate.GetUObject();
		if (Owner == NULL)
		{
			UE_LOG(LogEQS, Warning, TEXT("Unknown owner of request: %s"), *GetNameSafe(QueryTemplate));
			return INDEX_NONE;
		}
	}

	if (World == NULL)
	{
		World = GEngine->GetWorldFromContextObject(Owner);
		if (World == NULL)
		{
			UE_LOG(LogEQS, Warning, TEXT("Unable to access world with owner: %s"), *GetNameSafe(Owner));
			return INDEX_NONE;
		}
	}

	UEnvQueryManager* EnvQueryManager = UEnvQueryManager::GetCurrent(World);
	if (EnvQueryManager == NULL)
	{
		UE_LOG(LogEQS, Warning, TEXT("Missing EQS manager!"));
		return INDEX_NONE;
	}

	return EnvQueryManager->RunQuery(*this, RunMode, FinishDelegate);
}


//////////////////////////////////////////////////////////////////////////
// UEnvQueryManager

TArray<TSubclassOf<UEnvQueryItemType> > UEnvQueryManager::RegisteredItemTypes;

UEnvQueryManager::UEnvQueryManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NextQueryID = 0;
}

UWorld* UEnvQueryManager::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

void UEnvQueryManager::FinishDestroy()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	Super::FinishDestroy();
}

UEnvQueryManager* UEnvQueryManager::GetCurrent(UWorld* World)
{
	UAISystem* AISys = UAISystem::GetCurrentSafe(World);
	return AISys ? AISys->GetEnvironmentQueryManager() : NULL;
}

UEnvQueryManager* UEnvQueryManager::GetCurrent(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, false);
	UAISystem* AISys = UAISystem::GetCurrentSafe(World);
	return AISys ? AISys->GetEnvironmentQueryManager() : NULL;
}

#if USE_EQS_DEBUGGER
void UEnvQueryManager::NotifyAssetUpdate(UEnvQuery* Query)
{
#if WITH_EDITOR
	if (GEditor == NULL)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		UEnvQueryManager* EQS = UEnvQueryManager::GetCurrent(World);
		if (EQS)
		{
			EQS->InstanceCache.Reset();
		}

		// was as follows, but got broken with changes to actor iterator (FActorIteratorBase::SpawnedActorArray)
		// for (TActorIterator<AEQSTestingPawn> It(World); It; ++It)
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AEQSTestingPawn* EQSPawn = Cast<AEQSTestingPawn>(*It);
			if (EQSPawn == NULL)
			{
				continue;
			}

			if (EQSPawn->QueryTemplate == Query || Query == NULL)
			{
				EQSPawn->RunEQSQuery();
			}
		}
	}
#endif //WITH_EDITOR
}
#endif // USE_EQS_DEBUGGER

TStatId UEnvQueryManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnvQueryManager, STATGROUP_Tickables);
}

int32 UEnvQueryManager::RunQuery(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode, FQueryFinishedSignature const& FinishDelegate)
{
	TSharedPtr<FEnvQueryInstance> QueryInstance = PrepareQueryInstance(Request, RunMode);
	return RunQuery(QueryInstance, FinishDelegate);
}

int32 UEnvQueryManager::RunQuery(TSharedPtr<FEnvQueryInstance> QueryInstance, FQueryFinishedSignature const& FinishDelegate)
{
	if (QueryInstance.IsValid() == false)
	{
		return INDEX_NONE;
	}

	QueryInstance->FinishDelegate = FinishDelegate;
	RunningQueries.Add(QueryInstance);

	return QueryInstance->QueryID;
}

TSharedPtr<FEnvQueryResult> UEnvQueryManager::RunInstantQuery(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode)
{
	TSharedPtr<FEnvQueryInstance> QueryInstance = PrepareQueryInstance(Request, RunMode);
	if (!QueryInstance.IsValid())
	{
		return NULL;
	}

	RegisterExternalQuery(QueryInstance);
	while (QueryInstance->IsFinished() == false)
	{
		QueryInstance->ExecuteOneStep((double)FLT_MAX);
	}

	UnregisterExternalQuery(QueryInstance);

	UE_VLOG_EQS(*QueryInstance.Get(), LogEQS, All);

#if USE_EQS_DEBUGGER
	EQSDebugger.StoreQuery(GetWorld(), QueryInstance);
#endif // USE_EQS_DEBUGGER

	return QueryInstance;
}

TSharedPtr<FEnvQueryInstance> UEnvQueryManager::PrepareQueryInstance(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode)
{
	TSharedPtr<FEnvQueryInstance> QueryInstance = CreateQueryInstance(Request.QueryTemplate, RunMode);
	if (!QueryInstance.IsValid())
	{
		return NULL;
	}

	QueryInstance->World = Cast<UWorld>(GetOuter());
	QueryInstance->Owner = Request.Owner;

	DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, QueryInstance->NamedParams.GetAllocatedSize());

	// @TODO: interface for providing default named params (like custom ranges in AI)
	QueryInstance->NamedParams = Request.NamedParams;

	INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, QueryInstance->NamedParams.GetAllocatedSize());

	QueryInstance->QueryID = NextQueryID++;

	return QueryInstance;
}

bool UEnvQueryManager::AbortQuery(int32 RequestID)
{
	for (int32 QueryIndex = 0; QueryIndex < RunningQueries.Num(); QueryIndex++)
	{
		TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[QueryIndex];
		if (QueryInstance->QueryID == RequestID &&
			QueryInstance->IsFinished() == false)
		{
			QueryInstance->MarkAsAborted();
			QueryInstance->FinishDelegate.ExecuteIfBound(QueryInstance);
			
			RunningQueries.RemoveAt(QueryIndex);
			return true;
		}
	}

	return false;
}

void UEnvQueryManager::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_EQS_Tick);
	SET_DWORD_STAT(STAT_AI_EQS_NumInstances, RunningQueries.Num());
	// @TODO: threads?

	const double MaxAllowedSeconds = 0.010;
	double TimeLeft = MaxAllowedSeconds;
	int32 FinishedQueriesCount = 0;
		
	TArray<TSharedPtr<FEnvQueryInstance> > RunningQueriesCopy = RunningQueries;

	{
		SCOPE_CYCLE_COUNTER(STAT_AI_EQS_TickWork);
		while (TimeLeft > 0.0 && RunningQueriesCopy.Num() > 0)
		{
			for (int32 Index = 0; Index < RunningQueriesCopy.Num() && TimeLeft > 0.0; Index++)
			{
				const double StartTime = FPlatformTime::Seconds();

				TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueriesCopy[Index];
				//SCOPE_LOG_TIME(*FString::Printf(TEXT("Query %s step"), *QueryInstance->QueryName), nullptr);

				QueryInstance->ExecuteOneStep(TimeLeft);

				if (QueryInstance->IsFinished())
				{
					RunningQueriesCopy.RemoveAt(Index, 1, /*bAllowShrinking=*/false);
					Index--;
					++FinishedQueriesCount;
				}

				TimeLeft -= (FPlatformTime::Seconds() - StartTime);
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_AI_EQS_TickNotifies);
		for (int32 Index = RunningQueries.Num() - 1; Index >= 0 && FinishedQueriesCount > 0; --Index)
		{
			TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[Index];

			if (QueryInstance->IsFinished())
			{
				UE_VLOG_EQS(*QueryInstance.Get(), LogEQS, All);

#if USE_EQS_DEBUGGER
				EQSDebugger.StoreQuery(GetWorld(), QueryInstance);
#endif // USE_EQS_DEBUGGER

				QueryInstance->FinishDelegate.ExecuteIfBound(QueryInstance);
				RunningQueries.RemoveAtSwap(Index, 1, /*bAllowShrinking=*/false);

				--FinishedQueriesCount;
			}
		}
	}
}

void UEnvQueryManager::OnWorldCleanup()
{
	if (RunningQueries.Num() > 0)
	{
		// @todo investigate if this is even needed. We should be fine with just removing all queries
		TArray<TSharedPtr<FEnvQueryInstance> > RunningQueriesCopy = RunningQueries;
		RunningQueries.Reset();

		for (int32 Index = 0; Index < RunningQueriesCopy.Num(); Index++)
		{
			TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueriesCopy[Index];
			if (QueryInstance->IsFinished() == false)
			{
				QueryInstance->MarkAsFailed();
				QueryInstance->FinishDelegate.ExecuteIfBound(QueryInstance);
			}
		}
	}
}

void UEnvQueryManager::RegisterExternalQuery(TSharedPtr<FEnvQueryInstance> QueryInstance)
{
	ExternalQueries.Add(QueryInstance->QueryID, QueryInstance);
}

void UEnvQueryManager::UnregisterExternalQuery(TSharedPtr<FEnvQueryInstance> QueryInstance)
{
	ExternalQueries.Remove(QueryInstance->QueryID);
}

namespace EnvQueryTestSort
{
	struct FAllMatching
	{
		FORCEINLINE bool operator()(const UEnvQueryTest& TestA, const UEnvQueryTest& TestB) const
		{
			// cheaper tests go first
			if (TestB.Cost > TestA.Cost)
			{
				return true;
			}

			// conditions go first
			const bool bConditionA = (TestA.TestPurpose != EEnvTestPurpose::Score); // Is Test A Filtering?
			const bool bConditionB = (TestB.TestPurpose != EEnvTestPurpose::Score); // Is Test B Filtering?
			if (bConditionA && !bConditionB)
			{
				return true;
			}

			// keep connection order (sort stability)
			return (TestB.TestOrder > TestA.TestOrder);
		}
	};

	struct FSingleResult
	{
		FSingleResult(EEnvTestCost::Type InHighestCost) : HighestCost(InHighestCost) {}

		FORCEINLINE bool operator()(const UEnvQueryTest& TestA, const UEnvQueryTest& TestB) const
		{
			// cheaper tests go first
			if (TestB.Cost > TestA.Cost)
			{
				return true;
			}

			const bool bConditionA = (TestA.TestPurpose != EEnvTestPurpose::Score); // Is Test A Filtering?
			const bool bConditionB = (TestB.TestPurpose != EEnvTestPurpose::Score); // Is Test B Filtering?
			if (TestA.Cost == HighestCost)
			{
				// highest cost: weights go first, conditions later (first match will return result)
				if (!bConditionA && bConditionB)
				{
					return true;
				}
			}
			else
			{
				// lower costs: conditions go first to reduce amount of items
				if (bConditionA && !bConditionB)
				{
					return true;
				}
			}

			// keep connection order (sort stability)
			return (TestB.TestOrder > TestA.TestOrder);
		}

	protected:
		TEnumAsByte<EEnvTestCost::Type> HighestCost;
	};
}

UEnvQuery* UEnvQueryManager::FindQueryTemplate(const FString& QueryName) const
{
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCache.Num(); InstanceIndex++)
	{
		const UEnvQuery* QueryTemplate = InstanceCache[InstanceIndex].Template;

		if (QueryTemplate != NULL && QueryTemplate->GetName() == QueryName)
		{
			return const_cast<UEnvQuery*>(QueryTemplate);
		}
	}

	for (FObjectIterator It(UEnvQuery::StaticClass()); It; ++It)
	{
		if (It->GetName() == QueryName)
		{
			return Cast<UEnvQuery>(*It);
		}
	}

	return NULL;
}

TSharedPtr<FEnvQueryInstance> UEnvQueryManager::CreateQueryInstance(const UEnvQuery* Template, EEnvQueryRunMode::Type RunMode)
{
	if (Template == nullptr || Template->Options.Num() == 0)
	{
		UE_CLOG(Template != nullptr && Template->Options.Num() == 0, LogEQS, Warning, TEXT("Query [%s] doesn't have any valid options!"), *Template->GetName());
		return nullptr;
	}

	// try to find entry in cache
	FEnvQueryInstance* InstanceTemplate = NULL;
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCache.Num(); InstanceIndex++)
	{
		if (InstanceCache[InstanceIndex].Template->GetFName() == Template->GetFName() &&
			InstanceCache[InstanceIndex].Instance.Mode == RunMode)
		{
			InstanceTemplate = &InstanceCache[InstanceIndex].Instance;
			break;
		}
	}

	// and create one if can't be found
	if (InstanceTemplate == NULL)
	{
		SCOPE_CYCLE_COUNTER(STAT_AI_EQS_LoadTime);
		
		// duplicate template in manager's world for BP based nodes
		UEnvQuery* LocalTemplate = (UEnvQuery*)StaticDuplicateObject(Template, this, *Template->GetName());

		{
			// memory stat tracking: temporary variable will exist only inside this section
			FEnvQueryInstanceCache NewCacheEntry;
			NewCacheEntry.Template = LocalTemplate;
			NewCacheEntry.Instance.QueryName = LocalTemplate->GetName();
			NewCacheEntry.Instance.Mode = RunMode;

			const int32 Idx = InstanceCache.Add(NewCacheEntry);
			InstanceTemplate = &InstanceCache[Idx].Instance;
		}

		for (int32 OptionIndex = 0; OptionIndex < LocalTemplate->Options.Num(); OptionIndex++)
		{
			UEnvQueryOption* MyOption = LocalTemplate->Options[OptionIndex];
			if (MyOption == NULL || MyOption->Generator == NULL)
			{
				UE_LOG(LogEQS, Error, TEXT("Trying to spawn a query with broken Template (empty generator): %s, option %d"),
					*GetNameSafe(LocalTemplate), OptionIndex);

				continue;
			}

			UEnvQueryOption* LocalOption = (UEnvQueryOption*)StaticDuplicateObject(MyOption, this, TEXT("None"));
			UEnvQueryGenerator* LocalGenerator = (UEnvQueryGenerator*)StaticDuplicateObject(MyOption->Generator, this, TEXT("None"));
			LocalTemplate->Options[OptionIndex] = LocalOption;
			LocalOption->Generator = LocalGenerator;

			EEnvTestCost::Type HighestCost(EEnvTestCost::Low);
			TArray<UEnvQueryTest*> SortedTests = MyOption->Tests;
			TSubclassOf<UEnvQueryItemType> GeneratedType = MyOption->Generator->ItemType;
			for (int32 TestIndex = SortedTests.Num() - 1; TestIndex >= 0; TestIndex--)
			{
				UEnvQueryTest* TestOb = SortedTests[TestIndex];
				if (TestOb == NULL || !TestOb->IsSupportedItem(GeneratedType))
				{
					UE_LOG(LogEQS, Warning, TEXT("Query [%s] can't use test [%s] in option %d [%s], removing it"),
						*GetNameSafe(LocalTemplate), *GetNameSafe(TestOb), OptionIndex, *MyOption->Generator->OptionName);

					SortedTests.RemoveAt(TestIndex);
				}
				else if (HighestCost < TestOb->Cost)
				{
					HighestCost = TestOb->Cost;
				}
			}

			LocalOption->Tests.Reset(SortedTests.Num());
			for (int32 TestIdx = 0; TestIdx < SortedTests.Num(); TestIdx++)
			{
				UEnvQueryTest* LocalTest = (UEnvQueryTest*)StaticDuplicateObject(SortedTests[TestIdx], this, TEXT("None"));
				LocalOption->Tests.Add(LocalTest);
			}

			// use locally referenced duplicates
			SortedTests = LocalOption->Tests;

			if (SortedTests.Num())
			{
				switch (RunMode)
				{
				case EEnvQueryRunMode::SingleResult:
					SortedTests.Sort(EnvQueryTestSort::FSingleResult(HighestCost));
					break;

				case EEnvQueryRunMode::RandomBest5Pct:
				case EEnvQueryRunMode::RandomBest25Pct:
				case EEnvQueryRunMode::AllMatching:
					SortedTests.Sort(EnvQueryTestSort::FAllMatching());
					break;

				default:
					{
						UEnum* RunModeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EEnvQueryRunMode"));
						UE_LOG(LogEQS, Warning, TEXT("Query [%s] can't be sorted for RunMode: %d [%s]"),
							*GetNameSafe(LocalTemplate), (int32)RunMode, RunModeEnum ? *RunModeEnum->GetEnumName(RunMode) : TEXT("??"));
					}
				}
			}

			CreateOptionInstance(LocalOption, SortedTests, *InstanceTemplate);
		}
	}

	if (InstanceTemplate->Options.Num() == 0)
	{
		return nullptr;
	}

	// create new instance
	TSharedPtr<FEnvQueryInstance> NewInstance(new FEnvQueryInstance(*InstanceTemplate));
	return NewInstance;
}

void UEnvQueryManager::CreateOptionInstance(UEnvQueryOption* OptionTemplate, const TArray<UEnvQueryTest*>& SortedTests, FEnvQueryInstance& Instance)
{
	FEnvQueryOptionInstance OptionInstance;
	OptionInstance.Generator = OptionTemplate->Generator;
	OptionInstance.ItemType = OptionTemplate->Generator->ItemType;

	OptionInstance.Tests.AddZeroed(SortedTests.Num());
	for (int32 TestIndex = 0; TestIndex < SortedTests.Num(); TestIndex++)
	{
		UEnvQueryTest* TestOb = SortedTests[TestIndex];
		OptionInstance.Tests[TestIndex] = TestOb;
	}

	DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, Instance.Options.GetAllocatedSize());

	const int32 AddedIdx = Instance.Options.Add(OptionInstance);

	INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, Instance.Options.GetAllocatedSize() + Instance.Options[AddedIdx].GetAllocatedSize());
}

UEnvQueryContext* UEnvQueryManager::PrepareLocalContext(TSubclassOf<UEnvQueryContext> ContextClass)
{
	UEnvQueryContext* LocalContext = LocalContextMap.FindRef(ContextClass->GetFName());
	if (LocalContext == NULL)
	{
		LocalContext = (UEnvQueryContext*)StaticDuplicateObject(ContextClass.GetDefaultObject(), this, TEXT("None"));
		LocalContexts.Add(LocalContext);
		LocalContextMap.Add(ContextClass->GetFName(), LocalContext);
	}

	return LocalContext;
}

float UEnvQueryManager::FindNamedParam(int32 QueryId, FName ParamName) const
{
	float ParamValue = 0.0f;

	const TWeakPtr<FEnvQueryInstance>* QueryInstancePtr = ExternalQueries.Find(QueryId);
	if (QueryInstancePtr)
	{
		TSharedPtr<FEnvQueryInstance> QueryInstance = (*QueryInstancePtr).Pin();
		if (QueryInstance.IsValid())
		{
			ParamValue = QueryInstance->NamedParams.FindRef(ParamName);
		}
	}
	else
	{
		for (int32 QueryIndex = 0; QueryIndex < RunningQueries.Num(); QueryIndex++)
		{
			const TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[QueryIndex];
			if (QueryInstance->QueryID == QueryId)
			{
				ParamValue = QueryInstance->NamedParams.FindRef(ParamName);
				break;
			}
		}
	}

	return ParamValue;
}

//----------------------------------------------------------------------//
// BP functions
//----------------------------------------------------------------------//
UEnvQueryInstanceBlueprintWrapper* UEnvQueryManager::RunEQSQuery(UObject* WorldContext, UEnvQuery* QueryTemplate, UObject* Querier, TEnumAsByte<EEnvQueryRunMode::Type> RunMode, TSubclassOf<UEnvQueryInstanceBlueprintWrapper> WrapperClass)
{ 
	if (QueryTemplate == nullptr)
	{
		return nullptr;
	}

	UEnvQueryManager* EQSManager = GetCurrent(WorldContext);
	UEnvQueryInstanceBlueprintWrapper* QueryInstanceWrapper = nullptr;

	if (EQSManager)
	{
		QueryInstanceWrapper = NewObject<UEnvQueryInstanceBlueprintWrapper>((UClass*)(WrapperClass)  ? (UClass*)WrapperClass : UEnvQueryInstanceBlueprintWrapper::StaticClass());
		check(QueryInstanceWrapper);
		FEnvQueryRequest QueryRequest(QueryTemplate, Querier);
		// @todo named params still missing support
		//QueryRequest.SetNamedParams(QueryParams);

		QueryInstanceWrapper->SetRunMode(RunMode);
		QueryInstanceWrapper->SetQueryID(QueryRequest.Execute(RunMode, QueryInstanceWrapper, &UEnvQueryInstanceBlueprintWrapper::OnQueryFinished));
	}
	
	return QueryInstanceWrapper;
}

//----------------------------------------------------------------------//
// FEQSDebugger
//----------------------------------------------------------------------//
#if USE_EQS_DEBUGGER

void FEQSDebugger::StoreQuery(UWorld* InWorld, TSharedPtr<FEnvQueryInstance>& Query)
{
	StoredQueries.Remove(NULL);
	if (!Query.IsValid())
	{
		return;
	}
	TArray<FEnvQueryInfo>& AllQueries = StoredQueries.FindOrAdd(Query->Owner.Get());
		
	bool bFoundQuery = false;
	for (auto It = AllQueries.CreateIterator(); It; ++It)
	{
		auto& CurrentQuery = (*It);
		if (CurrentQuery.Instance.IsValid() && Query->QueryName == CurrentQuery.Instance->QueryName)
		{
			CurrentQuery.Instance = Query;
			CurrentQuery.Timestamp = InWorld->GetTimeSeconds();
			bFoundQuery = true;
			break;
		}
	}
	if (!bFoundQuery)
	{
		FEnvQueryInfo Info;
		Info.Instance = Query;
		Info.Timestamp = InWorld->GetTimeSeconds();
		AllQueries.AddUnique(Info);
	}
}

TArray<FEQSDebugger::FEnvQueryInfo>&  FEQSDebugger::GetAllQueriesForOwner(const UObject* Owner)
{
	TArray<FEnvQueryInfo>& AllQueries = StoredQueries.FindOrAdd(Owner);
	return AllQueries;
}

#endif // USE_EQS_DEBUGGER
