// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../GameProjectGenerationPrivatePCH.h"

#include "DesktopPlatformModule.h"
#include "TargetPlatform.h"
#include "GameProjectUtils.h"
#include "Tests/AutomationTestSettings.h"
#include "AutomationEditorCommon.h"
#include "GameFramework/PlayerStart.h"
#include "../TemplateCategory.h"
#include "../TemplateItem.h"

#if WITH_DEV_AUTOMATION_TESTS

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


	/* 
	 * Create a project from a templalte with a given criteria
	 * @oaram	InTemplates			List of available project templates
	 * @param	InTargetedHardware	Target hardware (EHardwareClass)
	 * @param	InGraphicPreset		Graphics preset (EGraphicsPreset)
	 * @param	InCategory			Target category (EContentSourceCategory)
	 * @param	bInCopyStarterContent Should the starter content be copied also
	 * @param	OutMatchedProjects	Total projects matching criteria
	 * @param	OutCreatedProjects	Total projects succesfully created
	 */
	static void CreateProjectSet(TMap<FName, TArray<TSharedPtr<FTemplateItem>> >& InTemplates, EHardwareClass::Type InTargetedHardware, EGraphicsPreset::Type InGraphicPreset, EContentSourceCategory InCategory, bool bInCopyStarterContent, int32 OutMatchedProjects, int32 OutCreatedProjects)
	{		
		// If this is empty, it will use the same name for each project, otherwise it will create a project based on target platform and source template
		FString TestRootFolder;// = "ProjectTests";
		
		// This has the code remove the projects once created
		bool bRemoveCreatedProjects = true;		
		OutCreatedProjects = 0;
		OutMatchedProjects = 0;
		UEnum* SourceCategoryEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EContentSourceCategory"));

		// The category name int he FTemplateItem is not the same as the enum definition EContentSourceCategory - convert it
		FName CategoryName;
		if( InCategory == EContentSourceCategory::BlueprintFeature)
		{
			CategoryName = FTemplateCategory::BlueprintCategoryName;
		}
		else if (InCategory == EContentSourceCategory::CodeFeature)
		{
			CategoryName = FTemplateCategory::CodeCategoryName;
		}
		else
		{
			// We didnt match a category
			if (SourceCategoryEnum == nullptr)
			{
				UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed! Unknown category type %d"), *SourceCategoryEnum->GetEnumName(int32(InCategory)));
			}
			else
			{
				UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed! Unknown category type %d"), (int32)(InCategory));
			}
			
			return;
		}

		// Iterate all templates and try to create those that match the required criteria
		for (auto EachTemplate : InTemplates)
		{
			FName Name = EachTemplate.Key;						
			// Check this is the correct category
			if (Name == CategoryName)
			{
				// Now iterate each template in the category
				for (TSharedPtr<FTemplateItem> OneTemplate : EachTemplate.Value)
				{				
					TSharedPtr<FTemplateItem> Item = OneTemplate;
					// If this template is flagged as not for creation dont try to create it
					if( Item->ProjectFile.IsEmpty())
						continue;

					FString DesiredProjectFilename;
					if( TestRootFolder.IsEmpty() == true)
					{
						// Same name for all
						DesiredProjectFilename = GameProjectAutomationUtils::GetDesiredProjectFilename();
					}
					else
					{
						// Unique names
						FString Hardware;
						if (InTargetedHardware == EHardwareClass::Desktop)
						{
							Hardware = TEXT("Dsk");
						}
						else
						{
							Hardware = TEXT("Mob");
						}
						FString ProjectName = FPaths::GetCleanFilename(Item->ProjectFile).Replace(TEXT("TP_"), TEXT(""));
						FString ProjectDirName = FPaths::GetBaseFilename(Item->ProjectFile).Replace(TEXT("TP_"), TEXT(""));
						FString BasePath = FPaths::RootDir();
						DesiredProjectFilename = FString::Printf(TEXT("%s/%s/%s%s/%s%s"), *BasePath, *TestRootFolder, *Hardware, *ProjectDirName, *Hardware, *ProjectName);
					}
					
					// Setup creation parameters
					FText FailReason, FailLog;
					FProjectInformation ProjectInfo(DesiredProjectFilename, Item->bGenerateCode, bInCopyStarterContent, Item->ProjectFile);
					ProjectInfo.TargetedHardware = InTargetedHardware;
					ProjectInfo.DefaultGraphicsPerformance = InGraphicPreset;
					TArray<FString> CreatedFiles;
					OutMatchedProjects++;

					// Finally try to create the project					
					if (!GameProjectUtils::CreateProject(ProjectInfo, FailReason, FailLog, &CreatedFiles))
					{
						// Failed, report the reason
						UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed! Failed to create %s project %s based on %s. Reason:%s"), *SourceCategoryEnum->GetEnumName((int32)InCategory), *DesiredProjectFilename, *Item->Name.ToString(), *FailReason.ToString());
					}
					else
					{
						// Created ok
						OutCreatedProjects++;
						
						// Now remove the files we just created (if required)
						if(bRemoveCreatedProjects == true)						
						{
							FString RootFolder = FPaths::GetPath(DesiredProjectFilename);
							GameProjectUtils::DeleteCreatedFiles(RootFolder, CreatedFiles);
						}
					}
				}
			}
		}
		return;
	}
}

