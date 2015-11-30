#include "SimplygonSwarmPrivatePCH.h"
#include "SimplygonRESTClient.h"

#define HOSTNAME "http://127.0.0.1"
#define PORT ":55002"


FSimplygonSwarmTask::FSimplygonSwarmTask(FString InZipFile, FString InSPLFile, FString InOutputZipFile, FString InJobDirectory, FCriticalSection* InStateLock, FGuid InJobID, FProxyCompleteDelegate InDelegate)
	:ZipFilePath(InZipFile),
	SplFilePath(InSPLFile),
	OutputFilename(InOutputZipFile),
	State(SRS_UNKNOWN),
	StateLock(InStateLock),
	IsCompleted(false),
	JobDirectory(InJobDirectory),
	TestJobID(InJobID),
	TestDelegate(InDelegate)
{
}

FSimplygonSwarmTask::~FSimplygonSwarmTask()
{
}



SimplygonRESTState FSimplygonSwarmTask::GetState() const
{
	FScopeLock Lock(StateLock);
	return State;
}

void FSimplygonSwarmTask::SetState(SimplygonRESTState InState)
{
	FScopeLock Lock(StateLock);
	State = InState;
	if (State == SRS_ASSETDOWNLOADED)
	{
		IsCompleted = true;
	}
}

bool FSimplygonSwarmTask::IsFinished() const
{
	FScopeLock Lock(StateLock);
	return IsCompleted;
}


FSimplygonRESTClient::FSimplygonRESTClient(TSharedPtr<FSimplygonSwarmTask> &InTask)
	:
	APIKey(TEXT("LOCAL")),
	HostName(TEXT(HOSTNAME)),
	SwarmTask(InTask)
{
	FString IP = GetDefault<UEditorPerProjectUserSettings>()->SimplygonServerIP;
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
}

FSimplygonRESTClient::~FSimplygonRESTClient()
{
	if (Thread != NULL)
	{
		delete Thread;
		Thread = NULL;
	}
}

void FSimplygonRESTClient::StartJobAsync()
{
	Thread = FRunnableThread::Create(this, TEXT("SimplygonRESTClient"));
}


void FSimplygonRESTClient::Wait()
{
	Thread->WaitForCompletion();
}

bool FSimplygonRESTClient::Init()
{
	return true;
}

uint32 FSimplygonRESTClient::Run()
{
	
	while (SwarmTask->GetState() != SRS_ASSETDOWNLOADED)
	{
		switch (SwarmTask->GetState())
		{
		case SRS_UNKNOWN:
			UploadAsset(TEXT("UE4_TEST"), SwarmTask->ZipFilePath);
			break;
		case SRS_FAILED:
			return 0;
			break;
		case SRS_ASSETUPLOADED:
			CreateJob(TEXT("UE4_TEST"), SwarmTask->InputAssetId);
			break;
		case SRS_JOBCREATED:
			UploadJobSettings(SwarmTask->JobId, SwarmTask->SplFilePath);
			break;
		case SRS_JOBSETTINGSUPLOADED:
			ProcessJob(SwarmTask->JobId);
			break;
		case SRS_JOBPROCESSING:
			GetJob(SwarmTask->JobId);
			break;
		case SRS_JOBPROCESSED:
			DownloadAsset(SwarmTask->OutputAssetId);
			break;
		case SRS_ASSETDOWNLOADED:
			break;
		}		

		if (SwarmTask->GetState() == SRS_FAILED)
		{
			break;
		}
		
		FPlatformProcess::Sleep(0.5);
	}

	return 0;
}

void FSimplygonRESTClient::Stop()
{
}

void FSimplygonRESTClient::AccountInfo()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	AddAuthenticationHeader(request);
	request->SetURL(FString::Printf(TEXT("%s/2.3/account?apikey=%s"), *HostName, *APIKey));
	request->SetVerb(TEXT("GET")); 
	
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonRESTClient::AccountInfo_Response);

	if (!request->ProcessRequest())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
}

void FSimplygonRESTClient::AccountInfo_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();
		UE_LOG(LogSimplygonRESTClient, Log, TEXT("Account info response: %s"), *msg);
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Account info response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonRESTClient::CreateJob(FString jobName, FString assetId)
{
	SwarmTask->SetState(SRS_JOBCREATED_PENDING);

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/create?apikey=%s&job_name=%s&asset_id=%s"), *HostName, *APIKey, *jobName, *assetId);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonRESTClient::CreateJob_Response);
	
	if (!request->ProcessRequest())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
}

void FSimplygonRESTClient::CreateJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();
		UE_LOG(LogSimplygonRESTClient, Log, TEXT("Create job response: %s"), *msg);

		TSharedPtr<FJsonObject> JsonParsed;
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(msg);
		if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
		{
			SwarmTask->JobId = JsonParsed->GetStringField("JobId");
			UE_LOG(LogSimplygonRESTClient, Log, TEXT("JobId: %s"), *SwarmTask->JobId);
		}

		SwarmTask->SetState(SRS_JOBCREATED);
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Create job response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonRESTClient::UploadJobSettings(FString jobId, FString filename)
{
	SwarmTask->SetState(SRS_JOBSETTINGSUPLOADED_PENDING);

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s/uploadsettings?apikey=%s"), *HostName, *jobId, *APIKey);

	TArray<uint8> data;
	FFileHelper::LoadFileToArray(data, *filename);
	
	AddAuthenticationHeader(request);
	request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	request->SetContent(data);

	

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonRESTClient::UploadJobSettings_Response);

	if (!request->ProcessRequest())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
}

