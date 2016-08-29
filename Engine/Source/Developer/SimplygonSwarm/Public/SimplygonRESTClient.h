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

struct FSwarmTaskkData
{
	FString ZipFilePath;
	FString SplFilePath;
	FString OutputZipFilePath;
	FString JobDirectory;
	FString JobName;
	FCriticalSection* StateLock;
	FGuid ProcessorJobID;
	bool bDitheredTransition;
};

struct FSwarmJsonResponse
{
	FString JobId;
	FString AssetId;
	FString ErrorMessage;
	uint32 Progress;
	FString Status;
	FString OutputAssetId;
};

class FSimplygonSwarmTask
{
public:
	DECLARE_DELEGATE_OneParam(FSimplygonSwarmTaskDelegate, const FSimplygonSwarmTask&);
	FSimplygonSwarmTask(const FSwarmTaskkData& InTaskData);
	~FSimplygonSwarmTask();
	SimplygonRESTState GetState() const;
	void SetState(SimplygonRESTState InState);
	bool IsFinished() const;
	FSimplygonSwarmTaskDelegate& OnAssetDownloaded() { return OnAssetDownloadedDelegate; }
	FSimplygonSwarmTaskDelegate& OnAssetUploaded() { return OnAssetUploadedDelegate; }

	void EnableDebugLogging();	
	void SetHost(FString InHostAddress);

	//~Being Rest methods
	void AccountInfo();
	void CreateJob();
	void UploadJobSettings();
	void ProcessJob();
	void GetJob();
	void UploadAsset();
	void DownloadAsset();

	void AccountInfo_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void CreateJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void UploadJobSettings_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ProcessJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void GetJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void UploadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void DownloadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void AddAuthenticationHeader(TSharedRef<IHttpRequest> request);
	//~End Rest methods

	bool ParseJsonMessage(FString InJsonMessage, FSwarmJsonResponse& OutResponseData);
	FSwarmTaskkData TaskData;
private:
	SimplygonRESTState State;
	FString JobId;
	FString InputAssetId;
	FString OutputAssetId;

	bool IsCompleted;
	bool bEnableDebugLogging;

	FString HostName;
	FString APIKey;

	//~ Delegates Begin
	FSimplygonSwarmTaskDelegate OnAssetDownloadedDelegate;
	FSimplygonSwarmTaskDelegate OnAssetUploadedDelegate;
	FSimplygonSwarmTaskDelegate OnProgressUpdated;
	FSimplygonSwarmTaskDelegate OnTaskFailed;
	//~ Delegates End
};

class FSimplygonRESTClient : public FRunnable
{
public:

	/* Add a swarm task to the Queue*/
	void AddSwarmTask(TSharedPtr<FSimplygonSwarmTask>& InTask);

	/*Get pointer to the runnable*/
	static FSimplygonRESTClient* Get();

	static void Shutdown();

private:
	FSimplygonRESTClient();

	~FSimplygonRESTClient();

	//~ Begin FRunnable Interface
	virtual bool Init();
	virtual uint32 Run();
	void UpdateTaskStates();
	void MoveItemsToBoundedArray();
	virtual void Stop();
	virtual void Exit();
	//~ End FRunnable Interface

	void Wait(const float InSeconds, const float InSleepTime = 0.1f);
	
	void EnusureCompletion();

	/** Checks if there's been any Stop requests */
	FORCEINLINE bool ShouldStop() const
	{
		return StopTaskCounter.GetValue() > 0;
	}

private:
	/*Static instance*/
	static FSimplygonRESTClient* Runnable;
	/*Critical Section*/
	FCriticalSection CriticalSectionData;	
	FThreadSafeCounter StopTaskCounter;	
	/* a local buffer as to limit the number of concurrent jobs. */
	TArray<TSharedPtr< class FSimplygonSwarmTask>> JobsBuffer;
	TQueue<TSharedPtr<class FSimplygonSwarmTask>, EQueueMode::Mpsc> PendingJobs;
	FRunnableThread* Thread;
	FString HostName;
	FString APIKey;
	bool bEnableDebugging;
	float DelayBetweenRuns;
	int32 JobLimit;
};