// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#if WITH_EDITOR
#include "Engine/Brush.h"
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Editor/UnrealEd/Public/FileHelpers.h"
#include "AssetRegistryModule.h"

extern UNREALED_API class UEditorEngine* GEditor;
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "FunctionalTesting"

struct FEditorProxy 
{
	FEditorProxy()
		: EditorEngine(NULL)
	{}

	UEditorEngine* operator->()
	{
		if (EditorEngine == NULL)
		{
			EditorEngine = Cast<UEditorEngine>(GEngine);
		}
		return EditorEngine;
	}

protected:
	UEditorEngine* EditorEngine;
};

static FEditorProxy EditorEngine;


DEFINE_LOG_CATEGORY_STATIC(LogFunctionalTesting, Log, All);

/**
 * Wait for the given amount of time
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FFTestWaitLatentCommand, float, Duration);

bool FFTestWaitLatentCommand::Update()
{
	float NewTime = FPlatformTime::Seconds();
	if (NewTime - StartTime >= Duration)
	{
		return true;
	}
	return false;
}

DEFINE_LATENT_AUTOMATION_COMMAND(FWaitForFTestsToFinish);
bool FWaitForFTestsToFinish::Update()
{
	return FFunctionalTestingModule::Get()->IsRunning() == false;
}

DEFINE_LATENT_AUTOMATION_COMMAND(FTriggerFTests);
bool FTriggerFTests::Update()
{
	IFuncTestManager* Manager = FFunctionalTestingModule::Get();
	if (Manager->IsFinished() == false)
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

DEFINE_LATENT_AUTOMATION_COMMAND(FFTestEndPlay);
bool FFTestEndPlay::Update()
{
#if WITH_EDITOR
	if (GIsEditor && EditorEngine->PlayWorld != NULL)
	{
		EditorEngine->EndPlayMap();
	}
#endif // WITH_EDITOR
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap);
bool FStartFTestsOnMap::Update()
{
	ADD_LATENT_AUTOMATION_COMMAND(FFTestWaitLatentCommand(3.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTriggerFTests);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFTestsToFinish);	
	ADD_LATENT_AUTOMATION_COMMAND(FFTestEndPlay);
	
	return true;
}

/**
 * 
 */

// create test base class
IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(FFunctionalTestingMapsBase, "Project.Maps.Functional Testing", (EAutomationTestFlags::ATF_Game | EAutomationTestFlags::ATF_Editor))
// implement specific class with non-standard overrides
class FFunctionalTestingMaps : public FFunctionalTestingMapsBase
{
public:
	FFunctionalTestingMaps(const FString& InName) : FFunctionalTestingMapsBase(InName)
	{}
	virtual ~FFunctionalTestingMaps()
	{
		UFunctionalTestingManager::SetAutomationExecutionInfo(NULL);
	}
};

namespace
{
	FFunctionalTestingMaps FFunctionalTestingMapsAutomationTestInstance(TEXT("FFunctionalTestingMaps"));

	void CreateFTestingLogPage()
	{
		static bool bMessageLogPageCreated = false;
		if (bMessageLogPageCreated == false)
		{
			FMessageLog("FunctionalTestingLog").NewPage(LOCTEXT("NewLogLabel", "Functional Test"));
			bMessageLogPageCreated = true;
		}
	}
}

/** 
 * Requests a enumeration of all maps to be loaded
 */
void FFunctionalTestingMapsBase::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	//Setting the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	//Variable setups
	TArray<FAssetData> ObjectList;
	FARFilter AssetFilter;

	//Generating the list of assets.
	//This list is being filtered by the game folder and class type.  The results are placed into the ObjectList variable.
	AssetFilter.ClassNames.Add(UWorld::StaticClass()->GetFName());

	//removed path as a filter as it causes two large lists to be sorted.  Filtering on "game" directory on iteration
	//AssetFilter.PackagePaths.Add("/Game");
	AssetFilter.bRecursiveClasses = false;
	AssetFilter.bRecursivePaths = true;
	AssetRegistryModule.Get().GetAssets(AssetFilter, ObjectList);

	//Loop through the list of assets, make their path full and a string, then add them to the test.
	for (auto ObjIter = ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
	{
		const FAssetData& Asset = *ObjIter;
		FString Filename = Asset.ObjectPath.ToString();

		if (Filename.StartsWith("/Game"))
		{
			//convert to full paths
			Filename = FPackageName::LongPackageNameToFilename(Filename);

			const FString BaseFilename = FPaths::GetBaseFilename(Filename);
			// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
			if (BaseFilename.Find(TEXT("FTEST_")) == 0)
			{
				if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
				{
					OutBeautifiedNames.Add(BaseFilename);
					OutTestCommands.Add(Filename);
				}
			}
		}
	}
}

#if WITH_EDITOR
struct FFailedGameStartHandler
{
	bool bCanProceed;

	FFailedGameStartHandler()
	{
		bCanProceed = true;
		FEditorDelegates::EndPIE.AddRaw(this, &FFailedGameStartHandler::OnEndPIE);
	}

	~FFailedGameStartHandler()
	{
		FEditorDelegates::EndPIE.RemoveAll(this);
	}

	bool CanProceed() const { return bCanProceed; }

	void OnEndPIE(const bool bInSimulateInEditor)
	{
		bCanProceed = false;
	}
};
#endif // WITH_EDITOR

/** 
 * Execute the loading of each map and performance captures
 *
 * @param Parameters - Should specify which map name to load
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FFunctionalTestingMapsBase::RunTest(const FString& Parameters)
{
	FString MapName = Parameters;

	CreateFTestingLogPage();

	SetSuppressLogs(true);
	ExecutionInfo.Clear();
	UFunctionalTestingManager::SetAutomationExecutionInfo(&ExecutionInfo);
	
	bool bCanProceed = true;

#if WITH_EDITOR
	if (GIsEditor)
	{
		bool bLoadAsTemplate = false;
		bool bShowProgress = false;
		FEditorFileUtils::LoadMap(MapName, bLoadAsTemplate, bShowProgress);

		// special precaution needs to be taken while triggering PIE since it can
		// fail if there are BP compilation issues
		FFailedGameStartHandler FailHandler;
		EditorEngine->PlayInEditor(GWorld, /*bInSimulateInEditor=*/false);
		bCanProceed = FailHandler.CanProceed();
	}
	else
#endif // WITH_EDITOR
	{
		check(GEngine->GetWorldContexts().Num() == 1);
		check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

		GEngine->Exec(GEngine->GetWorldContexts()[0].World(), *FString::Printf(TEXT("Open %s"), *MapName));
	}

	if (bCanProceed)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap());
		return true;
	}

	SetSuppressLogs(false);
	UE_LOG(LogFunctionalTesting, Error, TEXT("Failed to start the %s map (possibly due to BP compilation issues)"), *MapName);
	return false;
}

#undef LOCTEXT_NAMESPACE