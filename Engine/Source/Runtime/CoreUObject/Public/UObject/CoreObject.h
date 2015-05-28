// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectBase.h"
#include "WorldCompositionUtility.h"


/**
 * Structure to hold information about an external packages objects used in cross-level references
 */
struct FLevelGuids
{
	/** Name of the external level */
	FName LevelName;

	/** Array of Guids possible in the other level (can be emptied out if all references are resolved after level load) */
	TArray<FGuid> Guids;
};

/**
 * A package.
 */
class COREUOBJECT_API UPackage : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC(UPackage,UObject,0,CoreUObject, CASTCLASS_UPackage)

public:
	/** delegate type for package dirty state events.  ( Params: UPackage* ModifiedPackage ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPackageDirtyStateChanged, class UPackage*);
	/** delegate type for package saved events ( Params: const FString& PackageFileName, UObject* Outer ) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPackageSaved, const FString&, UObject*);					
	/** delegate type for when a package is marked as dirty via UObjectBaseUtilty::MarkPackageDirty ( Params: UPackage* ModifiedPackage, bool bWasDirty ) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPackageMarkedDirty, class UPackage*, bool);

	/** Delegate to notify subscribers when a package has been saved. This is triggered when the package saving
	 *  has completed and was successfull. */
	static FOnPackageSaved PackageSavedEvent;
	/** Delegate to notify subscribers when the dirty state of a package is changed.
	 *  Allows the editor to register the modified package as one that should be prompted for source control checkout. 
	 *  Use Package->IsDirty() to get the updated dirty state of the package */
	static FOnPackageDirtyStateChanged PackageDirtyStateChangedEvent;
	/** 
	 * Delegate to notify subscribers when a package is marked as dirty via UObjectBaseUtilty::MarkPackageDirty 
	 * Note: Unlike FOnPackageDirtyStateChanged, this is always called, even when the package is already dirty
	 * Use bWasDirty to check the previous dirty state of the package
	 * Use Package->IsDirty() to get the updated dirty state of the package
	 */
	static FOnPackageMarkedDirty PackageMarkedDirtyEvent;

private:
	/** Used by the editor to determine if a package has been changed.																							*/
	bool	bDirty;
public:
	/** Whether this package has been fully loaded (aka had all it's exports created) at some point.															*/
	bool	bHasBeenFullyLoaded;
private:
	/** Returns whether exports should be found in memory first before trying to load from disk from within CreateExport.										*/
	bool	bShouldFindExportsInMemoryFirst;
	/** Indicates which folder to display this package under in the Generic Browser's list of packages. If not specified, package is added to the root level.	*/
	FName	FolderName;
	/** Time in seconds it took to fully load this package. 0 if package is either in process of being loaded or has never been fully loaded.					*/
	float LoadTime;

	/** GUID of package if it was loaded from disk; used by netcode to make sure packages match between client and server */
	FGuid Guid;
	/** Chunk IDs for the streaming install chunks this package will be placed in.  Empty for no chunk */
	TArray<int32> ChunkIDs;
	/** for packages that were a forced export in another package (seekfree loading), the name of that base package, otherwise NAME_None */
	FName ForcedExportBasePackageName;
public:

	virtual bool IsNameStableForNetworking() const override { return true; }		// For now, assume all packages have stable net names

	/** Package flags, serialized.*/
	uint32	PackageFlags;

	/** The name of the file that this package was loaded from */
	FName	FileName;

	/** Linker load associated with this package */
	class FLinkerLoad* LinkerLoad;

	/** Linker package version this package has been serialized with. This is mostly used by PostLoad **/
	int32 LinkerPackageVersion;

	/** Linker licensee version this package has been serialized with. This is mostly used by PostLoad **/
	int32 LinkerLicenseeVersion;

	/** Linker custom version container this package has been serialized with. This is mostly used by PostLoad **/
	FCustomVersionContainer LinkerCustomVersion;

	/** size of the file for this package; if the package was not loaded from a file or was a forced export in another package, this will be zero */
	int64 FileSize;
	
	/** Editor only: Thumbnails stored in this package */
	TScopedPointer< FThumbnailMap > ThumbnailMap;

	// MetaData for the editor, or NULL in the game
	class UMetaData*	MetaData;
	
	// World browser information
	TScopedPointer< FWorldTileInfo > WorldTileInfo;

#if WITH_EDITOR
	/** Editor only: PIE instance ID this package belongs to, INDEX_NONE otherwise */
	int32 PIEInstanceID;
