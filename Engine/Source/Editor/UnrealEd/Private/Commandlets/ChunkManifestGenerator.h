// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "AssetData.h"
#include "Misc/AssetRegistryInterface.h"
#include "UObject/UObjectHash.h"

class FSandboxPlatformFile;
class IAssetRegistry;
class ITargetPlatform;
class UChunkDependencyInfo;
struct FChunkDependencyTreeNode;

/**
 * Helper class for generating streaming install manifests
 */
class FChunkManifestGenerator
{
	TArray<FName> StartupPackages;
	/** Map of Package name to Sandbox Paths */
	typedef TMap<FName, FString> FChunkPackageSet;
	/** Holds a reference to asset registry */
	IAssetRegistry& AssetRegistry;
	/** Platforms to generate the manifests for. */
	const TArray<ITargetPlatform*> Platforms;
	/** ChunkIDs associated with a package in the asset registry */
	TMap<FName, TArray<int32> > RegistryChunkIDsMap;
	/** List of all asset packages that were created while loading the last package in the cooker. */
	TSet<FName> AssetsLoadedWithLastPackage;
	/** The entire asset registry data */
	TArray<FAssetData> AssetRegistryData;
	/** Lookup for the original ChunkID Mappings. AssetRegistryData is modified making it invalid to query */
	TMap<FName, TArray<int32> > PackageChunkIDMap;
	/** Maps packages to assets from the asset registry */
	TMap<FName, TArray<int32> > PackageToRegistryDataMap;
	/** Should the chunks be generated or only asset registry */
	bool bGenerateChunks;
	/** True when all platforms should generate chunks, regardless of config settings */
	bool bForceGenerateChunksForAllPlatforms;
	/** Array of Maps with chunks<->packages assignments */
	TArray<FChunkPackageSet*>		ChunkManifests;
	/** Map of packages that has not been assigned to chunks */
	FChunkPackageSet				UnassignedPackageSet;
	/** Map of all cooked Packages */
	FChunkPackageSet				AllCookedPackages;
	/** Array of Maps with chunks<->packages assignments. This version contains all dependent packages */
	TArray<FChunkPackageSet*>		FinalChunkManifests;
	/** Lookup table of used package names used when searching references. */
	TSet<FName>						InspectedNames;
	/** */
	UChunkDependencyInfo*			DependencyInfo;

	/** Dependency type to follow when adding package dependencies to chunks.*/
	EAssetRegistryDependencyType::Type DependencyType;

	struct FReferencePair
	{
		FReferencePair() {}

		FReferencePair(const FName& InName, uint32 InParentIndex)
			: PackageName(InName)
			, ParentNodeIndex(InParentIndex)
		{}

		bool operator == (const FReferencePair& RHS) const
		{
			return PackageName == RHS.PackageName;
		}

		FName		PackageName;
		uint32		ParentNodeIndex;
	};


	/**
	 * Adds a package to chunk manifest
	 * 
	 * @param The sandbox filepath of the package
	 * @param The package name
	 * @param The ID of the chunk to assign it to
	 */
	void AddPackageToManifest(const FString& PackageSandboxPath, FName PackageName, int32 ChunkId);

	/**
	* Remove a package from a chunk manifest. Does nothing if package doesn't exist in chunk.
	*
	* @param The package name
	* @param The ID of the chunk to assign it to
	*/
	void RemovePackageFromManifest(FName PackageName, int32 ChunkId);

	/**
	 * Walks the dependency graph of assets and assigns packages to correct chunks.
	 * 
	 * @param the InSandboxFile used during cook
	 */
	void FixupPackageDependenciesForChunks(FSandboxPlatformFile* InSandboxFile);

	void AddPackageAndDependenciesToChunk(FChunkPackageSet* ThisPackageSet, FName InPkgName, const FString& InSandboxFile, int32 ChunkID, FSandboxPlatformFile* SandboxPlatformFile);

	/**
	 * Returns the path of the temporary packaging directory for the specified platform.
	 */
	FString GetTempPackagingDirectoryForPlatform(const FString& Platform) const
	{
		return FPaths::GameSavedDir() / TEXT("TmpPackaging") / Platform;
	}

