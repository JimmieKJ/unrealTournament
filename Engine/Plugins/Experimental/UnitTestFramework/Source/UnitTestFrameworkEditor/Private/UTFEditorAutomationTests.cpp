// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnitTestFrameworkEditorPCH.h"
#include "UTFEditorAutomationTests.h"
#include "AutomationTest.h"
#include "AssetRegistryModule.h"	// for FAssetRegistry::GetAssets()
#include "AssetData.h"
#include "Engine/World.h"			// for UWorld::StaticClass()
#include "PackageName.h"			// for FPackageName::LongPackageNameToFilename()
#include "FileHelpers.h"			// for FEditorFileUtils::LoadMap()
#include "Editor.h"					// for FEditorDelegates::OnMapOpened
#include "UTFUnitTestInterface.h"
#include "Script.h"					// for FEditorScriptExecutionGuard
#include "Engine/Blueprint.h"

/*******************************************************************************
 * FUTFEditorAutomationTests
 ******************************************************************************/
DEFINE_LOG_CATEGORY_STATIC(LogUTFEditorAutomationTests, Log, All);

const FName FUTFEditorAutomationTests::InEditorTestTag(TEXT("UTF_InEditorTest"));
const FName FUTFEditorAutomationTests::UnitTestLevelTag(TEXT("UTF_UnitTestLevel"));

//------------------------------------------------------------------------------
bool FUTFEditorAutomationTests::IsClassSuitableForInEditorTesting(UClass* Class)
{
	if (Class->ImplementsInterface(UUTFUnitTestInterface::StaticClass()))
	{
		if (UBlueprint* Blueprint = CastChecked<UBlueprint>(Class->ClassGeneratedBy))
		{
			UPackage* BlueprintPackage = Blueprint->GetOutermost();
			UMetaData* MetaData = BlueprintPackage->GetMetaData();

			return MetaData->HasValue(Blueprint, FUTFEditorAutomationTests::InEditorTestTag);
		}
	}
	return false;
}

/*******************************************************************************
 * FUTFEditorMapTest
 ******************************************************************************/

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FUTFEditorMapTest, "UnitTestFramework.Editor.RunUnitTestMaps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)

//------------------------------------------------------------------------------
void FUTFEditorMapTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	FARFilter AssetFilter;
	AssetFilter.ClassNames.Add(UWorld::StaticClass()->GetFName());
	AssetFilter.TagsAndValues.Add(FUTFEditorAutomationTests::UnitTestLevelTag, TEXT("TRUE"));

	TArray<FAssetData> UnitTestLevels;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.GetAssets(AssetFilter, UnitTestLevels);

	for (FAssetData const& LevelAsset : UnitTestLevels)
	{
		OutTestCommands.Add(LevelAsset.ObjectPath.ToString());
		OutBeautifiedNames.Add(LevelAsset.AssetName.ToString());
	}
}

//------------------------------------------------------------------------------
bool FUTFEditorMapTest::RunTest(const FString& LevelAssetPath)
{
	// don't care about any map load warnings/errors, etc. 
	SetSuppressLogs(true);

	FDelegateHandle OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddLambda([this, &LevelAssetPath](const FString& /*Filename*/, bool /*bAsTemplate*/)
	{ 
		SetSuppressLogs(false);

		FString PackageName, AssetName;
		LevelAssetPath.Split(TEXT("."), &PackageName, &AssetName);

		UPackage* WorldPackage = FindPackage(/*Outer =*/nullptr, *PackageName);
		if (UWorld* TestWorld = FindObject<UWorld>(WorldPackage, *AssetName))
		{
			if (ULevel* TestLevel = TestWorld->PersistentLevel)
			{
				bool bExecutedTests = false;
				for (AActor* Actor : TestLevel->Actors)
				{
					if (Actor == nullptr)
					{
						continue;
					}

					UClass* ActorClass = Actor->GetClass();
					if (ActorClass->ImplementsInterface(UUTFUnitTestInterface::StaticClass()))
					{
						if (!FUTFEditorAutomationTests::IsClassSuitableForInEditorTesting(ActorClass))
						{
							AddLogItem(FString::Printf(TEXT("Skipping test for '%s', as it is not marked safe for in-editor testing."), *ActorClass->GetName()));
							continue;
						}

						// notifies ProcessEvent() that it is ok to execute the following functions in the editor
						FEditorScriptExecutionGuard ScriptGuard;

						const FString TestName = IUTFUnitTestInterface::Execute_GetTestName(Actor);
						// @TODO: consider the need for flagging tests as "runtime only" (cannot be ran in the editor)
						EUTFUnitTestResult TestResult = IUTFUnitTestInterface::Execute_GetTestResult(Actor);
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
						
						bExecutedTests = true;
					}
				}

				if (!bExecutedTests)
				{
					AddWarning(FString::Printf(TEXT("No editor unit tests found in '%s'."), *PackageName));
				}
			}
			else
			{
				AddError(FString::Printf(TEXT("Null PersistentLevel after loading '%s'."), *PackageName));
			}
		}
		else
		{
			AddError(FString::Printf(TEXT("Failed to find '%s' world object, post load."), *PackageName));
		}
	});

	const FString MapFilePath = FPackageName::LongPackageNameToFilename(LevelAssetPath);
	FEditorFileUtils::LoadMap(MapFilePath, /*bLoadAsTemplate =*/false, /*bShowProgress =*/false);
	FEditorDelegates::OnMapOpened.Remove(OnMapOpenedHandle);

	return (ExecutionInfo.Errors.Num() == 0);
}

/*******************************************************************************
* FUTFEditorActorTest
******************************************************************************/

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FUTFEditorActorTest, "UnitTestFramework.Editor.UnitTestActors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)

//------------------------------------------------------------------------------
void FUTFEditorActorTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::Editor)
		{
			if (UWorld* EditorWorld = WorldContext.World())
			{
				if (ULevel* EditorLevel = EditorWorld->PersistentLevel)
				{
					for (AActor* Actor : EditorLevel->Actors)
					{
						if (Actor == nullptr)
						{
							continue;
						}

						UClass* ActorClass = Actor->GetClass();
						if (FUTFEditorAutomationTests::IsClassSuitableForInEditorTesting(ActorClass))
						{
							const FString TestName = IUTFUnitTestInterface::Execute_GetTestName(Actor);
							OutBeautifiedNames.Add(TestName);

							OutTestCommands.Add(Actor->GetPathName());
						}
					}
				}
			}
			break;
		}
	}
}

//------------------------------------------------------------------------------
bool FUTFEditorActorTest::RunTest(const FString& ObjectPath)
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
