// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "AutomationCommon.h"
#include "AssetRegistryModule.h"

#if WITH_DEV_AUTOMATION_TESTS

#define LOCTEXT_NAMESPACE "FunctionalTesting"

DEFINE_LOG_CATEGORY_STATIC(LogFunctionalTesting, Log, All);

DEFINE_LATENT_AUTOMATION_COMMAND(FWaitForFTestsToFinish);
bool FWaitForFTestsToFinish::Update()
{
	return FFunctionalTestingModule::Get()->IsRunning() == false;
}

DEFINE_LATENT_AUTOMATION_COMMAND(FTriggerFTests);
bool FTriggerFTests::Update()
{
	IFuncTestManager* Manager = FFunctionalTestingModule::Get();
	if (Manager->IsFinished())
	{
		// if tests have been already triggered by level script just make sure it's not looping
		if (Manager->IsRunning())
		{
			FFunctionalTestingModule::Get()->SetLooping(false);
		}
		else
		{
			FFunctionalTestingModule::Get()->RunAllTestsOnMap(false, false);
		}
	}

	return true;
}


DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTriggerFTest, FString, TestName);
bool FTriggerFTest::Update()
{
	IFuncTestManager* Manager = FFunctionalTestingModule::Get();
	if ( Manager->IsFinished() )
	{
		// if tests have been already triggered by level script just make sure it's not looping
		if ( Manager->IsRunning() )
		{
			FFunctionalTestingModule::Get()->SetLooping(false);
		}
		else
		{
			FFunctionalTestingModule::Get()->RunTestOnMap(TestName, false, false);
		}
	}

	return true;
}


DEFINE_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap);
bool FStartFTestsOnMap::Update()
{
	// This should now be handled by your IsReady override of the functional test.
	//should really be wait until the map is properly loaded....in PIE or gameplay....
	//ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(15.f));

	ADD_LATENT_AUTOMATION_COMMAND(FTriggerFTests);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFTestsToFinish);	

	return true;
}


DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FStartFTestOnMap, FString, TestName);
bool FStartFTestOnMap::Update()
{
	ADD_LATENT_AUTOMATION_COMMAND(FTriggerFTest(TestName));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFTestsToFinish);

	return true;
}

/**
 * 
 */

// Project.Maps.Client Functional Testing
// Project.Maps.Functional Tests


class FClientFunctionalTestingMapsBase : public FAutomationTestBase
{
public:
	FClientFunctionalTestingMapsBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	virtual FString GetTestOpenCommand(const FString& Parameter) const override
	{
		FString MapPath;
		FString MapTestName;
		Parameter.Split(TEXT(";"), &MapPath, &MapTestName);

		return FString::Printf(TEXT("Automate.OpenMapAndFocusActor %s %s"), *MapPath, *MapTestName);
	}

	virtual FString GetTestAssetPath(const FString& Parameter) const override
	{
		FString MapPath;
		FString MapTestName;
		Parameter.Split(TEXT(";"), &MapPath, &MapTestName);

		return MapPath;
	}
};


// create test base class
IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(FClientFunctionalTestingMaps, FClientFunctionalTestingMapsBase, "Project.Functional Tests", (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter))


/** 
 * Requests a enumeration of all maps to be loaded
 */
void FClientFunctionalTestingMaps::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	if ( !AssetRegistry.IsLoadingAssets() )
	{
		TArray<FAssetData> MapList;
		if ( AssetRegistry.GetAssetsByClass(UWorld::StaticClass()->GetFName(), /*out*/ MapList) )
		{
			for ( const FAssetData& MapAsset : MapList )
			{
				const FString* Tests = MapAsset.TagsAndValues.Find(TEXT("Tests"));
				const FString* TestNames = MapAsset.TagsAndValues.Find(TEXT("TestNames"));

				if ( Tests && TestNames )
				{
					int32 TestCount = FCString::Atoi(**Tests);
					if ( TestCount > 0 )
					{
						TArray<FString> MapTests;
						( *TestNames ).ParseIntoArray(MapTests, TEXT(";"), true);

						for ( const FString& MapTest : MapTests )
						{
							FString BeautifulTestName;
							FString RealTestName;

							if ( MapTest.Split(TEXT("|"), &BeautifulTestName, &RealTestName) )
							{
								OutBeautifiedNames.Add(MapAsset.AssetName.ToString() + TEXT(".") + *BeautifulTestName);
								OutTestCommands.Add(MapAsset.PackageName.ToString() + TEXT(";") + *RealTestName);
							}
						}
					}
				}
				else if ( MapAsset.AssetName.ToString().Find(TEXT("FTEST_")) == 0 )
				{
					OutBeautifiedNames.Add(MapAsset.AssetName.ToString());
					OutTestCommands.Add(MapAsset.PackageName.ToString());
				}
			}
		}
	}
}


// @todo this is a temporary solution. Once we know how to get test's hands on a proper world
// this function should be redone/removed
static UWorld* GetAnyGameWorld()
{
	UWorld* TestWorld = nullptr;
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for ( const FWorldContext& Context : WorldContexts )
	{
		if ( ( ( Context.WorldType == EWorldType::PIE ) || ( Context.WorldType == EWorldType::Game ) ) && ( Context.World() != NULL ) )
		{
			TestWorld = Context.World();
			break;
		}
	}

	return TestWorld;
}


/** 
 * Execute the loading of each map and performance captures
 *
 * @param Parameters - Should specify which map name to load
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FClientFunctionalTestingMaps::RunTest(const FString& Parameters)
{
	TArray<FString> ParamArray;
	Parameters.ParseIntoArray(ParamArray, TEXT(";"), true);

	FString MapName = ParamArray[0];
	FString MapTestName = ( ParamArray.Num() > 1 ) ? ParamArray[1] : TEXT("");

	bool bCanProceed = false;

	UWorld* TestWorld = GetAnyGameWorld();
	if ( TestWorld && TestWorld->GetMapName() == MapName )
	{
		// Map is already loaded.
		bCanProceed = true;
	}
	else
	{
		bCanProceed = AutomationOpenMap(MapName);
	}

	if (bCanProceed)
	{
		if ( MapTestName.IsEmpty() )
		{
			ADD_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap());
		}
		else
		{
			ADD_LATENT_AUTOMATION_COMMAND(FStartFTestOnMap(MapTestName));
		}

		return true;
	}

	/// FAutomationTestFramework::GetInstance().UnregisterAutomationTest

	//	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	//  ADD_LATENT_AUTOMATION_COMMAND(FExitGameCommand);

	UE_LOG(LogFunctionalTesting, Error, TEXT("Failed to start the %s map (possibly due to BP compilation issues)"), *MapName);
	return false;
}

#undef LOCTEXT_NAMESPACE

#endif //WITH_DEV_AUTOMATION_TESTS