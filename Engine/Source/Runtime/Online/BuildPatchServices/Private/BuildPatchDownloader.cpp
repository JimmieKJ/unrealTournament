// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FBuildPatchDownloader.cpp: Implements the BuildPatchChunkDownloader
	that runs a thread to download chunks in it's queue.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"
#include "Async.h"

using namespace BuildPatchConstants;

#define EXTRA_DOWNLOAD_LOGGING 0

int32 NUM_DOWNLOAD_THREADS = 8;

/* A class used to monitor the average chunk download time and standard deviation
*****************************************************************************/
class FMeanChunkTime
{
public:
	FMeanChunkTime()
		: Count(0)
		, Total(0)
		, TotalSqs(0)
	{}

	void Reset()
	{
		Count = 0;
		Total = 0;
		TotalSqs = 0;
	}

	bool IsReliable() const
	{
		return Count > 10;
	}

	double GetMean() const
	{
		return Total / Count;
	}

	double GetStd() const
	{
		const double Mean = GetMean();
		return FMath::Sqrt((TotalSqs / Count) - (Mean * Mean));
	}

	void GetValues(double& Mean, double& Std) const
	{
		Mean = GetMean();
		Std = FMath::Sqrt((TotalSqs / Count) - (Mean * Mean));
	}

	void AddSample(double Sample)
	{
		Total += Sample;
		TotalSqs += Sample * Sample;
		++Count;
	}

private:
	int32 Count;
	double Total;
	double TotalSqs;
};

/* A class used to monitor the download success rate
*****************************************************************************/
class FChunkSuccessRate
{
public:
	FChunkSuccessRate()
		: TotalSuccess(0)
		, Count(0)
	{}

	void Reset()
	{
		TotalSuccess = 0;
		Count = 0;
	}

	bool IsReliable() const
	{
		return Count > 10;
	}

	double GetOverall() const
	{
		if(Count == 0)
		{
			return 1.0;
		}
		const double Num = TotalSuccess;
		const double Denom = Count;
		return Num / Denom;
	}

	void AddSuccess()
	{
		++TotalSuccess;
		++Count;
	}

	void AddFail()
	{
		++Count;
	}

private:
	uint64 TotalSuccess;
	uint64 Count;
};

/* FBuildPatchDownloader implementation
*****************************************************************************/
FBuildPatchDownloader::FBuildPatchDownloader(const FString& InSaveDirectory, const TArray<FString>& InCloudDirectories, const FBuildPatchAppManifestRef& InInstallManifest, FBuildPatchProgress* InBuildProgress)
	: MaxRetryCount(LoadRetryCount())
	, RetryDelayTimes(LoadRetryTimes())
	, HealthPercentages(LoadHealthPercentages())
	, DisconnectedDelay(LoadDisconnectDelay())
	, SaveDirectory(InSaveDirectory)
	, CloudDirectories(InCloudDirectories)
	, InstallManifest(InInstallManifest)
	, Thread(nullptr)
	, bIsRunning(false)
	, bIsInited(false)
	, bIsIdle(false)
	, bIsDisconnected(false)
	, bWaitingForJobs(true)
	, ChunkSuccessRate(1.0f)
	, CyclesAtLastHealthState(0)
	, DownloadHealth(EBuildPatchDownloadHealth::Excellent)
	, CyclesAtLastData(0)
	, DataToDownloadTotalBytes(0)
	, BuildProgress(InBuildProgress)
{
	// Must be given at least one cloud directory
	check(CloudDirectories.Num());

	// Initialise health states to zero time
	HealthStateTimes.AddZeroed((int32)EBuildPatchDownloadHealth::NUM_Values);

	// config overrides to control downloads
	GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloads"), NUM_DOWNLOAD_THREADS, GEngineIni);
	NUM_DOWNLOAD_THREADS = FMath::Clamp<int32>(NUM_DOWNLOAD_THREADS, 1, 100);

	// Start thread!
	const TCHAR* ThreadName = TEXT("ChunkDownloaderThread");
	Thread = FRunnableThread::Create(this, ThreadName);
}

FBuildPatchDownloader::~FBuildPatchDownloader()
{
	if( Thread != NULL )
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = NULL;
	}
}

int32 FBuildPatchDownloader::LoadRetryCount() const
{
	int32 RetryCountConfig = 6;
	GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkRetries"), RetryCountConfig, GEngineIni);
	return FMath::Clamp<int32>(RetryCountConfig, -1, 1000);
}

float FBuildPatchDownloader::LoadDisconnectDelay() const
{
	float DisconnectedDelayConfig = 5.0f;
	GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("DisconnectedDelay"), DisconnectedDelayConfig, GEngineIni);
	return FMath::Clamp<float>(DisconnectedDelayConfig, 1.0f, 30.0f);
}

