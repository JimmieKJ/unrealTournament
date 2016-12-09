// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "UObject/Object.h"
#include "UObject/GarbageCollection.h"

#include "Tests/AutomationTestSettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "ShaderCompiler.h"
#include "Toolkits/AssetEditorManager.h"

#define ASSET_OPEN_INTERVAL 1.5f

bool FOpenActualAssetEditors(const FString& Parameters)
{
	//start with all editors closed
	FAssetEditorManager::Get().CloseAllAssetEditors();

	// below is all latent action, so before sending there, verify the asset exists
	UObject* Object = StaticLoadObject(UObject::StaticClass(), NULL, *Parameters);
	if (!Object)
	{
		UE_LOG(LogEditorAutomationTests, Error, TEXT("Failed to find object: %s."), *Parameters);
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FOpenEditorForAssetCommand(*Parameters));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCloseAllAssetEditorsCommand());

	return true;
}

//////////////////////////////////////////////////////////////////////////
/**
* Test to open the sub editor windows for a specified list of assets.
* This list can be setup in the Editor Preferences window within the editor or the DefaultEngine.ini file for that particular project.
*/
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FOpenAssetEditors, "System.QA.Open Asset Editors", EAutomationTestFlags::Disabled | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter);

void FOpenAssetEditors::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);

	bool bUnAttended = FApp::IsUnattended();

	TArray<FString> AssetNames;
	for (auto Iter = AutomationTestSettings->TestAssetsToOpen.CreateConstIterator(); Iter; ++Iter)
	{
		if (Iter->bSkipTestWhenUnAttended && bUnAttended)
		{
			continue;
		}

		if (Iter->AssetToOpen.FilePath.Len() > 0)
		{
			AssetNames.AddUnique(Iter->AssetToOpen.FilePath);
		}
	}

	//Location of the engine folder
	FString EngineContentFolderLocation = FPaths::ConvertRelativePathToFull(*FPaths::EngineContentDir());
	//Put the Engine Content Folder Location into an array.  
	TArray<FString> EngineContentFolderLocationArray;
	EngineContentFolderLocation.ParseIntoArray(EngineContentFolderLocationArray, TEXT("/"), true);

	for (int32 i = 0; i < AssetNames.Num(); ++i)
	{
		//true means that the life is located in the Engine/Content folder.
		bool bFileIsLocatedInTheEngineContentFolder = true;

		//Get the location of the asset that is being opened.
		FString Filename = FPaths::ConvertRelativePathToFull(AssetNames[i]);

		//Put File location into an array.  
		TArray<FString> FilenameArray;
		Filename.ParseIntoArray(FilenameArray, TEXT("/"), true);

		//Loop through the location array's and compare each element.  
		//The loop runs while the index is less than the number of elements in the EngineContentFolderLocation array.
		//If the elements are the same when the counter is up then it is assumed that the asset file is in the engine content folder. 
		//Otherwise we'll assume it's in the game content folder.
		for (int32 index = 0; index < EngineContentFolderLocationArray.Num(); index++)
		{
			if ((FilenameArray[index] != EngineContentFolderLocationArray[index]))
			{
				//If true it will proceed to add the asset to the Open Asset Editor test list.
				//This will be false if the asset is on a different drive.
				if (FPaths::MakePathRelativeTo(Filename, *FPaths::GameContentDir()))
				{
					FString ShortName = FPaths::GetBaseFilename(Filename);
					FString PathName = FPaths::GetPath(Filename);
					OutBeautifiedNames.Add(ShortName);
					FString AssetName = FString::Printf(TEXT("/Game/%s/%s.%s"), *PathName, *ShortName, *ShortName);
					OutTestCommands.Add(AssetName);
					bFileIsLocatedInTheEngineContentFolder = false;
					break;
				}
				else
				{
					UE_LOG(LogEditorAutomationTests, Error, TEXT("Invalid asset path: %s."), *Filename);
					bFileIsLocatedInTheEngineContentFolder = false;
					break;
				}
			}
		}
		//If true then the asset is in the Engine/Content folder and not in the Game/Content folder.
		if (bFileIsLocatedInTheEngineContentFolder)
		{
			//If true it will proceed to add the asset to the Open Asset Editor test list.
			//This will be false if the asset is on a different drive.
			if (FPaths::MakePathRelativeTo(Filename, *FPaths::EngineContentDir()))
			{
				FString ShortName = FPaths::GetBaseFilename(Filename);
				FString PathName = FPaths::GetPath(Filename);
				OutBeautifiedNames.Add(ShortName);
				FString AssetName = FString::Printf(TEXT("/Engine/%s/%s.%s"), *PathName, *ShortName, *ShortName);
				OutTestCommands.Add(AssetName);
			}
			else
			{
				UE_LOG(LogEditorAutomationTests, Error, TEXT("Invalid asset path: %s."), *Filename);
			}
		}
	}
}

