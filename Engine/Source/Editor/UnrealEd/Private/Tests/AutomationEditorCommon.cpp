// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AutomationEditorCommon.h"
#include "AssetEditorManager.h"
#include "ModuleManager.h"
#include "LevelEditor.h"
#include "Editor/MainFrame/Public/MainFrame.h"
#include "EngineVersion.h"
#include "ShaderCompiler.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Engine/DestructibleMesh.h"
#include "FileManagerGeneric.h"
#include "GameFramework/WorldSettings.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Engine/Blueprint.h"
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#include "Engine/Font.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "Particles/ParticleSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Sound/ReverbEffect.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Slate/SlateBrushAsset.h"
#include "SlateWidgetStyleAsset.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Engine/StaticMesh.h"
#include "Engine/SubsurfaceProfile.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/World.h"
#include "AssetSelection.h"

#include "Interfaces/ILauncherDeviceGroup.h"
#include "LauncherServices.h"
#include "TargetDeviceServices.h"
#include "Editor/EditorEngine.h"
#include "PlatformInfo.h"

#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Interfaces/ILauncherProfile.h"
#include "LightingBuildOptions.h"
#include "Engine/StaticMeshActor.h"

#define COOK_TIMEOUT 3600

DEFINE_LOG_CATEGORY_STATIC(LogAutomationEditorCommon, Log, All);

namespace AutomationEditorCommonUtils
{
	/**
	* Converts a package path to an asset path
	*
	* @param PackagePath - The package path to convert
	*/
	FString ConvertPackagePathToAssetPath(const FString& PackagePath)
	{
		const FString Filename = FPaths::ConvertRelativePathToFull(PackagePath);
		FString EngineFileName = Filename;
		FString GameFileName = Filename;
		if (FPaths::MakePathRelativeTo(EngineFileName, *FPaths::EngineContentDir()) && !EngineFileName.Contains(TEXT("../")))
		{
			const FString ShortName = FPaths::GetBaseFilename(EngineFileName);
			const FString PathName = FPaths::GetPath(EngineFileName);
			const FString AssetName = FString::Printf(TEXT("/Engine/%s/%s.%s"), *PathName, *ShortName, *ShortName);
			return AssetName;
		}
		else if (FPaths::MakePathRelativeTo(GameFileName, *FPaths::GameContentDir()) && !GameFileName.Contains(TEXT("../")))
		{
			const FString ShortName = FPaths::GetBaseFilename(GameFileName);
			const FString PathName = FPaths::GetPath(GameFileName);
			const FString AssetName = FString::Printf(TEXT("/Game/%s/%s.%s"), *PathName, *ShortName, *ShortName);
			return AssetName;
		}
		else
		{
			UE_LOG(LogAutomationEditorCommon, Error, TEXT("PackagePath (%s) is invalid for the current project"), *PackagePath);
			return TEXT("");
		}
	}

	/**
	* Imports an object using a given factory
	*
	* @param ImportFactory - The factory to use to import the object
	* @param ObjectName - The name of the object to create
	* @param PackagePath - The full path of the package file to create
	* @param ImportPath - The path to the object to import
	*/
	UObject* ImportAssetUsingFactory(UFactory* ImportFactory, const FString& ObjectName, const FString& PackagePath, const FString& ImportPath)
	{
		UObject* ImportedAsset = NULL;

		UPackage* Pkg = CreatePackage(NULL, *PackagePath);
		if (Pkg)
		{
			// Make sure the destination package is loaded
			Pkg->FullyLoad();

			UClass* ImportAssetType = ImportFactory->ResolveSupportedClass();
			bool bDummy = false;

			//If we are a texture factory suppress some warning dialog that we don't want
			if (ImportFactory->IsA(UTextureFactory::StaticClass()))
			{
				UTextureFactory::SuppressImportOverwriteDialog();
			}

			ImportedAsset = UFactory::StaticImportObject(ImportAssetType, Pkg, FName(*ObjectName), RF_Public | RF_Standalone, bDummy, *ImportPath, NULL, ImportFactory, NULL, GWarn, 0);

			if (ImportedAsset)
			{
				UE_LOG(LogAutomationEditorCommon, Display, TEXT("Imported %s"), *ImportPath);
			}
			else
			{
				UE_LOG(LogAutomationEditorCommon, Error, TEXT("Failed to import asset using factory %s!"), *ImportFactory->GetName());
			}
		}
		else
		{
			UE_LOG(LogAutomationEditorCommon, Error, TEXT("Failed to create a package!"));
		}

		return ImportedAsset;
	}

