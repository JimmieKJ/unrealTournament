// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#if WITH_EDITOR

//----------------------------------------------------------------------//
// 6/25 @todo these will be removed once marge from main comes
#include "UnrealEd.h"
class UFactory;
//----------------------------------------------------------------------//

#endif

namespace FFunctionalTesting
{
	const TCHAR* ReproStringTestSeparator = TEXT("@");
	const TCHAR* ReproStringParamsSeparator = TEXT("#");
}


//struct FFuncTestingTickHelper : FTickableGameObject
//{
//	class UFunctionalTestingManager* Manager;
//
//	FFuncTestingTickHelper() : Manager(NULL) {}
//	virtual void Tick(float DeltaTime);
//	virtual bool IsTickable() const { return Owner && !((AActor*)Owner)->IsPendingKillPending(); }
//	virtual bool IsTickableInEditor() const { return true; }
//	virtual TStatId GetStatId() const ;
//};
//
////----------------------------------------------------------------------//
//// 
////----------------------------------------------------------------------//
//void FFuncTestingTickHelper::Tick(float DeltaTime)
//{
//	if (Manager->IsPendingKill() == false)
//	{
//		Manager->TickMe(DeltaTime);
//	}
//}
//
//TStatId FFuncTestingTickHelper::GetStatId() const 
//{
//	RETURN_QUICK_DECLARE_CYCLE_STAT(FRecastTickHelper, STATGROUP_Tickables);
//}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//


UFunctionalTestingManager::UFunctionalTestingManager( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bIsRunning(false)
	, bFinished(false)
	, bLooped(false)
	, bWaitForNavigationBuildFinish(false)
	, bInitialDelayApplied(false)
	, CurrentIteration(INDEX_NONE)
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		TestFinishedObserver = FFunctionalTestDoneSignature::CreateUObject(this, &UFunctionalTestingManager::OnTestDone);
	}
}

void UFunctionalTestingManager::SetUpTests()
{
	OnSetupTests.Broadcast();
}

struct FSortTestActorsByName
{
	FORCEINLINE bool operator()(const AFunctionalTest& A, const AFunctionalTest& B) const
	{
		return A.GetName() > B.GetName(); 
	}
};

bool UFunctionalTestingManager::RunAllFunctionalTests(UObject* WorldContext, bool bNewLog, bool bRunLooped, bool bInWaitForNavigationBuildFinish, FString ReproString)
{
	UFunctionalTestingManager* Manager = GetManager(WorldContext);

	if (Manager->bIsRunning)
	{
		UE_LOG(LogFunctionalTest, Warning, TEXT("Functional tests are already running, aborting."));
		return true;
	}
	
	WorldContext->GetWorld()->ForceGarbageCollection(true);

	Manager->bFinished = false;
	Manager->bLooped = bRunLooped;
	Manager->bWaitForNavigationBuildFinish = bInWaitForNavigationBuildFinish;
	Manager->CurrentIteration = 0;
	Manager->TestsLeft.Reset();
	Manager->AllTests.Reset();
	Manager->SetReproString(ReproString);

	Manager->SetUpTests();

	if (Manager->TestReproStrings.Num() > 0)
	{
		UE_LOG(LogFunctionalTest, Log, TEXT("Running tests indicated by Repro String: %s"), *ReproString);
		Manager->TriggerFirstValidTest();
	}
	else
	{
		for (TActorIterator<APhasedAutomationActorBase> It(WorldContext->GetWorld()); It; ++It)
		{
			APhasedAutomationActorBase* PAA = (*It);
			Manager->OnTestsComplete.AddDynamic(PAA, &APhasedAutomationActorBase::OnFunctionalTestingComplete); 
			Manager->OnTestsBegin.AddDynamic(PAA, &APhasedAutomationActorBase::OnFunctionalTestingBegin); 
		}

		for (TActorIterator<AFunctionalTest> It(WorldContext->GetWorld()); It; ++It)
		{
			AFunctionalTest* Test = (*It);
			if (Test != nullptr && Test->IsEnabled() == true)
			{
				Manager->AllTests.Add(Test);
			}
		}

		Manager->AllTests.Sort(FSortTestActorsByName());

		if (Manager->AllTests.Num() > 0)
		{
			Manager->TestsLeft = Manager->AllTests;
			
			Manager->OnTestsBegin.Broadcast();

			Manager->TriggerFirstValidTest();
		}
	}

	if (Manager->bIsRunning == false)
	{
		UE_LOG(LogFunctionalTest, Warning, TEXT("No tests defined on map or . DONE."));
		return false;
	}

	return true;
}

