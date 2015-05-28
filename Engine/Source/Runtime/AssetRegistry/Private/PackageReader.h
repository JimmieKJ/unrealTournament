// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FPackageReader : public FArchiveUObject
{
public:
	FPackageReader();
	~FPackageReader();

	/** Creates a loader for the filename */
	bool OpenPackageFile(const FString& PackageFilename);

	/** Reads information from the asset registry data table and converts it to FBackgroundAssetData */
	bool ReadAssetRegistryData(TArray<FBackgroundAssetData*>& AssetDataList);

	/** Attempts to get the class name of an object from the thumbnail cache for packages older than VER_UE4_ASSET_REGISTRY_TAGS */
	bool ReadAssetDataFromThumbnailCache(TArray<FBackgroundAssetData*>& AssetDataList);

	/** Reads information used by the dependency graph */
	bool ReadDependencyData(FPackageDependencyData& OutDependencyData);

	/** Serializers for different package maps */
	void SerializeNameMap();
	void SerializeImportMap(TArray<FObjectImport>& OutImportMap);
	void SerializeStringAssetReferencesMap(TArray<FString>& OutStringAssetReferencesMap);

	// Farchive implementation to redirect requests to the Loader
	void Serialize( void* V, int64 Length );
	bool Precache( int64 PrecacheOffset, int64 PrecacheSize );
	void Seek( int64 InPos );
	int64 Tell();
	int64 TotalSize();
	FArchive& operator<<( FName& Name );
	virtual FString GetArchiveName() const override
	{
		return PackageFilename;
	}

private:
	FString PackageFilename;
	FArchive* Loader;
	FPackageFileSummary PackageFileSummary;
	TArray<FName> NameMap;
};