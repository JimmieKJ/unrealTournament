// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintNativeCodeGenManifest.generated.h"

// Forward declarations
struct FNativeCodeGenCommandlineParams;
class  FAssetData;

/*******************************************************************************
 * FCodeGenAssetRecord
 ******************************************************************************/

USTRUCT()
struct FConvertedAssetRecord
{
	GENERATED_USTRUCT_BODY()

public:
	FConvertedAssetRecord() {}
	FConvertedAssetRecord(const FAssetData& AssetInfo, const FString& TargetDir, const FString& ModuleName);

	/**
	 * @return True if this record demonstates a successful conversion; false if it is incomplete 
	 */
	bool IsValid();

public:
	TWeakObjectPtr<UObject> AssetPtr;

	UPROPERTY()
	UClass* AssetType;

	UPROPERTY()
	FString AssetPath;

	UPROPERTY()
	FString GeneratedHeaderPath;

	UPROPERTY()
	FString GeneratedCppPath;
};

/*******************************************************************************
 * FBlueprintNativeCodeGenManifest
 ******************************************************************************/
 
USTRUCT()
struct FBlueprintNativeCodeGenManifest
{
	GENERATED_USTRUCT_BODY()

	/** @return The default filename for manifest files. */
	static FString GetDefaultFilename();

public: 
	FBlueprintNativeCodeGenManifest(const FString TargetPath = TEXT(""));
	FBlueprintNativeCodeGenManifest(const FNativeCodeGenCommandlineParams& CommandlineParams);

	/**
	 * A query that retrieves a list of file/directory paths. Conceptually, 
	 * these files/directories are fully owned by the the conversion process.
	 * 
	 * @return An array of file/directory paths that will be generated the conversion process.
	 */
	TArray<FString> GetDestinationPaths() const;

	/**
	 * Constructs a .uplugin path, specifying the destination for the plugin's
	 * descriptor file.
	 * 
	 * @return The destination filepath for the .uplugin file.
	 */
	FString GetPluginFilePath() const;

	/**
	 * Constructs a .Build.cs path, detailing the destination for the module's 
	 * build file.
	 * 
	 * @return The destination filepath for the .Build.cs file.
	 */
	FString GetModuleFilePath() const;

	/**
	 * Logs an entry detailing the specified asset's conversion (the asset's 
	 * name, the resulting cpp/h files, etc.). Will return an existing entry, 
	 * if one exists for the specified asset.
	 * 
	 * @param  AssetInfo    Defines the asset that you plan on converting (and want to make an entry for).
	 * @return A record detailing the asset's expected conversion.
	 */
	FConvertedAssetRecord& CreateConversionRecord(const FAssetData& AssetInfo);

	/**
	 * Searches the manifest's conversion records for an entry pertaining to the
	 * specified asset.
	 * 
	 * @param  AssetPath    Serves as a lookup key for the asset in question.
	 * @return A pointer to the found conversion record, null if one doesn't exist.
	 */
	FConvertedAssetRecord* FindConversionRecord(const FString& AssetPath, bool bSlowSearch = false);

	/**
	 * Records module dependencies for the specified asset.
	 * 
	 * @param  AssetInfo    The asset you want to collect dependencies for.
	 * @return True if we were able to successfully collect dependencies from the asset; false if the process failed.
	 */
	bool LogDependencies(const FAssetData& AssetInfo);

	/** 
	 * @return A list of all known modules that this plugin will depend on. 
	 */
	const TArray<UPackage*>& GetModuleDependencies() const { return ModuleDependencies; }

	/**
	 * Saves this manifest as json, to its target destination (which it was 
	 * setup with).
	 * 
	 * @param  bSort    Defines if you want the manifest's conversion log sorted before the files is saved (making it easier to read).
	 * @return True if the save was a success, otherwise false.
	 */
	bool Save(bool bSort = true) const;

private:
	/**
	 * @return The destination directory for the plugin and all its related files.
	 */
	FString GetTargetDir() const;

	/**
	 * @return The target name for the plugin's primary module.
	 */
	const FString& GetTargetModuleName() const { return PluginName; }

	/**
	 * Empties the manifest, leaving only the destination directory and file
	 * names intact.
	 */
	void Clear();

	/**
	 * Maps the asset log so that we can easily query for specific conversion
	 * records.
	 */
	void ConstructRecordLookupTable();
	
private:
	/** Defines the destination filepath for this manifest */
	FString ManifestFilePath;

	UPROPERTY()
	FString PluginName;

	/** Relative to the project's directory */
	UPROPERTY()
	FString OutputDir;

	UPROPERTY()
	TArray<UPackage*> ModuleDependencies;

	/** Mutable so Save() can sort the array when requested */
	UPROPERTY()
	mutable TArray<FConvertedAssetRecord> ConvertedAssets;

	/** Serves as a quick way to look up records in the ConvertedAssets array */
	mutable TMap<FString, int32> RecordLookupTable;
};