void UFunctionalTestingManager::TriggerFirstValidTest()
{
	UWorld* World = GetWorld();
	bIsRunning = World != nullptr && World->GetNavigationSystem() != nullptr;

	if (bInitialDelayApplied == true && (bWaitForNavigationBuildFinish == false || UNavigationSystem::IsNavigationBeingBuilt(World) == false) && World->AreActorsInitialized())
	{
		bIsRunning = RunFirstValidTest();
		if (bIsRunning == false)
		{
			AllTestsDone();
		}
	}
	else
	{
		bInitialDelayApplied = true;
		static const float WaitingTime = 0.25f;
		World->GetTimerManager().SetTimer(TriggerFirstValidTestTimerHandle, this, &UFunctionalTestingManager::TriggerFirstValidTest, WaitingTime);
	}
}

UFunctionalTestingManager* UFunctionalTestingManager::GetManager(UObject* WorldContext)
{
	UFunctionalTestingManager* Manager = FFunctionalTestingModule::Get()->GetCurrentScript();

	if (Manager == NULL)
	{
		UObject* Outer = WorldContext ? WorldContext : (UObject*)GetTransientPackage();
		Manager = NewObject<UFunctionalTestingManager>(Outer);
		FFunctionalTestingModule::Get()->SetScript(Manager);

		// add to root and get notified on world cleanup to remove from root on map cleanup
		Manager->AddToRoot();
		FWorldDelegates::OnWorldCleanup.AddUObject(Manager, &UFunctionalTestingManager::OnWorldCleanedUp);
	}

	return Manager;
}

UWorld* UFunctionalTestingManager::GetWorld() const
{
	return GEngine->GetWorldFromContextObject(GetOuter());
}

void UFunctionalTestingManager::OnWorldCleanedUp(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	UWorld* MyWorld = GetWorld();
	if (MyWorld == World)
	{
		RemoveFromRoot();
	}
}

void UFunctionalTestingManager::OnTestDone(AFunctionalTest* FTest)
{
	// add a delay
	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Requesting to build next tile if necessary"),
		STAT_FSimpleDelegateGraphTask_RequestingToBuildNextTileIfNecessary,
		STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UFunctionalTestingManager::NotifyTestDone, FTest),
		GET_STATID(STAT_FSimpleDelegateGraphTask_RequestingToBuildNextTileIfNecessary), NULL, ENamedThreads::GameThread);
}

void UFunctionalTestingManager::NotifyTestDone(AFunctionalTest* FTest)
{
	//if (FTest->IsSuccessful() == false)
	//{
	//	if (GatheredFailedTestsReproString.IsEmpty() == false)
	//	{
	//		GatheredFailedTestsReproString += FFunctionalTesting::ReproStringTestSeparator;
	//	}
	//	GatheredFailedTestsReproString += FTest->GetReproString();
	//}

	if (FTest->OnWantsReRunCheck() == false && FTest->WantsToRunAgain() == false)
	{
		//We can also do named reruns. These are lower priority than those triggered above.
		//These names can be querried by phases to alter behviour in re-runs.
		if (FTest->RerunCauses.Num() > 0)
		{
			FTest->CurrentRerunCause = FTest->RerunCauses.Pop();
		}
		else
		{
			TestsLeft.RemoveSingle(FTest);
			/*if (bDiscardSuccessfulTests && FTest->IsSuccessful())
			{
			AllTests.RemoveSingle(FTest);
			}*/
			FTest->CleanUp();
		}
	}

	if (TestsLeft.Num() > 0 || TestReproStrings.Num() > 0)
	{
		bIsRunning = RunFirstValidTest();
	}
	else
	{
		bIsRunning = false;
	}

	if (bIsRunning == false)
	{
		AllTestsDone();
	}
}

