// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#if WITH_EDITOR

//----------------------------------------------------------------------//
// 6/25 @todo these will be removed once marge from main comes
#include "UnrealEd.h"
class UFactory;
//----------------------------------------------------------------------//

#endif

#define LOCTEXT_NAMESPACE "FunctionalTesting"

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

FAutomationTestExecutionInfo* UFunctionalTestingManager::ExecutionInfo = NULL;

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

bool UFunctionalTestingManager::RunAllFunctionalTests(UObject* WorldContext, bool bNewLog, bool bRunLooped, bool bInWaitForNavigationBuildFinish, FString ReproString)
{
	UFunctionalTestingManager* Manager = GetManager(WorldContext);

	if (Manager->bIsRunning)
	{
		UE_LOG(LogFunctionalTest, Warning, TEXT("Functional tests are already running, aborting."));
		return true;
	}
	
	FMessageLog FunctionalTestingLog("FunctionalTestingLog");
	FunctionalTestingLog.Open();
	if (bNewLog)
	{
		FunctionalTestingLog.NewPage(LOCTEXT("NewLogLabel", "Functional Test"));
	}

	Manager->bFinished = false;
	Manager->bLooped = bRunLooped;
	Manager->bWaitForNavigationBuildFinish = bInWaitForNavigationBuildFinish;
	Manager->CurrentIteration = 0;
	Manager->TestsLeft.Reset();
	Manager->SetReproString(ReproString);

	Manager->SetUpTests();

	if (Manager->TestReproStrings.Num() > 0)
	{
		static const FText RunningFromReproString = LOCTEXT("RunningFromReproString", "Running tests indicated by Repro String:");
		AddLogItem(RunningFromReproString);
		AddLogItem(FText::FromString(ReproString));
		Manager->TriggerFirstValidTest();
	}
	else
	{
		for (TActorIterator<AFunctionalTest> It(GWorld); It; ++It)
		{
			AFunctionalTest* Test = (*It);
			if (Test != NULL && Test->bIsEnabled == true)
			{
				Manager->AllTests.Add(Test);
			}
		}

		if (Manager->AllTests.Num() > 0)
		{
			Manager->TestsLeft = Manager->AllTests;
			Manager->TriggerFirstValidTest();
		}
	}

	if (Manager->bIsRunning == false)
	{
		AddLogItem(LOCTEXT("NoTestsDefined", "No tests defined on map or . DONE.")); 
		return false;
	}

#if WITH_EDITOR
	FEditorDelegates::EndPIE.AddUObject(Manager, &UFunctionalTestingManager::OnEndPIE);
#endif // WITH_EDITOR
	
	return true;
}

void UFunctionalTestingManager::OnEndPIE(const bool bIsSimulating)
{
	RemoveFromRoot();
}

void UFunctionalTestingManager::TriggerFirstValidTest()
{
	UWorld* World = GetWorld();
	bIsRunning = World != NULL && World->GetNavigationSystem() != NULL;

	if (bInitialDelayApplied == true && (bWaitForNavigationBuildFinish == false || UNavigationSystem::IsNavigationBeingBuilt(World) == false))
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
	if (FTest->IsSuccessful() == false)
	{
		if (GatheredFailedTestsReproString.IsEmpty() == false)
		{
			GatheredFailedTestsReproString += FFunctionalTesting::ReproStringTestSeparator;
		}
		GatheredFailedTestsReproString += FTest->GetReproString();
	}

	if (FTest->OnWantsReRunCheck() == false && FTest->WantsToRunAgain() == false)
	{
		TestsLeft.RemoveSingle(FTest);
		/*if (bDiscardSuccessfulTests && FTest->IsSuccessful())
		{
			AllTests.RemoveSingle(FTest);
		}*/
		FTest->CleanUp();
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
	if (GatheredFailedTestsReproString.IsEmpty() == false)
	{
		static const FText ReproStringLabel = LOCTEXT("ReproString", "Repro String:");
		AddLogItem(ReproStringLabel);
		AddLogItem(FText::FromString(GatheredFailedTestsReproString));
	}

	if (bLooped == true)
	{
		++CurrentIteration;

		// reset
		ensure(TestReproStrings.Num() == 0);
		SetReproString(StartingReproString);
		GatheredFailedTestsReproString = TEXT("");
		TestsLeft = AllTests;

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("IterationIndex"), CurrentIteration);
		AddLogItem(FText::Format(LOCTEXT("StartingIteration", "----- Starting iteration {IterationIndex} -----"), Arguments));
		bIsRunning = RunFirstValidTest();
		if (bIsRunning == false)
		{
			static const FText FailedToStartAnotherIterationText = LOCTEXT("FailedToStartAnotherIteration", "Failed to start another iteration.");
			AddWarning(FailedToStartAnotherIterationText);
		}
	}
	else
	{
		bFinished = true;
		AddLogItem(LOCTEXT("TestDone", "DONE."));
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
			const FString SingleTestReproString = TestReproStrings.Pop(/*bAllowShrinking=*/false);
			SingleTestReproString.ParseIntoArray(TestParams, TEXT("#"), /*InCullEmpty=*/true);
			
			if (TestParams.Num() == 0)
			{
				AddWarning(FText::FromString(FString::Printf(TEXT("Unable to parse \'%s\'"), *SingleTestReproString)));
				continue;
			}
			// first param is the test name. Look for it		
			const FString TestName = TestParams[0];
			TestParams.RemoveAt(0, 1, /*bAllowShrinking=*/false);
			AFunctionalTest* TestToRun = FindObject<AFunctionalTest>(TestsOuter, *TestName);			
			if (TestToRun)
			{
				TestToRun->TestFinishedObserver = TestFinishedObserver;
				if (TestToRun->StartTest(TestParams))
				{
					bTestSuccessfullyTriggered = true;
					break;
				}
				else
				{
					AddWarning(FText::FromString(FString::Printf(TEXT("Test \'%s\' failed to start"), *TestToRun->GetName())));
				}
			}
			else
			{
				AddWarning(FText::FromString(FString::Printf(TEXT("Unable to find test \'%s\'"), *TestName)));
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
				ensure(TestsLeft[Index]->bIsEnabled);
				TestsLeft[Index]->TestFinishedObserver = TestFinishedObserver;
				if (TestsLeft[Index]->StartTest())
				{
					bTestSuccessfullyTriggered = true;
					break;
				}
				else
				{
					AddWarning(FText::FromString(FString::Printf(TEXT("Test: %s failed to start"), *TestsLeft[Index]->GetName())));
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

//----------------------------------------------------------------------//
// logging
//----------------------------------------------------------------------//
void UFunctionalTestingManager::AddError(const FText& InError)
{
	FMessageLog FunctionalTestingLog("FunctionalTestingLog");
	FunctionalTestingLog.Error(InError);
	if (ExecutionInfo)
	{
		ExecutionInfo->Errors.Add(InError.ToString());
	}
}


void UFunctionalTestingManager::AddWarning(const FText& InWarning)
{
	FMessageLog FunctionalTestingLog("FunctionalTestingLog");
	FunctionalTestingLog.Warning(InWarning);
	if (ExecutionInfo)
	{
		ExecutionInfo->Warnings.Add(InWarning.ToString());
	}
}


void UFunctionalTestingManager::AddLogItem(const FText& InLogItem)
{
	FMessageLog FunctionalTestingLog("FunctionalTestingLog");
	FunctionalTestingLog.Info(InLogItem);
	if (ExecutionInfo)
	{
		ExecutionInfo->LogItems.Add(InLogItem.ToString());
	}
}
#undef LOCTEXT_NAMESPACE
