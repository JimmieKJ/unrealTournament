// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

/*----------------------------------------------------------------------------
	ULinkerSave.
----------------------------------------------------------------------------*/

/** A mapping of package name to generated script SHA keys */
TMap<FString, TArray<uint8> > ULinkerSave::PackagesToScriptSHAMap;

ULinkerSave::ULinkerSave( const FObjectInitializer& ObjectInitializer, UPackage* InParent, const TCHAR* InFilename, bool bForceByteSwapping, bool bInSaveUnversioned )
:	ULinker(ObjectInitializer, InParent, InFilename )
,	Saver( NULL )
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = IFileManager::Get().CreateFileWriter( InFilename, 0 );
		if( !Saver )
		{
			UE_LOG(LogLinker, Fatal, TEXT("%s"), *FString::Printf( TEXT("Error opening file '%s'."), InFilename ) );
		}

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag           = PACKAGE_FILE_TAG;
		Summary.SetFileVersions( GPackageFileUE4Version, GPackageFileLicenseeUE4Version, bInSaveUnversioned );
		Summary.EngineVersion =	GEngineVersion;
		Summary.PackageFlags  = Package ? Package->PackageFlags : 0;

		if (Package)
		{
			Summary.FolderName = Package->GetFolderName().ToString();
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		ArIsSaving				= 1;
		ArIsPersistent			= 1;
		ArForceByteSwapping		= bForceByteSwapping;
	}
}

ULinkerSave::ULinkerSave(const FObjectInitializer& ObjectInitializer, UPackage* InParent, bool bForceByteSwapping, bool bInSaveUnversioned )
:	ULinker( ObjectInitializer, InParent,TEXT("$$Memory$$") )
,	Saver( NULL )
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = new FBufferArchive( false, InParent->FileName );
		check(Saver);

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag           = PACKAGE_FILE_TAG;
		Summary.SetFileVersions( GPackageFileUE4Version, GPackageFileLicenseeUE4Version, bInSaveUnversioned );
		Summary.EngineVersion =	GEngineVersion;
		Summary.PackageFlags  = Package ? Package->PackageFlags : 0;

		if (Package)
		{
			Summary.FolderName = Package->GetFolderName().ToString();
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		ArIsSaving				= 1;
		ArIsPersistent			= 1;
		ArForceByteSwapping		= bForceByteSwapping;
	}
}
/**
 * Detaches file saver and hence file handle.
 */
void ULinkerSave::Detach()
{
	if( Saver )
	{
		delete Saver;
	}
	Saver = NULL;
}

void ULinkerSave::BeginDestroy()
{
	// Detach file saver/ handle.
	Detach();
	Super::BeginDestroy();
}

int32 ULinkerSave::MapName(const FName& Name) const
{
	const int32* IndexPtr = NameIndices.Find(Name);

	if (IndexPtr)
	{
		return *IndexPtr;
	}

	return INDEX_NONE;
}

FPackageIndex ULinkerSave::MapObject( const UObject* Object ) const
{
	if (Object)
	{
		const FPackageIndex *Found = ObjectIndicesMap.Find(Object);
		if (Found)
		{
			return *Found;
		}
	}
	return FPackageIndex();
}

void ULinkerSave::Seek( int64 InPos )
{
	Saver->Seek( InPos );
}

int64 ULinkerSave::Tell()
{
	return Saver->Tell();
}

void ULinkerSave::Serialize( void* V, int64 Length )
{
	Saver->Serialize( V, Length );
}
	
FArchive& ULinkerSave::operator<<( FName& InName )
{
	int32 Save = MapName(InName);
	int32 Number = InName.GetNumber();
	FArchive& Ar = *this;
	return Ar << Save << Number;
}

FArchive& ULinkerSave::operator<<( UObject*& Obj )
{
	FPackageIndex Save;
	if (Obj)
	{
		Save = MapObject(Obj);
	}
	return *this << Save;
}

FArchive& ULinkerSave::operator<<( FLazyObjectPtr& LazyObjectPtr)
{
	FUniqueObjectGuid ID;
	ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

FArchive& ULinkerSave::operator<<( FAssetPtr& AssetPtr)
{
	FStringAssetReference ID;
	UObject *Object = AssetPtr.Get();

	if (Object)
	{
		// Use object in case name has changed. 
		ID = FStringAssetReference(Object);
	}
	else
	{
		ID = AssetPtr.GetUniqueID();
	}

	ID.Serialize(*this);
	return *this;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(ULinkerSave, ULinker,
	{
	}
);
