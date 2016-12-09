// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HTTPChunkInstallerPrivatePCH.h"
#include "OnlineSubsystem.h"
#include "HTTPOnlineTitleFile.h"


#if PLATFORM_ANDROID
#include "OpenGLDrv.h"
#endif

#define LOCTEXT_NAMESPACE "HTTPChunkInstaller"

#define USE_MCP 0

FOnlineTitleFileHttp::FOnlineTitleFileHttp(const FString& InBaseUrl, const FString& InMcpConfig)
	: BaseUrl(InBaseUrl)
	, EnumerateFilesUrl(TEXT(""))
	, ErrorMsg()
	, Manifest(TEXT("Master.manifest"))
	, McpProtocol(TEXT("https"))
	, McpDomain(TEXT("localhost"))
	, McpService(TEXT("wex"))
	, BuildVersion(TEXT("CL_0"))
{
	FString Section = FString::Printf(TEXT("OnlineSubsystemMcp.GameServiceMcp %s"), *InMcpConfig);
	GConfig->GetString(TEXT("HTTPOnlineTitleFile"), TEXT("ManifestName"), Manifest, GEngineIni);
	GConfig->GetString(TEXT("HTTPOnlineTitleFile"), TEXT("EnumerateFilesUrl"), EnumerateFilesUrl, GEngineIni);
	GConfig->GetString(*Section, TEXT("Protocol"), McpProtocol, GEngineIni);
	GConfig->GetString(*Section, TEXT("Domain"), McpDomain, GEngineIni);
	GConfig->GetString(TEXT("OnlineSubsystemMcp.GameServiceMcp"), TEXT("ServiceName"), McpService, GEngineIni);
	bCacheFiles = true;
	bPlatformSupportsSHA256 = false;
	BuildVersion = TEXT("CL_") + FString::Printf(TEXT("%u"), FEngineVersion::Current().GetChangelist());

	// remove this line before submission
//	BaseUrl = TEXT("https://battlebreakers-gamedev-cdn.s3.amazonaws.com/WorldExplorersDevTesting");
#if PLATFORM_ANDROID
	if (FOpenGLES2::SupportsASTC())
	{
		EnumerateFilesUrl = EnumerateFilesUrl.Replace(TEXT("^FORMAT"), TEXT("ASTC"));
		Manifest = Manifest.Replace(TEXT("^FORMAT"), TEXT("astc"));
	}
	else if (FOpenGLES2::SupportsATITC())
	{
		EnumerateFilesUrl = EnumerateFilesUrl.Replace(TEXT("^FORMAT"), TEXT("ATC"));
		Manifest = Manifest.Replace(TEXT("^FORMAT"), TEXT("atc"));
	}
	else if (FOpenGLES2::SupportsDXT())
	{
		EnumerateFilesUrl = EnumerateFilesUrl.Replace(TEXT("^FORMAT"), TEXT("DXT"));
		Manifest = Manifest.Replace(TEXT("^FORMAT"), TEXT("dxt"));
	}
	else if (FOpenGLES2::SupportsPVRTC())
	{
		EnumerateFilesUrl = EnumerateFilesUrl.Replace(TEXT("^FORMAT"), TEXT("PVRTC"));
		Manifest = Manifest.Replace(TEXT("^FORMAT"), TEXT("pvrtc"));
	}
	else if (FOpenGLES2::SupportsETC2())
	{
		EnumerateFilesUrl = EnumerateFilesUrl.Replace(TEXT("^FORMAT"), TEXT("ETC2"));
		Manifest = Manifest.Replace(TEXT("^FORMAT"), TEXT("etc2"));
	}
	else
	{
		EnumerateFilesUrl = EnumerateFilesUrl.Replace(TEXT("^FORMAT"), TEXT("ETC1"));
		Manifest = Manifest.Replace(TEXT("^FORMAT"), TEXT("etc1"));
	}
#endif
}

