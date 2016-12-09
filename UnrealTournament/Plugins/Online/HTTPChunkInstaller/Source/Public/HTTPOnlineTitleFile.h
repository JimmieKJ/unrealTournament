// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineTitleFileInterface.h"

const FName NAME_SHA1(TEXT("SHA1"));
const FName NAME_SHA256(TEXT("SHA256"));

class FTitleFileHttpAsyncLoadAndVerify :
	public FNonAbandonableTask
{
public:
	/** File data loaded for the async read */
	TArray<uint8> FileData;
	/** Amount of data read from the file to be owned/referenced by the game thread */
	FThreadSafeCounter64* BytesRead;
	/** The original name of the file being read */
	const FString OriginalFileName;
	/** The name of the file being read off of disk */
	const FString FileName;
	/** The hash value the backend said it should have */
	const FString ExpectedHash;
	/** The hash type SHA1 or SHA256 right now */
	const FName HashType;
	/** Whether the hashes matched */
	bool bHashesMatched;

	/** Initializes the variables needed to load and verify the data */
	FTitleFileHttpAsyncLoadAndVerify(const FString& InOriginalFileName, const FString& InFileName, const FString& InExpectedHash, const FName InHashType, FThreadSafeCounter64* InBytesReadCounter) :
		BytesRead(InBytesReadCounter),
		OriginalFileName(InOriginalFileName),
		FileName(InFileName),
		ExpectedHash(InExpectedHash),
		HashType(InHashType),
		bHashesMatched(false)
	{

	}

	/**
	* Loads and hashes the file data. Empties the data if the hash check fails
	*/
	void DoWork()
	{
		// load file from disk
		bool bLoadedFile = false;

		FArchive* Reader = IFileManager::Get().CreateFileReader(*FileName, FILEREAD_Silent);
		if (Reader)
		{
			int64 SizeToRead = Reader->TotalSize();
			FileData.Reset(SizeToRead);
			FileData.AddUninitialized(SizeToRead);

			uint8* FileDataPtr = FileData.GetData();

			static const int64 ChunkSize = 100 * 1024;

			int64 TotalBytesRead = 0;
			while (SizeToRead > 0)
			{
				int64 Val = FMath::Min(SizeToRead, ChunkSize);
				Reader->Serialize(FileDataPtr + TotalBytesRead, Val);
				BytesRead->Add(Val);
				TotalBytesRead += Val;
				SizeToRead -= Val;
			}

			ensure(SizeToRead == 0 && Reader->TotalSize() == TotalBytesRead);
			bLoadedFile = Reader->Close();
			delete Reader;
		}

		// verify hash of file if it exists
		if (bLoadedFile)
		{
			UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("ReadFile request. Local file read from cache =%s"), *FileName);
			if (HashType == NAME_SHA1)
			{
				bHashesMatched = IsValidSHA1(ExpectedHash, FileData);
			}
			else if (HashType == NAME_SHA256)
			{
				bHashesMatched = IsValidSHA256(ExpectedHash, FileData);
			}
		}
		else
		{
			UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("Local file (%s) not cached locally"), *FileName);
		}
		if (!bHashesMatched)
		{
			// Empty local that was loaded
			FileData.Empty();
		}
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTitleFileHttpAsyncLoadAndVerify, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	/** Validates that a buffer matches the same signature as was specified */
	bool IsValidSHA1(const FString& Hash, const TArray<uint8>& Source) const
	{
		uint8 LocalHash[20];
		FSHA1::HashBuffer(Source.GetData(), Source.Num(), LocalHash);
		// concatenate 20 bye SHA1 hash to string
		FString LocalHashStr;
		for (int Idx = 0; Idx < 20; Idx++)
		{
			LocalHashStr += FString::Printf(TEXT("%02x"), LocalHash[Idx]);
		}
		return Hash == LocalHashStr;
	}
	bool IsValidSHA256(const FString& Hash, const TArray<uint8>& Source) const
	{
		FSHA256Signature Signature;
		if (FPlatformMisc::GetSHA256Signature(Source.GetData(), Source.Num(), Signature))
		{
			return Signature.ToString() == Hash;
		}
		return false;
	}
};


// helper class for an HTTP Online Title File
class FOnlineTitleFileHttp : public IOnlineTitleFile
{
public:
	/**
	* Constructor
	*
	* @param InSubsystem mcp subsystem being used
	*/
	FOnlineTitleFileHttp(const FString& InBaseUrl, const FString& InMcpConfig);

	/**
	* Destructor
	*/
	virtual ~FOnlineTitleFileHttp()
	{}

	/**
	* Copies the file data into the specified buffer for the specified file
	*
	* @param FileName the name of the file to read
	* @param FileContents the out buffer to copy the data into
	*
	* @return true if the data was copied, false otherwise
	*/
	virtual bool GetFileContents(const FString& FileName, TArray<uint8>& FileContents) override;

	/**
	* Empties the set of downloaded files if possible (no async tasks outstanding)
	*
	* @return true if they could be deleted, false if they could not
	*/
	virtual bool ClearFiles() override;

	/**
	* Empties the cached data for this file if it is not being downloaded currently
	*
	* @param FileName the name of the file to remove from the cache
	*
	* @return true if it could be deleted, false if it could not
	*/
	virtual bool ClearFile(const FString& FileName) override;

	/**
	* Delete cached files on disk
	*
	* @param bSkipEnumerated if true then only non-enumerated files are deleted
	*/
	virtual void DeleteCachedFiles(bool bSkipEnumerated) override;