TArray<float> FBuildPatchDownloader::LoadHealthPercentages() const
{
	// If the enum was changed since writing, the config here needs updating
	check((int32)EBuildPatchDownloadHealth::NUM_Values == 5);
	bool bUseDefault = false;
	TArray<float> HealthPercentagesConfig;
	HealthPercentagesConfig.AddZeroed((int32)EBuildPatchDownloadHealth::NUM_Values);
	if (!GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("OKHealth"), HealthPercentagesConfig[(int32)EBuildPatchDownloadHealth::OK], GEngineIni))
	{
		bUseDefault = true;
	}
	if (!GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("GoodHealth"), HealthPercentagesConfig[(int32)EBuildPatchDownloadHealth::Good], GEngineIni))
	{
		bUseDefault = true;
	}
	if (!GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("ExcellentHealth"), HealthPercentagesConfig[(int32)EBuildPatchDownloadHealth::Excellent], GEngineIni))
	{
		bUseDefault = true;
	}
	if (bUseDefault)
	{
		HealthPercentagesConfig[(int32)EBuildPatchDownloadHealth::OK] = 0.9f; // 10% or less failures
		HealthPercentagesConfig[(int32)EBuildPatchDownloadHealth::Good] = 0.99f; // 1% or less failures
		HealthPercentagesConfig[(int32)EBuildPatchDownloadHealth::Excellent] = 1.0f; // No failures
	}

	// We don't need to set Poor or Disconnected as they are fallback states.

	return MoveTemp(HealthPercentagesConfig);
}

const FString& FBuildPatchDownloader::GetCloudDirectory(int32 RetryNum) const
{
	return CloudDirectories[RetryNum % CloudDirectories.Num()];
}

TArray<float> FBuildPatchDownloader::LoadRetryTimes() const
{
	// Get config for retry times
	TArray<FString> ConfigStrings;
	TArray<float> ConfigFloats;
	GConfig->GetArray(TEXT("Portal.BuildPatch"), TEXT("RetryTimes"), ConfigStrings, GEngineIni);
	bool bReadArraySuccess = ConfigStrings.Num() > 0;
	ConfigFloats.AddZeroed(ConfigStrings.Num());
	for (int32 TimeIdx = 0; TimeIdx < ConfigStrings.Num() && bReadArraySuccess; ++TimeIdx)
	{
		float TimeValue = FPlatformString::Atof(*ConfigStrings[TimeIdx]);
		// Atof will return 0.0 if failed to parse, and we don't expect a time of 0.0 so presume error
		if (TimeValue > 0.0f)
		{
			ConfigFloats[TimeIdx] = FMath::Clamp<float>(TimeValue, 0.5f, 300.0f);
		}
		else
		{
			bReadArraySuccess = false;
		}
	}

	// If the retry array was not parsed successfully, use default.
	if (!bReadArraySuccess)
	{
		const float RetryFloats[] = { 0.5f, 1.0f, 1.0f, 3.0f, 3.0f, 10.0f, 10.0f, 20.0f, 20.0f, 30.0f };
		ConfigFloats.Empty(ARRAY_COUNT(RetryFloats));
		ConfigFloats.Append(RetryFloats, ARRAY_COUNT(RetryFloats));
	}

	return MoveTemp(ConfigFloats);
}

bool FBuildPatchDownloader::Init()
{
	bool bReady = true;
	return bReady;
}