bool FOnlineTitleFileHttp::GetFileContents(const FString& FileName, TArray<uint8>& FileContents)
{
	const TArray<FCloudFile>* FilesPtr = &Files;
	if (FilesPtr != NULL)
	{
		for (TArray<FCloudFile>::TConstIterator It(*FilesPtr); It; ++It)
		{
			if (It->FileName == FileName)
			{
				FileContents = It->Data;
				return true;
			}
		}
	}
	return false;
}

bool FOnlineTitleFileHttp::ClearFiles()
{
	for (int Idx = 0; Idx < Files.Num(); Idx++)
	{
		if (Files[Idx].AsyncState == EOnlineAsyncTaskState::InProgress)
		{
			UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("Cant clear files. Pending file op for %s"), *Files[Idx].FileName);
			return false;
		}
	}
	// remove all cached file entries
	Files.Empty();
	return true;
}

bool FOnlineTitleFileHttp::ClearFile(const FString& FileName)
{
	for (int Idx = 0; Idx < Files.Num(); Idx++)
	{
		if (Files[Idx].FileName == FileName)
		{
			if (Files[Idx].AsyncState == EOnlineAsyncTaskState::InProgress)
			{
				UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("Cant clear file. Pending file op for %s"), *Files[Idx].FileName);
				return false;
			}
			else
			{
				Files.RemoveAt(Idx);
				return true;
			}
		}
	}
	return false;
}

void FOnlineTitleFileHttp::DeleteCachedFiles(bool bSkipEnumerated)
{
	TArray<FString> CachedFiles;
	IFileManager::Get().FindFiles(CachedFiles, *(GetLocalCachePath() / TEXT("*")), true, false);

	for (auto CachedFile : CachedFiles)
	{
		bool bSkip = bSkipEnumerated && GetCloudFileHeader(CachedFile);
		if (!bSkip)
		{
			IFileManager::Get().Delete(*GetLocalFilePath(CachedFile), false, true);
		}
	}
}

bool FOnlineTitleFileHttp::EnumerateFiles(const FPagedQuery& Page)
{
	FString ErrorStr;
	bool bStarted = false;

	// Make sure an enumeration request  is not currently pending
	if (EnumerateFilesRequests.Num() > 0)
	{
		ErrorStr = TEXT("Request already in progress.");
	}
	else
	{
		// Create the Http request and add to pending request list
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		EnumerateFilesRequests.Add(HttpRequest, Page);

#if USE_MCP
		FString EnumerateUrl = McpProtocol + TEXT("://") + McpDomain + (McpService.IsEmpty() ? TEXT("/") : (TEXT("/") + McpService + TEXT("/"))) + TEXT("api/game/v2/manifests/") + Manifest;
		TSharedPtr< TArray<uint8> > CachedManifestFile = MakeShareable(new TArray<uint8>());
		if (FFileHelper::LoadFileToArray(*CachedManifestFile, *GetManifestCacheLocation()))
		{
			// compute etag
			FMD5 Hasher;
			Hasher.Update(CachedManifestFile->GetData(), CachedManifestFile->Num());
			uint8 Checksum[16];
			Hasher.Final(Checksum);

			// set it on the request
			FString ETag = FString::Printf(TEXT("\"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\""),
				Checksum[0], Checksum[1], Checksum[2], Checksum[3], Checksum[4], Checksum[5], Checksum[6], Checksum[7],
				Checksum[8], Checksum[9], Checksum[10], Checksum[11], Checksum[12], Checksum[13], Checksum[14], Checksum[15]);
			HttpRequest->SetHeader(TEXT("If-None-Match"), ETag);
		}
		else
		{
			// nothing cached right now
			CachedManifestFile.Reset();
		}
#else
		FString EnumerateUrl = GetBaseUrl() + EnumerateFilesUrl + TEXT("/") + Manifest;
#endif
		HttpRequest->SetURL(EnumerateUrl);
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->OnProcessRequestComplete().BindRaw(this, &FOnlineTitleFileHttp::EnumerateFiles_HttpRequestComplete);
		bStarted = HttpRequest->ProcessRequest();
	}
	if (!bStarted)
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("EnumerateFiles request failed. %s"), *ErrorStr);
		ErrorMsg = LOCTEXT("HttpResponse", "Enumerate Files request failed.");
		TriggerOnEnumerateFilesCompleteDelegates(false);
	}
	return bStarted;
}