#endif

	/**
	 * Called after the C++ constructor and after the properties have been initialized, but before the config has been loaded, etc.
	 * mainly this is to emulate some behavior of when the constructor was called after the properties were intialized.
	 */
	virtual void PostInitProperties() override;

	virtual void BeginDestroy() override;

	/** Serializer */
	virtual void Serialize( FArchive& Ar ) override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/** Packages are never assets */
	virtual bool IsAsset() const override { return false; }

	// UPackage interface.

	/**
	 * Sets the time it took to load this package.
	 */
	void SetLoadTime( float InLoadTime )
	{
		LoadTime = InLoadTime;
	}

	/**
	 * Returns the time it took the last time this package was fully loaded, 0 otherwise.
	 *
	 * @return Time it took to load.
	 */
	float GetLoadTime()
	{
		return LoadTime;
	}

	/**
	 * Get the package's folder name
	 * @return		Folder name
	 */
	FName GetFolderName() const
	{
		return FolderName;
	}

	/**
	 * Set the package's folder name
	 */
	void SetFolderName (FName name)
	{
		FolderName = name;
	}

	/**
	 * Marks/Unmarks the package's bDirty flag
	 */
	void SetDirtyFlag( bool bIsDirty );

	/**
	 * Returns whether the package needs to be saved.
	 *
	 * @return		true if the package is dirty and needs to be saved, false otherwise.
	 */
	bool IsDirty() const
	{
		return bDirty;
	}

	/**
	 * Marks this package as being fully loaded.
	 */
	void MarkAsFullyLoaded()
	{
		bHasBeenFullyLoaded = true;
	}

	/**
	 * Returns whether the package is fully loaded.
	 *
	 * @return true if fully loaded or no file associated on disk, false otherwise
	 */
	bool IsFullyLoaded();

	/**
	 * Fully loads this package. Safe to call multiple times and won't clobber already loaded assets.
	 */
	void FullyLoad();

	/**
	 * Tags the Package's metadata
	 */
	virtual void TagSubobjects(EObjectFlags NewFlags) override;

	/**
	 * Sets whether exports should be found in memory first or  not.
	 *
	 * @param bInShouldFindExportsInMemoryFirst	Whether to find in memory first or not
	 */
	void FindExportsInMemoryFirst( bool bInShouldFindExportsInMemoryFirst )
	{
		bShouldFindExportsInMemoryFirst = bInShouldFindExportsInMemoryFirst;
	}

	/**
	 * Returns whether exports should be found in memory first before trying to load from disk
	 * from within CreateExport.
	 *
	 * @return true if exports should be found via FindObject first, false otherwise.
	 */
	bool ShouldFindExportsInMemoryFirst()
	{
		return bShouldFindExportsInMemoryFirst;
	}

	/**
	 * Called to indicate that this package contains a ULevel or UWorld object.
	 */
	void ThisContainsMap() 
	{
		PackageFlags |= PKG_ContainsMap;
	}

	/**
	 * Returns whether this package contains a ULevel or UWorld object.
	 *
	 * @return		true if package contains ULevel/ UWorld object, false otherwise.
	 */
	bool ContainsMap() const
	{
		return (PackageFlags & PKG_ContainsMap) ? true : false;
	}

	/**
	 * Called to indicate that this package contains data required to be gathered for localization.
	 */
	void ThisRequiresLocalizationGather(bool Value)
	{
		if(Value)
		{
			PackageFlags |= PKG_RequiresLocalizationGather;
		}
		else
		{
			PackageFlags &= ~PKG_RequiresLocalizationGather;
		}
	}

	/**
	 * Returns whether this package contains data required to be gathered for localization.
	 *
	 * @return		true if package contains contains data required to be gathered for localization, false otherwise.
	 */
	bool RequiresLocalizationGather() const
	{
		return (PackageFlags & PKG_RequiresLocalizationGather) ? true : false;
	}

	/** Returns true if this package has a thumbnail map */
	bool HasThumbnailMap() const
	{
		return ThumbnailMap.IsValid();
	}
	
	/** Returns the thumbnail map for this package (const).  Only call this if HasThumbnailMap returns true! */
	const FThumbnailMap& GetThumbnailMap() const
	{
		check( HasThumbnailMap() );
		return *ThumbnailMap;
	}

	/** Access the thumbnail map for this package.  Only call this if HasThumbnailMap returns true! */
	FThumbnailMap& AccessThumbnailMap()
	{
		check( HasThumbnailMap() );
		return *ThumbnailMap;
	}
	
	/** returns our Guid */
	FORCEINLINE FGuid GetGuid() const
	{
		return Guid;
	}
	/** makes our a new fresh Guid */
	FORCEINLINE void MakeNewGuid()
	{
		Guid = FGuid::NewGuid();
	}
	/** sets a specific Guid */
	FORCEINLINE void SetGuid(FGuid NewGuid)
	{
		Guid = NewGuid;
	}

	/** returns our FileSize */
	FORCEINLINE int64 GetFileSize()
	{
		return FileSize;
	}

	/** returns our ChunkIDs */
	FORCEINLINE const TArray<int32>& GetChunkIDs() const
	{
		return ChunkIDs;
	}
	/** sets our ChunkIDs */
	FORCEINLINE void SetChunkIDs(const TArray<int32>& InChunkIDs)
	{
		ChunkIDs = InChunkIDs;
	}

	/** returns ForcedExportBasePackageName */
	FORCEINLINE FName GetForcedExportBasePackageName()
	{
		return ForcedExportBasePackageName;
	}

	////////////////////////////////////////////////////////
	// MetaData 

	/**
	 * Gets (after possibly creating) a metadata object for this package
	 *
	 * @param	bAllowLoadObject				Can load an object to find it's UMetaData if not currently loaded.
	 *
	 * @return A valid UMetaData pointer for all objects in this package
	 */
	class UMetaData* GetMetaData();

	/**
	 * Save one specific object (along with any objects it references contained within the same Outer) into an Unreal package.
	 * 
	 * @param	InOuter							the outer to use for the new package
	 * @param	Base							the object that should be saved into the package
	 * @param	TopLevelFlags					For all objects which are not referenced [either directly, or indirectly] through Base, only objects
	 *											that contain any of these flags will be saved.  If 0 is specified, only objects which are referenced
	 *											by Base will be saved into the package.
	 * @param	Filename						the name to use for the new package file
	 * @param	Error							error output
	 * @param	Conform							if non-NULL, all index tables for this will be sorted to match the order of the corresponding index table
	 *											in the conform package
	 * @param	bForceByteSwapping				whether we should forcefully byte swap before writing to disk
	 * @param	bWarnOfLongFilename				[opt] If true (the default), warn when saving to a long filename.
	 * @param	SaveFlags						Flags to control saving
	 * @param	TargetPlatform					The platform being saved for
	 * @param	FinalTimeStamp					If not FDateTime::MinValue(), the timestamp the saved file should be set to. (Intended for cooking only...)
	 *
	 * @return	true if the package was saved successfully.
	 */
	static bool SavePackage( UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename, 
		FOutputDevice* Error=GError, FLinkerLoad* Conform=NULL, bool bForceByteSwapping=false, bool bWarnOfLongFilename=true, 
		uint32 SaveFlags=SAVE_None, const class ITargetPlatform* TargetPlatform = NULL, const FDateTime& FinalTimeStamp = FDateTime::MinValue(), bool bSlowTask = true );

	/** Wait for any SAVE_Async file writes to complete **/
	static void WaitForAsyncFileWrites();

	/**
	 * Static: Saves thumbnail data for the specified package outer and linker
	 *
	 * @param	InOuter							the outer to use for the new package
	 * @param	Linker							linker we're currently saving with
	 */
	static void SaveThumbnails( UPackage* InOuter, FLinkerSave* Linker );

	/**
	 * Static: Saves asset registry data for the specified package outer and linker
	 *
	 * @param	InOuter							the outer to use for the new package
	 * @param	Linker							linker we're currently saving with
	 */
	static void SaveAssetRegistryData( UPackage* InOuter, FLinkerSave* Linker );

	/**
	 * Static: Saves the level information used by the World browser
	 *
	 * @param	InOuter							the outer to use for the new package
	 * @param	Linker							linker we're currently saving with
	 */
	static void SaveWorldLevelInfo( UPackage* InOuter, FLinkerSave* Linker );

	/**
	  * Determines if a package contains no more assets.
	  *
	  * @param Package			the package to test
	  * @param LastReferencer	the optional last UObject referencer to this package. This object will be excluded when determining if the package is empty
	  * @return true if Package contains no more assets.
	  */
	static bool IsEmptyPackage(UPackage* Package, const UObject* LastReferencer = NULL);


	/**
	 * @param TargetPlatform - The platform being saved for
	 * @param bIsCooking	 - Whether we are cooking or not
	 *
	 * @return Objects marks specific for the particular target platform, objects with these marks will be rejected from the cook
	 */
	static EObjectMark GetObjectMarksForTargetPlatform( const class ITargetPlatform* TargetPlatform, const bool bIsCooking );

};

