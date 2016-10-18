// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchManifest.h: Declares the manifest classes.
=============================================================================*/

#include "CoreUObject.h"
#include "BuildPatchChunk.h"
#include "BuildPatchManifest.generated.h"

#pragma once

typedef TSharedPtr< class FBuildPatchCustomField, ESPMode::ThreadSafe > FBuildPatchCustomFieldPtr;
typedef TSharedRef< class FBuildPatchCustomField, ESPMode::ThreadSafe > FBuildPatchCustomFieldRef;
typedef TSharedPtr< class FBuildPatchAppManifest, ESPMode::ThreadSafe > FBuildPatchAppManifestPtr;
typedef TSharedRef< class FBuildPatchAppManifest, ESPMode::ThreadSafe > FBuildPatchAppManifestRef;

/**
 * An enum type to describe supported features of a certain manifest
 */
namespace EBuildPatchAppManifestVersion
{
	enum Type
	{
		// The original version
		Original = 0,
		// Support for custom fields
		CustomFields,
		// Started storing the version number
		StartStoringVersion,
		// Made after data files where renamed to include the hash value, these chunks now go to ChunksV2
		DataFileRenames,
		// Manifest stores whether build was constructed with chunk or file data
		StoresIfChunkOrFileData,
		// Manifest stores group number for each chunk/file data for reference so that external readers don't need to know how to calculate them
		StoresDataGroupNumbers,
		// Added support for chunk compression, these chunks now go to ChunkV3. NB: Not File Data Compression yet
		ChunkCompressionSupport,
		// Manifest stores product prerequisites info
		StoresPrerequisitesInfo,
		// Manifest stores chunk download sizes
		StoresChunkFileSizes,
		// Manifest can optionally be stored using UObject serialization and compressed
		StoredAsCompressedUClass,
		// These two features were removed and never used
		UNUSED_0,
		UNUSED_1,
		// Manifest stores chunk data SHA1 hash to use in place of data compare, for faster generation
		StoresChunkDataShaHashes,


		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity
		LatestPlusOne,
		// This is for UObject default, so that we always serialize it
		Invalid = -1
	};
}

namespace EBuildPatchAppManifestVersion
{
	/** @return The last known manifest feature of this code base. Handy for manifest constructor */
	Type GetLatestVersion();

	/** @return The latest version of a manifest supported by JSON serialization method */
	Type GetLatestJsonVersion();

	/** @return The latest version of a manifest supported by file data (nochunks) */
	Type GetLatestFileDataVersion();

	/** @return The latest version of a manifest supported by chunk data */
	Type GetLatestChunkDataVersion();

	/**
	 * Get the chunk subdirectory for used for a specific manifest version, e.g. Chunks, ChunksV2 etc
	 * @param ManifestVersion     The version of the manifest
	 * @return The subdirectory name that this manifest version will access
	 */
	const FString& GetChunkSubdir(const Type ManifestVersion);

	/**
	 * Get the file data subdirectory for used for a specific manifest version, e.g. Files, FilesV2 etc
	 * @param ManifestVersion     The version of the manifest
	 * @return The subdirectory name that this manifest version will access
	 */
	const FString& GetFileSubdir(const Type ManifestVersion);
};


/**
 * An enum type to describe the format that manifest data is stored
 */
UENUM()
namespace EManifestFileHeader
{
	enum Type
	{
		// Storage flags, can be raw or a combination of others.

		/** Zero means raw data. */
		STORED_RAW = 0x0,
		/** Flag for compressed. */
		STORED_COMPRESSED = 0x1,
	};
}

/**
 * A data structure that hold a manifests custom field. This is a key value pair of strings
 */
USTRUCT()
struct FCustomFieldData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Key;

	UPROPERTY()
	FString Value;

	FCustomFieldData();
	FCustomFieldData(const FString& Key, const FString& Value);
};

/**
 * A data structure describing a chunk file
 */
USTRUCT()
struct FChunkInfoData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FGuid Guid;

	UPROPERTY()
	uint64 Hash;

	UPROPERTY()
	FSHAHashData ShaHash;

	UPROPERTY()
	int64 FileSize;

	UPROPERTY()
	uint8 GroupNumber;

	FChunkInfoData();
};

