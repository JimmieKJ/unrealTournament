// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"
#include "DataScanner.h"
#include "ThreadingBase.h"
#include "Async.h"

namespace BuildPatchServices
{
	const uint32 WindowSize = FBuildPatchData::ChunkDataSize;

	class FDataScanner
		: public IDataScanner
	{
	public:
		FDataScanner(const TArray<uint8>& Data, const ICloudEnumerationRef& CloudEnumeration, const FStatsCollectorRef& StatsCollector);
		virtual ~FDataScanner();

		virtual bool IsComplete() override;
		virtual TArray<FChunkMatch> GetResultWhenComplete() override;

	private:
		uint32 ConsumeData(const uint8* Data, uint32 DataLen);
		bool FindChunkDataMatch(FGuid& ChunkMatch, FSHAHash& ChunkSha);
		TArray<FChunkMatch> ScanData();

	private:
		const TArray<uint8>& Data;
		ICloudEnumerationRef CloudEnumeration;
		FStatsCollectorRef StatsCollector;
		FThreadSafeBool bIsComplete;
		FThreadSafeBool bShouldAbort;
		TFuture<TArray<FChunkMatch>> FutureResult;
		FRollingHash<WindowSize> RollingHash;
		TMap<uint64, TSet<FGuid>> ChunkInventory;
		TMap<FGuid, FSHAHash> ChunkShaHashes;
		volatile int64* StatCreatedScanners;
		volatile int64* StatRunningScanners;
		volatile int64* StatCpuTime;
		volatile int64* StatRealTime;
		volatile int64* StatHashCollisions;
		volatile int64* StatProcessedData;
		volatile int64* StatProcessingSpeed;

	public:
		static FThreadSafeCounter NumIncompleteScanners;
		static FThreadSafeCounter NumRunningScanners;
	};

	FDataScanner::FDataScanner(const TArray<uint8>& InData, const ICloudEnumerationRef& InCloudEnumeration, const FStatsCollectorRef& InStatsCollector)
		: Data(InData)
		, CloudEnumeration(InCloudEnumeration)
		, StatsCollector(InStatsCollector)
		, bIsComplete(false)
		, bShouldAbort(false)
	{
		// Create statistics.
		StatCreatedScanners = StatsCollector->CreateStat(TEXT("Scanner: Created Scanners"), EStatFormat::Value);
		StatRunningScanners = StatsCollector->CreateStat(TEXT("Scanner: Running Scanners"), EStatFormat::Value);
		StatCpuTime = StatsCollector->CreateStat(TEXT("Scanner: CPU Time"), EStatFormat::Timer);
		StatRealTime = StatsCollector->CreateStat(TEXT("Scanner: Real Time"), EStatFormat::Timer);
		StatHashCollisions = StatsCollector->CreateStat(TEXT("Scanner: Hash Collisions"), EStatFormat::Value);
		StatProcessedData = StatsCollector->CreateStat(TEXT("Scanner: Processed Data"), EStatFormat::DataSize);
		StatProcessingSpeed = StatsCollector->CreateStat(TEXT("Scanner: Processing Speed"), EStatFormat::DataSpeed);
		FStatsCollector::Accumulate(StatCreatedScanners, 1);

		// Queue thread.
		NumIncompleteScanners.Increment();
		TFunction<TArray<FChunkMatch>()> Task = [this]()
		{
			TArray<FChunkMatch> Result = ScanData();
			NumIncompleteScanners.Decrement();
			return MoveTemp(Result);
		};
		FutureResult = Async(EAsyncExecution::ThreadPool, MoveTemp(Task));
	}

	FDataScanner::~FDataScanner()
	{
		// Make sure the task is complete.
		bShouldAbort = true;
		FutureResult.Wait();
	}

	bool FDataScanner::IsComplete()
	{
		return bIsComplete;
	}

	TArray<FChunkMatch> FDataScanner::GetResultWhenComplete()
	{
		return MoveTemp(FutureResult.Get());
	}