	/**
	* Nulls out references to a given object
	*
	* @param InObject - Object to null references to
	*/
	void NullReferencesToObject(UObject* InObject)
	{
		TArray<UObject*> ReplaceableObjects;
		TMap<UObject*, UObject*> ReplacementMap;
		ReplacementMap.Add(InObject, NULL);
		ReplacementMap.GenerateKeyArray(ReplaceableObjects);

		// Find all the properties (and their corresponding objects) that refer to any of the objects to be replaced
		TMap< UObject*, TArray<UProperty*> > ReferencingPropertiesMap;
		for (FObjectIterator ObjIter; ObjIter; ++ObjIter)
		{
			UObject* CurObject = *ObjIter;

			// Find the referencers of the objects to be replaced
			FFindReferencersArchive FindRefsArchive(CurObject, ReplaceableObjects);

			// Inform the object referencing any of the objects to be replaced about the properties that are being forcefully
			// changed, and store both the object doing the referencing as well as the properties that were changed in a map (so that
			// we can correctly call PostEditChange later)
			TMap<UObject*, int32> CurNumReferencesMap;
			TMultiMap<UObject*, UProperty*> CurReferencingPropertiesMMap;
			if (FindRefsArchive.GetReferenceCounts(CurNumReferencesMap, CurReferencingPropertiesMMap) > 0)
			{
				TArray<UProperty*> CurReferencedProperties;
				CurReferencingPropertiesMMap.GenerateValueArray(CurReferencedProperties);
				ReferencingPropertiesMap.Add(CurObject, CurReferencedProperties);
				for (TArray<UProperty*>::TConstIterator RefPropIter(CurReferencedProperties); RefPropIter; ++RefPropIter)
				{
					CurObject->PreEditChange(*RefPropIter);
				}
			}

		}

		// Iterate over the map of referencing objects/changed properties, forcefully replacing the references and then
		// alerting the referencing objects the change has completed via PostEditChange
		int32 NumObjsReplaced = 0;
		for (TMap< UObject*, TArray<UProperty*> >::TConstIterator MapIter(ReferencingPropertiesMap); MapIter; ++MapIter)
		{
			++NumObjsReplaced;

			UObject* CurReplaceObj = MapIter.Key();
			const TArray<UProperty*>& RefPropArray = MapIter.Value();

			FArchiveReplaceObjectRef<UObject> ReplaceAr(CurReplaceObj, ReplacementMap, false, true, false);

			for (TArray<UProperty*>::TConstIterator RefPropIter(RefPropArray); RefPropIter; ++RefPropIter)
			{
				FPropertyChangedEvent PropertyEvent(*RefPropIter);
				CurReplaceObj->PostEditChangeProperty(PropertyEvent);
			}

			if (!CurReplaceObj->HasAnyFlags(RF_Transient) && CurReplaceObj->GetOutermost() != GetTransientPackage())
			{
				if (!CurReplaceObj->RootPackageHasAnyFlags(PKG_CompiledIn))
				{
					CurReplaceObj->MarkPackageDirty();
				}
			}
		}
	}

	/**
	* gets a factory class based off an asset file extension
	*
	* @param AssetExtension - The file extension to use to find a supporting UFactory
	*/
	UClass* GetFactoryClassForType(const FString& AssetExtension)
	{
		// First instantiate one factory for each file extension encountered that supports the extension
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			if ((*ClassIt)->IsChildOf(UFactory::StaticClass()) && !((*ClassIt)->HasAnyClassFlags(CLASS_Abstract)))
			{
				UFactory* Factory = Cast<UFactory>((*ClassIt)->GetDefaultObject());
				if (Factory->bEditorImport)
				{
					TArray<FString> FactoryExtensions;
					Factory->GetSupportedFileExtensions(FactoryExtensions);

					// Case insensitive string compare with supported formats of this factory
					if (FactoryExtensions.Contains(AssetExtension))
					{
						return *ClassIt;
					}
				}
			}
		}

