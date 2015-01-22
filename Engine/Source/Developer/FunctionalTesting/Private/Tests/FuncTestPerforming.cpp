// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#if WITH_EDITOR
#include "Engine/Brush.h"
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#include "Editor/UnrealEd/Public/FileHelpers.h"

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
IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(FFunctionalTestingMapsBase, "Maps.Functional Testing", (EAutomationTestFlags::ATF_Game | EAutomationTestFlags::ATF_Editor))
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
	TArray<FString> FileList;
#if WITH_EDITOR
	FEditorFileUtils::FindAllPackageFiles(FileList);
#else
	// Look directly on disk. Very slow!
	{
		TArray<FString> RootContentPaths;
		FPackageName::QueryRootContentPaths( RootContentPaths );
		for( TArray<FString>::TConstIterator RootPathIt( RootContentPaths ); RootPathIt; ++RootPathIt )
		{
			const FString& RootPath = *RootPathIt;
			const FString& ContentFolder = FPackageName::LongPackageNameToFilename( RootPath );
			FileList.Add( ContentFolder );
		}
	}
#endif

	// Iterate over all files, adding the ones with the map extension..
	for( int32 FileIndex = 0; FileIndex< FileList.Num(); FileIndex++ )
	{
		const FString& Filename = FileList[FileIndex];

		const FString BaseFilename = FPaths::GetBaseFilename(Filename);
		// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
		if ( FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension() 
			&& BaseFilename.Find(TEXT("FTEST_")) == 0) 
		{
			if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
			{
				OutBeautifiedNames.Add(BaseFilename);
				OutTestCommands.Add(Filename);
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
bool FFunctionalTestingMapsBase::RunTest(const FString& Parameters)
{
	FString MapName = Parameters;

	CreateFTestingLogPage();

	SetSuppressLogs(true);
	ExecutionInfo.Clear();
	UFunctionalTestingManager::SetAutomationExecutionInfo(&ExecutionInfo);

#if WITH_EDITOR
	if (GIsEditor)
	{
		bool bLoadAsTemplate = false;
		bool bShowProgress = false;
		FEditorFileUtils::LoadMap(MapName, bLoadAsTemplate, bShowProgress);
		EditorEngine->PlayInEditor(GWorld, /*bInSimulateInEditor=*/false);
	}
	else
#endif // WITH_EDITOR
	{
		check(GEngine->GetWorldContexts().Num() == 1);
		check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

		GEngine->Exec(GEngine->GetWorldContexts()[0].World(), *FString::Printf(TEXT("Open %s"), *MapName));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap());

	return true;
}

#undef LOCTEXT_NAMESPACE