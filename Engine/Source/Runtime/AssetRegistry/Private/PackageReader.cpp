// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryPCH.h"

FPackageReader::FPackageReader()
{
	Loader = NULL;
	ArIsLoading		= true;
	ArIsPersistent	= true;
}

FPackageReader::~FPackageReader()
{
	if (Loader)
	{
		delete Loader;
	}
}

bool FPackageReader::OpenPackageFile(const FString& InPackageFilename)
{
	PackageFilename = InPackageFilename;
	Loader = IFileManager::Get().CreateFileReader( *PackageFilename );

	if( Loader == NULL )
	{
		// Couldn't open the file
		return false;
	}

	// Read package file summary from the file
	*this << PackageFileSummary;

	// Validate the summary.

	// Make sure this is indeed a package
	if( PackageFileSummary.Tag != PACKAGE_FILE_TAG )
	{
		// Unrecognized or malformed package file
		return false;
	}

	// Don't read packages that are too old
	if( PackageFileSummary.GetFileVersionUE4() < VER_UE4_OLDEST_LOADABLE_PACKAGE )
	{
		return false;
	}

	// Don't read packages that were saved with an package version newer than the current one.
	if( (PackageFileSummary.GetFileVersionUE4() > GPackageFileUE4Version) || (PackageFileSummary.GetFileVersionLicenseeUE4() > GPackageFileLicenseeUE4Version) )
	{
		return false;
	}

	// Check serialized custom versions against latest custom versions.
	const FCustomVersionContainer& LatestCustomVersions  = FCustomVersionContainer::GetRegistered();
	const TArray<FCustomVersion>&  PackageCustomVersions = PackageFileSummary.GetCustomVersionContainer().GetAllVersions();
	for (TArray<FCustomVersion>::TConstIterator It(PackageCustomVersions); It; ++It)
	{
		const FCustomVersion& SerializedCustomVersion = *It;

		auto* LatestVersion = LatestCustomVersions.GetVersion(SerializedCustomVersion.Key);
		if (!LatestVersion || SerializedCustomVersion.Version > LatestVersion->Version)
		{
			return false;
		}
	}

	// check if this is a compressed package and decompress header 
	if ( !!(PackageFileSummary.PackageFlags & PKG_StoreCompressed) )
	{
		check(PackageFileSummary.CompressedChunks.Num() > 0);

		int64 CurPos = Loader->Tell();

		if ( !Loader->SetCompressionMap(&PackageFileSummary.CompressedChunks, (ECompressionFlags)PackageFileSummary.CompressionFlags) )
		{
			delete Loader;

			Loader = new FArchiveAsync(*PackageFilename);
			check(!Loader->IsError());

			verify(Loader->SetCompressionMap(&PackageFileSummary.CompressedChunks, (ECompressionFlags)PackageFileSummary.CompressionFlags));
		}
		
		Seek(Loader->Tell());
	}

	//make sure the filereader gets the correct version number (it defaults to latest version)
	SetUE4Ver(PackageFileSummary.GetFileVersionUE4());
	SetLicenseeUE4Ver(PackageFileSummary.GetFileVersionLicenseeUE4());
	SetEngineVer(PackageFileSummary.SavedByEngineVersion);
	Loader->SetUE4Ver(PackageFileSummary.GetFileVersionUE4());
	Loader->SetLicenseeUE4Ver(PackageFileSummary.GetFileVersionLicenseeUE4());
	Loader->SetEngineVer(PackageFileSummary.SavedByEngineVersion);

	const FCustomVersionContainer& PackageFileSummaryVersions = PackageFileSummary.GetCustomVersionContainer();
	SetCustomVersions(PackageFileSummaryVersions);
	Loader->SetCustomVersions(PackageFileSummaryVersions);

	return true;
}

