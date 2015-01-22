// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Tests/AutomationTestSettings.h"
#include "AutomationEditorCommon.h"
#include "AutomationTest.h"
#include "AssertionMacros.h"
#include "AutomationCommon.h"
#include "AssetEditorManager.h"
#include "ShaderCompiler.h"

//Includes needed for opening certain assets
#include "Materials/MaterialFunction.h"
#include "Slate/SlateBrushAsset.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Engine/DestructibleMesh.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/BlendSpace1D.h"
#include "Engine/UserDefinedEnum.h"
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Engine/SubsurfaceProfile.h"
#include "Engine/Font.h"

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
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FOpenAssetEditors, "QA.Open Asset Editors", EAutomationTestFlags::ATF_Editor);

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
	EngineContentFolderLocation.ParseIntoArray(&EngineContentFolderLocationArray, TEXT("/"), true);

	for (int32 i = 0; i < AssetNames.Num(); ++i)
	{
		//true means that the life is located in the Engine/Content folder.
		bool bFileIsLocatedInTheEngineContentFolder = true;

		//Get the location of the asset that is being opened.
		FString Filename = FPaths::ConvertRelativePathToFull(AssetNames[i]);

		//Put File location into an array.  
		TArray<FString> FilenameArray;
		Filename.ParseIntoArray(&FilenameArray, TEXT("/"), true);

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
	if ((FPlatformTime::Seconds() - AssetList.TimeSpent) > ASSET_OPEN_INTERVAL)
	{
		FAssetEditorManager::Get().CloseAllAssetEditors();

		//Get all assets currently being tracked with open editors and make sure they are not opened.
		if (FAssetEditorManager::Get().GetAllEditedAssets().Num() > 0)
		{
			UE_LOG(LogEditorAutomationTests, Warning, TEXT("Not all of the editors were closed."));
		}

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		UE_LOG(LogEditorAutomationTests, Log, TEXT("Remaining assets to open: %i"), AssetList.AssetPathList.Num() - AssetList.AssetListLength);

		if (AssetList.AssetListLength < AssetList.AssetPathList.Num())
		{
			UObject* Object = StaticLoadObject(UObject::StaticClass(), NULL, *AssetList.AssetPathList[AssetList.AssetListLength]);
			if (Object)
			{
				FAssetEditorManager::Get().OpenEditorForAsset(Object);
				//This checks to see if the asset sub editor is loaded.
				if (FAssetEditorManager::Get().FindEditorForAsset(Object, true) != NULL)
				{
					UE_LOG(LogEditorAutomationTests, Log, TEXT("Verified asset editor for: %s."), *AssetList.AssetPathList[AssetList.AssetListLength]);
					UE_LOG(LogEditorAutomationTests, Display, TEXT("The editor successffully loaded for: %s."), *AssetList.AssetPathList[AssetList.AssetListLength]);
					if (GShaderCompilingManager->IsCompiling())
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
* This test opens each AIM OFFSET BLENDSPACE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenAimOffsetBlendspaceAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All Aim Offset Blendspace Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenAimOffsetBlendspaceAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UAimOffsetBlendSpace::StaticClass(), true, AssetList.AssetPathList);
	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each AIM OFFSET BLENDSPACE 1D into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenAimOffSetBlendspace1DAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All Aim Offset Blendspace 1D Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenAimOffSetBlendspace1DAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UAimOffsetBlendSpace1D::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each ANIMMONTAGE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenAnimMontageAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All Anim Montage Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenAnimMontageAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UAnimMontage::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each ANIMSEQUENCE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenAnimSequenceAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All Anim Sequence Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenAnimSequenceAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UAnimSequence::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each BEHAVIORTREE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenBehaviorTreeAssetEditorsSLOW, "Tools.AI.Open All Behaviour Tree Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenBehaviorTreeAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UBehaviorTree::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each BLENDSPACE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenBlendSpaceAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All BlendSpace Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenBlendSpaceAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UBlendSpace::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each BLENDSPACE 1D into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenBlendSpace1DAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All BlendSpace 1D Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenBlendSpace1DAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UBlendSpace::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each BLUEPRINT into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenBlueprintAssetEditorsSLOW, "Tools.Blueprint.Open All Blueprint Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenBlueprintAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UBlueprint::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each DESTRUCTIBLE MESH into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenDestructibleMeshAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All Destructible Mesh Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenDestructibleMeshAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UDestructibleMesh::StaticClass(), false, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each DIALOGUE VOICE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenDialogueVoiceAssetEditorsSLOW, "Tools.Sounds.Open All Dialogue Voice Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenDialogueVoiceAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UDialogueVoice::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each DIALOGUE WAVE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenDialogueWaveAssetEditorsSLOW, "Tools.Sounds.Open All Dialogue Wave Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenDialogueWaveAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UDialogueWave::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each MATERIAL into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenMaterialAssetEditorsSLOW, "Tools.Materials.Open All Material Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenMaterialAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UMaterial::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each MATERIAL FUNCTION into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenMaterialFunctionAssetEditorsSLOW, "Tools.Materials.Open All Material Function Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenMaterialFunctionAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UMaterialFunction::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each MATERIAL INSTANCE CONSTANT into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenMICAssetEditorsSLOW, "Tools.Materials.Open All Material Instance Constant Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenMICAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UMaterialInstanceConstant::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each MATERIAL PARAMETER COLLECTION into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenMaterialParameterCollectionAssetEditorsSLOW, "Tools.Materials.Open All Material Parameter Collection Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenMaterialParameterCollectionAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UMaterialParameterCollection::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each PARTICLE SYSTEM into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenParticleSystemAssetEditorsSLOW, "Tools.Particle System.Open All Particle System Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenParticleSystemAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UParticleSystem::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each PHYSICAL MATERIAL into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenPhysicalMaterialAssetEditorsSLOW, "Tools.Physics.Open All Physical Material Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenPhysicalMaterialAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UPhysicalMaterial::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each PHYSICS ASSET into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenPhysicAssetEditorsSLOW, "Tools.Physics.Open All Physic Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenPhysicAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UPhysicsAsset::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each REVERB EFFECT into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenReverbEffectAssetEditorsSLOW, "Tools.Sounds.Open All Reverb Effect Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenReverbEffectAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UReverbEffect::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SLATE WIDGET STYLE ASSET into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSlateWidgetStyleAssetEditorsSLOW, "Tools.Slate.Open All Slate Widget Style Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSlateWidgetStyleAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USlateWidgetStyleAsset::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SLATE BRUSH ASSET into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSlateBrushAssetEditorsSLOW, "Tools.Slate.Open All Slate Brush Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSlateBrushAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USlateBrushAsset::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SOUNDATTENUATION into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSoundAttenuationAssetEditorsSLOW, "Tools.Sounds.Open All Sound Attenuation Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSoundAttenuationAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USoundAttenuation::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each STATIC MESH into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenStaticMeshAssetEditorsSLOW, "Tools.Static Mesh.Open All Static Mesh Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenStaticMeshAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UStaticMesh::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SKELETAL MESH into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSkeletalMeshAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All Skeletal Mesh Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSkeletalMeshAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USkeletalMesh::StaticClass(), false, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SKELETON into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSkeletonAssetEditorsSLOW, "Tools.Skeletal Mesh.Open All Skeleton Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSkeletonAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USkeleton::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SOUND CLASS into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSoundClassAssetEditorsSLOW, "Tools.Sounds.Open All Sound Class Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSoundClassAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USoundClass::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SOUND CUE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSoundCueAssetEditorsSLOW, "Tools.Sounds.Open All Sound Cue Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSoundCueAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USoundCue::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SOUND MIX into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSoundMixAssetEditorsSLOW, "Tools.Sounds.Open All Sound Mix Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSoundMixAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USoundMix::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SOUND WAVE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSoundWaveAssetEditorsSLOW, "Tools.Sounds.Open All Sound Wave Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSoundWaveAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USoundWave::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each SUBSURFACEPROFILE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenSubsurfaceProfileAssetEditorsSLOW, "Tools.Materials.Open All Subsurface Profile Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenSubsurfaceProfileAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(USubsurfaceProfile::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each TEXTURE into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenTextureAssetEditorsSLOW, "Tools.Textures.Open All Texture Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenTextureAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UTexture::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each USER DEFINED ENUM into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenUserDefinedEnumAssetEditorsSLOW, "Tools.Blueprint.Open All User Defined Enum Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenUserDefinedEnumAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UUserDefinedEnum::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

/**
* This test opens each FONT into its sub-editor.
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFontAssetEditorsSLOW, "Tools.Textures.Open All Font Assets (SLOW)", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));

bool FOpenFontAssetEditorsSLOW::RunTest(const FString& Parameters)
{
	OpenAssetParameters AssetList;
	AssetList.TimeSpent = FPlatformTime::Seconds();

	FEditorAutomationTestUtilities::CollectGameContentByClass(UFont::StaticClass(), true, AssetList.AssetPathList);

	FOpenAllAssetTests(AssetList);

	return true;
}

////////////////////////////////////////////////////////////////////////////
///**
//* This test opens each misc asset into its sub-editor.
//*/
//IMPLEMENT_COMPLEX_AUTOMATION_TEST(FOpenMiscAssetEditors, "Tools.Misc.Open Misc Assets", (EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_RequiresUser));
//
//void FOpenMiscAssetEditors::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
//{
//	//This grabs each TextureRenderTarget2D asset in the Game/Content directory
//	FEditorAutomationTestUtilities::CollectMiscGameContentTestsByClass(OutBeautifiedNames, OutTestCommands);
//}
//
//bool FOpenMiscAssetEditors::RunTest(const FString& Parameters)
//{
//	bool bDidTheTestPass = FOpenActualAssetEditors(Parameters);
//	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
//	return bDidTheTestPass;
//}
//
//
//