void FOnlineTitleFileHttp::GetFileList(TArray<FCloudFileHeader>& OutFiles)
{
	TArray<FCloudFileHeader>* FilesPtr = &FileHeaders;
	if (FilesPtr != NULL)
	{
		OutFiles = *FilesPtr;
	}
	else
	{
		OutFiles.Empty();
	}
}

bool FOnlineTitleFileHttp::ReadFile(const FString& FileName)
{
	bool bStarted = false;

	FCloudFileHeader* CloudFileHeader = GetCloudFileHeader(FileName);

	// Make sure valid filename was specified3
	if (FileName.IsEmpty() || FileName.Contains(TEXT(" ")))
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("Invalid filename filename=%s"), *FileName);
		ErrorMsg = FText::Format(LOCTEXT("ReadFiles", "File not found: %s"), FText::FromString(FileName));
		TriggerOnReadFileCompleteDelegates(false, FileName);
		return false;
	}

	// Make sure a file request for this file is not currently pending
	for (const auto& Pair : FileRequests)
	{
		if (Pair.Value == FPendingFileRequest(FileName))
		{
			UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("ReadFileRemote is already in progress for (%s)"), *FileName);
			return true;
		}
	}

	FCloudFile* CloudFile = GetCloudFile(FileName, true);
	if (CloudFile->AsyncState == EOnlineAsyncTaskState::InProgress)
	{
		UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("ReadFile is already in progress for (%s)"), *FileName);
		return true;
	}

	if (bCacheFiles)
	{
		// Try to read this from the cache if possible
		bStarted = StartReadFileLocal(FileName);
	}
	if (!bStarted)
	{
		// Failed locally (means not on disk) so fetch from server
		bStarted = ReadFileRemote(FileName);
	}

	if (!bStarted || CloudFile->AsyncState == EOnlineAsyncTaskState::Failed)
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("ReadFile request failed for file (%s)"), *FileName);
		ErrorMsg = FText::Format(LOCTEXT("ReadFiles", "Read request failed for file (%s)"), FText::FromString(FileName));
		TriggerOnReadFileCompleteDelegates(false, FileName);
	}
	else if (CloudFile->AsyncState == EOnlineAsyncTaskState::Done)
	{
		ErrorMsg = FText::GetEmpty();
		TriggerOnReadFileCompleteDelegates(true, FileName);
	}
	return bStarted;
}

void FOnlineTitleFileHttp::Tick(float DeltaTime)
{
	TArray<int32> ItemsToRemove;
	ItemsToRemove.Reserve(AsyncLocalReads.Num());

	// Check for any completed tasks
	for (int32 TaskIdx = 0; TaskIdx < AsyncLocalReads.Num(); TaskIdx++)
	{
		FTitleAsyncReadData& Task = AsyncLocalReads[TaskIdx];
		if (Task.AsyncTask->IsDone())
		{
			FinishReadFileLocal(Task.AsyncTask->GetTask());
			ItemsToRemove.Add(TaskIdx);
			UE_LOG(LogHTTPChunkInstaller, VeryVerbose, TEXT("Title Task Complete: %s"), *Task.Filename);
		}
		else
		{
			const int64 NewValue = Task.BytesRead.GetValue();
			if (NewValue != Task.LastBytesRead)
			{
				Task.LastBytesRead = NewValue;
				TriggerOnReadFileProgressDelegates(Task.Filename, NewValue);
			}
		}
	}

	// Clean up any tasks that were completed
	for (int32 ItemIdx = ItemsToRemove.Num() - 1; ItemIdx >= 0; ItemIdx--)
	{
		const int32 TaskIdx = ItemsToRemove[ItemIdx];
		FTitleAsyncReadData& TaskToDelete = AsyncLocalReads[TaskIdx];
		UE_LOG(LogHTTPChunkInstaller, VeryVerbose, TEXT("Title Task Removal: %s read: %d"), *TaskToDelete.Filename, TaskToDelete.BytesRead.GetValue());
		delete TaskToDelete.AsyncTask;
		AsyncLocalReads.RemoveAtSwap(TaskIdx);
	}
}