		return NULL;
	}

	/**
	* Applies settings to an object by finding UProperties by name and calling ImportText
	*
	* @param InObject - The object to search for matching properties
	* @param PropertyChain - The list UProperty names recursively to search through
	* @param Value - The value to import on the found property
	*/
	void ApplyCustomFactorySetting(UObject* InObject, TArray<FString>& PropertyChain, const FString& Value)
	{
		const FString PropertyName = PropertyChain[0];
		PropertyChain.RemoveAt(0);

		UProperty* TargetProperty = FindField<UProperty>(InObject->GetClass(), *PropertyName);
		if (TargetProperty)
		{
			if (PropertyChain.Num() == 0)
			{
				TargetProperty->ImportText(*Value, TargetProperty->ContainerPtrToValuePtr<uint8>(InObject), 0, InObject);
			}
			else
			{
				UStructProperty* StructProperty = Cast<UStructProperty>(TargetProperty);
				UObjectProperty* ObjectProperty = Cast<UObjectProperty>(TargetProperty);

				UObject* SubObject = NULL;
				bool bValidPropertyType = true;

				if (StructProperty)
				{
					SubObject = StructProperty->Struct;
				}
				else if (ObjectProperty)
				{
					SubObject = ObjectProperty->GetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<UObject>(InObject));
				}
				else
				{
					//Unknown nested object type
					bValidPropertyType = false;
					UE_LOG(LogAutomationEditorCommon, Error, TEXT("ERROR: Unknown nested object type for property: %s"), *PropertyName);
				}

				if (SubObject)
				{
					ApplyCustomFactorySetting(SubObject, PropertyChain, Value);
				}
				else if (bValidPropertyType)
				{
					UE_LOG(LogAutomationEditorCommon, Error, TEXT("Error accessing null property: %s"), *PropertyName);
				}
			}
		}
		else
		{
			UE_LOG(LogAutomationEditorCommon, Error, TEXT("ERROR: Could not find factory property: %s"), *PropertyName);
		}
	}

	/**
	* Applies the custom factory settings
	*
	* @param InFactory - The factory to apply custom settings to
	* @param FactorySettings - An array of custom settings to apply to the factory
	*/
	void ApplyCustomFactorySettings(UFactory* InFactory, const TArray<FImportFactorySettingValues>& FactorySettings)
	{
		bool bCallConfigureProperties = true;

		for (int32 i = 0; i < FactorySettings.Num(); ++i)
		{
			if (FactorySettings[i].SettingName.Len() > 0 && FactorySettings[i].Value.Len() > 0)
			{
				//Check if we are setting an FBX import type override.  If we are, we don't want to call ConfigureProperties because that enables bDetectImportTypeOnImport
				if (FactorySettings[i].SettingName.Contains(TEXT("MeshTypeToImport")))
				{
					bCallConfigureProperties = false;
				}

				TArray<FString> PropertyChain;
				FactorySettings[i].SettingName.ParseIntoArray(PropertyChain, TEXT("."), false);
				ApplyCustomFactorySetting(InFactory, PropertyChain, FactorySettings[i].Value);
			}
		}

		if (bCallConfigureProperties)
		{
			InFactory->ConfigureProperties();
		}
	}

	/**
	* Writes a number to a text file.
	* @param InTestName is the folder that has the same name as the test. (For Example: "Performance").
	* @param InItemBeingTested is the name for the thing that is being tested. (For Example: "MapName").
	* @param InFileName is the name of the file with an extension
	* @param InNumberToBeWritten is the float number that is expected to be written to the file.
	* @param Delimiter is the delimiter to be used. TEXT(",")
	*/
	void WriteToTextFile(const FString& InTestName, const FString& InTestItem, const FString& InFileName, const float& InEntry, const FString& Delimiter)
	{
		//Performance file locations and setups.
		FString FileSaveLocation = FPaths::Combine(*FPaths::AutomationLogDir(), *InTestName, *InTestItem, *InFileName);

		if (FPaths::FileExists(FileSaveLocation))
		{
			//The text files existing content.
			FString TextFileContents;

			//Write to the text file the combined contents from the text file with the number to write.
			FFileHelper::LoadFileToString(TextFileContents, *FileSaveLocation);
			FString FileSetup = TextFileContents + Delimiter + FString::SanitizeFloat(InEntry);
			FFileHelper::SaveStringToFile(FileSetup, *FileSaveLocation);
			return;
		}

		FFileHelper::SaveStringToFile(FString::SanitizeFloat(InEntry), *FileSaveLocation);
	}

	/**
	* Returns the sum of the numbers available in an array of float.
	* @param InFloatArray is the name of the array intended to be used.
	* @param bisAveragedInstead will return the average of the available numbers instead of the sum.
	*/
	float TotalFromFloatArray(const TArray<float>& InFloatArray, bool bIsAveragedInstead)
	{
		//Total Value holds the sum of all the numbers available in the array.
		float TotalValue = 0;

		//Get the sum of the array.
		for (int32 I = 0; I < InFloatArray.Num(); ++I)
		{
			TotalValue += InFloatArray[I];
		}

		//If bAverageInstead equals true then only the average is returned.
		if (bIsAveragedInstead)
		{
			UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Average value of the Array is %f"), (TotalValue / InFloatArray.Num()));
			return (TotalValue / InFloatArray.Num());
		}

		UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Total Value of the Array is %f"), TotalValue);
		return TotalValue;
	}

	/**
	* Returns the largest value from an array of float numbers.
	* @param InFloatArray is the name of the array intended to be used.
	*/
	float LargestValueInFloatArray(const TArray<float>& InFloatArray)
	{
		//Total Value holds the sum of all the numbers available in the array.
		float LargestValue = 0;

		//Find the largest value
		for (int32 I = 0; I < InFloatArray.Num(); ++I)
		{
			if (LargestValue < InFloatArray[I])
			{
				LargestValue = InFloatArray[I];
			}
		}
		UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("The Largest value of the array is %f"), LargestValue);
		return LargestValue;
	}

	/**
	* Returns the contents of a text file as an array of FString.
	* @param InFileLocation -  is the location of the file.
	* @param OutArray - The name of the array where the 
	*/
	void CreateArrayFromFile(const FString& InFileLocation, TArray<FString>& OutArray)
	{
		FString RawData;

		if (FPaths::FileExists(*InFileLocation))
		{
			UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Loading and parsing the data from '%s' into an array."), *InFileLocation);
			FFileHelper::LoadFileToString(RawData, *InFileLocation);
			RawData.ParseIntoArray(OutArray, TEXT(","), false);
		}

		UE_LOG(LogEditorAutomationTests, Warning, TEXT("Unable to create an array.  '%s' does not exist."), *InFileLocation);
		RawData = TEXT("0");
		OutArray.Add(RawData);
	}

	/**
	* Returns true if the archive/file can be written to otherwise false..
	* @param InFilePath - is the location of the file.
	* @param InArchiveName - is the name of the archive to be used.
	*/
	bool IsArchiveWriteable(const FString& InFilePath, const FArchive* InArchiveName)
	{
		if (!InArchiveName)
		{
			UE_LOG(LogEditorAutomationTests, Error, TEXT("Failed to write to the csv file: %s"), *FPaths::ConvertRelativePathToFull(InFilePath));
			return false;
		}
		return true;
	}


	void GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName)
	{
		UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
		check(AutomationTestSettings);

		OutDeviceID = "None";

		FString LaunchOnDeviceId;
		for (auto LaunchIter = AutomationTestSettings->LaunchOnSettings.CreateConstIterator(); LaunchIter; LaunchIter++)
		{
			FString LaunchOnSettings = LaunchIter->DeviceID;
			FString LaunchOnMap = FPaths::GetBaseFilename(LaunchIter->LaunchOnTestmap.FilePath);
			if (LaunchOnMap.Equals(InMapName))
			{
				// shared devices section
				TSharedPtr<ITargetDeviceServicesModule> TargetDeviceServicesModule = StaticCastSharedPtr<ITargetDeviceServicesModule>(FModuleManager::Get().LoadModule(TEXT("TargetDeviceServices")));
				// for each platform...
				TArray<ITargetDeviceProxyPtr> DeviceProxies;
				TargetDeviceServicesModule->GetDeviceProxyManager()->GetProxies(FName(*LaunchOnSettings), true, DeviceProxies);
				// for each proxy...
				for (auto DeviceProxyIt = DeviceProxies.CreateIterator(); DeviceProxyIt; ++DeviceProxyIt)
				{
					ITargetDeviceProxyPtr DeviceProxy = *DeviceProxyIt;
					if (DeviceProxy->IsConnected())
					{
						OutDeviceID = DeviceProxy->GetTargetDeviceId((FName)*LaunchOnSettings);
						break;
					}
				}
			}
		}
	}

	void GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName, const FString& InDeviceName)
	{
		UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
		check(AutomationTestSettings);

		//Output device name will default to "None".
		OutDeviceID = "None";

		// shared devices section
		TSharedPtr<ITargetDeviceServicesModule> TargetDeviceServicesModule = StaticCastSharedPtr<ITargetDeviceServicesModule>(FModuleManager::Get().LoadModule(TEXT("TargetDeviceServices")));
		// for each platform...
		TArray<ITargetDeviceProxyPtr> DeviceProxies;
		TargetDeviceServicesModule->GetDeviceProxyManager()->GetProxies(FName(*InDeviceName), true, DeviceProxies);
		// for each proxy...
		for (auto DeviceProxyIt = DeviceProxies.CreateIterator(); DeviceProxyIt; ++DeviceProxyIt)
		{
			ITargetDeviceProxyPtr DeviceProxy = *DeviceProxyIt;
			if (DeviceProxy->IsConnected())
			{
				OutDeviceID = DeviceProxy->GetTargetDeviceId((FName)*InDeviceName);
				break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Common Latent commands

//Latent Undo and Redo command
//If bUndo is true then the undo action will occur otherwise a redo will happen.
bool FUndoRedoCommand::Update()
{
	if (bUndo == true)
	{
		//Undo
		GEditor->UndoTransaction();
	}
	else
	{
		//Redo
		GEditor->RedoTransaction();
	}

	return true;
}

/**
* Open editor for a particular asset
*/
bool FOpenEditorForAssetCommand::Update()
{
	UObject* Object = StaticLoadObject(UObject::StaticClass(), NULL, *AssetName);
	if (Object)
	{
		FAssetEditorManager::Get().OpenEditorForAsset(Object);
		//This checks to see if the asset sub editor is loaded.
		if (FAssetEditorManager::Get().FindEditorForAsset(Object, true) != NULL)
		{
			UE_LOG(LogEditorAutomationTests, Log, TEXT("Verified asset editor for: %s."), *AssetName);
			UE_LOG(LogEditorAutomationTests, Display, TEXT("The editor successffully loaded for: %s."), *AssetName);
			return true;
		}
	}
	else
	{
		UE_LOG(LogEditorAutomationTests, Error, TEXT("Failed to find object: %s."), *AssetName);
	}
	return true;
}

/**
* Close all sub-editors
*/
bool FCloseAllAssetEditorsCommand::Update()
{
	FAssetEditorManager::Get().CloseAllAssetEditors();

	//Get all assets currently being tracked with open editors and make sure they are not still opened.
	if(FAssetEditorManager::Get().GetAllEditedAssets().Num() >= 1)
	{
		UE_LOG(LogEditorAutomationTests, Warning, TEXT("Not all of the editors were closed."));
		return true;
	}

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Verified asset editors were closed"));
	UE_LOG(LogEditorAutomationTests, Display, TEXT("The asset editors closed successffully"));
	return true;
}

/**
* Start PIE session
*/
bool FStartPIECommand::Update()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<class ILevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

	GUnrealEd->RequestPlaySession(false, ActiveLevelViewport, bSimulateInEditor, NULL, NULL, -1, false);
	return true;
}

/**
* End PlayMap session
*/
bool FEndPlayMapCommand::Update()
{
	GUnrealEd->RequestEndPlayMap();
	return true;
}

/**
* This this command loads a map into the editor.
*/
bool FEditorLoadMap::Update()
{
	//Get the base filename for the map that will be used.
	FString ShortMapName = FPaths::GetBaseFilename(MapName);

	//Get the current number of seconds before loading the map.
	double MapLoadStartTime = FPlatformTime::Seconds();

	//Load the map
	FEditorAutomationTestUtilities::LoadMap(MapName);

	//This is the time it took to load the map in the editor.
	double MapLoadTime = FPlatformTime::Seconds() - MapLoadStartTime;

	//Gets the main frame module to get the name of our current level.
	const IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked< IMainFrameModule >("MainFrame");
	FString LoadedMapName = MainFrameModule.GetLoadedLevelName();

	UE_LOG(LogEditorAutomationTests, Log, TEXT("%s has been loaded."), *ShortMapName);

	//Log out to a text file the time it takes to load the map.
	AutomationEditorCommonUtils::WriteToTextFile(TEXT("Performance"), LoadedMapName, TEXT("RAWMapLoadTime.txt"), MapLoadTime, TEXT(","));
	
	UE_LOG(LogEditorAutomationTests, Display, TEXT("%s took %.3f to load."), *LoadedMapName, MapLoadTime);
	
	return true;
}

/**
* This will cause the test to wait for the shaders to finish compiling before moving on.
*/
bool FWaitForShadersToFinishCompiling::Update()
{
	UE_LOG(LogEditorAutomationTests, Log, TEXT("Waiting for %i shaders to finish."), GShaderCompilingManager->GetNumRemainingJobs());
	GShaderCompilingManager->FinishAllCompilation();
	UE_LOG(LogEditorAutomationTests, Log, TEXT("Done waiting for shaders to finish."));
	return true;
}

/**
* Latent command that changes the editor viewport to the first available bookmarked view.
*/
bool FChangeViewportToFirstAvailableBookmarkCommand::Update()
{
	FEditorModeTools EditorModeTools;
	FLevelEditorViewportClient* ViewportClient;
	uint32 ViewportIndex = 0;

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Attempting to change the editor viewports view to the first set bookmark."));

	//Move the perspective viewport view to show the test.
	for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); i++)
	{
		ViewportClient = GEditor->LevelViewportClients[i];

		for (ViewportIndex = 0; ViewportIndex <= AWorldSettings::MAX_BOOKMARK_NUMBER; ViewportIndex++)
		{
			if (EditorModeTools.CheckBookmark(ViewportIndex, ViewportClient))
			{
				UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Changing a viewport view to the set bookmark %i"), ViewportIndex);
				EditorModeTools.JumpToBookmark(ViewportIndex, true, ViewportClient);
				break;
			}
		}
	}
	return true;
}

