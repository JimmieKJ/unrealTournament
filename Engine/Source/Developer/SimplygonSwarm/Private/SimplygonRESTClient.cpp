// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SimplygonSwarmPrivatePCH.h"
#include "SimplygonRESTClient.h"

#define HOSTNAME "http://127.0.0.1"
#define PORT ":55002"

/*
UploadPartInit
/2.3/asset/uploadpart?apikey={1}&asset_name={2}&asset_id={3}

UploadPartAbort
/2.3/asset/uploadpart/{1}/abort?apikey={2}

UploadPart_Get
/2.3/asset/uploadpart/{1}?apikey={2}


UploadPart
/2.3/asset/uploadpart/{1}/upload?apikey={2}&part_number={3}


UploadPartComplete
/2.3/asset/uploadpart/{1}/Complete?apikey={2}&part_count={3}&upload_size={4}
*/


FSimplygonSwarmTask::FSimplygonSwarmTask(const FSwarmTaskkData& InTaskData)
	:
    TaskData(InTaskData),
	State(SRS_UNKNOWN),
	IsCompleted(false)
{
}

FSimplygonSwarmTask::~FSimplygonSwarmTask()
{
	//un register the delegates
	OnAssetDownloaded().Unbind();
	OnAssetUploaded().Unbind();
}

SimplygonRESTState FSimplygonSwarmTask::GetState() const
{
	FScopeLock Lock(TaskData.StateLock);
	return State;
}

void FSimplygonSwarmTask::SetHost(FString InHostAddress)
{
	HostName = InHostAddress;
}

void FSimplygonSwarmTask::EnableDebugLogging()
{
	bEnableDebugLogging = true;
}

void FSimplygonSwarmTask::SetState(SimplygonRESTState InState)
{
	if (this != nullptr && TaskData.StateLock)
	{
		FScopeLock Lock(TaskData.StateLock);

		//do not update the sate if either of these set is already set
		if (InState != State
			&& (State != SRS_ASSETDOWNLOADED && State != SRS_FAILED))
		{
			State = InState;
		}
		else if ((InState != State) && (State != SRS_FAILED) && (InState == SRS_ASSETDOWNLOADED))
		{
			State = SRS_ASSETDOWNLOADED;
			this->IsCompleted = true;
		}
		else if (InState != State && InState == SRS_FAILED)
		{
			State = SRS_ASSETDOWNLOADED;
		}
		
	}
	else
	{
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Object in invalid state can not acquire state lock"));
	}
	
}

bool FSimplygonSwarmTask::IsFinished() const
{
	FScopeLock Lock(TaskData.StateLock);
	return IsCompleted;
}

bool FSimplygonSwarmTask::ParseJsonMessage(FString InJsonMessage, FSwarmJsonResponse& OutResponseData)
{
	bool sucess = false;
	
	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(InJsonMessage);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
	{
		FString Status;
		if (JsonParsed->HasField("JobId"))
		{
			JsonParsed->TryGetStringField("JobId", OutResponseData.JobId);
		}

		if (JsonParsed->HasField("Status"))
		{
			OutResponseData.Status = JsonParsed->GetStringField("Status");
		}

		if (JsonParsed->HasField("OutputAssetId"))
		{
			JsonParsed->TryGetStringField("OutputAssetId", OutResponseData.OutputAssetId);
		}

		if (JsonParsed->HasField("AssetId"))
		{
			JsonParsed->TryGetStringField("AssetId", OutResponseData.AssetId);
		}

		if (JsonParsed->HasField("ProgressPercentage"))
		{
			JsonParsed->TryGetNumberField("ProgressPercentage", OutResponseData.Progress);
		}
		
		if (JsonParsed->HasField("ProgressPercentage"))
		{
			JsonParsed->TryGetNumberField("ProgressPercentage", OutResponseData.Progress);
		}

		sucess = true;
	}
	

	return sucess;
}

void FSimplygonSwarmTask::AccountInfo()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	AddAuthenticationHeader(request);
	request->SetURL(FString::Printf(TEXT("%s/2.3/account?apikey=%s"), *HostName, *APIKey));
	request->SetVerb(TEXT("GET"));

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::AccountInfo_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Account info request failed to process."));
	}
}

void FSimplygonSwarmTask::AccountInfo_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Account info response is invalid."));
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();
		UE_LOG(LogSimplygonRESTClient, Display, TEXT("Account info response: %s"), *msg);
	}
	else
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Account info response: %i"), Response->GetResponseCode());
	}
}