bool FOnlineTitleFileHttp::StartReadFileLocal(const FString& FileName)
{
	UE_LOG(LogHTTPChunkInstaller, VeryVerbose, TEXT("StartReadFile %s"), *FileName);
	bool bStarted = false;
	FCloudFileHeader* CloudFileHeader = GetCloudFileHeader(FileName);
	if (CloudFileHeader != nullptr)
	{
		// Mark file entry as in progress
		FCloudFile* CloudFile = GetCloudFile(FileName, true);
		CloudFile->AsyncState = EOnlineAsyncTaskState::InProgress;
		if (CloudFileHeader->Hash.IsEmpty())
		{
			UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("Requested file (%s) is missing a hash, so can't be verified"), *FileName);
		}
		FTitleAsyncReadData* NewItem = new FTitleAsyncReadData();
		NewItem->Filename = FileName;

		// Create the async task and start it
		NewItem->AsyncTask = new FAsyncTask<FTitleFileHttpAsyncLoadAndVerify>(FileName,
			GetLocalFilePath(FileName), CloudFileHeader->Hash, CloudFileHeader->HashType, &NewItem->BytesRead);

		AsyncLocalReads.Add(NewItem);
		NewItem->AsyncTask->StartBackgroundTask();
		bStarted = true;
	}
	return bStarted;
}

void FOnlineTitleFileHttp::FinishReadFileLocal(FTitleFileHttpAsyncLoadAndVerify& AsyncLoad)
{
	UE_LOG(LogHTTPChunkInstaller, VeryVerbose, TEXT("FinishReadFileLocal %s"), *AsyncLoad.OriginalFileName);
	FCloudFileHeader* CloudFileHeader = GetCloudFileHeader(AsyncLoad.OriginalFileName);
	FCloudFile* CloudFile = GetCloudFile(AsyncLoad.OriginalFileName, true);
	if (CloudFileHeader != nullptr && CloudFile != nullptr)
	{
		// if hash matches then just use the local file
		if (AsyncLoad.bHashesMatched)
		{
			UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("Local file hash matches cloud header. No need to download for filename=%s"), *AsyncLoad.OriginalFileName);
			CloudFile->Data = AsyncLoad.FileData;
			CloudFile->AsyncState = EOnlineAsyncTaskState::Done;
			ErrorMsg = FText::GetEmpty();
			TriggerOnReadFileProgressDelegates(AsyncLoad.OriginalFileName, static_cast<uint64>(AsyncLoad.BytesRead->GetValue()));
			TriggerOnReadFileCompleteDelegates(true, AsyncLoad.OriginalFileName);
		}
		else
		{
			// Request it from server
			ReadFileRemote(AsyncLoad.OriginalFileName);
		}
	}
	else
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("ReadFile request failed for file (%s)"), *AsyncLoad.OriginalFileName);
		ErrorMsg = FText::Format(LOCTEXT("ReadFiles", "Read request failed for file (%s)"), FText::FromString(AsyncLoad.OriginalFileName));
		TriggerOnReadFileCompleteDelegates(false, AsyncLoad.OriginalFileName);
	}
}