/**
* Latent command that adds a static mesh to the worlds origin.
*/
bool FAddStaticMeshCommand::Update()
{
	//Gather assets.
	UObject* Cube = (UStaticMesh*)StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/EngineMeshes/Cube.Cube"), NULL, LOAD_None, NULL);
	//Add Cube mesh to the world
	AStaticMeshActor* StaticMesh = Cast<AStaticMeshActor>(FActorFactoryAssetProxy::AddActorForAsset(Cube));
	StaticMesh->TeleportTo(FVector(0.0f, 0.0f, 0.0f), FRotator(0, 0, 0));
	StaticMesh->SetActorRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Static Mesh cube has been added to 0, 0, 0."))

		return true;
}

/**
* Latent command that builds lighting for the current level.
*/
bool FBuildLightingCommand::Update()
{
	//If we are running with -NullRHI then we have to skip this step.
	if (GUsingNullRHI)
	{
		UE_LOG(LogEditorAutomationTests, Warning, TEXT("SKIPPED Build Lighting Step.  You're currently running with -NullRHI."));
		return true;
	}

	if (GUnrealEd->WarnIfLightingBuildIsCurrentlyRunning())
	{
		UE_LOG(LogEditorAutomationTests, Warning, TEXT("Lighting is already being built."));
		return true;
	}

	UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
	GUnrealEd->Exec(CurrentWorld, TEXT("MAP REBUILD"));

	FLightingBuildOptions LightingBuildOptions;

	// Retrieve settings from ini.
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelected"), LightingBuildOptions.bOnlyBuildSelected, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildCurrentLevel"), LightingBuildOptions.bOnlyBuildCurrentLevel, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelectedLevels"), LightingBuildOptions.bOnlyBuildSelectedLevels, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildVisibility"), LightingBuildOptions.bOnlyBuildVisibility, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("UseErrorColoring"), LightingBuildOptions.bUseErrorColoring, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("ShowLightingBuildInfo"), LightingBuildOptions.bShowLightingBuildInfo, GEditorPerProjectIni);
	int32 QualityLevel;
	GConfig->GetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), QualityLevel, GEditorPerProjectIni);
	QualityLevel = FMath::Clamp<int32>(QualityLevel, Quality_Preview, Quality_Production);
	LightingBuildOptions.QualityLevel = Quality_Production;

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Building lighting in Production Quality."));
	GUnrealEd->BuildLighting(LightingBuildOptions);

	return true;
}