	static FString GetManifestCacheLocation()
	{
		return FString::Printf(TEXT("%s/master.manifest"), *FPaths::GameSavedDir());
	}

	/**
	* Requests a list of available files from the network store
	*
	* @return true if the request has started, false if not
	*/
	virtual bool EnumerateFiles(const FPagedQuery& Page = FPagedQuery()) override;

	/**
	* Returns the list of files that was returned by the network store
	*
	* @param Files out array of file metadata
	*
	*/
	virtual void GetFileList(TArray<FCloudFileHeader>& OutFiles) override;

	/**
	* Starts an asynchronous read of the specified file from the network platform's file store
	*
	* @param FileToRead the name of the file to read
	*
	* @return true if the calls starts successfully, false otherwise
	*/
	virtual bool ReadFile(const FString& FileName) override;

	/** Used to check that async tasks have completed and can be completed */
	virtual void Tick(float DeltaTime);

	void Shutdown()
	{

	}

	FText GetErrorMsg() const {
		return ErrorMsg;
	}

private:

	/** Reads the file from the local cache if it can. This is async */
	bool StartReadFileLocal(const FString& FileName);

	/** Completes the async operation of the local file read */
	void FinishReadFileLocal(FTitleFileHttpAsyncLoadAndVerify& AsyncLoad);

	/** Requests the file from MCP. This is async */
	bool ReadFileRemote(const FString& FileName);

	/**
	* Delegate called when a Http request completes for enumerating list of file headers
	*/
	void EnumerateFiles_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/**
	* Delegate called when a Http request completes for reading a cloud file
	*/
	void ReadFile_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/**
	* Delegate called as a Http request progresses for reading a cloud file
	*/
	void ReadFile_HttpRequestProgress(FHttpRequestPtr HttpRequest, int32 BytesSent, int32 BytesReceived);

	/**
	* Find/create cloud file entry
	*
	* @param FileName cached file entry to find
	* @param bCreateIfMissing create the file entry if not found
	*
	* @return cached cloud file entry
	*/
	FCloudFile* GetCloudFile(const FString& FileName, bool bCreateIfMissing);

	/**
	* Find cloud file header entry
	*
	* @param FileName cached file entry to find
	*
	* @return cached cloud file header entry
	*/
	FCloudFileHeader* GetCloudFileHeader(const FString& FileName);

	/**
	* Converts filename into a local file cache path
	*
	* @param FileName name of file being loaded
	*
	* @return unreal file path to be used by file manager
	*/
	FString GetLocalFilePath(const FString& FileName)
	{
		return GetLocalCachePath() + FileName;
	}

	/**
	* @return full path to cache directory
	*/
	FString GetLocalCachePath()
	{
		return FPaths::GamePersistentDownloadDir() / TEXT("EMS/");
	}

	/**
	* Save a file from a given user to disk
	*
	* @param FileName name of file being saved
	* @param Data data to write to disk
	*/
	void SaveCloudFileToDisk(const FString& Filename, const TArray<uint8>& Data);

	/**
	* Should use the initialization constructor instead
	*/
	FOnlineTitleFileHttp();

	/** Config based url for accessing the HTTP server */
	FString BaseUrl;

	/** Config based url for enumerating list of cloud files*/
	FString EnumerateFilesUrl;

	/** Extended error message */
	FText ErrorMsg;

	/** Manifest file to be read from */
	FString Manifest;

	FString McpProtocol;
	FString McpDomain;
	FString McpService;

	FString BuildVersion;

public:
	FString GetBaseUrl()
	{
		return BaseUrl + TEXT("/") + BuildVersion + TEXT("/");
	}

private:
	/** List of pending Http requests for enumerating files */
	TMap<FHttpRequestPtr, FPagedQuery> EnumerateFilesRequests;

	/** Info used to send request for a file */
	struct FPendingFileRequest
	{
		/**
		* Constructor
		*/
		FPendingFileRequest(const FString& InFileName = FString(TEXT("")))
			: FileName(InFileName)
		{

		}

		/**
		* Equality op
		*/
		inline bool operator==(const FPendingFileRequest& Other) const
		{
			return FileName == Other.FileName;
		}

		/** File being operated on by the pending request */
		FString FileName;
	};
	/** List of pending Http requests for reading files */
	TMap<FHttpRequestPtr, FPendingFileRequest> FileRequests;
	TMap<FHttpRequestPtr, FPendingFileRequest> FileProgressRequestsMap;

	TArray<FCloudFileHeader> FileHeaders;
	TArray<FCloudFile> Files;
	bool bCacheFiles;
	bool bPlatformSupportsSHA256;

	/** Information about local file reads that are in progress */
	struct FTitleAsyncReadData
	{
		/** Name of the file being loaded */
		FString Filename;
		/** Amount of data that has been loaded on the async thread so far */
		FThreadSafeCounter64 BytesRead;
		/** Bytes read last time game thread noticed */
		int64 LastBytesRead;
		/** Async tasks doing the work */
		FAsyncTask<FTitleFileHttpAsyncLoadAndVerify>*  AsyncTask;

		FTitleAsyncReadData() :
			LastBytesRead(0),
			AsyncTask(nullptr)
		{ }

		bool operator==(const FTitleAsyncReadData& Other) const
		{
			return Filename == Other.Filename && AsyncTask == Other.AsyncTask;
		}
	};
	/** Holds the outstanding tasks for hitch free loading and hash calculation */
	TIndirectArray<FTitleAsyncReadData> AsyncLocalReads;
};