bool FOnlineTitleFileHttp::ReadFileRemote(const FString& FileName)
{
	UE_LOG(LogHTTPChunkInstaller, VeryVerbose, TEXT("ReadFileRemote %s"), *FileName);

	bool bStarted = false;
	FCloudFileHeader* CloudFileHeader = GetCloudFileHeader(FileName);
	if (CloudFileHeader != nullptr)
	{
		FCloudFile* CloudFile = GetCloudFile(FileName, true);
		CloudFile->Data.Empty();
		CloudFile->AsyncState = EOnlineAsyncTaskState::InProgress;

		// Create the Http request and add to pending request list
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		FileRequests.Add(HttpRequest, FPendingFileRequest(FileName));
		FileProgressRequestsMap.Add(HttpRequest, FPendingFileRequest(FileName));

		HttpRequest->OnProcessRequestComplete().BindRaw(this, &FOnlineTitleFileHttp::ReadFile_HttpRequestComplete);
		HttpRequest->OnRequestProgress().BindRaw(this, &FOnlineTitleFileHttp::ReadFile_HttpRequestProgress);
		FString RequestUrl;
		// Grab the file from the specified URL if that was set, otherwise use the old method that hits the game service
		if (CloudFileHeader != nullptr && !CloudFileHeader->URL.IsEmpty())
		{
			RequestUrl = CloudFileHeader->URL;
		}
		else
		{
			RequestUrl = GetBaseUrl() + FileName;
		}
		HttpRequest->SetURL(RequestUrl);
		HttpRequest->SetVerb(TEXT("GET"));
		bStarted = HttpRequest->ProcessRequest();

		if (!bStarted)
		{
			UE_LOG(LogHTTPChunkInstaller, Error, TEXT("Unable to start the HTTP request to fetch file (%s)"), *FileName);
		}
	}
	else
	{
		UE_LOG(LogHTTPChunkInstaller, Error, TEXT("No cloud file header entry for filename=%s."), *FileName);
	}
	return bStarted;
}

