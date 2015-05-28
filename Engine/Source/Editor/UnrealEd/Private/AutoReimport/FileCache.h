// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/CustomVersion.h"
#include "AutoReimportUtilities.h"
#include "IDirectoryWatcher.h"

/** Custom serialization version for FFileCache */
struct UNREALED_API FFileCacheCustomVersion
{
	static const FGuid Key;
	enum Type {	Initial, IncludeFileHash, Latest = IncludeFileHash };
};

/** Structure representing specific information about a particular file */
struct FFileData
{
	/** Constructiors */
	FFileData() : Timestamp(0) {}
	FFileData(const FDateTime& InTimestamp, const FMD5Hash& InFileHash) : Timestamp(InTimestamp), FileHash(InFileHash) {}

	friend bool operator==(const FFileData& LHS, const FFileData& RHS)
	{
		return LHS.Timestamp == RHS.Timestamp && LHS.FileHash == RHS.FileHash;
	}

	friend bool operator!=(const FFileData& LHS, const FFileData& RHS)
	{
		return LHS.Timestamp != RHS.Timestamp || LHS.FileHash != RHS.FileHash;	
	}

	/** Serializer for this type */
	friend FArchive& operator<<(FArchive& Ar, FFileData& Data)
	{
		if (Ar.CustomVer(FFileCacheCustomVersion::Key) >= FFileCacheCustomVersion::Initial)
		{
			Ar << Data.Timestamp;
		}

		if (Ar.CustomVer(FFileCacheCustomVersion::Key) >= FFileCacheCustomVersion::IncludeFileHash)
		{
			Ar << Data.FileHash;
		}

		return Ar;
	}

	/** The cached timestamp of the file on disk */
	FDateTime Timestamp;

	/** The cached MD5 hash of the file on disk */
	FMD5Hash FileHash;
};

/** Structure representing the file data for a number of files in a directory */
struct FDirectoryState
{
	/** Default construction */
	FDirectoryState() {}

	/** Move construction */
	FDirectoryState(FDirectoryState&& In) : Rules(MoveTemp(In.Rules)), Files(MoveTemp(In.Files)) {}
	FDirectoryState& operator=(FDirectoryState&& In) { Swap(Rules, In.Rules); Swap(Files, In.Files); return *this; }

	/** The rules that define what this state applies to */
	FMatchRules Rules;

	/** Filename -> data map */
	TMap<FImmutableString, FFileData> Files;

	/** Serializer for this type */
	friend FArchive& operator<<(FArchive& Ar, FDirectoryState& State)
	{
		Ar.UsingCustomVersion(FFileCacheCustomVersion::Key);

		// Ignore any old versions to data to ensure that we generate a new cache
		if (Ar.CustomVer(FFileCacheCustomVersion::Key) >= FFileCacheCustomVersion::IncludeFileHash)
		{
			Ar << State.Rules;

			// Number of files
			int32 Num = State.Files.Num();
			Ar << Num;
			if (Ar.IsLoading())
			{
				State.Files.Reserve(Num);
			}

			Ar << State.Files;
		}

		return Ar;
	}
};

enum class EFileAction : uint8
{
	Added, Modified, Removed, Moved
};

/** A transaction issued by FFileCache to describe a change to the cache. The change is only committed once the transaction is returned to the cache (see FFileCache::CompleteTransaction). */
struct FUpdateCacheTransaction
{
	/** The path of the file to which this transaction relates */
	FImmutableString Filename;

	/** In the case of a moved file, this represents the path the file was moved from */
	FImmutableString MovedFromFilename;

	/** File data pertaining to this change at the time of dispatch */
	FFileData FileData;

	/** The type of action that prompted this transaction */
	EFileAction Action;

	/** Publically moveable */
	FUpdateCacheTransaction(FUpdateCacheTransaction&& In) : Filename(MoveTemp(In.Filename)), MovedFromFilename(MoveTemp(In.MovedFromFilename)), FileData(MoveTemp(In.FileData)), Action(MoveTemp(In.Action)) {}
	FUpdateCacheTransaction& operator=(FUpdateCacheTransaction&& In) { Swap(Filename, In.Filename); Swap(MovedFromFilename, In.MovedFromFilename); Swap(FileData, In.FileData); Swap(Action, In.Action); return *this; }

private:
	friend class FFileCache;