/*-----------------------------------------------------------------------------
	UMetaData.
-----------------------------------------------------------------------------*/

/**
 * An object that holds a map of key/value pairs. 
 */
class COREUOBJECT_API UMetaData : public UObject
{
	DECLARE_CLASS_INTRINSIC(UMetaData, UObject, 0, CoreUObject)

public:
	// Variables.
	TMap< FWeakObjectPtr, TMap<FName, FString> > ObjectMetaDataMap;

public:
	// MetaData utility functions

	/**
	 * Return the value for the given key in the given property
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const UObject* Object, const TCHAR* Key);
	
	/**
	 * Return the value for the given key in the given property
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const UObject* Object, FName Key);

	/**
	 * Return whether or not the Key is in the meta data
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to query for existence
	 * @return true if found
	 */
	bool HasValue(const UObject* Object, const TCHAR* Key);

	/**
	 * Return whether or not the Key is in the meta data
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to query for existence
	 * @return true if found
	 */
	bool HasValue(const UObject* Object, FName Key);

	/**
	 * Is there any metadata for this property?
	 * @param Object the object to lookup the metadata for
	 * @return TrUE if the object has any metadata at all
	 */
	bool HasObjectValues(const UObject* Object);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param Object the object to set the metadata for
	 * @Values The metadata key/value pairs
	 */
	void SetObjectValues(const UObject* Object, const TMap<FName, FString>& Values);