	/**
	* Returns an array of chunks ID for a package name that have been assigned during the cook process.
	*/
	FORCEINLINE TArray<int32> GetExistingPackageChunkAssignments(FName PackageFName)
	{
		TArray<int32> ExistingChunkIDs;
		for (uint32 ChunkIndex = 0, MaxChunk = ChunkManifests.Num(); ChunkIndex < MaxChunk; ++ChunkIndex)
		{
			if (ChunkManifests[ChunkIndex] && ChunkManifests[ChunkIndex]->Contains(PackageFName))
			{
				ExistingChunkIDs.AddUnique(ChunkIndex);
			}
		}

		if ( StartupPackages.Contains(PackageFName ))
		{
			ExistingChunkIDs.AddUnique(0);
		}

		return ExistingChunkIDs;
	}

	/**
	* Returns an array of chunks IDs for a package that have been assigned in the editor.
	*/
	FORCEINLINE TArray<int32> GetAssetRegistryChunkAssignments(UPackage* Package)
	{
		TArray<int32> RegistryChunkIDs;
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, false);
		for (const auto* Object : ObjectsInPackage)
		{
			if (Object && Object->IsAsset())
			{
				FAssetData Asset(Object);
				for (auto AssetChunk : Asset.ChunkIDs)
				{
					RegistryChunkIDs.AddUnique(AssetChunk);
				}
			}
		}

		return RegistryChunkIDs;
	}

	/**
	* Returns an array of chunks IDs for a package that have been assigned in the editor.
	*/
	FORCEINLINE TArray<int32> GetAssetRegistryChunkAssignments(const FName& PackageFName)
	{
		TArray<int32> RegistryChunkIDs;
		auto* FoundIDs = PackageChunkIDMap.Find(PackageFName);
		if (FoundIDs)
		{
			RegistryChunkIDs = *FoundIDs;
		}
		return RegistryChunkIDs;
	}

	/**
	 * Deletes the temporary packaging directory for the specified platform.
	 */
	bool CleanTempPackagingDirectory(const FString& Platform) const;

	/** Returns true if the specific platform desires a chunk manifest */
	bool ShouldPlatformGenerateStreamingInstallManifest(const ITargetPlatform* Platform) const;

	/**
	 * Generates and saves streaming install chunk manifest.
	 *
	 * @param Platform Platform this manifest is going to be generated for.
	 * @param Chunks List of chunk manifests.
	 */
	bool GenerateStreamingInstallManifest(const FString& Platform);

	/**
	 * Gather a list of dependencies required by to completely load this package.
	 */
	bool GatherAllPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames);

	/**
	* Gather the list of dependencies that link the source to the target.  Output array includes the target.
	*/
	bool GetPackageDependencyChain(FName SourcePackage, FName TargetPackage, TSet<FName>& VisitedPackages, TArray<FName>& OutDependencyChain);

	/**
	* Get an array of Packages this package will import.
	*/
	bool GetPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames, EAssetRegistryDependencyType::Type InDependencyType);

	/**
	 * Save a CSV dump of chunk asset information.
	 */
	bool GenerateAssetChunkInformationCSV(const FString& OutputPath);

	/**
	* Finds the asset belonging to ChunkID with the smallest number of links to Packages In PackageNames.
	*/
	void			FindShortestReferenceChain(TArray<FReferencePair> PackageNames, int32 ChunkID, uint32& OutParentIndex, FString& OutChainPath);

	/**
	* Helper function for FindShortestReferenceChain
	*/
	FString			GetShortestReferenceChain(FName PackageName, int32 ChunkID);

	/**
	* 
	*/
	void			ResolveChunkDependencyGraph(const FChunkDependencyTreeNode& Node, FChunkPackageSet BaseAssetSet, TArray<TArray<FName>>& OutPackagesMovedBetweenChunks);

	/**
	* Helper function to verify Chunk asset assigment is valid.
	*/
	bool			CheckChunkAssetsAreNotInChild(const FChunkDependencyTreeNode& Node);

	/**
	* Helper function to create a given collection.
	*/
	bool			CreateOrEmptyCollection(FName CollectionName);

	/**
	* Helper function to fill a given collection with a set of packages.
	*/
	void			WriteCollection(FName CollectionName, const TArray<FName>& PackageNames);