bool FSaveLevelCommand::Update()
{
	if (!GUnrealEd->IsLightingBuildCurrentlyExporting() && !GUnrealEd->IsLightingBuildCurrentlyRunning())
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		ULevel* Level = World->GetCurrentLevel();
		MapName += TEXT("_Copy.umap");
		FString TempMapLocation = FPaths::Combine(*FPaths::GameContentDir(), TEXT("Maps"), TEXT("Automation_TEMP"), *MapName);
		FEditorFileUtils::SaveLevel(Level, TempMapLocation);

		return true;
	}
	return false;
}

bool FLaunchOnCommand::Update()
{
	GUnrealEd->AutomationPlayUsingLauncher(InLauncherDeviceID);
	return true;
}

bool FWaitToFinishCookByTheBookCommand::Update()
{
	if (!GUnrealEd->CookServer->IsCookByTheBookRunning())
	{
		if (GUnrealEd->IsCookByTheBookInEditorFinished())
		{
			UE_LOG(LogEditorAutomationTests, Log, TEXT("The cook by the book operation has finished."));
		}
		return true;
	}
	else if ((FPlatformTime::Seconds() - StartTime) == COOK_TIMEOUT)
	{
		GUnrealEd->CancelCookByTheBookInEditor();
		UE_LOG(LogEditorAutomationTests, Error, TEXT("It has been an hour or more since the cook has started."));
		return false;
	}
	return false;
}