bool FPackageReader::ReadAssetRegistryData (TArray<FBackgroundAssetData*>& AssetDataList)
{
	check(Loader);

	// Does the package contain asset registry tags
	if( PackageFileSummary.AssetRegistryDataOffset == 0 )
	{
		// No Tag Table!
		return false;
	}

	// Seek the the part of the file where the asset registry tags live
	Seek( PackageFileSummary.AssetRegistryDataOffset );

	// Determine the package name and path
	FString PackageName = FPackageName::FilenameToLongPackageName(PackageFilename);
	FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	const bool bIsMapPackage = (PackageFileSummary.PackageFlags & PKG_ContainsMap) != 0;

	// Assets do not show up in map packages unless we launch with -WorldAssets
	static const bool bUsingWorldAssets = FAssetRegistry::IsUsingWorldAssets();
	if ( bIsMapPackage && !bUsingWorldAssets )
	{
		return true;
	}

	// Load the object count
	int32 ObjectCount = 0;
	*this << ObjectCount;

	// Worlds that were saved before they were marked public do not have asset data so we will synthesize it here to make sure we see all legacy umaps
	// We will also do this for maps saved after they were marked public but no asset data was saved for some reason. A bug caused this to happen for some maps.
	if (bUsingWorldAssets && bIsMapPackage)
	{
		const bool bLegacyPackage = PackageFileSummary.GetFileVersionUE4() < VER_UE4_PUBLIC_WORLDS;
		const bool bNoMapAsset = (ObjectCount == 0);
		if (bLegacyPackage || bNoMapAsset)
		{
			const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
			TMultiMap<FString, FString> TagsAndValues;
			AssetDataList.Add(new FBackgroundAssetData(PackageName, PackagePath, TEXT(""), AssetName, TEXT("World"), TagsAndValues, PackageFileSummary.ChunkIDs));
		}
	}

	// UAsset files only have one object, but legacy or map packages may have more.
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectCount; ++ObjectIdx)
	{
		FString ObjectPath;
		FString ObjectClassName;
		int32 TagCount = 0;
		*this << ObjectPath;
		*this << ObjectClassName;
		*this << TagCount;

		TMultiMap<FString, FString> TagsAndValues;

		for(int32 TagIdx = 0; TagIdx < TagCount; ++TagIdx)
		{
			FString Key;
			FString Value;
			*this << Key;
			*this << Value;

			TagsAndValues.Add(Key, Value);
		}

		FString GroupNames;
		FString AssetName;

		if ( ObjectPath.Contains(TEXT("."), ESearchCase::CaseSensitive))
		{
			ObjectPath.Split(TEXT("."), &GroupNames, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		}
		else
		{
			AssetName = ObjectPath;
		}

		// Before world were RF_Public, other non-public assets were added to the asset data table in map packages.
		// Here we simply skip over them
		if ( bIsMapPackage && PackageFileSummary.GetFileVersionUE4() < VER_UE4_PUBLIC_WORLDS )
		{
			if ( AssetName != FPackageName::GetLongPackageAssetName(PackageName) )
			{
				continue;
			}
		}

		// Create a new FBackgroundAssetData for this asset and update it with the gathered data
		AssetDataList.Add(new FBackgroundAssetData(PackageName, PackagePath, GroupNames, AssetName, ObjectClassName, TagsAndValues, PackageFileSummary.ChunkIDs));
	}

	return true;
}

bool FPackageReader::ReadAssetDataFromThumbnailCache(TArray<FBackgroundAssetData*>& AssetDataList)
{
	check(Loader);

	// Does the package contain a thumbnail table?
	if( PackageFileSummary.ThumbnailTableOffset == 0 )
	{
		return false;
	}

	// Seek the the part of the file where the thumbnail table lives
	Seek( PackageFileSummary.ThumbnailTableOffset );

	// Determine the package name and path
	FString PackageName = FPackageName::FilenameToLongPackageName(PackageFilename);
	FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	// Load the thumbnail count
	int32 ObjectCount = 0;
	*this << ObjectCount;

	// Iterate over every thumbnail entry and harvest the objects classnames
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectCount; ++ObjectIdx)
	{
		// Serialize the classname
		FString AssetClassName;
		*this << AssetClassName;

		// Serialize the object path.
		FString ObjectPathWithoutPackageName;
		*this << ObjectPathWithoutPackageName;

		// Serialize the rest of the data to get at the next object
		int32 FileOffset = 0;
		*this << FileOffset;

		FString GroupNames;
		FString AssetName;

		if ( ObjectPathWithoutPackageName.Contains(TEXT("."), ESearchCase::CaseSensitive) )
		{
			ObjectPathWithoutPackageName.Split(TEXT("."), &GroupNames, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		}
		else
		{
			AssetName = ObjectPathWithoutPackageName;
		}

		// Create a new FBackgroundAssetData for this asset and update it with the gathered data
		TMultiMap<FString, FString> TagsAndValues;
		AssetDataList.Add(new FBackgroundAssetData(PackageName, PackagePath, GroupNames, AssetName, AssetClassName, TagsAndValues, PackageFileSummary.ChunkIDs));
	}

	return true;
}