/**
 * A data structure describing the part of a chunk used to construct a file
 */
USTRUCT()
struct FChunkPartData
{
	GENERATED_USTRUCT_BODY()

	// The GUID of the chunk containing this part
	UPROPERTY()
	FGuid Guid;

	// The offset of the first byte into the chunk
	UPROPERTY()
	uint32 Offset;

	// The size of this part
	UPROPERTY()
	uint32 Size;

	FChunkPartData();
};

/**
 * A data structure that describes a file's construction information
 */
USTRUCT()
struct FFileManifestData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Filename;

	UPROPERTY()
	FSHAHashData FileHash;

	UPROPERTY()
	TArray<FChunkPartData> FileChunkParts;

	UPROPERTY()
	TArray<FString> InstallTags;

	UPROPERTY()
	bool bIsUnixExecutable;

	UPROPERTY()
	FString SymlinkTarget;

	UPROPERTY()
	bool bIsReadOnly;

	UPROPERTY()
	bool bIsCompressed;

	FFileManifestData();

	void Init();

	int64 GetFileSize() const;

	bool operator<(const FFileManifestData& Other) const;

private:
	int64 FileSize;
};

/**
 * This is the manifest UObject where all manifest data is stored
 */
UCLASS()
class UBuildPatchManifest : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	uint8 ManifestFileVersion;

	UPROPERTY()
	bool bIsFileData;

	UPROPERTY()
	uint32 AppID;

	UPROPERTY()
	FString AppName;

	UPROPERTY()
	FString BuildVersion;

	UPROPERTY()
	FString LaunchExe;

	UPROPERTY()
	FString LaunchCommand;

	UPROPERTY()
	FString PrereqName;

	UPROPERTY()
	FString PrereqPath;

	UPROPERTY()
	FString PrereqArgs;

	UPROPERTY()
	TArray<FFileManifestData> FileManifestList;

	UPROPERTY()
	TArray<FChunkInfoData> ChunkList;

	UPROPERTY()
	TArray<FCustomFieldData> CustomFields;

	void Clear();

	~UBuildPatchManifest();
};

/**
 * The manifest file header wraps the manifest data stored in a file on disc to describe how to read it
 */
struct FManifestFileHeader
{
	uint32 Magic;
	uint32 HeaderSize;
	uint32 DataSize;
	uint32 CompressedSize;
	FSHAHashData SHAHash;
	uint8 StoredAs;

	FManifestFileHeader();
	bool CheckMagic() const;
	friend FArchive& operator<<(FArchive& Ar, FManifestFileHeader& Header);
};

/**
 * Declare the FBuildPatchCustomField object class, which is the implementation of the object we return to
 * clients of the module
 */
class FBuildPatchCustomField
	: public IManifestField
{
private:
	// Holds the underlying value
	FString CustomValue;

	/**
	 * Hide the default constructor
	 */
	FBuildPatchCustomField(){}
public:
	
	/**
	 * Constructor taking the custom value
	 */
	FBuildPatchCustomField(const FString& Value);

	// START IBuildManifest Interface
	virtual FString AsString() const override;
	virtual double AsDouble() const override;
	virtual int64 AsInteger() const override;
	// END IBuildManifest Interface
};

/**
 * Declares a struct to store the info about a piece of a chunk that is inside a file
 */
struct FFileChunkPart
{
	// The file containing this piece
	FString Filename;
	// The offset into the file of this piece
	uint64 FileOffset;
	// The FChunkPartData that can be salvaged from this file
	FChunkPartData ChunkPart;

	/**
	* Default Constructor
	*/
	FFileChunkPart()
		: Filename()
		, FileOffset(0)
		, ChunkPart()
	{}
};

// Required to allow private access to manifest builder for now..
namespace BuildPatchServices
{
	class FManifestBuilder;
}

/**
 * Declare the FBuildPatchAppManifest object class. This holds the UObject data, and the implemented build manifest functionality
 */