void FSimplygonSwarmTask::CreateJob()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/create?apikey=%s&job_name=%s&asset_id=%s"), *HostName, *APIKey, *TaskData.JobName, *InputAssetId);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::CreateJob_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Create job request failed."));
	}
	else
	{
		SetState(SRS_JOBCREATED_PENDING);
	}
}

void FSimplygonSwarmTask::CreateJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Create job response failed."));
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();

		FSwarmJsonResponse Data;
		if (ParseJsonMessage(msg, Data))
		{
			if (!Data.JobId.IsEmpty())
			{
				JobId = Data.JobId;
				UE_LOG(LogSimplygonRESTClient, Display, TEXT("Created JobId: %s"), *Data.JobId);
				SetState(SRS_JOBCREATED);
			}
			else
				UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Empty JobId for task"));
			
		}
		else
		{
			SetState(SRS_FAILED);
		}
	}
	else
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Create job response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::UploadJobSettings()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s/uploadsettings?apikey=%s"), *HostName, *JobId, *APIKey);

	TArray<uint8> data;
	FFileHelper::LoadFileToArray(data, *TaskData.SplFilePath);

	AddAuthenticationHeader(request);
	request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	request->SetContent(data);
	
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::UploadJobSettings_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload settings request failed."));
	}
	else
	{
		SetState(SRS_JOBSETTINGSUPLOADED_PENDING);
	}
}

void FSimplygonSwarmTask::UploadJobSettings_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload settings response failed."));
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		
		if (!OnAssetUploaded().IsBound())
		{
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("OnAssetUploaded delegate not bound to any object"));
		}
		else
		{
			OnAssetUploaded().Execute(*this);
			SetState(SRS_JOBSETTINGSUPLOADED);
		}
	}
	else
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload job settings response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::ProcessJob()
{
	SetState(SRS_JOBPROCESSING_PENDING);

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s/Process?apikey=%s"), *HostName, *JobId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("PUT"));

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::ProcessJob_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Process job request failed."));
	}
}

void FSimplygonSwarmTask::ProcessJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Process job response failed."));
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		SetState(SRS_JOBPROCESSING);
	}
	else
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Process job response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::GetJob()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s?apikey=%s"), *HostName, *JobId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("GET"));

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::GetJob_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Get job request failed."));
	}
}

void FSimplygonSwarmTask::GetJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Get job response failed."));
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();

		FSwarmJsonResponse Data;
		if (!msg.IsEmpty() && ParseJsonMessage(msg, Data))
		{

			if (!Data.Status.IsEmpty())
			{
				if (Data.Status == "Processed")
				{
					if (!Data.OutputAssetId.IsEmpty())
					{
						OutputAssetId = Data.OutputAssetId;
						SetState(SRS_JOBPROCESSED);
						UE_LOG(LogSimplygonRESTClient, Log, TEXT("Job with Id %s processed"), *Data.JobId);						
					}
					else
					{
						SetState(SRS_FAILED);
					}
				}
				if (Data.Status == "Failed")
				{
					SetState(SRS_FAILED);
					UE_LOG(LogSimplygonRESTClient, Error, TEXT("Job with id %s Failed %s"), *Data.JobId, *Data.ErrorMessage);
				}
			}
		}
		else
		{
			SetState(SRS_FAILED);
		}
	}
	else
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Get Job Response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::UploadAsset()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/asset/upload?apikey=%s&asset_name=%s"), *HostName, *APIKey, *TaskData.JobName);

	TArray<uint8> data;
	

	if (FFileHelper::LoadFileToArray(data, *TaskData.ZipFilePath))
	{
		if (data.Num() > 2147483648)
		{
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("File larger than 2GB are not supported."));
			SetState(SRS_FAILED);
			return;
		}

		AddAuthenticationHeader(request);
		request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
		request->SetURL(url);
		request->SetVerb(TEXT("POST"));
		request->SetContent(data);

		request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::UploadAsset_Response);
		//request->OnRequestProgress().BindLambda()

		uint32 bufferSize = data.Num();
		FHttpModule::Get().SetMaxReadBufferSize(bufferSize);
		

		if (!request->ProcessRequest())
		{
			SetState(SRS_FAILED);
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload asset request failed."));
		}
		else
		{ 
			SetState(SRS_ASSETUPLOADED_PENDING);
		}

	}
	else
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Failed to load file from path %s."), *TaskData.ZipFilePath);
	}


}