void UFunctionalTestingManager::AllTestsDone()
{
	//TODO AUTOMATION Should we revive this?  There's no good way at the moment to know if the test -actually-
	// failed, because warnings and such could actually fail the test.

	//if (GatheredFailedTestsReproString.IsEmpty() == false)
	//{
	//	UE_LOG(LogFunctionalTest, Log, TEXT("Repro String : %s"), *GatheredFailedTestsReproString);
	//}

	if (bLooped == true)
	{
		++CurrentIteration;

		// reset
		ensure(TestReproStrings.Num() == 0);
		SetReproString(StartingReproString);
		//GatheredFailedTestsReproString = TEXT("");
		TestsLeft = AllTests;

		UE_LOG(LogFunctionalTest, Log, TEXT("----- Starting iteration %d -----"), CurrentIteration);
		bIsRunning = RunFirstValidTest();
		if (bIsRunning == false)
		{
			UE_LOG(LogFunctionalTest, Warning, TEXT("Failed to start another iteration."));
		}
	}
	else
	{
		OnTestsComplete.Broadcast();
		bFinished = true;
		UE_LOG(LogFunctionalTest, Log, TEXT("DONE."));
		RemoveFromRoot();
	}
}

bool UFunctionalTestingManager::RunFirstValidTest()
{
	bool bTestSuccessfullyTriggered = false;

	if (TestReproStrings.Num() > 0)
	{
		UWorld* World = GetWorld();
		UObject* TestsOuter = World ? (UObject*)(World->PersistentLevel) : (UObject*)(ANY_PACKAGE);

		while (TestReproStrings.Num() > 0)
		{
			TArray<FString> TestParams;
			const FString SingleTestReproString = TestReproStrings[0];
			TestReproStrings.RemoveAt(0);

			SingleTestReproString.ParseIntoArray(TestParams, TEXT("#"), /*InCullEmpty=*/true);
			
			if (TestParams.Num() == 0)
			{
				UE_LOG(LogFunctionalTest, Warning, TEXT("Unable to parse \'%s\'"), *SingleTestReproString);
				continue;
			}

			// first param is the test name. Look for it		
			const FString TestName = TestParams[0];
			TestParams.RemoveAt(0, 1, /*bAllowShrinking=*/false);
			AFunctionalTest* TestToRun = FindObject<AFunctionalTest>(TestsOuter, *TestName);			
			if (TestToRun)
			{
				TestToRun->TestFinishedObserver = TestFinishedObserver;
				if (TestToRun->RunTest(TestParams))
				{
					bTestSuccessfullyTriggered = true;
					break;
				}
				else
				{
					UE_LOG(LogFunctionalTest, Warning, TEXT("Test \'%s\' failed to start"), *TestToRun->GetName());
				}
			}
			else
			{
				UE_LOG(LogFunctionalTest, Warning, TEXT("Unable to find test \'%s\'"), *TestName);
			}
		}
	}
	
	if (bTestSuccessfullyTriggered == false)
	{
		for (int32 Index = TestsLeft.Num()-1; Index >= 0; --Index)
		{
			bool bRemove = TestsLeft[Index] == NULL;
			if (TestsLeft[Index] != NULL)
			{
				ensure(TestsLeft[Index]->IsEnabled());
				TestsLeft[Index]->TestFinishedObserver = TestFinishedObserver;
				if (TestsLeft[Index]->RunTest())
				{
					if (TestsLeft[Index]->IsRunning() == true)
					{
						bTestSuccessfullyTriggered = true;
						break;
					}
					else
					{
						// test finished instantly, remove it
						bRemove = true;
					}
				}
				else
				{
					UE_LOG(LogFunctionalTest, Warning, TEXT("Test: %s failed to start"), *TestsLeft[Index]->GetName());
					bRemove = true;
				}
			}

			if (bRemove)
			{
				TestsLeft.RemoveAtSwap(Index, 1, false);
			}
		}
	}

	return bTestSuccessfullyTriggered;
}

void UFunctionalTestingManager::TickMe(float DeltaTime)
{
		
}

void UFunctionalTestingManager::SetReproString(FString ReproString)
{
	TestReproStrings.Reset();
	StartingReproString = ReproString;
	if (ReproString.IsEmpty() == false)
	{
		ReproString.ParseIntoArray(TestReproStrings, FFunctionalTesting::ReproStringTestSeparator, /*InCullEmpty=*/true);
	}
}