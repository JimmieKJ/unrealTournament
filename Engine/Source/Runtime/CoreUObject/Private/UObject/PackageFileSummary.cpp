// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "EngineVersion.h"

FPackageFileSummary::FPackageFileSummary()
{
	FMemory::Memzero( this, sizeof(*this) );
}

FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum )
{
	bool bCanStartSerializing = true;
	int64 ArchiveSize = 0;
	if (Ar.IsLoading())
	{
		// Sanity checks before we even start serializing the archive
		ArchiveSize = Ar.TotalSize();
		const int64 MinimumPackageSize = 32; // That should get us safely to Sum.TotalHeaderSize
		bCanStartSerializing = ArchiveSize >= MinimumPackageSize;
		UE_CLOG(!bCanStartSerializing, LogLinker, Warning,
			TEXT("Failed to read package file summary, the file \"%s\" is too small (%lld bytes, expected at least %lld bytes)"),
			*Ar.GetArchiveName(), ArchiveSize, MinimumPackageSize);
	}
	if (bCanStartSerializing)
	{
		Ar << Sum.Tag;
	}
	// only keep loading if we match the magic
	if( Sum.Tag == PACKAGE_FILE_TAG || Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
	{
		// The package has been stored in a separate endianness than the linker expected so we need to force
		// endian conversion. Latent handling allows the PC version to retrieve information about cooked packages.
		if( Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
		{
			// Set proper tag.
			Sum.Tag = PACKAGE_FILE_TAG;
			// Toggle forced byte swapping.
			if( Ar.ForceByteSwapping() )
			{
				Ar.SetByteSwapping( false );
			}
			else
			{
				Ar.SetByteSwapping( true );
			}
		}
		/**
		 * The package file version number when this package was saved.
		 *
		 * Lower 16 bits stores the UE3 engine version
		 * Upper 16 bits stores the UE4/licensee version
		 * For newer packages this is -5
		 *		-2 indicates presence of enum-based custom versions
		 *		-3 indicates guid-based custom versions
		 *		-4 indicates removal of the UE3 version. Packages saved with this ID cannot be loaded in older engine versions 
		 *      -5 indicates the replacement of writing out the "UE3 version" so older versions of engine can gracefully fail to open newer packages
		 */
		int32 LegacyFileVersion = -5;
		Ar << LegacyFileVersion;

		if (Ar.IsLoading())
		{
			if (LegacyFileVersion < 0) // means we have modern version numbers
			{
				if (LegacyFileVersion != -4)
				{
					int32 LegacyUE3Version = 0;
					Ar << LegacyUE3Version;
				}
				Ar << Sum.FileVersionUE4;
				Ar << Sum.FileVersionLicenseeUE4;

				if (LegacyFileVersion <= -2)
				{
					Sum.CustomVersionContainer.Serialize(Ar, (LegacyFileVersion == -2) ? ECustomVersionSerializationFormat::Enums : ECustomVersionSerializationFormat::Guids);
				}

				if (!Sum.FileVersionUE4 && !Sum.FileVersionLicenseeUE4)
				{
					// this file is unversioned, remember that, then use current versions
					Sum.bUnversioned = true;
					Sum.FileVersionUE4 = GPackageFileUE4Version;
					Sum.FileVersionLicenseeUE4 = GPackageFileLicenseeUE4Version;

					Sum.CustomVersionContainer = FCustomVersionContainer::GetRegistered();
				}
			}
			else
			{
				// This is probably an old UE3 file, make sure that the linker will fail to load with it.
				Sum.FileVersionUE4 = 0;
				Sum.FileVersionLicenseeUE4 = 0;
			}
		}
		else
		{
			if (Sum.bUnversioned)
			{
				int32 Zero = 0;
				Ar << Zero; // LegacyUE3version
				Ar << Zero; // VersionUE4
				Ar << Zero; // VersionLicenseeUE4

				FCustomVersionContainer NoCustomVersions;
				NoCustomVersions.Serialize(Ar);
			}
			else
			{
				// Must write out the last UE3 engine version, so that older versions identify it as new
				int32 LegacyUE3Version = 864;
				Ar << LegacyUE3Version;
				Ar << Sum.FileVersionUE4;
				Ar << Sum.FileVersionLicenseeUE4;

				// Serialise custom version map.
				Sum.CustomVersionContainer.Serialize(Ar);
			}
		}
		Ar << Sum.TotalHeaderSize;
		Ar << Sum.FolderName;
		Ar << Sum.PackageFlags;

#if WITH_EDITOR
		if (Ar.IsLoading())
		{
			// This flag should never be saved and its reused, so we need to make sure it hasn't been loaded.
			Sum.PackageFlags &= ~PKG_NewlyCreated;
		}
#endif // WITH_EDITOR

		if( Sum.PackageFlags & PKG_FilterEditorOnly )
		{
			Ar.SetFilterEditorOnly(true);
		}
		Ar << Sum.NameCount     << Sum.NameOffset;
		Ar << Sum.ExportCount   << Sum.ExportOffset;
		Ar << Sum.ImportCount   << Sum.ImportOffset;
		Ar << Sum.DependsOffset;

		if (Ar.IsLoading() && (Sum.FileVersionUE4 < VER_UE4_OLDEST_LOADABLE_PACKAGE || Sum.FileVersionUE4 > GPackageFileUE4Version))
		{
			return Ar; // we can't safely load more than this because the below was different in older files.
		}

		if (Ar.IsSaving() || Sum.FileVersionUE4 >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP)
		{
			Ar << Sum.StringAssetReferencesCount << Sum.StringAssetReferencesOffset;
		}

		Ar << Sum.ThumbnailTableOffset;

		int32 GenerationCount = Sum.Generations.Num();
		Ar << Sum.Guid << GenerationCount;
		if( Ar.IsLoading() && GenerationCount > 0 )
		{
			Sum.Generations.Empty( 1 );
			Sum.Generations.AddUninitialized( GenerationCount );
		}
		for( int32 i=0; i<GenerationCount; i++ )
		{
			Sum.Generations[i].Serialize(Ar, Sum);
		}

		if( Sum.GetFileVersionUE4() >= VER_UE4_ENGINE_VERSION_OBJECT )
		{
			if(Ar.IsCooking() || (Ar.IsSaving() && !GEngineVersion.IsPromotedBuild()))
			{
				FEngineVersion EmptyEngineVersion;
				Ar << EmptyEngineVersion;
			}
			else
			{
				Ar << Sum.SavedByEngineVersion;
			}
		}
		else
		{
			int32 EngineChangelist = 0;
			Ar << EngineChangelist;

			if(Ar.IsLoading() && EngineChangelist != 0)
			{
				Sum.SavedByEngineVersion.Set(4, 0, 0, EngineChangelist, TEXT(""));
			}
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION )
		{
			if(Ar.IsCooking() || (Ar.IsSaving() && !GEngineVersion.IsPromotedBuild()))
			{
				FEngineVersion EmptyEngineVersion;
				Ar << EmptyEngineVersion;
			}
			else
			{
				Ar << Sum.CompatibleWithEngineVersion;
			}
		}
		else
		{
			if (Ar.IsLoading())
			{
				Sum.CompatibleWithEngineVersion = Sum.SavedByEngineVersion;
			}
		}

		Ar << Sum.CompressionFlags;
		Ar << Sum.CompressedChunks;
		Ar << Sum.PackageSource;

		Ar << Sum.AdditionalPackagesToCook;

#if WITH_ENGINE
		//@todo legacy
		Ar << Sum.TextureAllocations;
#else
		check(!"this can't serialize successfully");
#endif		// WITH_ENGINE

		Ar << Sum.AssetRegistryDataOffset;
		Ar << Sum.BulkDataStartOffset;
		
		if (Sum.GetFileVersionUE4() >= VER_UE4_WORLD_LEVEL_INFO)
		{
			Ar << Sum.WorldTileInfoDataOffset;
		}

		if (Sum.GetFileVersionUE4() >= VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS)
		{
			Ar << Sum.ChunkIDs;
		}
		else if (Sum.GetFileVersionUE4() >= VER_UE4_ADDED_CHUNKID_TO_ASSETDATA_AND_UPACKAGE)
		{
			// handle conversion of single ChunkID to an array of ChunkIDs
			if (Ar.IsLoading())
			{
				int ChunkID = -1;
				Ar << ChunkID;

				// don't load <0 entries since an empty array represents the same thing now
				if (ChunkID >= 0)
				{
					Sum.ChunkIDs.Add( ChunkID );
				}
			}
		}
	}

	return Ar;
}
