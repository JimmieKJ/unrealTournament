// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"

#include "UObject/Object.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogFunctionalTest);

FFuncTestManager::FFuncTestManager()
{
#if WITH_EDITOR
	FWorldDelegates::GetAssetTags.AddRaw(this, &FFuncTestManager::OnGetAssetTagsForWorld);
#endif
}

FFuncTestManager::~FFuncTestManager()
{
#if WITH_EDITOR
	FWorldDelegates::GetAssetTags.RemoveAll(this);
#endif
}

void FFuncTestManager::OnGetAssetTagsForWorld(const UWorld* World, TArray<UObject::FAssetRegistryTag>& OutTags)
{
#if WITH_EDITOR
	int32 Tests = 0;
	FString TestNames;
	for ( TActorIterator<AFunctionalTest> ActorItr(const_cast<UWorld*>( World )); ActorItr; ++ActorItr )
	{
		AFunctionalTest* FunctionalTest = *ActorItr;

		// Only include enabled tests in the list of functional tests to run.
		if ( FunctionalTest->IsEnabled() )
		{
			Tests++;
			TestNames.Append(FunctionalTest->GetActorLabel() + TEXT("|") + FunctionalTest->GetName());
			TestNames.Append(TEXT(";"));
		}
	}

	OutTags.Add(UObject::FAssetRegistryTag("Tests", FString::FromInt(Tests), UObject::FAssetRegistryTag::TT_Numerical));
	OutTags.Add(UObject::FAssetRegistryTag("TestNames", TestNames, UObject::FAssetRegistryTag::TT_Hidden));
#endif
}

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

UWorld* FFuncTestManager::GetTestWorld()
{
	UWorld* TestWorld = nullptr;

#if WITH_EDITOR
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for ( const FWorldContext& Context : WorldContexts )
	{
		if ( ( Context.WorldType == EWorldType::PIE ) && ( Context.World() != nullptr ) )
		{
			TestWorld = Context.World();
		}
	}
#endif
	if ( !TestWorld )
	{
		TestWorld = GWorld;
		if (GIsEditor)
		{
			UE_LOG(LogFunctionalTest, Warning, TEXT("Functional Test using GWorld.  Not correct for PIE"));
		}
	}

	return TestWorld;
}

void FFuncTestManager::RunAllTestsOnMap(bool bClearLog, bool bRunLooped)
{
	if ( UWorld* TestWorld = GetTestWorld() )
	{
		if ( UFunctionalTestingManager::RunAllFunctionalTests(TestWorld, bClearLog, bRunLooped) == false )
		{
			UE_LOG(LogFunctionalTest, Error, TEXT("No functional testing script on map."));
		}
	}
}

void FFuncTestManager::RunTestOnMap(const FString& TestName, bool bClearLog, bool bRunLooped)
{
	if ( UWorld* TestWorld = GetTestWorld() )
	{
		if ( UFunctionalTestingManager::RunAllFunctionalTests(TestWorld, bClearLog, bRunLooped, true, TestName) == false )
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
			if (!FFunctionalTestingModule::Get()->IsRunning())
			{
				FFunctionalTestingModule::Get()->RunAllTestsOnMap(/*bClearLog=*/true, bLooped);
			}
		}
		return true;
	}
	return false;
}

FStaticSelfRegisteringExec FuncTestExecRegistration(FuncTestExec);