	/** Construction responsibility is held by FFileCache */
	FUpdateCacheTransaction(FImmutableString InFilename, EFileAction InAction, const FFileData& InFileData = FFileData())
		: Filename(MoveTemp(InFilename)), FileData(InFileData), Action(InAction)
	{}

	/** Construction responsibility is held by FFileCache */
	FUpdateCacheTransaction(FImmutableString InMovedFromFilename, FImmutableString InMovedToFilename, const FFileData& InFileData)
		: Filename(MoveTemp(InMovedToFilename)), MovedFromFilename(MoveTemp(InMovedFromFilename)), FileData(InFileData), Action(EFileAction::Moved)
	{}
	
	/** Not Copyable */
	FUpdateCacheTransaction(const FUpdateCacheTransaction&) = delete;
	FUpdateCacheTransaction& operator=(const FUpdateCacheTransaction&) = delete;
};

/** Enum specifying whether a path should be relative or absolute */
enum EPathType
{
	/** Paths should be cached relative to the root cache directory */
	Relative,

	/** Paths should be cached as absolute file system paths */
	Absolute
};

struct IAsyncFileCacheTask : public TSharedFromThis<IAsyncFileCacheTask, ESPMode::ThreadSafe>
{
	enum class EProgressResult
	{
		Finished, Pending
	};

	IAsyncFileCacheTask() : StartTime(FPlatformTime::Seconds()) {}
	virtual ~IAsyncFileCacheTask() {}
	
	/** Tick this task. Only to be called on the task thread. */
	virtual EProgressResult Tick(const FTimeLimit& TimeLimit) = 0;

	/** Check whether this task is complete. Must be implemented in a thread-safe manner. */
	virtual bool IsComplete() const = 0;

	/** Get the age of this task in seconds */
	double GetAge() const { return FPlatformTime::Seconds() - StartTime; }

protected:

	/** The time this task started */
	double StartTime;
};

/** Simple struct that encapsulates a filename and its associated MD5 hash */
struct FFilenameAndHash
{
	FString AbsoluteFilename;
	FMD5Hash FileHash;

	FFilenameAndHash(){}
	FFilenameAndHash(const FString& File) : AbsoluteFilename(File) {}
};

/** Async task responsible for MD5 hashing a number of files, reporting completed hashes to the client when done */
struct FAsyncFileHasher : public IAsyncFileCacheTask
{
	/** Constructor */
	FAsyncFileHasher(TArray<FFilenameAndHash> InFilesThatNeedHashing);

	/** Return any completed filenames and their corresponding hashes */
	TArray<FFilenameAndHash> GetCompletedData();

	/** Returns true when this directory reader has finished scanning the directory */
	virtual bool IsComplete() const override;

protected:

	/** Tick this reader (discover new directories / files). Returns progress state. */
	virtual EProgressResult Tick(const FTimeLimit& Limit) override;

	/** The array of data that we will process */
	TArray<FFilenameAndHash> Data;

	/** The number of items we have returned to the client. Only accessed from the main thread. */
	int32 NumReturned;

	/** The number of files that we have hashed on the task thread. Atomic - safe to access from any thread. */
	FThreadSafeCounter CurrentIndex;

	/** Scratch buffer used for reading in files */
	TArray<uint8> ScratchBuffer;
};

/**
 * Class responsible for 'asynchronously' scanning a folder for files and timestamps.
 * Example usage:
 *		FAsyncDirectoryReader Reader(TEXT("C:\\Path"), EPathType::Relative);
 *
 *		while(!Reader.IsComplete())
 *		{
 *			FPlatformProcess::Sleep(1);
 *			Reader.Tick(FTimedSignal(1));	// Do 1 second of work
 *		}
 *		TOptional<FDirectoryState> State = Reader.GetFinalState();
 */
struct FAsyncDirectoryReader : public IAsyncFileCacheTask
{
	/** Constructor that sets up the directory reader to the specified directory */
	FAsyncDirectoryReader(const FString& InDirectory, EPathType InPathType);

