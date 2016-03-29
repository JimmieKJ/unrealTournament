// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnitTestFrameworkPCH.h"
#include "AutomationTest.h"
#include "EngineGlobals.h"			// for GEngine
#include "Engine/GameEngine.h"		// for GetGameWorld()
#include "Engine/World.h"			// for PersistentLevel
#include "UObjectHash.h"			// for StaticFindObjectFastExplicit
#include "UTFUnitTestInterface.h"

namespace UTFGameAutomationTestsImpl
{
	void GatherGameTestActors(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands)
	{
		auto GatherUnitTestActors = [&OutBeautifiedNames, &OutTestCommands](UWorld* GameWorld)
		{
			if (GameWorld != nullptr)
			{
				if (ULevel* GameLevel = GameWorld->PersistentLevel)
				{
					for (AActor* Actor : GameLevel->Actors)
					{
						if (Actor == nullptr)
						{
							continue;
						}

						UClass* ActorClass = Actor->GetClass();
						if (ActorClass->ImplementsInterface(UUTFUnitTestInterface::StaticClass()))
						{
							const FString TestName = IUTFUnitTestInterface::Execute_GetTestName(Actor);
							OutBeautifiedNames.Add(TestName);

							OutTestCommands.Add(Actor->GetPathName());
						}
					}
				}
			}
		};

		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			GatherUnitTestActors(GameEngine->GetGameWorld());
		}
		else
		{
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				if (WorldContext.WorldType == EWorldType::PIE ||
					WorldContext.WorldType == EWorldType::Game)
				{
					GatherUnitTestActors(WorldContext.World());
					break;
				}
			}
		}
	}
}

/*******************************************************************************
 * FUTFResetGameActorTest
 ******************************************************************************/

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FUTFResetGameActorTest, "UnitTestFramework.Game.ResetUnitTestActors", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)

//------------------------------------------------------------------------------
void FUTFResetGameActorTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UTFGameAutomationTestsImpl::GatherGameTestActors(OutBeautifiedNames, OutTestCommands);
}

//------------------------------------------------------------------------------
bool FUTFResetGameActorTest::RunTest(const FString& ObjectPath)
{
	AActor* TargetActor = FindObject<AActor>(/*Outer =*/nullptr, *ObjectPath);
	if (TargetActor == nullptr)
	{
		AddError(FString::Printf(TEXT("Failed to find '%s' game object."), *ObjectPath));
		return false;
	}

	UClass* ActorClass = TargetActor->GetClass();
	if (!ActorClass->ImplementsInterface(UUTFUnitTestInterface::StaticClass()))
	{
		AddError(FString::Printf(TEXT("Target object does not implement the unit test interface: '%s'."), *ObjectPath));
		return false;
	}
	
	IUTFUnitTestInterface::Execute_ResetTest(TargetActor);
	return true;
}

/*******************************************************************************
 * FUTFGameActorTest
 ******************************************************************************/

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FUTFGameActorTest, "UnitTestFramework.Game.UnitTestActors", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)

//------------------------------------------------------------------------------
void FUTFGameActorTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	UTFGameAutomationTestsImpl::GatherGameTestActors(OutBeautifiedNames, OutTestCommands);
}

//------------------------------------------------------------------------------
bool FUTFGameActorTest::RunTest(const FString& ObjectPath)
{
	AActor* TargetActor = FindObject<AActor>(/*Outer =*/nullptr, *ObjectPath);
	if (TargetActor == nullptr)
	{
		AddError(FString::Printf(TEXT("Failed to find '%s' game object."), *ObjectPath));
		return false;
	}

	UClass* ActorClass = TargetActor->GetClass();
	if (!ActorClass->ImplementsInterface(UUTFUnitTestInterface::StaticClass()))
	{
		AddError(FString::Printf(TEXT("Target object does not implement the unit test interface: '%s'."), *ObjectPath));
		return false;
	}

	const FString TestName = IUTFUnitTestInterface::Execute_GetTestName(TargetActor);
	
	IUTFUnitTestInterface::Execute_ResetTest(TargetActor);
	EUTFUnitTestResult TestResult = IUTFUnitTestInterface::Execute_GetTestResult(TargetActor);
	
	switch (TestResult)
	{
	case EUTFUnitTestResult::UTF_Success:
		{
			AddLogItem(FString::Printf(TEXT("Unit Test '%s': SUCCESS"), *TestName));
		} break;

	case EUTFUnitTestResult::UTF_Failure:
		{
			AddError(FString::Printf(TEXT("Unit Test '%s': FAILURE"), *TestName));
		} break;

	default:
	case EUTFUnitTestResult::UTF_Unresolved:
		{
			AddWarning(FString::Printf(TEXT("Unit Test '%s': UNRESOLVED"), *TestName));
		} break;
	}

	return (TestResult != EUTFUnitTestResult::UTF_Failure);
}