void FSimplygonRESTClient::UploadJobSettings_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		SwarmTask->SetState(SRS_JOBSETTINGSUPLOADED);
		SwarmTask->OnAssetUploaded().ExecuteIfBound(*SwarmTask);
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Upload job settings response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonRESTClient::ProcessJob(FString jobId)
{
	SwarmTask->SetState(SRS_JOBPROCESSING_PENDING);

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s/Process?apikey=%s"), *HostName, *jobId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("PUT"));
	
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonRESTClient::ProcessJob_Response);

	if (!request->ProcessRequest())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
}

void FSimplygonRESTClient::ProcessJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		SwarmTask->SetState(SRS_JOBPROCESSING);
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Process job response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonRESTClient::GetJob(FString jobId)
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s?apikey=%s"), *HostName, *jobId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("GET"));
	
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonRESTClient::GetJob_Response);

	if (!request->ProcessRequest())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
}

void FSimplygonRESTClient::GetJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();
		UE_LOG(LogSimplygonRESTClient, Log, TEXT("Get job response: %s"), *msg);

		TSharedPtr<FJsonObject> JsonParsed;
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(msg);
		if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
		{
			FString status = JsonParsed->GetStringField("Status");
			int32 Progress = JsonParsed->GetNumberField("ProgressPercentage");

			if (status == "Processed")
			{
				SwarmTask->OutputAssetId = JsonParsed->GetStringField("OutputAssetId");
				SwarmTask->SetState(SRS_JOBPROCESSED);
			}
			if (status == "Failed")
			{
				SwarmTask->SetState(SRS_FAILED);
			}

			UE_LOG(LogSimplygonRESTClient, Log, TEXT("Status: %s"), *status);
		}
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonRESTClient::UploadAsset(FString assetName, FString filename)
{
	SwarmTask->SetState(SRS_ASSETUPLOADED_PENDING);

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();
	
	FString url = FString::Printf(TEXT("%s/2.3/asset/upload?apikey=%s&asset_name=%s"), *HostName, *APIKey, *assetName);

	TArray<uint8> data;
	if (FFileHelper::LoadFileToArray(data, *filename))
	{
		AddAuthenticationHeader(request);
		request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
		request->SetURL(url);
		request->SetVerb(TEXT("POST"));
		request->SetContent(data);
		
		request->OnProcessRequestComplete().BindRaw(this, &FSimplygonRESTClient::UploadAsset_Response);

		FHttpModule::Get().SetMaxReadBufferSize(data.Num());
		FHttpModule::Get().SetHttpTimeout(300);

		if (!request->ProcessRequest())
		{
			SwarmTask->SetState(SRS_FAILED);
		}
		
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
	}

	
}

void FSimplygonRESTClient::UploadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
	
		FString msg = Response->GetContentAsString();
		UE_LOG(LogSimplygonRESTClient, Log, TEXT("Upload asset response: %s"), *msg);

		TSharedPtr<FJsonObject> JsonParsed;
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(msg);
		if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
		{
			SwarmTask->InputAssetId = JsonParsed->GetStringField("AssetId");
			UE_LOG(LogSimplygonRESTClient, Log, TEXT("AssetId: %s"), *SwarmTask->InputAssetId);
		}
		
		SwarmTask->SetState(SRS_ASSETUPLOADED);
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonRESTClient::DownloadAsset(FString assetId)
{
	SwarmTask->SetState(SRS_ASSETDOWNLOADED_PENDING);

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/asset/%s/download?apikey=%s"), *HostName, *assetId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("GET"));
	
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonRESTClient::DownloadAsset_Response);

	if (!request->ProcessRequest())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
}

void FSimplygonRESTClient::DownloadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!Response.IsValid())
	{
		SwarmTask->SetState(SRS_FAILED);
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		TArray<uint8> data = Response->GetContent();
		
		if (!FFileHelper::SaveArrayToFile(data, *SwarmTask->OutputFilename))
		{
			UE_LOG(LogSimplygonRESTClient, Log, TEXT("Unable to save file %S"), *SwarmTask->OutputFilename);
			SwarmTask->SetState(SRS_FAILED);
			
		}
		else
		{
			SwarmTask->OnAssetDownloaded().ExecuteIfBound(*SwarmTask);
			SwarmTask->SetState(SRS_ASSETDOWNLOADED);
			UE_LOG(LogSimplygonRESTClient, Log, TEXT("Asset downloaded"));
		}

		
	}
	else
	{
		SwarmTask->SetState(SRS_FAILED);
		UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Response: %i"), Response->GetResponseCode());
	}
}

void FSimplygonRESTClient::AddAuthenticationHeader(TSharedRef<IHttpRequest> request)
{
	//Basic User:User
	request->SetHeader("Authorization", "Basic dXNlcjp1c2Vy");
}