uint32 FBuildPatchDownloader::Run()
{
	SetRunning(true);
	SetInited(true);

	// Cache is chunk data
	const bool bIsChunkData = !InstallManifest->IsFileDataManifest();
	const FBuildPatchData::Type DataType = bIsChunkData ? FBuildPatchData::ChunkData : FBuildPatchData::FileData;

	// The Average chunk download time
	FMeanChunkTime MeanChunkTime;
	FChunkSuccessRate SuccessRateTracker;
	TArray< FGuid > InFlightKeys;

	// To time the health states, we need to start the timers on first download
	bool bDownloadsStarted = false;

	// While there is the possibility of more chunks to download
	while ( ShouldBeRunning() && !FBuildPatchInstallError::HasFatalError() )
	{
		// Give other threads some time
		FPlatformProcess::Sleep( 0.0f );
		// Check inflight downloads for completion and process them
		InFlightDownloadsLock.Lock();
		InFlightKeys.Empty();
		InFlightDownloads.GetKeys( InFlightKeys );
		bool bAllDownloadsRetrying = InFlightDownloads.Num() > 0;
		for( auto InFlightKeysIt = InFlightKeys.CreateConstIterator(); InFlightKeysIt && !FBuildPatchInstallError::HasFatalError(); ++InFlightKeysIt )
		{
			const FGuid& InFlightKey = *InFlightKeysIt;
			FDownloadJob& InFlightJob = InFlightDownloads[ InFlightKey ];
			// Keep track of any time when
			if (InFlightJob.RetryCount.GetValue() == 0)
			{
				bAllDownloadsRetrying = false;
			}
			// If the download has finished running and not restarting
			if( InFlightJob.StateFlag.GetValue() > EDownloadState::DownloadRunning )
			{
				InFlightDownloadsLock.Unlock();
				bool bSuccess = InFlightJob.StateFlag.GetValue() == EDownloadState::DownloadSuccess;
				if( bSuccess )
				{
					const double ChunkTime = InFlightJob.DownloadRecord.EndTime - InFlightJob.DownloadRecord.StartTime;
					MeanChunkTime.AddSample(ChunkTime);
					// If we know the SHA for this chunk, add it to the header for improved verification
					FSHAHashData ChunkShaHash;
					if (InstallManifest->GetChunkShaHash(InFlightKey, ChunkShaHash))
					{
						FBuildPatchUtils::InjectShaToChunkFile(InFlightJob.DataArray, ChunkShaHash);
					}
					// Uncompress if needed
					if (bIsChunkData)
					{
						bSuccess = FBuildPatchUtils::UncompressChunkFile(InFlightJob.DataArray);
					}
					else
					{
						bSuccess = FBuildPatchUtils::UncompressFileDataFile(InFlightJob.DataArray);
					}
					// Check if the chunk was successfully uncompressed
					if( !bSuccess )
					{
						FBuildPatchAnalytics::RecordChunkDownloadError( InFlightJob.DownloadUrl, FPlatformMisc::GetLastError(), TEXT( "Uncompress Fail" ) );
						GWarn->Logf( TEXT( "BuildPatchServices: ERROR: Failed to uncompress chunk data %s" ), *InFlightJob.DownloadUrl );
						NumBadDownloads.Increment();
					}
					// Verify the data in memory
					else if( FBuildPatchUtils::VerifyChunkFile( InFlightJob.DataArray ) )
					{
						// When saving file data locally, we can just use the old filename
						const FString FileDataPath = FBuildPatchUtils::GetFileOldFilename( SaveDirectory, InFlightJob.Guid );
						// Save file data to staging area
						if( bIsChunkData || FFileHelper::SaveArrayToFile( InFlightJob.DataArray, *FileDataPath ) )
						{
							// Record download
							{
								FScopeLock Lock( &DownloadRecordsLock );
								DownloadRecords.Add( InFlightJob.DownloadRecord );
							}

							// Register with the chunk cache
							if( bIsChunkData )
							{
								FBuildPatchChunkCache::Get().AddDataToCache( InFlightJob.Guid, InFlightJob.DataArray );
							}
							else
							{
								FBuildPatchFileConstructor::AddFileDataToInventory( InFlightJob.Guid, FileDataPath );
							}

							bSuccess = true;
						}
						else
						{
							bSuccess = false;
							FBuildPatchAnalytics::RecordChunkDownloadError( InFlightJob.DownloadUrl, FPlatformMisc::GetLastError(), TEXT( "SaveToDisk Fail" ) );
							GWarn->Logf(TEXT("BuildPatchServices: ERROR: Failed to save file data %s"), *FileDataPath);
						}
					}
					else
					{
						bSuccess = false;
						FBuildPatchAnalytics::RecordChunkDownloadError( InFlightJob.DownloadUrl, InFlightJob.ResponseCode, TEXT( "Verify Fail" ) );
						GWarn->Logf(TEXT("BuildPatchServices: ERROR: Verify failed on chunk %s"), *InFlightJob.DownloadUrl);
						NumBadDownloads.Increment();
					}
				}
				else
				{
					bSuccess = false;
					FBuildPatchAnalytics::RecordChunkDownloadError( InFlightJob.DownloadUrl, InFlightJob.ResponseCode, TEXT( "Download Fail" ) );
					GWarn->Logf(TEXT("BuildPatchServices: ERROR: %d Failed to download chunk %s"), InFlightJob.RetryCount.GetValue(), *InFlightJob.DownloadUrl);
					NumFailedDownloads.Increment();
				}
				
				// Handle failed
				if (!bSuccess)
				{
					SuccessRateTracker.AddFail();
					// Always increment
					int32 JobRetryCount = InFlightJob.RetryCount.Increment();
					// Check retry count or always retrying
					if (JobRetryCount <= MaxRetryCount || MaxRetryCount < 0)
					{
						DataToRetry.Add(InFlightJob.Guid);
						InFlightJob.StateFlag.Set(EDownloadState::DownloadInit);
					}
					else
					{
						// Out of retries, fail
						FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::DownloadError, DownloadErrorCodes::OutOfRetries);
					}
				}

				// Remove from list if successful, we still need to lock as it ties in with looping
				InFlightDownloadsLock.Lock();
				if( bSuccess )
				{
					SuccessRateTracker.AddSuccess();
					InFlightDownloads.Remove( InFlightKey );
				}
			}
			else if (bIsChunkData && MeanChunkTime.IsReliable() && InFlightJob.RetryCount.GetValue() == 0)
			{
				// If still on first try, cancel any chunk taking longer than the mean time plus 4x standard deviation. In statistical terms, that's 1 in 15,787 chance of being
				// a download time that appears in the normal distribution. So we are guessing that this chunk would be an abnormally delayed one.
				const double ChunkTime = (FPlatformTime::Seconds() - InFlightJob.DownloadRecord.StartTime);
				double ChunkMean, ChunkStd;
				MeanChunkTime.GetValues(ChunkMean, ChunkStd);
				// The point at which we decide the chunk is delayed, with a sane minimum
				const double BreakingPoint = FMath::Max<double>(20.0, ChunkMean + (ChunkStd * 4.0));
				if (ChunkTime > BreakingPoint)
				{
					GWarn->Logf(TEXT("BuildPatchServices: WARNING: Canceling download %s. T:%.2f Av:%.2f Std:%.2f Bp:%.2f"), *InFlightJob.Guid.ToString(), ChunkTime, ChunkMean, ChunkStd, BreakingPoint);
					MeanChunkTime.Reset();
					InFlightDownloadsLock.Unlock();
					FBuildPatchAnalytics::RecordChunkDownloadAborted(InFlightJob.DownloadUrl, ChunkTime, ChunkMean, ChunkStd, BreakingPoint);
					FBuildPatchHTTP::CancelHttpRequest(InFlightJob.HttpRequestId);
					InFlightDownloadsLock.Lock();
				}
			}

		}
		InFlightDownloadsLock.Unlock();
		SetSuccessRate(SuccessRateTracker.GetOverall());
		double SecondsSinceData = FStatsCollector::CyclesToSeconds(FStatsCollector::GetCycles() - CyclesAtLastData);
		SetIsDisconnected(bAllDownloadsRetrying && SecondsSinceData > DisconnectedDelay);

		// Pause
		if (BuildProgress->GetPauseState())
		{
			uint64 PausedForCycles = FStatsCollector::SecondsToCycles(BuildProgress->WaitWhilePaused());
			// Skew timers
			FPlatformAtomics::InterlockedAdd(&CyclesAtLastHealthState, PausedForCycles);
		}

		// Check if there were any errors while paused, like canceling
		if( FBuildPatchInstallError::HasFatalError() )
		{
			continue;
		}

		// Try get a job, or if we don't have a job yet, wait a little.
		FGuid NextGuid;
		if( !GetNextDownload( NextGuid ) )
		{
			SetIdle( true );
			FPlatformProcess::Sleep( 0.1f );
			continue;
		}
		SetIdle( false );

		// First download init
		if (bDownloadsStarted == false)
		{
			bDownloadsStarted = true;
			// Set timer
			FPlatformAtomics::InterlockedExchange(&CyclesAtLastHealthState, FStatsCollector::GetCycles());
			UpdateDownloadHealth();
		}

		// Make the filename and download url
		InFlightDownloadsLock.Lock();
		int32 RetryNum = InFlightDownloads[NextGuid].RetryCount.GetValue();
		InFlightDownloadsLock.Unlock();
		FString DownloadUrl = FBuildPatchUtils::GetDataFilename(InstallManifest, GetCloudDirectory(RetryNum), NextGuid);
		const bool bIsHTTPRequest = DownloadUrl.Contains( TEXT( "http" ), ESearchCase::IgnoreCase );

		// Start the download
		int32 HttpRequestId = INDEX_NONE;
		if( bIsHTTPRequest )
		{
			// Kick off the download
#if EXTRA_DOWNLOAD_LOGGING
			GWarn->Logf(TEXT("BuildPatchDownloader: Queuing chunk HTTP download %s"), *NextGuid.ToString());
#endif
			HttpRequestId = FBuildPatchHTTP::QueueHttpRequest(DownloadUrl, FHttpRequestCompleteDelegate::CreateThreadSafeSP(this, &FBuildPatchDownloader::HttpRequestComplete), FHttpRequestProgressDelegate::CreateThreadSafeSP(this, &FBuildPatchDownloader::HttpRequestProgress));

			InFlightDownloadsLock.Lock();
			InFlightDownloads[NextGuid].HttpRequestId = HttpRequestId;
			InFlightDownloads[NextGuid].DownloadUrl = DownloadUrl;
			InFlightDownloadsLock.Unlock();
		}
		else
		{
			// Load file from drive/network
			TFunction<void()> Task = [this, DownloadUrl, NextGuid]() {
				TArray< uint8 > FileDataArray;
				FArchive* Reader = IFileManager::Get().CreateFileReader( *DownloadUrl );
				bool bSuccess = Reader != NULL;
				if( Reader )
				{
					const int64 BytesPerCall = 256*1024;
					const int64 FileSize = Reader->TotalSize();
					FileDataArray.Reset();
					FileDataArray.AddUninitialized( FileSize );
					int64 BytesRead = 0;
					while ( BytesRead < FileSize && !FBuildPatchInstallError::HasFatalError() )
					{
						const int64 ReadLen = FMath::Min<int64>( BytesPerCall, FileSize - BytesRead );
						Reader->Serialize( FileDataArray.GetData() + BytesRead, ReadLen );
						BytesRead += ReadLen;
						OnDownloadProgress( NextGuid, BytesRead );
					}
					bSuccess = Reader->Close() && !FBuildPatchInstallError::HasFatalError();
					delete Reader;
				}
				OnDownloadComplete(NextGuid, DownloadUrl, FileDataArray, bSuccess, INDEX_NONE);
				FileDataArray.Empty();
			};
			Async(EAsyncExecution::ThreadPool, MoveTemp(Task));
		}
	}

	// Wait for all canceled downloads to complete
	InFlightDownloadsLock.Lock();
	while (InFlightDownloads.Num() > 0)
	{
		InFlightKeys.Empty();
		InFlightDownloads.GetKeys(InFlightKeys);
		for (const FGuid& InFlightKey : InFlightKeys)
		{
			// If the download is not running
			if (InFlightDownloads[InFlightKey].StateFlag.GetValue() != EDownloadState::DownloadRunning)
			{
				InFlightDownloads.Remove(InFlightKey);
			}
		}
		InFlightDownloadsLock.Unlock();
		FPlatformProcess::Sleep(0.0f);
		InFlightDownloadsLock.Lock();
	}
	InFlightDownloadsLock.Unlock();

	SetIdle(true);
	SetRunning(false);
	if (bDownloadsStarted)
	{
		UpdateDownloadHealth(true);
	}
	return 0;
}

