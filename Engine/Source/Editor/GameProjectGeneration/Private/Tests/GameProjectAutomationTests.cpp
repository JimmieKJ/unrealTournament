// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../GameProjectGenerationPrivatePCH.h"

#include "DesktopPlatformModule.h"
#include "TargetPlatform.h"
#include "GameProjectUtils.h"
#include "Tests/AutomationTestSettings.h"
#include "AutomationEditorCommon.h"
#include "GameFramework/PlayerStart.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameProjectGenerationTests, Log, All);

namespace GameProjectAutomationUtils
{
	/**
	* Generates the desired project file name
	*/
	static FString GetDesiredProjectFilename()
	{
		UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
		check(AutomationTestSettings);

		FString ProjectName;
		const FString ProjectNameOverride = AutomationTestSettings->BuildPromotionTest.NewProjectSettings.NewProjectNameOverride;
		if (ProjectNameOverride.Len())
		{
			ProjectName = ProjectNameOverride;
		}
		else
		{
			ProjectName = TEXT("NewTestProject");
		}

		FString ProjectPath;
		const FString ProjectPathOverride = AutomationTestSettings->BuildPromotionTest.NewProjectSettings.NewProjectFolderOverride.Path;
		if (ProjectPathOverride.Len())
		{
			ProjectPath = ProjectPathOverride;
		}
		else
		{
			ProjectPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FDesktopPlatformModule::Get()->GetDefaultProjectCreationPath());
		}


		const FString Filename = ProjectName + TEXT(".") + FProjectDescriptor::GetExtension();
		FString ProjectFilename = FPaths::Combine(*ProjectPath, *ProjectName, *Filename);
		FPaths::MakePlatformFilename(ProjectFilename);

		return ProjectFilename;
	}
}

/**
* Automation test to clean up old test project files
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectCleanupTest, "System.Promotion.Project Promotion Pass.Step 1 Blank Project Creation.Cleanup Potential Project Location", EAutomationTestFlags::ATF_Editor);
bool FBuildPromotionNewProjectCleanupTest::RunTest(const FString& Parameters)
{
	FString DesiredProjectFilename = GameProjectAutomationUtils::GetDesiredProjectFilename();

	if (FPaths::FileExists(DesiredProjectFilename))
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Found an old project file at %s"), *DesiredProjectFilename);
		if (FPaths::IsProjectFilePathSet())
		{
			FString CurrentProjectPath = FPaths::GetProjectFilePath();
			if (CurrentProjectPath == DesiredProjectFilename)
			{
				UE_LOG(LogGameProjectGenerationTests, Warning, TEXT("Can not clean up the target project location because it is the current active project."));
			}
			else
			{
				UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Removing files from old project path: %s"), *FPaths::GetPath(DesiredProjectFilename));
				bool bEnsureExists = false;
				bool bDeleteEntireTree = true;
				IFileManager::Get().DeleteDirectory(*FPaths::GetPath(DesiredProjectFilename), bEnsureExists, bDeleteEntireTree);
			}
		}
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Target project location is clear"));
	}

	return true;
}

/**
* Automation test to create a new project
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectCreateTest, "System.Promotion.Project Promotion Pass.Step 1 Blank Project Creation.Create Project", EAutomationTestFlags::ATF_Editor);
bool FBuildPromotionNewProjectCreateTest::RunTest(const FString& Parameters)
{
	FString DesiredProjectFilename = GameProjectAutomationUtils::GetDesiredProjectFilename();

	if (FPaths::FileExists(DesiredProjectFilename))
	{
		UE_LOG(LogGameProjectGenerationTests, Warning, TEXT("A project already exists at the target location: %s"), *DesiredProjectFilename);
		const FString OldProjectFolder = FPaths::GetPath(DesiredProjectFilename);
		const FString OldProjectName = FPaths::GetBaseFilename(DesiredProjectFilename);
		const FString RootFolder = FPaths::GetPath(OldProjectFolder);

		//Add a number to the end
		for (uint32 i = 2;; ++i)
		{
			const FString PossibleProjectName = FString::Printf(TEXT("%s%i"), *OldProjectName, i);
			const FString PossibleProjectFilename = FString::Printf(TEXT("%s/%s/%s.%s"), *RootFolder, *PossibleProjectName, *PossibleProjectName, *FProjectDescriptor::GetExtension());
			if (!FPaths::FileExists(PossibleProjectFilename))
			{
				DesiredProjectFilename = PossibleProjectFilename;
				UE_LOG(LogGameProjectGenerationTests, Warning, TEXT("Changing the target project name to: %s"), *FPaths::GetBaseFilename(DesiredProjectFilename));
				break;
			}
		}
	}

	FText FailReason, FailLog;
	if (GameProjectUtils::CreateProject(FProjectInformation(DesiredProjectFilename, false, true), FailReason, FailLog))
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Generated a new project: %s"), *DesiredProjectFilename);
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test successful!"));
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("\nPlease switch to the new project and continue to Step 2."));
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Error, TEXT("Could not generate new project: %s - %s"), *FailReason.ToString(), *FailLog.ToString());
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed!"));
	}

	return true;
}

/**
* Automation test to create a simple level and save it
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectMapTest, "System.Promotion.Project Promotion Pass.Step 2 Basic Level Creation.Create Basic Level", EAutomationTestFlags::ATF_Editor);
bool FBuildPromotionNewProjectMapTest::RunTest(const FString& Parameters)
{
	//New level
	UWorld* CurrentWorld = AutomationEditorCommonUtils::CreateNewMap();
	if (CurrentWorld)
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Adding Level Geometry"));
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Error, TEXT("Failed to create an empty level"));
	}

	

	//Add some bsp and a player start
	GEditor->Exec(CurrentWorld, TEXT("BRUSH Scale 1 1 1"));
	for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); i++)
	{
		FLevelEditorViewportClient* ViewportClient = GEditor->LevelViewportClients[i];
		if (!ViewportClient->IsOrtho())
		{
			ViewportClient->SetViewLocation(FVector(176, 2625, 2075));
			ViewportClient->SetViewRotation(FRotator(319, 269, 1));
		}
	}
	ULevel* CurrentLevel = CurrentWorld->GetCurrentLevel();

	//Cube Additive Brush
	UCubeBuilder* CubeAdditiveBrushBuilder = Cast<UCubeBuilder>(GEditor->FindBrushBuilder(UCubeBuilder::StaticClass()));
	CubeAdditiveBrushBuilder->X = 4096.0f;
	CubeAdditiveBrushBuilder->Y = 4096.0f;
	CubeAdditiveBrushBuilder->Z = 128.0f;
	CubeAdditiveBrushBuilder->Build(CurrentWorld);
	GEditor->Exec(CurrentWorld, TEXT("BRUSH MOVETO X=0 Y=0 Z=0"));
	GEditor->Exec(CurrentWorld, TEXT("BRUSH ADD"));

	//Add a playerstart
	const FTransform Transform(FRotator(-16384, 0, 0), FVector(0.f, 1750.f, 166.f));
	AActor* PlayerStart = GEditor->AddActor(CurrentWorld->GetCurrentLevel(), APlayerStart::StaticClass(), Transform);
	if (PlayerStart)
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Added a player start"));
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Error, TEXT("Failed to add a player start"));
	}

	// Save the map
	FEditorFileUtils::SaveLevel(CurrentLevel, TEXT("/Game/Maps/NewProjectTest"));
	UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Saved map"));

	if (ExecutionInfo.Errors.Num() > 0)
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed!"));
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test successful!"));
	}

	return true;
}