/**
* Automation test to clean up old test project files
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectCleanupTest, "System.Promotion.Project Promotion Pass.Step 1 Blank Project Creation.Cleanup Potential Project Location", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectCreateTest, "System.Promotion.Project Promotion Pass.Step 1 Blank Project Creation.Create Project", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectMapTest, "System.Promotion.Project Promotion Pass.Step 2 Basic Level Creation.Create Basic Level", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
bool FBuildPromotionNewProjectMapTest::RunTest(const FString& Parameters)
{
	//New level
	UWorld* CurrentWorld = FAutomationEditorCommonUtils::CreateNewMap();
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

/*
* Template project creation test
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreateBPTemplateProjectAutomationTests, "System.Promotion.Project Promotion Pass.Step 3 NewProjectCreationTests.CreateBlueprintProjects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 
 * Uses the new project wizard to locate all templates available for new blueprint project creation and verifies creation succeeds.
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FCreateBPTemplateProjectAutomationTests::RunTest(const FString& Parameters)
{
	TSharedPtr<SNewProjectWizard> NewProjectWizard;
	NewProjectWizard = SNew(SNewProjectWizard);
	//return ;
	TMap<FName, TArray<TSharedPtr<FTemplateItem>> >& Templates = NewProjectWizard->FindTemplateProjects();//GameProjectAutomationUtils::CreateTemplateList();
	int32 OutMatchedProjectsDesk = 0;
	int32 OutCreatedProjectsDesk = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Desktop, EGraphicsPreset::Maximum, EContentSourceCategory::BlueprintFeature, false, OutMatchedProjectsDesk, OutCreatedProjectsDesk);

	int32 OutMatchedProjectsMob = 0;
	int32 OutCreatedProjectsMob = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Mobile, EGraphicsPreset::Maximum, EContentSourceCategory::BlueprintFeature, false, OutMatchedProjectsMob, OutCreatedProjectsMob);
	
	return ( OutMatchedProjectsDesk == OutCreatedProjectsDesk ) && ( OutMatchedProjectsMob == OutCreatedProjectsMob  );
}

/*
* Template project creation test
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreateCPPTemplateProjectAutomationTests, "System.Promotion.Project Promotion Pass.Step 3 NewProjectCreationTests.CreateCodeProjects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 
 * Uses the new project wizard to locate all templates available for new code project creation and verifies creation succeeds.
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FCreateCPPTemplateProjectAutomationTests::RunTest(const FString& Parameters)
{
	TSharedPtr<SNewProjectWizard> NewProjectWizard;
	NewProjectWizard = SNew(SNewProjectWizard);
	//return ;
	TMap<FName, TArray<TSharedPtr<FTemplateItem>> >& Templates = NewProjectWizard->FindTemplateProjects();//GameProjectAutomationUtils::CreateTemplateList();
	int32 OutMatchedProjectsDesk = 0;
	int32 OutCreatedProjectsDesk = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Desktop, EGraphicsPreset::Maximum, EContentSourceCategory::CodeFeature, false, OutMatchedProjectsDesk, OutCreatedProjectsDesk);

	int32 OutMatchedProjectsMob = 0;
	int32 OutCreatedProjectsMob = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Mobile, EGraphicsPreset::Maximum, EContentSourceCategory::CodeFeature, false, OutMatchedProjectsMob, OutCreatedProjectsMob);

	return (OutMatchedProjectsDesk == OutCreatedProjectsDesk) && (OutMatchedProjectsMob == OutCreatedProjectsMob);

}

#endif //WITH_DEV_AUTOMATION_TESTS