bool FDeleteDirCommand::Update()
{
	FString FullFolderPath = FPaths::ConvertRelativePathToFull(*InFolderLocation);
	if (IFileManager::Get().DirectoryExists(*FullFolderPath))
	{
		IFileManager::Get().DeleteDirectory(*FullFolderPath, false, true);
	}
	return true;
}

bool FWaitToFinishBuildDeployCommand::Update()
{
	if (GEditor->LauncherWorker->GetStatus() == ELauncherWorkerStatus::Completed)
	{
		UE_LOG(LogEditorAutomationTests, Log, TEXT("The build game and deploy operation has finished."));
		return true;
	}
	else if (GEditor->LauncherWorker->GetStatus() == ELauncherWorkerStatus::Canceled || GEditor->LauncherWorker->GetStatus() == ELauncherWorkerStatus::Canceling)
	{
		UE_LOG(LogEditorAutomationTests, Warning, TEXT("The build was canceled."));
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////
//Find Asset Commands

/**
* Generates a list of assets from the ENGINE and the GAME by a specific type.
* This is to be used by the GetTest() function.
*/
void FEditorAutomationTestUtilities::CollectTestsByClass(UClass * Class, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> ObjectList;
	AssetRegistryModule.Get().GetAssetsByClass(Class->GetFName(), ObjectList);

	for (TObjectIterator<UClass> AllClassesIt; AllClassesIt; ++AllClassesIt)
	{
		UClass* ClassList = *AllClassesIt;
		FName ClassName = ClassList->GetFName();
	}

	for (auto ObjIter = ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
	{
		const FAssetData& Asset = *ObjIter;
		FString Filename = Asset.ObjectPath.ToString();
		//convert to full paths
		Filename = FPackageName::LongPackageNameToFilename(Filename);
		if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
		{
			FString BeautifiedFilename = Asset.AssetName.ToString();
			OutBeautifiedNames.Add(BeautifiedFilename);
			OutTestCommands.Add(Asset.ObjectPath.ToString());
		}
	}
}

/**
* Generates a list of assets from the GAME by a specific type.
* This is to be used by the GetTest() function.
*/
void FEditorAutomationTestUtilities::CollectGameContentTestsByClass(UClass * Class, bool bRecursiveClass, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
{
	//Setting the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	//Variable setups
	TArray<FAssetData> ObjectList;
	FARFilter AssetFilter;

	//Generating the list of assets.
	//This list is being filtered by the game folder and class type.  The results are placed into the ObjectList variable.
	AssetFilter.ClassNames.Add(Class->GetFName());

	//removed path as a filter as it causes two large lists to be sorted.  Filtering on "game" directory on iteration
	//AssetFilter.PackagePaths.Add("/Game");
	AssetFilter.bRecursiveClasses = bRecursiveClass;
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
			if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
			{
				FString BeautifiedFilename = Asset.AssetName.ToString();
				OutBeautifiedNames.Add(BeautifiedFilename);
				OutTestCommands.Add(Asset.ObjectPath.ToString());
			}
		}
	}
}

/**
* Generates a list of misc. assets from the GAME.
* This is to be used by the GetTest() function.
*/
void FEditorAutomationTestUtilities::CollectMiscGameContentTestsByClass(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
{
	//Setting the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> ObjectList;
	FARFilter AssetFilter;

	//This is a list of classes that we don't want to be in the misc category.
	TArray<FName> ExcludeClassesList;
	ExcludeClassesList.Add(UAimOffsetBlendSpace::StaticClass()->GetFName());
	ExcludeClassesList.Add(UAimOffsetBlendSpace1D::StaticClass()->GetFName());
	ExcludeClassesList.Add(UAnimBlueprint::StaticClass()->GetFName());
	ExcludeClassesList.Add(UAnimMontage::StaticClass()->GetFName());
	ExcludeClassesList.Add(UAnimSequence::StaticClass()->GetFName());
	ExcludeClassesList.Add(UBehaviorTree::StaticClass()->GetFName());
	ExcludeClassesList.Add(UBlendSpace::StaticClass()->GetFName());
	ExcludeClassesList.Add(UBlendSpace1D::StaticClass()->GetFName());
	ExcludeClassesList.Add(UBlueprint::StaticClass()->GetFName());
	ExcludeClassesList.Add(UDestructibleMesh::StaticClass()->GetFName());
	ExcludeClassesList.Add(UDialogueVoice::StaticClass()->GetFName());
	ExcludeClassesList.Add(UDialogueWave::StaticClass()->GetFName());
	ExcludeClassesList.Add(UFont::StaticClass()->GetFName());
	ExcludeClassesList.Add(UMaterial::StaticClass()->GetFName());
	ExcludeClassesList.Add(UMaterialFunction::StaticClass()->GetFName());
	ExcludeClassesList.Add(UMaterialInstanceConstant::StaticClass()->GetFName());
	ExcludeClassesList.Add(UMaterialParameterCollection::StaticClass()->GetFName());
	ExcludeClassesList.Add(UParticleSystem::StaticClass()->GetFName());
	ExcludeClassesList.Add(UPhysicalMaterial::StaticClass()->GetFName());
	ExcludeClassesList.Add(UPhysicsAsset::StaticClass()->GetFName());
	ExcludeClassesList.Add(UReverbEffect::StaticClass()->GetFName());
	ExcludeClassesList.Add(USkeletalMesh::StaticClass()->GetFName());
	ExcludeClassesList.Add(USkeleton::StaticClass()->GetFName());
	ExcludeClassesList.Add(USlateBrushAsset::StaticClass()->GetFName());
	ExcludeClassesList.Add(USlateWidgetStyleAsset::StaticClass()->GetFName());
	ExcludeClassesList.Add(USoundAttenuation::StaticClass()->GetFName());
	ExcludeClassesList.Add(USoundClass::StaticClass()->GetFName());
	ExcludeClassesList.Add(USoundCue::StaticClass()->GetFName());
	ExcludeClassesList.Add(USoundMix::StaticClass()->GetFName());
	ExcludeClassesList.Add(USoundWave::StaticClass()->GetFName());
	ExcludeClassesList.Add(UStaticMesh::StaticClass()->GetFName());
	ExcludeClassesList.Add(USubsurfaceProfile::StaticClass()->GetFName());
	ExcludeClassesList.Add(UTexture::StaticClass()->GetFName());
	ExcludeClassesList.Add(UTexture2D::StaticClass()->GetFName());
	ExcludeClassesList.Add(UTextureCube::StaticClass()->GetFName());
	ExcludeClassesList.Add(UTextureRenderTarget::StaticClass()->GetFName());
	ExcludeClassesList.Add(UTextureRenderTarget2D::StaticClass()->GetFName());
	ExcludeClassesList.Add(UUserDefinedEnum::StaticClass()->GetFName());
	ExcludeClassesList.Add(UWorld::StaticClass()->GetFName());

	//Generating the list of assets.
	//This list is being filtered by the game folder and class type.  The results are placed into the ObjectList variable.
	AssetFilter.PackagePaths.Add("/Game");
	AssetFilter.bRecursivePaths = true;
	AssetRegistryModule.Get().GetAssets(AssetFilter, ObjectList);

	//Loop through the list of assets, make their path full and a string, then add them to the test.
	for (auto ObjIter = ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
	{
		const FAssetData& Asset = *ObjIter;
		//First we check if the class is valid.  If not then we move onto the next object.
		if (Asset.GetClass() != NULL)
		{
			//Variable that holds the class FName for the current asset iteration.
			FName AssetClassFName = Asset.GetClass()->GetFName();

			//Counter used to keep track for the following for loop.
			float ExcludedClassesCounter = 1;

			for (auto ExcludeIter = ExcludeClassesList.CreateConstIterator(); ExcludeIter; ++ExcludeIter)
			{
				FName ExludedName = *ExcludeIter;

				//If the classes are the same then we don't want this asset. So we move onto the next one instead.
				if (AssetClassFName == ExludedName)
				{
					break;
				}

				//We run out of class names in our Excluded list then we want the current ObjectList asset.
				if ((ExcludedClassesCounter + 1) > ExcludeClassesList.Num())
				{
					FString Filename = Asset.ObjectPath.ToString();
					//convert to full paths
					Filename = FPackageName::LongPackageNameToFilename(Filename);

					if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
					{
						FString BeautifiedFilename = Asset.AssetName.ToString();
						OutBeautifiedNames.Add(BeautifiedFilename);
						OutTestCommands.Add(Asset.ObjectPath.ToString());
					}

					break;
				}
				ExcludedClassesCounter++;
			}
		}
	}
}

/**
* Generates a list of assets from the GAME by a specific type.
*/
void FEditorAutomationTestUtilities::CollectGameContentByClass(const UClass * Class, bool bRecursiveClass, TArray<FString>& OutAssetList)
{
	//Setting the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	//Variable setups
	TArray<FAssetData> ObjectList;
	FARFilter AssetFilter;

	//Generating the list of assets.
	//This list is being filtered by the game folder and class type.  The results are placed into the ObjectList variable.
	AssetFilter.ClassNames.Add(Class->GetFName());
	AssetFilter.PackagePaths.Add("/Game");
	AssetFilter.bRecursiveClasses = bRecursiveClass;
	AssetFilter.bRecursivePaths = true;
	AssetRegistryModule.Get().GetAssets(AssetFilter, ObjectList);

	//Loop through the list of assets, make their path full and a string, then add them to the test.
	for (auto ObjIter = ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
	{
		const FAssetData& Asset = *ObjIter;
		FString Filename = Asset.ObjectPath.ToString();
		//convert to full paths
		Filename = FPackageName::LongPackageNameToFilename(Filename);
		if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
		{
			OutAssetList.Add(Asset.ObjectPath.ToString());
		}
	}
}