	/**
	 * Set the key/value pair in the Object's metadata
	 * @param Object the object to set the metadata for
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 */
	void SetValue(const UObject* Object, const TCHAR* Key, const TCHAR* Value);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param Object the object to set the metadata for
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 * @Values The metadata key/value pairs
	 */
	void SetValue(const UObject* Object, FName Key, const TCHAR* Value);

	/** 
	 *	Remove any entry with the supplied Key form the Property's metadata 
	 *	@param Object the object to clear the metadata for
	 *	@param Key A key to clear the data for
	 */
	void RemoveValue(const UObject* Object, const TCHAR* Key);

	/** 
	 *	Remove any entry with the supplied Key form the Property's metadata 
	 *	@param Object the object to clear the metadata for
	 *	@param Key A key to clear the data for
	 */
	void RemoveValue(const UObject* Object, FName Key);

	/** Find the name/value map for metadata for a specific object */
	static TMap<FName, FString>* GetMapForObject(const UObject* Object);

	/** Copy all metadata from the source object to the destination object. This will add to any existing metadata entries for SourceObject. */
	static void CopyMetadata(UObject* SourceObject, UObject* DestObject);

	/**
	 * Removes any metadata entries that are to objects not inside the same package as this UMetaData object.
	 */
	void RemoveMetaDataOutsidePackage();

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual bool NeedsLoadForEditorGame() const override;
	virtual bool IsAsset() const override { return false; }
	// End of UObject interface

#if HACK_HEADER_GENERATOR
	// Returns the remapped key name, or NAME_None was not remapped.
	static FName GetRemappedKeyName(FName OldKey);
#endif

private:
	static void InitializeRedirectMap();

private:
	// Redirect map from deprecated keys to current key names
	static TMap<FName, FName> KeyRedirectMap;
};


#if WITH_EDITOR
struct FMetaDataUtilities
{
private:
	/** Console command for dumping all metadata */
	static class FAutoConsoleCommand DumpAllConsoleCommand;

public:
	/** Find all UMetadata and print its contents to the log */
	COREUOBJECT_API static void DumpAllMetaData();

	/** Output contents of this metadata object to the log */
	COREUOBJECT_API static void DumpMetaData(UMetaData* Object);

private:
	friend class UObject;

	/** Helper class to backup and move the metadata for a given UObject (and optionally its children). */
	class FMoveMetadataHelperContext
	{
	public:
		/**
		 * Backs up the metadata for the UObject (and optionally its children).
		 *
		 * @param	SourceObject		The main UObject to move metadata for.
		 * @param	bSearchChildren		When true all the metadata for classes 
		 */
		FMoveMetadataHelperContext(UObject *SourceObject, bool bSearchChildren);

		/**
		 * Patches up the new metadata on destruction.
		 */
		~FMoveMetadataHelperContext();
	private:

		/** Keep the old package around so we can pull in the metadata without actually duplicating it. */
		UPackage* OldPackage;

		/** Cache a pointer to the object so we can do the search on the old metadata. */
		UObject* OldObject;

		/** When true, search children as well. */
		bool bShouldSearchChildren;
	};

private:
	FMetaDataUtilities() {}
};

#endif //WITH_EDITOR