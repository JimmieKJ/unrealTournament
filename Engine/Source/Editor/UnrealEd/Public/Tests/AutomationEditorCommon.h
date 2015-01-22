// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetRegistryModule.h"
#include "ModuleManager.h"
#include "AutomationCommon.h"
#include "Tests/AutomationTestSettings.h"

//Includes needed for opening certain assets
#include "Materials/MaterialFunction.h"
#include "Slate/SlateBrushAsset.h"

namespace AutomationEditorCommonUtils
{
	/**
	* Creates a new map for editing.  Also clears editor tools that could cause issues when changing maps
	*
	* @return - The UWorld for the new map
	*/
	static UWorld* CreateNewMap()
	{
		// Change out of Matinee when opening new map, so we avoid editing data in the old one.
		if (GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_InterpEdit))
		{
			GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_InterpEdit);
		}

		// Also change out of Landscape mode to ensure all references are cleared.
		if (GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Landscape))
		{
			GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_Landscape);
		}

		// Also change out of Foliage mode to ensure all references are cleared.
		if (GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Foliage))
		{
			GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_Foliage);
		}

		// Change out of mesh paint mode when opening a new map.
		if (GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_MeshPaint))
		{
			GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_MeshPaint);
		}

		return GEditor->NewMap();
	}

	/**
	* Converts a package path to an asset path
	*
	* @param PackagePath - The package path to convert
	*/
	FString ConvertPackagePathToAssetPath(const FString& PackagePath);

	/**
	* Imports an object using a given factory
	*
	* @param ImportFactory - The factory to use to import the object
	* @param ObjectName - The name of the object to create
	* @param PackagePath - The full path of the package file to create
	* @param ImportPath - The path to the object to import
	*/
	UObject* ImportAssetUsingFactory(UFactory* ImportFactory, const FString& ObjectName, const FString& PackageName, const FString& ImportPath);

	/**
	* Nulls out references to a given object
	*
	* @param InObject - Object to null references to
	*/
	void NullReferencesToObject(UObject* InObject);

	/**
	* gets a factory class based off an asset file extension
	*
	* @param AssetExtension - The file extension to use to find a supporting UFactory
	*/
	UClass* GetFactoryClassForType(const FString& AssetExtension);

	/**
	* Applies settings to an object by finding UProperties by name and calling ImportText
	*
	* @param InObject - The object to search for matching properties
	* @param PropertyChain - The list UProperty names recursively to search through
	* @param Value - The value to import on the found property
	*/
	void ApplyCustomFactorySetting(UObject* InObject, TArray<FString>& PropertyChain, const FString& Value);

	/**
	* Applies the custom factory settings
	*
	* @param InFactory - The factory to apply custom settings to
	* @param FactorySettings - An array of custom settings to apply to the factory
	*/
	void ApplyCustomFactorySettings(UFactory* InFactory, const TArray<FImportFactorySettingValues>& FactorySettings);

	/**
	* Writes a number to a text file.
	*
	* @param InTestName - is the folder that has the same name as the test. (For Example: "Performance").
	* @param InItemBeingTested - is the name for the thing that is being tested. (For Example: "MapName").
	* @param InFileName - is the name of the file with an extension
	* @param InNumberToBeWritten - is the float number that is expected to be written to the file.
	* @param Delimiter - is the delimiter to be used. TEXT(",")
	*/
	void WriteToTextFile(const FString& InTestName, const FString& InTestItem, const FString& InFileName, const float& InEntry, const FString& Delimiter);

	/**
	* Returns the sum of the numbers available in an array of float.

	* @param InFloatArray - is the name of the array intended to be used.
	* @param bisAveragedInstead - will return the average of the available numbers instead of the sum.
	*/
	float TotalFromFloatArray(const TArray<float>& InFloatArray, bool bisAveragedInstead);

	/**
	* Returns the largest value from an array of float numbers.

	* @param InFloatArray - is the name of the array intended to be used.
	*/
	float LargestValueInFloatArray(const TArray<float>& InFloatArray);

	/**
	* Returns the contents of a text file as an array of FString.

	* @param InFileLocation - is the location of the file.
	* @param OutArray - The name of the array that will store the data.
	*/
	void CreateArrayFromFile(const FString& InFileLocation, TArray<FString>& OutArray);

	/**
	* Returns true if the archive/file can be written to otherwise false.

	* @param InFilePath - is the location of the file.
	* @param InArchiveName - is the name of the archive to be used.
	*/
	bool IsArchiveWriteable(const FString& InFilePath, const FArchive* InArchiveName);
		
	/**
	* Returns the first DeviceID 'Platform@Device'
	* Searches the automation preferences for the platform to use.

	* @param OutDeviceID - The variable that will hold the device ID.
	* @param InMapName - The map name to check against in the automation preferences.
	*/
	void GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName);

	/**
	* Returns the DeviceID 'Platform@Device'
	* Searches the automation preferences for the platform to use.

	* @param OutDeviceID - The variable that will hold the device ID.
	* @param InMapName - The map name to check against in the automation preferences.
	* @param InDeviceName - Device Name 
	*/
	void GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName, const FString& InDeviceName);
}


