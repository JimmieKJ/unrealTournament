// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogFunctionalTest);

void FFuncTestManager::SetScript(class UFunctionalTestingManager* NewScript)
{
	TestScript = NewScript;
}

bool FFuncTestManager::IsRunning() const 
{ 
	return TestScript.IsValid() && TestScript->IsRunning();
}

bool FFuncTestManager::IsFinished() const
{
	return (!TestScript.IsValid() || TestScript->IsFinished());
}

void FFuncTestManager::SetLooping(const bool bLoop)
{ 
	if (TestScript.IsValid())
	{
		TestScript->SetLooped(bLoop);
	}
}

void FFuncTestManager::RunAllTestsOnMap(bool bClearLog, bool bRunLooped)
{
	UWorld* TestWorld = NULL;
#if WITH_EDITOR
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : WorldContexts)
	{
		if ((Context.WorldType == EWorldType::PIE) && (Context.World() != NULL))
		{
			TestWorld = Context.World();
		}
	}

#endif
	if (!TestWorld)
	{
		TestWorld = GWorld;
		UE_LOG(LogFunctionalTest, Warning, TEXT("Functional Test using GWorld.  Not correct for PIE"));
	}

	if (TestWorld)
	{
		if (UFunctionalTestingManager::RunAllFunctionalTests(TestWorld, bClearLog, bRunLooped) == false)
		{
			UE_LOG(LogFunctionalTest, Error, TEXT("No functional testing script on map."));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Exec
//////////////////////////////////////////////////////////////////////////

static bool FuncTestExec(UWorld* InWorld, const TCHAR* Command,FOutputDevice& Ar)
{
	if(FParse::Command(&Command,TEXT("ftest")))
	{
		if(FParse::Command(&Command,TEXT("start")))
		{
			const bool bLooped = FParse::Command(&Command,TEXT("loop"));

			//instead of allowing straight use of the functional test framework, this should go through the automation framework and kick off one of the Editor/Client functional tests
			//FFunctionalTestingModule::Get()->RunAllTestsOnMap(/*bClearLog=*/true, bLooped);
			FString TestName = InWorld->GetName();
			FString AutomationString = FString::Printf(TEXT("Automation RunTests FunctionalTesting.%s"), *TestName);
			GEngine->Exec(InWorld, *AutomationString);
		}
		return true;
	}
	return false;
}

FStaticSelfRegisteringExec FuncTestExecRegistration(FuncTestExec);