bool FPackageReader::ReadDependencyData (FPackageDependencyData& OutDependencyData)
{
	OutDependencyData.PackageName = FName(*FPackageName::FilenameToLongPackageName(PackageFilename));

	SerializeNameMap();
	SerializeImportMap(OutDependencyData.ImportMap);
	SerializeStringAssetReferencesMap(OutDependencyData.StringAssetReferencesMap);

	return true;
}

void FPackageReader::SerializeNameMap()
{
	if( PackageFileSummary.NameCount > 0 )
	{
		Seek( PackageFileSummary.NameOffset );

		for ( int32 NameMapIdx = 0; NameMapIdx < PackageFileSummary.NameCount; ++NameMapIdx )
		{
			// Read the name entry from the file.
			FNameEntry NameEntry(ENAME_LinkerConstructor);
			*this << NameEntry;

			NameMap.Add( 
				NameEntry.IsWide() ? 
					FName(ENAME_LinkerConstructor, NameEntry.GetWideName()) : 
					FName(ENAME_LinkerConstructor, NameEntry.GetAnsiName())
				);
		}
	}
}

void FPackageReader::SerializeImportMap(TArray<FObjectImport>& OutImportMap)
{
	if( PackageFileSummary.ImportCount > 0 )
	{
		Seek( PackageFileSummary.ImportOffset );
		for ( int32 ImportMapIdx = 0; ImportMapIdx < PackageFileSummary.ImportCount; ++ImportMapIdx )
		{
			FObjectImport* Import = new(OutImportMap)FObjectImport;
			*this << *Import;
		}
	}
}

void FPackageReader::SerializeStringAssetReferencesMap(TArray<FString>& OutStringAssetReferencesMap)
{
	if (UE4Ver() >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP && PackageFileSummary.StringAssetReferencesCount > 0)
	{
		Seek(PackageFileSummary.StringAssetReferencesOffset);

		for (int32 ReferenceIdx = 0; ReferenceIdx < PackageFileSummary.StringAssetReferencesCount; ++ReferenceIdx)
		{
			FString Buf;
			*this << Buf;
			OutStringAssetReferencesMap.Add(MoveTemp(Buf));
		}
	}
}

void FPackageReader::Serialize( void* V, int64 Length )
{
	check(Loader);
	Loader->Serialize( V, Length );
}

bool FPackageReader::Precache( int64 PrecacheOffset, int64 PrecacheSize )
{
	check(Loader);
	return Loader->Precache( PrecacheOffset, PrecacheSize );
}

void FPackageReader::Seek( int64 InPos )
{
	check(Loader);
	Loader->Seek( InPos );
}

int64 FPackageReader::Tell()
{
	check(Loader);
	return Loader->Tell();
}

int64 FPackageReader::TotalSize()
{
	check(Loader);
	return Loader->TotalSize();
}

FArchive& FPackageReader::operator<<( FName& Name )
{
	check(Loader);

	NAME_INDEX NameIndex;
	FArchive& Ar = *this;
	Ar << NameIndex;

	if( !NameMap.IsValidIndex(NameIndex) )
	{
		UE_LOG(LogAssetRegistry, Fatal, TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num() );
	}

	// if the name wasn't loaded (because it wasn't valid in this context)
	if (NameMap[NameIndex] == NAME_None)
	{
		int32 TempNumber;
		Ar << TempNumber;
		Name = NAME_None;
	}
	else
	{
		int32 Number;
		Ar << Number;
		// simply create the name from the NameMap's name and the serialized instance number
		Name = FName(NameMap[NameIndex], Number);
	}

	return *this;
}