	/** Set what files are relevant to this reader. Calling this once the reader starts results in undefined behaviour. */
	void SetMatchRules(const FMatchRules& InRules)
	{
		if (LiveState.IsSet())
		{
			LiveState->Rules = InRules;
		}
	}

	/**
	 * Get the state of the directory once finished. Relinquishes the currently stored directory state to the client.
	 * Returns nothing if incomplete, or if GetLiveState() has already been called.
	 */
	TOptional<FDirectoryState> GetLiveState();

	/** Retrieve the cached state supplied to this class through UseCachedState(). */
	TOptional<FDirectoryState> GetCachedState();

	/** Retrieve the cached state supplied to this class through UseCachedState(). */
	TArray<FFilenameAndHash> GetFilesThatNeedHashing()
	{
		TArray<FFilenameAndHash> Swapped;
		Swap(FilesThatNeedHashing, Swapped);
		return Swapped;
	}

	/** Instruct the directory reader to use the specified cached state to lookup file hashes, where timestamps haven't changed */
	void UseCachedState(FDirectoryState InCachedState)
	{
		CachedState = MoveTemp(InCachedState);
	}

	/** Returns true when this directory reader has finished scanning the directory */
	virtual bool IsComplete() const override;

	/** Tick this reader (discover new directories / files). Returns progress state. */
	virtual EProgressResult Tick(const FTimeLimit& Limit) override;

private:
	/** Non-recursively scan a single directory for its contents. Adds results to Pending arrays. */
	void ScanDirectory(const FString& InDirectory);

	/** Path to the root directory we want to scan */
	FString RootPath;

	/** Whether we should return relative or absolute paths */
	EPathType PathType;

	/** The currently discovered state of the directory - reset once relinquished to the client through GetLiveState */
	TOptional<FDirectoryState> LiveState;

	/** The previously cached  state of the directory, optional */
	TOptional<FDirectoryState> CachedState;

	/** An array of files that need hashing */
	TArray<FFilenameAndHash> FilesThatNeedHashing;

	/** A list of directories we have recursively found on our travels */
	TArray<FString> PendingDirectories;

	/** A list of files we have recursively found on our travels */
	TArray<FString> PendingFiles;

	/** Thread safe flag to signify when this class has finished reading */
	FThreadSafeBool bIsComplete;
};

/** Configuration structure required to construct a FFileCache */
struct FFileCacheConfig
{
	FFileCacheConfig(FString InDirectory, FString InCacheFile)
		: Directory(InDirectory), CacheFile(InCacheFile), PathType(EPathType::Relative), bDetectChangesSinceLastRun(false), bDetectMoves(true)
	{}

	/** String specifying the directory on disk that the cache should reflect */
	FString Directory;

	/** String specifying the file that the cache should be saved to. When empty, no cache file will be maintained (thus only an in-memory cache is used) */
	FString CacheFile;

	/** List of rules which define what we will be watching */
	FMatchRules Rules;

	/** Path type to return, relative to the directory or absolute. */
	EPathType PathType;

	/** When true, changes to the directory since the cache shutdown will be detected and reported. When false, said changes will silently be applied to the serialized cache. */
	bool bDetectChangesSinceLastRun;

	/** True to detect moves and renames (based on file hash), false otherwise. When true, an additional thread will be launched on startup to harvest MD5 hashes.
	 *  Changes that occur *before* hashes have been gatherd will not be detected as moves/renames. */
	bool bDetectMoves;
};

/**
 * A class responsible for scanning a directory, and maintaining a cache of its state (files and timestamps).
 * Changes in the cache can be retrieved through GetOutstandingChanges(). Changes will be reported for any
 * change in the cached state even between runs of the process.
 */
class UNREALED_API FFileCache
{
public:

	/** Construction from a config */
	FFileCache(const FFileCacheConfig& InConfig);

	FFileCache(const FFileCache&) = delete;
	FFileCache& operator=(const FFileCache&) = delete;

	/** Destructor */
	~FFileCache();

	/** Destroy this cache. Cleans out the in-memory state and deletes the cache file, if present. */
	void Destroy();

	/** Get the absolute path of the directory this cache reflects */
	const FString& GetDirectory() const { return Config.Directory; }