public:

	/**
	 * Constructor
	 */
	FChunkManifestGenerator(const TArray<ITargetPlatform*>& InPlatforms);

	/**
	 * Destructor
	 */
	~FChunkManifestGenerator();

	/**
	 * Initializes manifest generator - creates manifest lists, hooks up delegates.
	 */
	void Initialize(const TArray<FName> &StartupPackages);


	const TArray<ITargetPlatform*>& GetTargetPlatforms() const { return Platforms; }

	/**
	 * GenerateChunkManifest 
	 * generate chunk manifest for the packages passed in using the asset registry to determine dependencies
	 *
	 * @param StartupPackages list of startup packages which are forced into chunk 0
	 * @param CookedPackages list of packages which were cooked
	 * @param bGenerateStreamingInstallManifest should we build a streaming install manifest 
	 */
	void BuildChunkManifest(const TArray<FName>& CookedPackages, FSandboxPlatformFile* InSandboxFile, bool bGenerateStreamingInstallManifest);

	/**
	 * ContainsMap
	 * Does this package contain a map file (determined by finding if this package contains a UWorld / ULevel object)
	 *
	 * @param PackageName long package name of the package we want to determine if contains a map 
	 * @return return if the package contains a UWorld / ULevel object (contains a map)
	 */
	bool ContainsMap(const FName& PackageName) const;

	/**
	 * Adds a package to chunk manifest (just calls the other AddPackageToChunkManifestFunction with more parameters)
	 *
	 * @param Package Package to add to one of the manifests
	 * @param SandboxFilename Cooked sandbox path of the package to add to a manifest
	 * @param LastLoadedMapName Name of the last loaded map (can be empty)
	 * @param the SandboxPlatformFile used during cook
	 */
	void AddPackageToChunkManifest(const FName& PackageFName, const FString& PackagePathName, const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile* InSandboxFile);
	

	void GenerateChunkManifestForPackage(const FName& PackageFName, const FString& PackagePathName, const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile* InSandboxFile);

	/**
	 * Add a package to the manifest but don't assign it to any chunk yet, packages which are not assigned by the end of the cook will be put into chunk 0
	 * 
	 * @param Package which is unassigned
	 * @param The sandbox file path of the package
	 */
	void AddUnassignedPackageToManifest(UPackage* Package, const FString& PackageSandboxPath );

	/**
	 * Deletes temporary manifest directories.
	 */
	void CleanManifestDirectories();

	/**
	 * Saves all generated manifests for each target platform.
	 * 
	 * @param the InSandboxFile used during cook
	 */
	bool SaveManifests(FSandboxPlatformFile* InSandboxFile);

	/**
	* Saves generated asset registry data for each platform.
	*/
	bool SaveAssetRegistry(const FString& SandboxPath, const TArray<FName>* IgnorePackageList = nullptr);


	/**
	 * Loads asset registry data and incorporates it into the current asset registry data
	 * 
	 * @param the SandboxPath to load teh asset registry from
	 * @param PackagesToLoadMask is used to mask out any packages which we don't want to incorporate into the internal asset registry when we load
	 */
	bool LoadAssetRegistry(const FString& SandboxPath, const TSet<FName>* PackagesToLoadMask = nullptr);


	/**
	* Follows an assets dependency chain to build up a list of package names in the same order as the runtime would attempt to load them
	* 
	* @param InAsset - The asset to (potentially) add to the file order
	* @param OutFileOrder - Output array which collects the package names
	* @param OutEncounteredArray - Temporary collection of package names we've seen. Similar to OutFileOrder but updated BEFORE following dependencies so as to avoid circular references
	* @param InAssets - The source asset list. Used to distinguish between dependencies on other packages and internal objects
	* @param InAssetTypeMap - Lookup table of package name to type. Used to determine if a dependency is a streamed level so as not to follow dependencies
	*/
	void AddAssetToFileOrderRecursive(FAssetData* InAsset, TArray<FName>& OutFileOrder, TArray<FName>& OutEncounteredNames, const TMap<FName, FAssetData*>& InAssets, const TArray<FName>& InMapList);

	/**
	* Build a file order string which represents the order in which files would be loaded at runtime. 
	* 
	* @param InAssetData - Assets data for those assets which were cooked on this run. 
	* @param InTopLevelAssets - Names of map assets
	*/
	FString CreateCookerFileOrderString(const TMap<FName, FAssetData*>& InAssetData, const TArray<FName>& InMaps);
};