void FOnlineTitleFileHttp::EnumerateFiles_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	const FPagedQuery& PendingOp = EnumerateFilesRequests.FindRef(HttpRequest);
	EnumerateFilesRequests.Remove(HttpRequest);

	bool bResult = false;
	FString ResponseStr;

	if (HttpResponse.IsValid() && (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()) || HttpResponse->GetResponseCode() == EHttpResponseCodes::NotModified))
	{
#if USE_MCP
		TArray<uint8> Content;
		if (HttpResponse->GetResponseCode() == EHttpResponseCodes::NotModified)
		{
			FFileHelper::LoadFileToArray(Content, *GetManifestCacheLocation());
		}
		else
		{
			if (HttpResponse->GetContentType() == TEXT("text/text"))
			{
				// use the response from the server
				Content = HttpResponse->GetContent();

				// save the response to a file for next time
				if (!FFileHelper::SaveArrayToFile(Content, *GetManifestCacheLocation()))
				{
					UE_LOG(LogHTTPChunkInstaller, Error, TEXT("Unable to save to %s"), *GetManifestCacheLocation());
				}

			}
		}
		// convert to string
		FFileHelper::BufferToString(ResponseStr, Content.GetData(), Content.Num());
#else
		ResponseStr = HttpResponse->GetContentAsString();
#endif
		UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("EnumerateFiles request complete. url=%s code=%d response=%s"),
			*HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *ResponseStr);

		if (PendingOp.Start == 0)
		{
			FileHeaders.Empty();
		}

		// parse the html for the file list
		if (ResponseStr.StartsWith(TEXT("<!DOCTYPE")))
		{
			TArray<FString> Lines;
			ResponseStr.ParseIntoArrayLines(Lines);
			for (int Index = 0; Index < Lines.Num(); ++Index)
			{
				if (Lines[Index].StartsWith(TEXT("<li>")))
				{
					TArray<FString> Elements;
					Lines[Index].ParseIntoArray(Elements, TEXT(">"));
					if (!Elements[2].StartsWith(TEXT("Chunks")))
					{
						FString File = Elements[2].Replace(TEXT("</a"), TEXT(""));
						FCloudFileHeader FileHeader;
						FileHeader.DLName = File;
						FileHeader.FileName = File;
						FileHeader.URL = GetBaseUrl() + File;
						FileHeader.Hash.Empty();
						FileHeader.FileSize = 0;
						FileHeaders.Add(FileHeader);
					}
				}
			}
			bResult = true;
		}
		else
		{
			// Create the Json parser
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(ResponseStr);

			if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
				JsonObject.IsValid())
			{
				// get the data location, just the same as the build version
				BuildVersion = JsonObject->GetStringField(TEXT("BuildUrl"));

				// Parse the array of file headers
				TArray<TSharedPtr<FJsonValue> > JsonFileHeaders = JsonObject->GetArrayField(TEXT("files"));
				for (TArray<TSharedPtr<FJsonValue> >::TConstIterator It(JsonFileHeaders); It; ++It)
				{
					TSharedPtr<FJsonObject> JsonFileHeader = (*It)->AsObject();
					if (JsonFileHeader.IsValid())
					{
						FCloudFileHeader FileHeader;
						if (JsonFileHeader->HasField(TEXT("hash")))
						{
							FileHeader.Hash = JsonFileHeader->GetStringField(TEXT("hash"));
							FileHeader.HashType = FileHeader.Hash.IsEmpty() ? NAME_None : NAME_SHA1;
						}
						// This one takes priority over the old SHA1 hash if present (requires platform support)
						if (bPlatformSupportsSHA256 && JsonFileHeader->HasField(TEXT("hash256")))
						{
							FString Hash256 = JsonFileHeader->GetStringField(TEXT("hash256"));
							if (!Hash256.IsEmpty())
							{
								FileHeader.Hash = Hash256;
								FileHeader.HashType = FileHeader.Hash.IsEmpty() ? NAME_None : NAME_SHA256;
							}
						}
						if (JsonFileHeader->HasField(TEXT("uniqueFilename")))
						{
							FileHeader.DLName = JsonFileHeader->GetStringField(TEXT("uniqueFilename"));
						}
						if (JsonFileHeader->HasField(TEXT("filename")))
						{
							FileHeader.FileName = JsonFileHeader->GetStringField(TEXT("filename"));
						}
						if (JsonFileHeader->HasField(TEXT("length")))
						{
							FileHeader.FileSize = FMath::TruncToInt(JsonFileHeader->GetNumberField(TEXT("length")));
						}
						if (JsonFileHeader->HasField(TEXT("URL")))
						{
							FileHeader.URL = GetBaseUrl() + JsonFileHeader->GetStringField(TEXT("URL"));
						}

						if (FileHeader.FileName.IsEmpty())
						{
							FileHeader.FileName = FileHeader.DLName;
						}

						if (FileHeader.Hash.IsEmpty() ||
							(FileHeader.DLName.IsEmpty() && FileHeader.URL.IsEmpty()) ||
							FileHeader.HashType == NAME_None)
						{
							UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("Invalid file entry hash=%s hashType=%s dlname=%s filename=%s URL=%s"),
								*FileHeader.Hash,
								*FileHeader.HashType.ToString(),
								*FileHeader.DLName,
								*FileHeader.FileName,
								*FileHeader.URL);
						}
						else
						{
							int32 FoundIdx = INDEX_NONE;
							for (int32 Idx = 0; Idx < FileHeaders.Num(); Idx++)
							{
								const FCloudFileHeader& ExistingFile = FileHeaders[Idx];
								if (ExistingFile.DLName == FileHeader.DLName)
								{
									FoundIdx = Idx;
									break;
								}
							}
							if (FoundIdx != INDEX_NONE)
							{
								FileHeaders[FoundIdx] = FileHeader;
							}
							else
							{
								FileHeaders.Add(FileHeader);
							}
						}
					}
				}
			}
			bResult = true;
			ErrorMsg = FText::GetEmpty();
		}
	}
	else
	{
		if (HttpResponse.IsValid())
		{
			switch (HttpResponse->GetResponseCode())
			{
			case 403:
			case 404:
				ErrorMsg = LOCTEXT("MissingFile", "File not found.");
				break;

			default:
				ErrorMsg = FText::Format(LOCTEXT("HttpResponse", "HTTP {0} response"),
					FText::AsNumber(HttpResponse->GetResponseCode()));
				break;
			}
		}
		else
		{
			ErrorMsg = LOCTEXT("ConnectionFailure", "Connection to the server failed.");
		}
	}

	if (!ErrorMsg.IsEmpty())
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("EnumerateFiles request failed. %s"), *ErrorMsg.ToString());
	}
	else
	{
		// Everything went ok, so we can remove any cached files that are not in the current list
		DeleteCachedFiles(true);
	}

	TriggerOnEnumerateFilesCompleteDelegates(bResult);
}