	/** Check whether this file cache has finished starting up yet. Does not imply rename/move detection is fully initialized. (see MoveDetectionInitialized()) */
	bool HasStartedUp() const;

	/** Check whether this move/rename detection has been initiated or not. This can take much longer than startup, so can be checked separately */
	bool MoveDetectionInitialized() const;

	/** Attempt to locate file data pertaining to the specified filename.
	 *	@param		InFilename		The filename to find data for - either relative to the directory, or absolute, depending on Config.PathType.
	 *	@return		The current cached file data pertaining to the specified filename, or nullptr. May not be completely up to date if there are outstanding transactions.
	 */
	const FFileData* FindFileData(FImmutableString InFilename) const;

	/** Tick this FileCache */
	void Tick();

	/** Write out the cached file, if we have any changes to write */
	void WriteCache();

	/**
	 * Return a transaction to the cache for completion. Will update the cached state with the change
	 * described in the transaction, and mark the cache as needing to be saved.
	 */
	void CompleteTransaction(FUpdateCacheTransaction&& Transaction);

	/** Report an external change to the manager, such that a subsequent equal change reported by the os be ignored */
	void IgnoreNewFile(const FString& Filename);
	void IgnoreFileModification(const FString& Filename);
	void IgnoreMovedFile(const FString& SrcFilename, const FString& DstFilename);
	void IgnoreDeletedFile(const FString& Filename);

	/** Get the number of pending changes to the cache. */
	int32 GetNumOutstandingChanges() const { return DirtyFiles.Num(); }

	/** Get pending changes to the cache. Transactions must be returned to CompleteTransaction to update the cache. */
	TArray<FUpdateCacheTransaction> FilterOutstandingChanges(const TFunctionRef<bool(const FUpdateCacheTransaction&, const FDateTime&)>& InPredicate);
	TArray<FUpdateCacheTransaction> GetOutstandingChanges();

private:

	/** Called when the directory we are monitoring has been changed in some way */
	void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);

	/** Diff the specified set of dirty files (absolute paths, or relative to the monitor directory), adding transactions to the specified array if necessary.
	 *	Optionally takes a directory state from which we can retrieve current file system state, without having to ask the FS directly.
	 *  Optionally exclude files that have changed since the specified threshold, to ensure that related events get grouped together correctly.
	 */
	void DiffDirtyFiles(TMap<FImmutableString, FDateTime>& InDirtyFiles, TArray<FUpdateCacheTransaction>& OutTransactions, const FDirectoryState* InFileSystemState = nullptr, const FDateTime& ThreasholdTime = FDateTime::UtcNow()) const;

	/** Get the absolute path from a transaction filename */
	FString GetAbsolutePath(const FString& InTransactionPath) const;

	/** Get the transaction path from an absolute filename. Returns nothing if the absolute path is invalid (not under the correct folder, or not applicable) */
	TOptional<FString> GetTransactionPath(const FString& InAbsolutePath) const;

	/** Unbind the watcher from the directory. Called on destruction. */
	void UnbindWatcher();

	/** Read the cache file data and return the contents */
	TOptional<FDirectoryState> ReadCache() const;

	/** Called when the initial async reader has finished harvesting file system timestamps */
	void ReadStateFromAsyncReader();

private:

	/** Configuration settings applied on construction */
	FFileCacheConfig Config;

	/** 'Asynchronous' directory reader responsible for gathering all file/timestamp information recursively from our cache directory */
	TSharedPtr<FAsyncDirectoryReader, ESPMode::ThreadSafe> DirectoryReader;
	TSharedPtr<FAsyncFileHasher, ESPMode::ThreadSafe> AsyncFileHasher;

	/** A map of dirty file times that we will use to report changes to the user */
	TMap<FImmutableString, FDateTime> DirtyFiles;

	/** Our in-memory view of the cached directory state. */
	FDirectoryState CachedDirectoryState;

	/** Handle to the directory watcher delegate so we can delete it properly */
	FDelegateHandle WatcherDelegate;

	/** True when the cached state we have in memory is more up to date than the serialized file. Enables WriteCache() when true. */
	bool bSavedCacheDirty;

	/** The time we last retrieved file hashes from the thread */
	double LastFileHashGetTime;

};