bool FBuildPatchDownloader::IsComplete()
{
	FScopeLock Lock( &FlagsLock );
	return !bIsRunning && bIsInited;
}

bool FBuildPatchDownloader::IsIdle()
{
	FScopeLock Lock( &FlagsLock );
	return bIsIdle;
}

bool FBuildPatchDownloader::IsDisconnected()
{
	FScopeLock Lock(&FlagsLock);
	return bIsDisconnected;
}

TArray< FBuildPatchDownloadRecord > FBuildPatchDownloader::GetDownloadRecordings()
{
	FScopeLock Lock( &DownloadRecordsLock );
	return DownloadRecords;
}

void FBuildPatchDownloader::AddChunkToDownload( const FGuid& Guid, const bool bInsertAtFront /*= false */ )
{
	check( Guid.IsValid() );
	FScopeLock Lock( &DataToDownloadLock );
	if( bInsertAtFront )
	{
		DataToDownload.Insert( Guid, 0 );
	}
	else
	{
		DataToDownload.Add( Guid );
	}
	DataToDownloadTotalBytes += InstallManifest->GetDataSize(Guid);
}

void FBuildPatchDownloader::AddChunksToDownload( const TArray< FGuid >& Guids, const bool bInsertAtFront /*= false */ )
{
	FScopeLock Lock( &DataToDownloadLock );
	for ( FGuid Guid: Guids )
	{
		check( Guid.IsValid() );
		if( bInsertAtFront )
		{
			DataToDownload.Insert( Guid, 0 );
		}
		else
		{
			DataToDownload.Add( Guid );
		}
		DataToDownloadTotalBytes += InstallManifest->GetDataSize(Guid);
	}
}