class FBuildPatchAppManifest
	: public IBuildManifest, FGCObject
{
	// Allow access to build processor classes
	friend class FBuildDataGenerator;
	friend class FBuildDataFileProcessor;
	friend class FBuildPatchInstaller;
	friend class BuildPatchServices::FManifestBuilder;
	friend class FBuildMergeManifests;
public:

	/**
	 * Default constructor
	 */
	FBuildPatchAppManifest();

	/**
	 * Basic details constructor
	 */
	FBuildPatchAppManifest(const uint32& InAppID, const FString& AppName);

	/**
	 * Copy constructor
	 */
	FBuildPatchAppManifest(const FBuildPatchAppManifest& Other);

	/**
	 * Default destructor
	 */
	~FBuildPatchAppManifest();

	// START IBuildManifest Interface
	virtual uint32  GetAppID() const override;
	virtual const FString& GetAppName() const override;
	virtual const FString& GetVersionString() const override;
	virtual const FString& GetLaunchExe() const override;
	virtual const FString& GetLaunchCommand() const override;
	virtual const FString& GetPrereqName() const override;
	virtual const FString& GetPrereqPath() const override;
	virtual const FString& GetPrereqArgs() const override;
	virtual int64 GetDownloadSize() const override;
	virtual int64 GetDownloadSize(const TSet<FString>& Tags) const override;
	virtual int64 GetBuildSize() const override;
	virtual int64 GetBuildSize(const TSet<FString>& Tags) const override;
	virtual TArray<FString> GetBuildFileList() const override;
	virtual void GetFileTagList(TSet<FString>& Tags) const override;
	virtual void GetRemovableFiles(IBuildManifestRef OldManifest, TArray< FString >& RemovableFiles) const override;
	virtual void GetRemovableFiles(const TCHAR* InstallPath, TArray< FString >& RemovableFiles) const override;
	virtual bool NeedsResaving() const override;
	virtual void CopyCustomFields(IBuildManifestRef Other, bool bClobber) override;
	virtual const IManifestFieldPtr GetCustomField(const FString& FieldName) const override;
	virtual const IManifestFieldPtr SetCustomField(const FString& FieldName, const FString& Value) override;
	virtual const IManifestFieldPtr SetCustomField(const FString& FieldName, const double& Value) override;
	virtual const IManifestFieldPtr SetCustomField(const FString& FieldName, const int64& Value) override;
	virtual void RemoveCustomField(const FString& FieldName) override;
	virtual IBuildManifestRef Duplicate() const override;
	// END IBuildManifest Interface

	/**
	 * Sets up the internal map from a file
	 * @param Filename		The file to load JSON from
	 * @return		True if successful.
	 */
	bool LoadFromFile(const FString& Filename);

	/**
	 * Sets up the object from the passed in data
	 * @param DataInput		The data to deserialize from
	 * @return		True if successful.
	 */
	bool DeserializeFromData(const TArray<uint8>& DataInput);

	/**
	 * Sets up the object from the passed in JSON string
	 * @param JSONInput		The JSON string to deserialize from
	 * @return		True if successful.
	 */
	bool DeserializeFromJSON(const FString& JSONInput);

	/**
	 * Saves out the manifest information
	 * @param Filename		The file to save to
	 * @param bUseBinary	Whether to save out in the new UObject binary format
	 * @return		True if successful.
	 */
	bool SaveToFile(const FString& Filename, bool bUseBinary);

	/**
	 * Creates the object in JSON format
	 * @param JSONOutput		A string to receive the JSON representation
	 */
	void SerializeToJSON(FString& JSONOutput);

	/**
	 * Gets the version for this manifest. Useful for manifests that were loaded from JSON.
	 * @return		The highest available feature support
	 */
	EBuildPatchAppManifestVersion::Type GetManifestVersion() const;

	/**
	 * Provides a list of chunks required to produce the list of given files.
	 * @param FileList			IN		The list of files.
	 * @param RequiredChunks	OUT		The list of chunk GUIDs needed for those files.
	 */
	void GetChunksRequiredForFiles(const TArray< FString >& FileList, TArray< FGuid >& RequiredChunks, bool bAddUnique = true) const;

	/**
	 * Get the number of times a chunks is referenced in this manifest
	 * @param ChunkGuid		The chunk GUID
	 * @return	The number of references to this chunk
	 */
	uint32 GetNumberOfChunkReferences(const FGuid& ChunkGuid) const;

	/**
	 * Returns the size of a particular data file by it's GUID.
	 * @param DataGuid		The GUID for the data
	 * @return		File size.
	 */
	int64 GetDataSize(const FGuid& DataGuid) const;

	/**
	 * Returns the total size of all data files in it's list.
	 * @param DataGuids		The GUID array for the data
	 * @return		File size.
	 */
	int64 GetDataSize(const TArray< FGuid >& DataGuids) const;

	/**
	 * Returns the size of a particular file in the build
	 * VALID FOR ANY MANIFEST
	 * @param Filename		The file.
	 * @return		File size.
	 */
	int64 GetFileSize(const FString& Filename) const;

	/**
	 * Returns the total size of all files in the array
	 * VALID FOR ANY MANIFEST
	 * @param Filenames		The array of files.
	 * @return		Total size of files in array.
	 */
	int64 GetFileSize(const TArray< FString >& Filenames) const;

	/**
	 * Returns the number of files in this build.
	 * @return		The number of files.
	 */
	uint32 GetNumFiles() const;

	/**
	 * Get the list of files described by this manifest
	 * @param Filenames		OUT		Receives the array of files.
	 */
	void GetFileList(TArray<FString>& Filenames) const;

	/**
	 * Get the list of files that are tagged with the provided tags
	 * @param Tags					The tags for the required file groups.
	 * @param TaggedFiles	OUT		Receives the tagged files.
	 */
	void GetTaggedFileList(const TSet<FString>& Tags, TSet<FString>& TaggedFiles) const;

	/**
	* Get the list of Guids for all files described by this manifest
	* @param DataGuids		OUT		Receives the array of Guids.
	*/
	void GetDataList(TArray< FGuid >& DataGuids) const;

	/**
	 * Returns the manifest for a particular file in the app, nullptr if non-existing
	 * @param Filename	The filename.
	 * @return	The file manifest, or invalid ptr
	 */
	const FFileManifestData* GetFileManifest(const FString& Filename);

	/**
	 * Gets whether this manifest is made up of file data instead of chunk data
	 * @return	True if the build is made from file data. False if the build is constructed from chunk data.
	 */
	bool IsFileDataManifest() const;

	/**
	 * Gets the chunk hash for a given chunk
	 * @param ChunkGuid		IN		The guid of the chunk to get hash for
	 * @param OutHash		OUT		Receives the hash value if found
	 * @return	true if we had the hash for this chunk
	 */
	bool GetChunkHash(const FGuid& ChunkGuid, uint64& OutHash) const;

	/**
	 * Gets the SHA1 hash for a given chunk
	 * @param ChunkGuid		IN		The guid of the chunk to get hash for
	 * @param OutHash		OUT		Receives the hash value if found
	 * @return	true if we had the hash for this chunk
	 */
	bool GetChunkShaHash(const FGuid& ChunkGuid, FSHAHashData& OutHash) const;

	/**
	 * Gets the file hash for given file data
	 * @param FileGuid		IN		The guid of the file data to get hash for
	 * @param OutHash		OUT		Receives the hash value if found
	 * @return	true if we had the hash for this file
	 */
	bool GetFileHash(const FGuid& FileGuid, FSHAHashData& OutHash) const; // DEPRECATE ME

	/**
	 * Gets the file hash for a given file
	 * @param Filename		IN		The filename in the build
	 * @param OutHash		OUT		Receives the hash value if found
	 * @return	true if we had the hash for this file
	 */
	bool GetFileHash(const FString& Filename, FSHAHashData& OutHash) const;

	/**
	 * Gets the file hash for given file data. Valid for non-chunked manifest
	 * @param FileGuid		IN		The guid of the file data to get hash for
	 * @param OutHash		OUT		Receives the hash value if found
	 * @return	true if we had the hash for this file
	 */
	bool GetFilePartHash(const FGuid& FilePartGuid, uint64& OutHash) const;

	/**
	 * Populates an array of chunks that should be producible from this local build, given the list of chunks needed. Also checks source files exist and match size.
	 * @param InstallDirectory	IN		The directory of the build where chunks would be sourced from.
	 * @param ChunksRequired	IN		A list of chunks that are needed.
	 * @param ChunksAvailable	OUT		A list to receive the chunks that could be constructed locally.
	 */
	void EnumerateProducibleChunks(const FString& InstallDirectory, const TArray<FGuid>& ChunksRequired, TArray<FGuid>& ChunksAvailable) const;

	/**
	 * Gets a list of files that were installed with the Old Manifest, but no longer required by the New Manifest.
	 * @param OldManifest		IN		The Build Manifest that is currently installed
	 * @param NewManifest		IN		The Build Manifest that is being patched to
	 * @param RemovableFiles	OUT		A list to receive the files that may be removed.
	 */
	static void GetRemovableFiles(FBuildPatchAppManifestRef OldManifest, FBuildPatchAppManifestRef NewManifest, TArray< FString >& RemovableFiles);

	/**
	 * Gets a list of files that have different hash values in the New Manifest, to those in the Old Manifest.
	 * @param OldManifest		IN		The Build Manifest that is currently installed. Shared Ptr - Can be invalid.
	 * @param NewManifest		IN		The Build Manifest that is being patched to. Shared Ref - Implicitly valid.
	 * @param InstallDirectory	IN		The Build installation directory, so that it can be checked for missing files.
	 * @param OutDatedFiles		OUT		The files that changed hash, are new, are wrong size, or missing on disk.
	 */
	static void GetOutdatedFiles(FBuildPatchAppManifestPtr OldManifest, FBuildPatchAppManifestRef NewManifest, const FString& InstallDirectory, TSet<FString>& OutDatedFiles);

	/**
	 * Check a single file to see if it will be effected by patching
	 * @param OldManifest		The Build Manifest that is currently installed. Shared Ref - Implicitly valid.
	 * @param NewManifest		The Build Manifest that is being patched to. Shared Ref - Implicitly valid.
	 * @param Filename			The Build installation directory, so that it can be checked for missing files.
	 */
	static bool IsFileOutdated(FBuildPatchAppManifestRef OldManifest, FBuildPatchAppManifestRef NewManifest, const FString& Filename);

	/**
	 * Populates a map of FFileChunkParts referring to the chunks in the given array, that should be accessible locally.
	 * @param ChunksRequired		IN		A list of chunks that are needed.
	 * @param ChunkPartsAvailable	OUT		A map to receive the FFileChunkParts keyed by chunk guid.
	 */
	void EnumerateChunkPartInventory(const TArray< FGuid >& ChunksRequired, TMap< FGuid, TArray< FFileChunkPart > >& ChunkPartsAvailable) const;

	/** @return True if any files in this manifest have file attributes to be set */
	bool HasFileAttributes() const;

	/** @return True if this manifest is for the same build, i.e. same ID, Name, and Version */
	bool IsSameAs(FBuildPatchAppManifestRef InstallManifest) const;
public:

	// FGCObject API
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

private:

	bool Serialize(FArchive& Ar);

	/**
	 * Destroys any memory we have allocated and clears out ready for generation of a new manifest
	 */
	void DestroyData();

	/**
	 * Setups the lookup maps that optimize data access, should be called when Data changes
	 */
	void InitLookups();

private:

	/** Holds the actual manifest data. Some other variables point to the memory held by this object */
	UBuildPatchManifest* Data;

	/** Some lookups to optimize data access */
	TMap<FGuid, FString*> FileNameLookup;
	TMap<FString, FFileManifestData*> FileManifestLookup;
	TMap<FString, TArray<FFileManifestData*>> TaggedFilesLookup;
	TMap<FGuid, FChunkInfoData*> ChunkInfoLookup;
	TMap<FString, FCustomFieldData*> CustomFieldLookup;

	/** Holds the total build size in bytes */
	int64 TotalBuildSize;
	int64 TotalDownloadSize;

	/** Flag marked true if we loaded from disk as an old manifest version that should be updated */
	bool bNeedsResaving;
};