	uint32 FDataScanner::ConsumeData(const uint8* DataPtr, uint32 DataLen)
	{
		uint32 NumDataNeeded = RollingHash.GetNumDataNeeded();
		if (NumDataNeeded > 0 && NumDataNeeded <= DataLen)
		{
			RollingHash.ConsumeBytes(DataPtr, NumDataNeeded);
			checkSlow(RollingHash.GetNumDataNeeded() == 0);
			return NumDataNeeded;
		}
		return 0;
	}

	bool FDataScanner::FindChunkDataMatch(FGuid& ChunkMatch, FSHAHash& ChunkSha)
	{
		TSet<FGuid>* PotentialMatches = ChunkInventory.Find(RollingHash.GetWindowHash());
		if (PotentialMatches != nullptr)
		{
			RollingHash.GetWindowData().GetShaHash(ChunkSha);
			for (const FGuid& PotentialMatch : *PotentialMatches)
			{
				FSHAHash* PotentialMatchSha = ChunkShaHashes.Find(PotentialMatch);
				if (PotentialMatchSha != nullptr && *PotentialMatchSha == ChunkSha)
				{
					ChunkMatch = PotentialMatch;
					return true;
				}
				else
				{
					FStatsCollector::Accumulate(StatHashCollisions, 1);
				}
			}
		}
		return false;
	}

	TArray<FChunkMatch> FDataScanner::ScanData()
	{
		static volatile int64 TempTimerValue;
		// The return data.
		TArray<FChunkMatch> DataScanResult;

		// Count running scanners.
		NumRunningScanners.Increment();

		// Get a copy of the chunk inventory.
		ChunkInventory = CloudEnumeration->GetChunkInventory();
		ChunkShaHashes = CloudEnumeration->GetChunkShaHashes();

		// Temp values.
		FGuid ChunkMatch;
		FSHAHash ChunkSha;
		uint64 CpuTimer;

		// Loop over and process all data.
		uint32 NextByte = ConsumeData(&Data[0], Data.Num());
		bool bScanningData = true;
		{
			FStatsCollector::AccumulateTimeBegin(CpuTimer);
			FStatsParallelScopeTimer ParallelScopeTimer(&TempTimerValue, StatRealTime, StatRunningScanners);
			while (bScanningData && !bShouldAbort)
			{
				// Check for a chunk match at this offset.
				if (FindChunkDataMatch(ChunkMatch, ChunkSha))
				{
					DataScanResult.Emplace(NextByte - WindowSize, ChunkMatch);
				}

				const bool bHasMoreData = NextByte < static_cast<uint32>(Data.Num());
				if (bHasMoreData)
				{
					// Roll over next byte.
					RollingHash.RollForward(Data[NextByte++]);
				}
				else
				{
					bScanningData = false;
				}
			}
			FStatsCollector::AccumulateTimeEnd(StatCpuTime, CpuTimer);
			FStatsCollector::Accumulate(StatProcessedData, Data.Num());
			FStatsCollector::Set(StatProcessingSpeed, *StatProcessedData / FStatsCollector::CyclesToSeconds(ParallelScopeTimer.GetCurrentTime()));
		}

		// Count running scanners.
		NumRunningScanners.Decrement();

		bIsComplete = true;
		return DataScanResult;
	}

	FThreadSafeCounter FDataScanner::NumIncompleteScanners;
	FThreadSafeCounter FDataScanner::NumRunningScanners;

	int32 FDataScannerCounter::GetNumIncompleteScanners()
	{
		return FDataScanner::NumIncompleteScanners.GetValue();
	}

	int32 FDataScannerCounter::GetNumRunningScanners()
	{
		return FDataScanner::NumRunningScanners.GetValue();
	}

	IDataScannerRef FDataScannerFactory::Create(const TArray<uint8>& Data, const ICloudEnumerationRef& CloudEnumeration, const FStatsCollectorRef& StatsCollector)
	{
		return MakeShareable(new FDataScanner(Data, CloudEnumeration, StatsCollector));
	}
}