void FBuildPatchDownloader::ClearDownloadJobs()
{
	FScopeLock Lock( &DataToDownloadLock );
	DataToDownload.Empty();
}

void FBuildPatchDownloader::NotifyNoMoreChunksToAdd()
{
	FScopeLock Lock( &FlagsLock );
	bWaitingForJobs = false;
}

int32 FBuildPatchDownloader::GetNumChunksLeft()
{
	int32 Count = 0;
	{
		FScopeLock Lock( &DataToDownloadLock );
		Count += DataToDownload.Num();
	}
	{
		FScopeLock Lock( &InFlightDownloadsLock );
		Count += InFlightDownloads.Num();
	}
	return Count;
}

int64 FBuildPatchDownloader::GetNumBytesLeft()
{
	int64 Count = 0;
	// Count the bytes for each file in the queue
	{
		FScopeLock Lock(&DataToDownloadLock);
		for (auto DataToDownloadIt = DataToDownload.CreateConstIterator(); DataToDownloadIt; ++DataToDownloadIt)
		{
			const FGuid& DataGuid = *DataToDownloadIt;
			Count += InstallManifest->GetDataSize(DataGuid);
		}
	}
	// Count the bytes left for each in-flight file
	{
		FScopeLock Lock(&InFlightDownloadsLock);
		for (auto InFlightDownloadsIt = InFlightDownloads.CreateConstIterator(); InFlightDownloadsIt; ++InFlightDownloadsIt)
		{
			const FGuid& InFlightKey = InFlightDownloadsIt.Key();
			const FDownloadJob& InFlightJob = InFlightDownloadsIt.Value();
			Count += FMath::Max<int64>(InstallManifest->GetDataSize(InFlightKey) - InFlightJob.DownloadRecord.DownloadSize, 0);
		}
	}
	return Count;
}