//////////////////////////////////////////////////////////////////////////
//Common latent commands used for automated editor testing.

/**
* Creates a latent command which the user can either undo or redo an action.
* True will trigger Undo, False will trigger a redo
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FUndoRedoCommand, bool, bUndo);

/**
* Open editor for a particular asset
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOpenEditorForAssetCommand, FString, AssetName);

/**
* Close all asset editors
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FCloseAllAssetEditorsCommand);

/**
* Start PIE session
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FStartPIECommand, bool, bSimulateInEditor);

/**
* End PlayMap session
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand);

/**
* Loads a map
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FEditorLoadMap, FString, MapName);

/**
* Waits for shaders to finish compiling before moving on to the next thing.
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling);

/**
* Latent command that changes the editor viewport to the first available bookmarked view.
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FChangeViewportToFirstAvailableBookmarkCommand);

/**
* Latent command that adds a static mesh to the worlds origin.
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FAddStaticMeshCommand);

/**
* Latent command that builds the lighting for the current level.
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FBuildLightingCommand);

/**
* Latent command that saves a copy of the level to a transient folder.
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSaveLevelCommand, FString, MapName);

/**
* Triggers a launch on using a specified device ID.
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FLaunchOnCommand, FString, InLauncherDeviceID);

/**
* Wait for a cook by the book to finish.  Will time out after 3600 seconds (1 hr).
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FWaitToFinishCookByTheBookCommand);

/**
* Wait for a build and deploy to finish before moving on.
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FWaitToFinishBuildDeployCommand);

/**
* Latent command to delete a directory.
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FDeleteDirCommand, FString, InFolderLocation);


//////////////////////////////////////////////////////////////////////////
// FEditorAutomationTestUtilities
class FEditorAutomationTestUtilities
{
public:
	/**
	* Loads the map specified by an automation test
	*
	* @param MapName - Map to load
	*/
	static void LoadMap(const FString& MapName)
	{
		bool bLoadAsTemplate = false;
		bool bShowProgress = false;
		FEditorFileUtils::LoadMap(MapName, bLoadAsTemplate, bShowProgress);
	}

	/**
	* Run PIE
	*/
	static void RunPIE()
	{
		bool bInSimulateInEditor = true;
		//once in the editor
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

		//wait between tests
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

		//once not in the editor
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
	
	/**
	* Generates a list of assets from the ENGINE and the GAME by a specific type.
	* This is to be used by the GetTest() function.
	*/
	static void CollectTestsByClass(UClass * Class, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands);
	
	/**
	* Generates a list of assets from the GAME by a specific type.
	* This is to be used by the GetTest() function.
	*/
	static void CollectGameContentTestsByClass(UClass * Class, bool bRecursiveClass, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands);
		
	/**
	* Generates a list of misc. assets from the GAME.
	* This is to be used by the GetTest() function.
	*/
	static void CollectMiscGameContentTestsByClass(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands);
	
	/**
	* Generates a list of assets from the GAME by a specific type.
	*/
	static void CollectGameContentByClass(const UClass * Class, bool bRecursiveClass, TArray<FString>& OutAssetList);
};


