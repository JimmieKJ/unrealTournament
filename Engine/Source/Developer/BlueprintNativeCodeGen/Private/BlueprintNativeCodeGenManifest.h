// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/StringAssetReference.h"
#include "BlueprintNativeCodeGenManifest.generated.h"

class FAssetData;
struct FBlueprintNativeCodeGenPaths;

/*******************************************************************************
 * FCodeGenAssetRecord
 ******************************************************************************/

USTRUCT()
struct FConvertedAssetRecord
{
	GENERATED_USTRUCT_BODY()

public:
	FConvertedAssetRecord() {}
	FConvertedAssetRecord(const FAssetData& AssetInfo, const FBlueprintNativeCodeGenPaths& TargetPaths);

	/**
	 * @return 
	 */
	FStringAssetReference ToAssetRef() const;

public:
	UPROPERTY()
	UClass* AssetType;

	// cannot use a FStringAssetReference, as the json serializer has problems 
	// with some asset paths (for example, I had a folder named 'Folder()')
	UPROPERTY()
	FString TargetObjPath; 

	UPROPERTY()
	FString GeneratedHeaderPath;

	UPROPERTY()
	FString GeneratedCppPath;
};

/*******************************************************************************
 * FUnconvertedDependencyRecord
 ******************************************************************************/

USTRUCT()
struct FUnconvertedDependencyRecord
{
	GENERATED_USTRUCT_BODY()

public:
	FUnconvertedDependencyRecord() {}
	FUnconvertedDependencyRecord(const FString& InGeneratedWrapperPath);

public:
	UPROPERTY()
	FString GeneratedWrapperPath;
};

/*******************************************************************************
 * FBlueprintNativeCodeGenPaths
 ******************************************************************************/

struct FBlueprintNativeCodeGenPaths
{
public:
	enum ESourceFileType
	{
		HFile, CppFile,
	};

	static FString GetDefaultManifestPath();

private:
	friend struct FBlueprintNativeCodeGenManifest;
	FBlueprintNativeCodeGenPaths(const FString& PluginName, const FString& TargetDir, const FString& ManifestPath);

public:
	/**  */
	FString ManifestFilePath() const;

	/**  */
	FString PluginRootDir() const;
	FString PluginFilePath() const;
	FString PluginSourceDir() const;

	/**  */
	FString RuntimeModuleDir() const;
	FString RuntimeModuleName() const;
	FString RuntimeBuildFile() const;
	FString RuntimeSourceDir(ESourceFileType SourceType) const;
	FString RuntimeModuleFile(ESourceFileType SourceType) const;
	FString RuntimePCHFilename() const;

private:
	FString TargetDir;
	FString PluginName;
	FString ManifestPath;
};

/*******************************************************************************
 * FBlueprintNativeCodeGenManifest
 ******************************************************************************/
 
USTRUCT()
struct FBlueprintNativeCodeGenManifest
{
	GENERATED_USTRUCT_BODY()

	typedef FName FAssetId;
	typedef TMap<FAssetId, FConvertedAssetRecord> FConversionRecord;
	typedef TMap<FAssetId, FUnconvertedDependencyRecord> FUnconvertedRecord;

public: 
	FBlueprintNativeCodeGenManifest(); // default ctor for USTRUCT() system
	FBlueprintNativeCodeGenManifest(const FString& InPluginName, const FString& InOutputDir);
	FBlueprintNativeCodeGenManifest(const FString& ManifestFilePath);

	/**
	 * @return A utility object that you can query for various file/directory paths used/targeted by the conversion process.
	 */
	FBlueprintNativeCodeGenPaths GetTargetPaths() const;

	/**
	 * Logs an entry detailing the specified asset's conversion (the asset's 
	 * name, the resulting cpp/h files, etc.). Will return an existing entry, 
	 * if one exists for the specified asset.
	 * 
	 * @param  AssetInfo    Defines the asset that you plan on converting (and want to make an entry for).
	 * @return A record detailing the asset's expected conversion.
	 */
	FConvertedAssetRecord& CreateConversionRecord(const FAssetId Key, const FAssetData& AssetInfo);

	/**
	 * 
	 * 
	 * @param  UnconvertedAssetKey    
	 * @param  AssetObj    
	 * @param  ReferencingAsset    
	 * @return 
	 */
	FUnconvertedDependencyRecord& CreateUnconvertedDependencyRecord(const FAssetId UnconvertedAssetKey, const FAssetData& AssetInfo);

	/**
	 * Records module dependencies for the specified asset.
	 * 
	 * @param  Package    The asset you want to collect dependencies for.
	 */
	void GatherModuleDependencies(UPackage* Package);

	/** 
	 * @return A list of all known modules that this plugin will depend on. 
	 */
	const TArray<UPackage*>& GetModuleDependencies() const { return ModuleDependencies; }

	/** 
	 * @return A list of all asset conversion that have been recorded. 
	 */
	const FConversionRecord& GetConversionRecord() const { return ConvertedAssets; }

	/**
	* @return A list of all asset conversion that require stubs.
	*/
	const FUnconvertedRecord& GetUnconvertedDependencies() const { return UnconvertedDependencies; }

	/**
	 * Saves this manifest as json, to its target destination (which it was 
	 * setup with).
	 * 
	 * @param Id an identifier used to uniquely identify this manifest, used by multiprocess cook
	 * @return True if the save was a success, otherwise false.
	 */
	bool Save(int32 Id) const;

	/**
	* Merges the OtherManifest into the current manifest
	*/
	void Merge(const FBlueprintNativeCodeGenManifest& OtherManifest);

private:
	/**
	 * @return The destination directory for the plugin and all its related files.
	 */
	FString GetTargetDir() const;

	/**
	 * Empties the manifest, leaving only the destination directory and file
	 * names intact.
	 */
	void Clear();
	
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

	/**  */
	UPROPERTY()
	TMap<FName, FConvertedAssetRecord> ConvertedAssets;

	UPROPERTY()
	TMap<FName, FUnconvertedDependencyRecord> UnconvertedDependencies;
};