int64 FBuildPatchDownloader::GetNumBytesQueued()
{
	FScopeLock Lock(&DataToDownloadLock);
	return DataToDownloadTotalBytes;
}

int32 FBuildPatchDownloader::GetByteDownloadCountReset()
{
	return ByteDownloadCount.Reset();
}

float FBuildPatchDownloader::GetDownloadSuccessRate()
{
	FScopeLock Lock(&FlagsLock);
	return ChunkSuccessRate;
}

EBuildPatchDownloadHealth FBuildPatchDownloader::GetDownloadHealth()
{
	FScopeLock Lock(&FlagsLock);
	return DownloadHealth;
}

TArray<float> FBuildPatchDownloader::GetDownloadHealthTimers()
{
	FScopeLock Lock(&FlagsLock);
	return HealthStateTimes;
}

int32 FBuildPatchDownloader::GetNumFailedDownloads()
{
	return NumFailedDownloads.GetValue();
}

int32 FBuildPatchDownloader::GetNumBadDownloads()
{
	return NumBadDownloads.GetValue();
}

void FBuildPatchDownloader::IncrementByteDownloadCount(const int32& NumBytes)
{
	ByteDownloadCount.Add( NumBytes );
	if (NumBytes > 0)
	{
		FPlatformAtomics::InterlockedExchange(&CyclesAtLastData, FStatsCollector::GetCycles());
	}
}

void FBuildPatchDownloader::SetRunning( bool bRunning )
{
	FScopeLock Lock( &FlagsLock );
	bIsRunning = bRunning;
}

void FBuildPatchDownloader::SetInited( bool bInited )
{
	FScopeLock Lock( &FlagsLock );
	bIsInited = bInited;
}

void FBuildPatchDownloader::SetIdle( bool bIdle )
{
	FScopeLock Lock( &FlagsLock );
	bIsIdle = bIdle;
}

void FBuildPatchDownloader::SetIsDisconnected(bool bInIsDisconnected)
{
	FScopeLock Lock(&FlagsLock);
	bIsDisconnected = bInIsDisconnected;
	UpdateDownloadHealth();
}

void FBuildPatchDownloader::SetSuccessRate(float InSuccessRate)
{
	FScopeLock Lock(&FlagsLock);
	ChunkSuccessRate = InSuccessRate;
	UpdateDownloadHealth();
}

void FBuildPatchDownloader::UpdateDownloadHealth(bool bFlushTimer)
{
	FScopeLock Lock(&FlagsLock);
	EBuildPatchDownloadHealth PreviousHealth = DownloadHealth;
	if (bIsDisconnected)
	{
		DownloadHealth = EBuildPatchDownloadHealth::Disconnected;
	}
	else if (ChunkSuccessRate >= HealthPercentages[(int32)EBuildPatchDownloadHealth::Excellent])
	{
		DownloadHealth = EBuildPatchDownloadHealth::Excellent;
	}
	else if (ChunkSuccessRate >= HealthPercentages[(int32)EBuildPatchDownloadHealth::Good])
	{
		DownloadHealth = EBuildPatchDownloadHealth::Good;
	}
	else if (ChunkSuccessRate >= HealthPercentages[(int32)EBuildPatchDownloadHealth::OK])
	{
		DownloadHealth = EBuildPatchDownloadHealth::OK;
	}
	else
	{
		DownloadHealth = EBuildPatchDownloadHealth::Poor;
	}
	if (PreviousHealth != DownloadHealth || bFlushTimer)
	{
		// Update time in state
		uint64 CyclesNow = FStatsCollector::GetCycles();
		HealthStateTimes[(int32)PreviousHealth] += FStatsCollector::CyclesToSeconds(CyclesNow - CyclesAtLastHealthState);
		FPlatformAtomics::InterlockedExchange(&CyclesAtLastHealthState, CyclesNow);
	}
}

