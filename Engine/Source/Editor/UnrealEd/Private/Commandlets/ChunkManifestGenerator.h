// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UChunkDependencyInfo;
struct FChunkDependencyTreeNode;

/**
 * Helper class for generating streaming install manifests
 */
class FChunkManifestGenerator
{
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
	/** Maps packages to assets from the asset registry */
	TMap<FName, TArray<int32> > PackageToRegistryDataMap;
	/** Should the chunks be generated or only asset registry */
	bool bGenerateChunks;
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
	 * Adds a package to the unassigned package list for managing later
	 *
	 * @param The sandbox filepath of the package
	 * @param The package name
	 */
	void NotifyPackageWasNotAssigned(const FString& PackageSandboxPath, FName PackageName);

	/**
	 * Adds a package to the global list of cooked packages.
	 *
	 * @param The sandbox filepath of the package
	 * @param The package name
	 */
	void NotifyPackageWasCooked(const FString& PackageSandboxPath, FName PackageName);

	/**
	 * Walks the dependency graph of assets and assigns packages to correct chunks.
	 * 
	 * @param the SandboxPlatformFile used during cook
	 */
	void FixupPackageDependenciesForChunks(FSandboxPlatformFile* SandboxFile);

	/**
	 * Adds a package and all its dependencies to a Chunk
	 *
	 * @param The Package set to add to
	 * @param The Name of the package to add
	 * @param The sandbox path of the asset
	 * @param The ID of the Chunk being added to
	 * @param the SandboxPlatformFile used during cook
	 */
	void AddPackageAndDependenciesToChunk(FChunkPackageSet* ChunkPackageSet, FName PkgName, const FString& SandboxFile, int32 ChunkID, FSandboxPlatformFile* SandboxPlatformFile);

	/**
	 * Callback for FCoreDelegates::FOnAssetLoaded delegate.
	 * Collects all assets loaded with the last package.
	 */
	void OnAssetLoaded(UObject* Asset);

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
		//get the objects in this package
		TArray<int32> RegistryChunkIDs;
		for ( const auto& AssetData : AssetRegistryData )
		{
			if ( AssetData.PackageName == PackageFName ) 
			{
				RegistryChunkIDs.Append( AssetData.ChunkIDs );
			}
		}
		return RegistryChunkIDs;
	}

	/**
	 * Deletes the temporary packaging directory for the specified platform.
	 */
	bool CleanTempPackagingDirectory(const FString& Platform) const;

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
	* Get an array of Packages this package will import.
	*/
	bool GetPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames);

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
	void			ResolveChunkDependencyGraph(const FChunkDependencyTreeNode& Node, FChunkPackageSet BaseAssetSet);

	/**
	* Helper function to verify Chunk asset assigment is valid.
	*/
	bool			CheckChunkAssetsAreNotInChild(const FChunkDependencyTreeNode& Node);

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
	void Initialize(bool InGenerateChunks);

	/**
	 * Adds a package to chunk manifest (just calls the other AddPackageToChunkManifestFunction with more parameters)
	 *
	 * @param Package Package to add to one of the manifests
	 * @param SandboxFilename Cooked sandbox path of the package to add to a manifest
	 * @param LastLoadedMapName Name of the last loaded map (can be empty)
	 * @param the SandboxPlatformFile used during cook
	 */
	void AddPackageToChunkManifest(UPackage* Package, const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile* SandboxFile);
	void AddPackageToChunkManifest(const FName& PackageFName, const FString& PackagePathName, const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile* InSandboxFile);
	

	/**
	 * Add a package to the manifest but don't assign it to any chunk yet, packages which are not assigned by the end of the cook will be put into chunk 0
	 * 
	 * @param Package which is unassigned
	 * @param The sandbox file path of the package
	 */
	void AddUnassignedPackageToManifest(UPackage* Package, const FString& PackageSandboxPath );

	/**
	 * Collects all the packages loaded 
	 *
	 * @param Package Package which was loaded 
	 */
	void OnLastPackageLoaded( UPackage* Package );


	/**
	 * Collects all the packages loaded 
	 * Does the same as other overload of OnLastPackageLoaded except takes in the name of the package as a fname 
	 * 
	 * @param Package name
	 */
	void OnLastPackageLoaded( const FName& PackageName );

	/**
	 * The cooker is about to load a new package from the list, reset AssetsLoadedWithLastPackage
	 */
	void PrepareToLoadNewPackage(const FString& Filename);

	/**
	 * Deletes temporary manifest directories.
	 */
	void CleanManifestDirectories();

	/**
	 * Saves all generated manifests for each target platform.
	 * 
	 * @param the SandboxPlatformFile used during cook
	 */
	bool SaveManifests(FSandboxPlatformFile* SandboxFile);

	/**
	* Saves generated asset registry data for each platform.
	*/
	bool SaveAssetRegistry(const FString& SandboxPath);


	/**
	 * Loads asset registry data and incorporates it into the current asset registry data
	 * 
	 * @param the SandboxPath to load teh asset registry from
	 * @param PackagesToLoadMask is used to mask out any packages which we don't want to incorporate into the internal asset registry when we load
	 */
	bool LoadAssetRegistry(const FString& SandboxPath, const TSet<FName>* PackagesToLoadMask = nullptr);

	/**
	 * Saves cooked package and asset information about all the cooked packages and assets contained within for stats purposes
	 * in json format
	 *
	 * @param Sandbox path to save the registry to
	 * @param Append to the previous registry if there is one
	 */
	bool SaveCookedPackageAssetRegistry( const FString& SandboxPath, const bool Append );

};
