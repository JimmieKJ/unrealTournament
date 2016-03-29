// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "AutomationCommon.h"

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


DEFINE_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap);
bool FStartFTestsOnMap::Update()
{
	//should really be wait until the map is properly loaded....in PIE or gameplay....
	//ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(10.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTriggerFTests);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFTestsToFinish);	
	
	return true;
}

/**
 * 
 */

// create test base class
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FClientFunctionalTestingMapsBase, "Project.Maps.Client Functional Testing", (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter))


/** 
 * Requests a enumeration of all maps to be loaded
 */
void FClientFunctionalTestingMapsBase::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	TArray<FString> FileList;
	// Look directly on disk. Very slow!
	FPackageName::FindPackagesInDirectory(FileList, *FPaths::GameContentDir());

	// Iterate over all files, adding the ones with the map extension..
	for (int32 FileIndex = 0; FileIndex < FileList.Num(); FileIndex++)
	{
		const FString& Filename = FileList[FileIndex];

		// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
		if (FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension())
		{
			if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
			{
				const FString BaseFilename = FPaths::GetBaseFilename(Filename);
				// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
				if (BaseFilename.Find(TEXT("FTEST_")) == 0)
				{
					OutBeautifiedNames.Add(BaseFilename);
					OutTestCommands.Add(Filename);
				}
			}
		}
	}
}


/** 
 * Execute the loading of each map and performance captures
 *
 * @param Parameters - Should specify which map name to load
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FClientFunctionalTestingMapsBase::RunTest(const FString& Parameters)
{
	FString MapName = Parameters;

	bool bCanProceed = AutomationOpenMap(MapName);

	if (bCanProceed)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap());
		return true;
	}

	UE_LOG(LogFunctionalTesting, Error, TEXT("Failed to start the %s map (possibly due to BP compilation issues)"), *MapName);
	return false;
}

#undef LOCTEXT_NAMESPACE

#endif //WITH_DEV_AUTOMATION_TESTS