bool FBuildPatchDownloader::ShouldBeRunning()
{
	FScopeLock Lock( &FlagsLock );
	return bWaitingForJobs || GetNumChunksLeft() > 0;
}

void FBuildPatchDownloader::HttpRequestProgress( FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived )
{
#if EXTRA_DOWNLOAD_LOGGING
	GWarn->Logf(TEXT("BuildPatchDownloader: Request %p BytesSent: %d BytesReceived: %d"), Request.Get(), BytesSent, BytesReceived);
#endif
	if ( !FBuildPatchInstallError::HasFatalError() )
	{
		FGuid DataGUID;
		FBuildPatchUtils::GetGUIDFromFilename( Request->GetURL(), DataGUID );
		OnDownloadProgress( DataGUID, BytesReceived );
	}
}

void FBuildPatchDownloader::HttpRequestComplete( FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded )
{
	// track the request if we want CDN analytics.
	FBuildPatchAnalytics::TrackRequest(Request);
	FGuid DataGUID;
	FBuildPatchUtils::GetGUIDFromFilename( Request->GetURL(), DataGUID );
	if( bSucceeded && Response.IsValid() && EHttpResponseCodes::IsOk( Response->GetResponseCode() ) )
	{
		OnDownloadComplete( DataGUID, Request->GetURL(), Response->GetContent(), true, Response->GetResponseCode() );
	}
	else
	{
		OnDownloadComplete( DataGUID, Request->GetURL(), TArray< uint8 >(), false, Response.IsValid() ? Response->GetResponseCode() : INDEX_NONE );
	}
}

void FBuildPatchDownloader::OnDownloadProgress( const FGuid& Guid, const int32& BytesSoFar )
{
	if ( !FBuildPatchInstallError::HasFatalError() )
	{
		FScopeLock ScopeLock( &InFlightDownloadsLock );
		FDownloadJob& CurrentJob = InFlightDownloads[ Guid ];
		const int32 DeltaBytes = BytesSoFar - CurrentJob.DownloadRecord.DownloadSize;
		CurrentJob.DownloadRecord.DownloadSize = BytesSoFar;
		IncrementByteDownloadCount( DeltaBytes );
#if EXTRA_DOWNLOAD_LOGGING
		// Log download progress
		const double tNow = FPlatformTime::Seconds();
		const double tDelta = tNow - CurrentJob.DownloadRecord.EndTime;
		const double tRunning = tNow - CurrentJob.DownloadRecord.StartTime;
		const double currSpeed = DeltaBytes / tDelta;
		const double avSpeed = BytesSoFar / tRunning;
		GWarn->Logf(TEXT("BuildPatchDownloader: Download %s Running:%.2fsec\tDownloaded:%d\tSpeed:%.0f\tAverageSpeed:%.0f (B/s)"), *Guid.ToString(), tRunning, BytesSoFar, currSpeed, avSpeed);
		CurrentJob.DownloadRecord.EndTime = tNow;
#endif
	}
}

void FBuildPatchDownloader::OnDownloadComplete( const FGuid& Guid, const FString& DownloadUrl, const TArray< uint8 >& DataArray, bool bSucceeded, int32 ResponseCode )
{
#if EXTRA_DOWNLOAD_LOGGING
	GWarn->Logf(TEXT("BuildPatchDownloader: OnDownloadComplete %s %s %d"), *Guid.ToString(), bSucceeded?TEXT("SUCCESS"):TEXT("FAIL"), ResponseCode);
#endif
	{
		FScopeLock ScopeLock(&InFlightDownloadsLock);
		FDownloadJob& CurrentJob = InFlightDownloads[Guid];

		// Safety check, if this fails, we will be causing thread access issues with Run() processing data on complete downloads.
		// The StateFlag acts as a thread-safe gate between access here and access there, InFlightDownloadsLock must be a short lived lock to avoid
		// stalling the main thread here
		check(CurrentJob.StateFlag.GetValue() == EDownloadState::DownloadRunning);

		const int32 RequestSize = DataArray.Num();
		const int32 DeltaBytes = FMath::Clamp<int32>(RequestSize - CurrentJob.DownloadRecord.DownloadSize, 0, RequestSize);
		IncrementByteDownloadCount(DeltaBytes);
		CurrentJob.DataArray.Empty(RequestSize);
		CurrentJob.DataArray.Append(DataArray);
		CurrentJob.DownloadRecord.DownloadSize = RequestSize;
		CurrentJob.DownloadRecord.EndTime = FPlatformTime::Seconds();
		CurrentJob.StateFlag.Set(bSucceeded ? EDownloadState::DownloadSuccess : EDownloadState::DownloadFail);
		CurrentJob.ResponseCode = ResponseCode;
		CurrentJob.DownloadUrl = DownloadUrl;
	}
}

