// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "Json.h"

#include "MeshUtilities.h"

#include "Runtime/Online/HTTP/Public/Interfaces/IHttpResponse.h"
#include "Runtime/Online/HTTP/Public/Interfaces/IHttpRequest.h"
#include "Runtime/Online/HTTP/Public/HttpModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimplygonRESTClient, Verbose, All);

enum SimplygonRESTState
{
	SRS_UNKNOWN,
	SRS_FAILED,
	SRS_ASSETUPLOADED_PENDING,
	SRS_ASSETUPLOADED,
	SRS_JOBCREATED_PENDING,
	SRS_JOBCREATED,
	SRS_JOBSETTINGSUPLOADED_PENDING,
	SRS_JOBSETTINGSUPLOADED,
	SRS_JOBPROCESSING_PENDING,
	SRS_JOBPROCESSING,
	SRS_JOBPROCESSED,
	SRS_ASSETDOWNLOADED_PENDING,
	SRS_ASSETDOWNLOADED
};

//Probabaly should use a multi-cast delegate
struct FTaskData
{
	FString ZipFilePath;
	FString SplFilePath;
	FString OutputZipFilePath;
	FString JobDirectory;
	FCriticalSection* StateLock;
	FGuid ProcessorJobID;
	bool bDitheredTransition;
};

class FSimplygonSwarmTask
{
public:
	DECLARE_DELEGATE_OneParam(FSimplygonSwarmTaskDelegate, const FSimplygonSwarmTask&);
	FSimplygonSwarmTask(const FTaskData& InTaskData);
	~FSimplygonSwarmTask();
	SimplygonRESTState GetState() const;
	void SetState(SimplygonRESTState InState);
	bool IsFinished() const;
	FSimplygonSwarmTaskDelegate& OnAssetDownloaded() { return OnAssetDownloadedDelegate; }
	FSimplygonSwarmTaskDelegate& OnAssetUploaded() { return OnAssetUploadedDelegate; }
	FString JobId;
	FString InputAssetId;
	FString OutputAssetId;

	FTaskData TaskData;
private:
	SimplygonRESTState State;
	bool IsCompleted;
	FSimplygonSwarmTaskDelegate OnAssetDownloadedDelegate;
	FSimplygonSwarmTaskDelegate OnAssetUploadedDelegate;
};

class FSimplygonRESTClient : public FRunnable
{
public:

	FSimplygonRESTClient(TSharedPtr<class FSimplygonSwarmTask> &InTask);
	~FSimplygonRESTClient();

	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	void StartJobAsync();
	bool IsRunning() const
	{
		return (Thread != nullptr);
	}
	void Wait();
private:

	void AccountInfo();
	void CreateJob(FString jobName, FString assetId);
	void UploadJobSettings(FString jobId, FString filename);
	void ProcessJob(FString jobId);
	void GetJob(FString jobId);
	void UploadAsset(FString assetName, FString filename);
	void DownloadAsset(FString assetId);

	void AccountInfo_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void CreateJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void UploadJobSettings_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ProcessJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void GetJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void UploadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void DownloadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	void AddAuthenticationHeader(TSharedRef<IHttpRequest> request);
	
	TSharedPtr<class FSimplygonSwarmTask> SwarmTask;
	FRunnableThread* Thread;
	FString HostName;
	FString APIKey;
};