bool FOpenAssetEditors::RunTest(const FString& Parameters)
{
	bool bDidTheTestPass = FOpenActualAssetEditors(Parameters);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return bDidTheTestPass;
}


struct OpenAssetParameters
{
	TArray<FString> AssetPathList;
	int32 AssetListLength;
	int64 TimeSpent;

	OpenAssetParameters()
		: AssetListLength(0)
		, TimeSpent(0)
	{
	}
};

//////////////////////////////////////////////////////////////////////////
/**
* Latent commands for Open Assets Type tests
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOpenAllofAssetTypeCommand, OpenAssetParameters, AssetList);

bool FOpenAllofAssetTypeCommand::Update()
{
	if ( ( FPlatformTime::Seconds() - AssetList.TimeSpent ) > ASSET_OPEN_INTERVAL )
	{
		FAssetEditorManager::Get().CloseAllAssetEditors();

		//Get all assets currently being tracked with open editors and make sure they are not opened.
		if ( FAssetEditorManager::Get().GetAllEditedAssets().Num() > 0 )
		{
			UE_LOG(LogEditorAutomationTests, Warning, TEXT("Not all of the editors were closed."));
		}

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		UE_LOG(LogEditorAutomationTests, Log, TEXT("Remaining assets to open: %i"), AssetList.AssetPathList.Num() - AssetList.AssetListLength);

		if ( AssetList.AssetListLength < AssetList.AssetPathList.Num() )
		{
			UObject* Object = StaticLoadObject(UObject::StaticClass(), NULL, *AssetList.AssetPathList[AssetList.AssetListLength]);
			if ( Object )
			{
				FAssetEditorManager::Get().OpenEditorForAsset(Object);
				//This checks to see if the asset sub editor is loaded.
				if ( FAssetEditorManager::Get().FindEditorForAsset(Object, true) != NULL )
				{
					UE_LOG(LogEditorAutomationTests, Log, TEXT("Verified asset editor for: %s."), *AssetList.AssetPathList[AssetList.AssetListLength]);
					UE_LOG(LogEditorAutomationTests, Display, TEXT("The editor successfully loaded for: %s."), *AssetList.AssetPathList[AssetList.AssetListLength]);
					if ( GShaderCompilingManager->IsCompiling() )
					{
						UE_LOG(LogEditorAutomationTests, Log, TEXT("Waiting for %i shaders to finish."), GShaderCompilingManager->GetNumRemainingJobs());
						GShaderCompilingManager->FinishAllCompilation();
						UE_LOG(LogEditorAutomationTests, Log, TEXT("Done waiting for shaders to finish."));
					}
				}
			}
			else
			{
				UE_LOG(LogEditorAutomationTests, Error, TEXT("Failed to find object: %s."), *AssetList.AssetPathList[AssetList.AssetListLength]);
			}
			AssetList.AssetListLength++;
			AssetList.TimeSpent = FPlatformTime::Seconds();
			return false;
		}
		else
		{
			return true;
		}
	}

	return false;
}

void FOpenAllAssetTests(OpenAssetParameters& AssetList)
{
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FOpenAllofAssetTypeCommand(AssetList));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCloseAllAssetEditorsCommand());
}


//////////////////////////////////////////////////////////////////////////
/**
* This is a complex test that opens every asset into it's editor.
*/
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FOpenMiscAssetEditors, "Project.Tools.Open Assets", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::RequiresUser));

void FOpenMiscAssetEditors::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	//This grabs each TextureRenderTarget2D asset in the Game/Content directory
	FAutomationEditorCommonUtils::CollectGameContentTests(OutBeautifiedNames, OutTestCommands);
}

bool FOpenMiscAssetEditors::RunTest(const FString& Parameters)
{
	bool bDidTheTestPass = FOpenActualAssetEditors(Parameters);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return bDidTheTestPass;
}