void FOnlineTitleFileHttp::ReadFile_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	bool bResult = false;
	FString ResponseStr;

	// should have a pending Http request
	FPendingFileRequest PendingRequest = FileRequests.FindChecked(HttpRequest);
	FileRequests.Remove(HttpRequest);
	// remove from progress updates
	FileProgressRequestsMap.Remove(HttpRequest);
	HttpRequest->OnRequestProgress().Unbind();

	// Cloud file being operated on
	FCloudFile* CloudFile = GetCloudFile(PendingRequest.FileName, true);
	CloudFile->AsyncState = EOnlineAsyncTaskState::Failed;
	CloudFile->Data.Empty();

	if (HttpResponse.IsValid() && EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("ReadFile request complete. url=%s code=%d"),
			*HttpRequest->GetURL(), HttpResponse->GetResponseCode());

		// update the memory copy of the file with data that was just downloaded
		CloudFile->AsyncState = EOnlineAsyncTaskState::Done;
		CloudFile->Data = HttpResponse->GetContent();

		if (bCacheFiles)
		{
			// cache to disk on successful download
			SaveCloudFileToDisk(CloudFile->FileName, CloudFile->Data);
		}

		bResult = true;
		ErrorMsg = FText::GetEmpty();
	}
	else
	{
		if (HttpResponse.IsValid())
		{
			ErrorMsg = FText::Format(LOCTEXT("HttpResponse", "HTTP {0} response from {1}"),
				FText::AsNumber(HttpResponse->GetResponseCode()),
				FText::FromString(HttpResponse->GetURL()));
		}
		else
		{
			ErrorMsg = FText::Format(LOCTEXT("HttpResponse", "Connection to {0} failed"), FText::FromString(HttpRequest->GetURL()));
		}
	}

	if (!ErrorMsg.IsEmpty())
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("EnumerateFiles request failed. %s"), *ErrorMsg.ToString());
	}
	TriggerOnReadFileCompleteDelegates(bResult, PendingRequest.FileName);
}

void FOnlineTitleFileHttp::ReadFile_HttpRequestProgress(FHttpRequestPtr HttpRequest, int32 BytesSent, int32 BytesReceived)
{
	FPendingFileRequest PendingRequest = FileProgressRequestsMap.FindChecked(HttpRequest);
	// Just forward this to anyone that is listening
	TriggerOnReadFileProgressDelegates(PendingRequest.FileName, BytesReceived);
}

FCloudFile* FOnlineTitleFileHttp::GetCloudFile(const FString& FileName, bool bCreateIfMissing)
{
	FCloudFile* CloudFile = NULL;
	for (int Idx = 0; Idx < Files.Num(); Idx++)
	{
		if (Files[Idx].FileName == FileName)
		{
			CloudFile = &Files[Idx];
			break;
		}
	}
	if (CloudFile == NULL && bCreateIfMissing)
	{
		CloudFile = new(Files)FCloudFile(FileName);
	}
	return CloudFile;
}

FCloudFileHeader* FOnlineTitleFileHttp::GetCloudFileHeader(const FString& FileName)
{
	FCloudFileHeader* CloudFileHeader = NULL;

	for (int Idx = 0; Idx < FileHeaders.Num(); Idx++)
	{
		if (FileHeaders[Idx].DLName == FileName)
		{
			CloudFileHeader = &FileHeaders[Idx];
			break;
		}
	}

	return CloudFileHeader;
}

void FOnlineTitleFileHttp::SaveCloudFileToDisk(const FString& Filename, const TArray<uint8>& Data)
{
	// save local disk copy as well
	FString LocalFilePath = GetLocalFilePath(Filename);
	bool bSavedLocal = FFileHelper::SaveArrayToFile(Data, *LocalFilePath);
	if (bSavedLocal)
	{
		UE_LOG(LogHTTPChunkInstaller, Verbose, TEXT("WriteUserFile request complete. Local file cache updated =%s"),
			*LocalFilePath);
	}
	else
	{
		UE_LOG(LogHTTPChunkInstaller, Warning, TEXT("WriteUserFile request complete. Local file cache failed to update =%s"),
			*LocalFilePath);
	}
}

#undef LOCTEXT_NAMESPACE