bool FBuildPatchDownloader::GetNextDownload( FGuid& Job )
{
	// Check ChunkCache is ready
	const bool bIsFileData = InstallManifest->IsFileDataManifest();
	if( FBuildPatchInstallError::HasFatalError() || ( !bIsFileData && !FBuildPatchChunkCache::IsAvailable() ) )
	{
		return false;
	}
	// Check retry list, or number of running downloads
	bool bAllowedDownload = DataToRetry.Num() > 0;
	if( !bAllowedDownload )
	{
		FScopeLock ScopeLock( &InFlightDownloadsLock );
		bAllowedDownload = InFlightDownloads.Num() < NUM_DOWNLOAD_THREADS;
	}
	// Find a guid that the cache wants
	if( bAllowedDownload )
	{
		// Check retrying a download
		bool bRetrying = false;
		int32 NumDataToRetry = DataToRetry.Num();
		if( DataToRetry.Num() > 0 )
		{
			FScopeLock ScopeLock( &InFlightDownloadsLock );
			for (int32 idx = 0; idx < DataToRetry.Num(); ++idx)
			{
				FDownloadJob& InFlightJob = InFlightDownloads[DataToRetry[idx]];
				const double TimeSinceFail = FPlatformTime::Seconds() - InFlightJob.DownloadRecord.EndTime;
				bAllowedDownload = TimeSinceFail >= GetRetryDelay(InFlightJob.RetryCount.GetValue());
				if( bAllowedDownload )
				{
					Job = InFlightJob.Guid;
					DataToRetry.RemoveAt(idx);
					bRetrying = true;
					break;
				}
			}
		}
		// If we are not retrying, and not all chunks are failing, allow another chunk
		if (!bRetrying && NumDataToRetry < NUM_DOWNLOAD_THREADS)
		{
			bAllowedDownload = false;
			DataToDownloadLock.Lock();
			for( int32 DataToDownloadIdx = 0; DataToDownloadIdx < DataToDownload.Num(); ++DataToDownloadIdx )
			{
				const FGuid& DownloadJob = DataToDownload[ DataToDownloadIdx ];
				check( DownloadJob.IsValid() );
				bAllowedDownload = bIsFileData || FBuildPatchChunkCache::Get().ReserveChunkInventorySlot( DownloadJob );
				if( bAllowedDownload )
				{
					Job = DownloadJob;
					DataToDownload.RemoveAt( DataToDownloadIdx );
					break;
				}
				// Allow other threads chance
				int32 PrevNum = DataToDownload.Num();
				DataToDownloadLock.Unlock();
				FPlatformProcess::Sleep( 0.0f );
				DataToDownloadLock.Lock();
				// Start over if array changed
				if( PrevNum != DataToDownload.Num() )
				{
					DataToDownloadIdx = 0;
				}
			}
			DataToDownloadLock.Unlock();
		}
	}
	// If allowed to download, add to InFlight
	if( bAllowedDownload )
	{
		check( Job.IsValid() );
		FScopeLock ScopeLock( &InFlightDownloadsLock );
		FDownloadJob& InFlightJob = InFlightDownloads.FindOrAdd( Job );
		InFlightJob.Guid = Job;
		InFlightJob.DataArray.Empty();
		InFlightJob.ResponseCode = 0;
		InFlightJob.StateFlag.Set( EDownloadState::DownloadRunning );
		InFlightJob.DownloadRecord.DownloadSize = 0;
		InFlightJob.DownloadRecord.StartTime = FPlatformTime::Seconds();
	}
	return bAllowedDownload;
}

float FBuildPatchDownloader::GetRetryDelay(int32 RetryCount)
{
	const int32 RetryTimeIndex = FMath::Clamp<int32>(RetryCount - 1, 0, RetryDelayTimes.Num() -1);
	return RetryDelayTimes[RetryTimeIndex];
}

/* FBuildPatchChunkCache system singleton setup
*****************************************************************************/
TSharedPtr< FBuildPatchDownloader, ESPMode::ThreadSafe > FBuildPatchDownloader::SingletonInstance = NULL;

void FBuildPatchDownloader::Create(const FString& SaveDirectory, const TArray<FString>& CloudDirectories, const FBuildPatchAppManifestRef& InstallManifest, FBuildPatchProgress* BuildProgress)
{
	// We won't allow misuse of these functions
	check( !SingletonInstance.IsValid() );
	SingletonInstance = MakeShareable(new FBuildPatchDownloader(SaveDirectory, CloudDirectories, InstallManifest, BuildProgress));
}

FBuildPatchDownloader& FBuildPatchDownloader::Get()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	return *SingletonInstance.Get();
}

void FBuildPatchDownloader::Shutdown()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	SingletonInstance.Reset();
}