void FSimplygonSwarmTask::UploadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		SetState(SRS_FAILED);
		if (Response.IsValid())
		{
			UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Upload asset Response: %i"), Response->GetResponseCode());
		}		
		else
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload asset response failed."));
	}
	else
	{
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			FString msg = Response->GetContentAsString();
			
			FSwarmJsonResponse Data;
			if (ParseJsonMessage(msg, Data))
			{
				if (!Data.AssetId.IsEmpty())
				{
					InputAssetId = Data.AssetId;
					SetState(SRS_ASSETUPLOADED);
				}
				else
					UE_LOG(LogSimplygonRESTClient, Display, TEXT("Could not parse Input asset Id for job: %s"), *Data.JobId);
				
			}
			else
			{
				SetState(SRS_FAILED);
			}			
		}		
	}	
}

void FSimplygonSwarmTask::DownloadAsset()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	if (OutputAssetId.IsEmpty())
	{
		SetState(SRS_FAILED);
	}

	FString url = FString::Printf(TEXT("%s/2.3/asset/%s/download?apikey=%s"), *HostName, *OutputAssetId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("GET"));
	FHttpModule::Get().

	FHttpModule::Get().SetHttpTimeout(300);

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::DownloadAsset_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Error, TEXT("Download asset request failed."));
	}
	else
	{
		UE_LOG(LogSimplygonRESTClient, Log, TEXT("Downloading Job with Id %s"), *JobId);
		SetState(SRS_ASSETDOWNLOADED_PENDING);
	}
}

