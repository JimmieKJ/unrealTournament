// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	return TestScript.IsValid() && TestScript->IsFinished();
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
	if (UFunctionalTestingManager::RunAllFunctionalTests(GWorld, bClearLog, bRunLooped) == false)
	{
		UE_LOG(LogFunctionalTest, Error, TEXT("No functional testing script on map."));
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
			FFunctionalTestingModule::Get()->RunAllTestsOnMap(/*bClearLog=*/true, bLooped);
		}
		return true;
	}
	return false;
}

FStaticSelfRegisteringExec FuncTestExecRegistration(FuncTestExec);