void FSimplygonSwarmTask::DownloadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
	//	SetState(SRS_FAILED);
	//	UE_LOG(LogSimplygonRESTClient, Error, TEXT("Download asset response failed."));
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		if (this == nullptr || JobId.IsEmpty())
		{
			UE_LOG(LogSimplygonRESTClient, Display, TEXT("Object has already been destoryed or job id was empty"));
			return;
		}
		TArray<uint8> data = Response->GetContent();
		/*FString msg = Response->GetContentAsString();
		FSwarmJsonResponse Data;*/
		
		if (data.Num() > 0)
		{
			if (!TaskData.OutputZipFilePath.IsEmpty() && !FFileHelper::SaveArrayToFile(data, *TaskData.OutputZipFilePath))
			{
				UE_LOG(LogSimplygonRESTClient, Display, TEXT("Unable to save file %s"), *TaskData.OutputZipFilePath);
				SetState(SRS_FAILED);

			}
			else
			{
				if (!this->IsCompleted &&  OnAssetDownloaded().IsBound())
				{

					UE_LOG(LogSimplygonRESTClient, Display, TEXT("Asset downloaded"));
					OnAssetDownloaded().Execute(*this);
					SetState(SRS_ASSETDOWNLOADED);
				}
				else
					UE_LOG(LogSimplygonRESTClient, Display, TEXT("OnAssetDownloaded delegatge not bound to any objects"));


			}
		}

	}
	else
	{
		SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Download asset Response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::AddAuthenticationHeader(TSharedRef<IHttpRequest> request)
{
	//Basic User:User
	request->SetHeader("Authorization", "Basic dXNlcjp1c2Vy");
}

FSimplygonRESTClient* FSimplygonRESTClient::Runnable = nullptr;

FSimplygonRESTClient::FSimplygonRESTClient()
	:
    Thread(nullptr),
    HostName(TEXT(HOSTNAME)),
	APIKey(TEXT("LOCAL")),
	bEnableDebugging(false)
{
	FString IP = GetDefault<UEditorPerProjectUserSettings>()->SimplygonServerIP;
	bEnableDebugging = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
	if (IP != "")
	{
		if (!IP.Contains("http://"))
		{
			HostName = "http://" + IP;
		}
		else
		{
			HostName = IP;
		}
	}
	else
	{
		HostName = TEXT(HOSTNAME);
	}

	HostName += TEXT(PORT);
	JobLimit = 16;
	

	DelayBetweenRuns = GetDefault<UEditorPerProjectUserSettings>()->SimplygonSwarmDelay / 1000;
	DelayBetweenRuns = DelayBetweenRuns <= 0 ? 0.5 : DelayBetweenRuns;

	Thread = FRunnableThread::Create(this, TEXT("SimplygonRESTClient"));
}

FSimplygonRESTClient::~FSimplygonRESTClient()
{
	if (Thread != nullptr)
	{
		delete Thread;
		Thread = nullptr;
	}
}


bool FSimplygonRESTClient::Init()
{
	return true;
}


void FSimplygonRESTClient::Wait(const float InSeconds, const float InSleepTime /* = 0.1f */)
{
	// Instead of waiting the given amount of seconds doing nothing
	// check periodically if there's been any Stop requests.
	for (float TimeToWait = InSeconds; TimeToWait > 0.0f && ShouldStop() == false; TimeToWait -= InSleepTime)
	{
		FPlatformProcess::Sleep(FMath::Min(InSleepTime, TimeToWait));
	}
}

uint32 FSimplygonRESTClient::Run()
{
	Wait(5);
	//float SleepDelay = DelayBetweenRuns;
	do
	{				
		MoveItemsToBoundedArray();
		UpdateTaskStates();
		/*if (PendingJobs.IsEmpty() && JobsBuffer.Num() < JobLimit)
			SleepDelay = 5 + DelayBetweenRuns;
		else 
			SleepDelay = 5 + DelayBetweenRuns;*/		
		Wait(DelayBetweenRuns);

	} 
	while (ShouldStop() == false);	

	return 0;
}

void FSimplygonRESTClient::UpdateTaskStates()
{
	TArray<TSharedPtr<class FSimplygonSwarmTask>> TasksMarkedForRemoval;
	//TasksMarkedForRemoval.AddUninitialized(JobLimit);

	FScopeLock Lock(&CriticalSectionData);

	for (int32 Index = 0; Index < JobsBuffer.Num(); Index++)
	{
		//
		TSharedPtr<FSimplygonSwarmTask>& SwarmTask = JobsBuffer[Index];
		switch (SwarmTask->GetState())
		{
		case SRS_UNKNOWN:
			SwarmTask->UploadAsset();
			break;
		case SRS_FAILED:
			TasksMarkedForRemoval.Add(SwarmTask);
			break;
		case SRS_ASSETUPLOADED:
			SwarmTask->CreateJob();
			break;
		case SRS_JOBCREATED:
			SwarmTask->UploadJobSettings();
			break;
		case SRS_JOBSETTINGSUPLOADED:
			SwarmTask->ProcessJob();
			break;
		case SRS_JOBPROCESSING:
			SwarmTask->GetJob();
			break;
		case SRS_JOBPROCESSED:
			SwarmTask->DownloadAsset();
			break;
		case SRS_ASSETDOWNLOADED:
			TasksMarkedForRemoval.Add(SwarmTask);
			break;
		}
	}

	for (int32 Index = 0; Index < TasksMarkedForRemoval.Num(); Index++)
	{
		JobsBuffer.Remove(TasksMarkedForRemoval[Index]);
	}

	TasksMarkedForRemoval.Empty();
}

void FSimplygonRESTClient::MoveItemsToBoundedArray()
{
	if (!PendingJobs.IsEmpty())
	{
		int32 ItemsToInsert = 0;
		if (JobsBuffer.Num() >= JobLimit)
		{
			ItemsToInsert = JobLimit;
		}
		else if (JobsBuffer.Num() < JobLimit)
		{
			JobsBuffer.Shrink();
			ItemsToInsert = JobLimit - JobsBuffer.Num();
			
			FScopeLock Lock(&CriticalSectionData);
			do 
			{			
				TSharedPtr<FSimplygonSwarmTask> OutItem;
				if (PendingJobs.Dequeue(OutItem))
					JobsBuffer.Add(OutItem);

			} while (!PendingJobs.IsEmpty() && JobsBuffer.Num() < JobLimit);
		}
	
	}
}

FSimplygonRESTClient* FSimplygonRESTClient::Get()
{
	if (!Runnable && FPlatformProcess::SupportsMultithreading())
	{
		Runnable = new FSimplygonRESTClient();
	}
	return Runnable;
}

void FSimplygonRESTClient::Shutdown()
{
	if (Runnable)
	{
		Runnable->EnusureCompletion();
		delete Runnable;
		Runnable = nullptr;
	}
}

void FSimplygonRESTClient::Stop()
{
	StopTaskCounter.Increment();
}

void FSimplygonRESTClient::EnusureCompletion()
{
	Stop();
	Thread->WaitForCompletion();
}

void FSimplygonRESTClient::AddSwarmTask(TSharedPtr<FSimplygonSwarmTask>& InTask)
{
	//FScopeLock Lock(&CriticalSectionData);
	InTask->SetHost(HostName);
	PendingJobs.Enqueue(InTask);
}

void FSimplygonRESTClient::Exit()
{

}

