// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#if WITH_BUILDPATCHGENERATION

#include "BuildPatchServicesPrivatePCH.h"

#include "DataScanner.h"

#include "ThreadingBase.h"
#include "Async.h"
#include "../BuildPatchHash.h"
#include "../BuildPatchChunk.h"
#include "../BuildPatchManifest.h"
#include "StatsCollector.h"

namespace BuildPatchServices
{
	const uint32 WindowSize = FBuildPatchData::ChunkDataSize;

	struct FScopeCounter
	{
	public:
		FScopeCounter(FThreadSafeCounter* Counter);
		~FScopeCounter();

	private:
		FThreadSafeCounter* Counter;
	};

	class FDataStructure
	{
	public:
		FDataStructure(const uint64 DataOffset);
		~FDataStructure();

		FORCEINLINE const FGuid& GetCurrentChunkId() const;

		FORCEINLINE void PushKnownChunk(const FGuid& MatchId, uint32 DataSize);
		FORCEINLINE void PushUnknownByte();

		FORCEINLINE void RemapCurrentChunk(const FGuid& NewId);
		FORCEINLINE void CompleteCurrentChunk();

		FORCEINLINE TArray<FChunkPart> GetFinalDataStructure();

	private:
		FGuid NewChunkGuid;
		TArray<FChunkPart> DataStructure;
	};

	class FDataScannerImpl
		: public FDataScanner
	{
	public:
		FDataScannerImpl(const uint64 DataOffset, const TArray<uint8>& Data, const FCloudEnumerationRef& CloudEnumeration, const FDataMatcherRef& DataMatcher, const FStatsCollectorRef& StatsCollector);
		virtual ~FDataScannerImpl();

		virtual bool IsComplete() override;
		virtual FDataScanResult GetResultWhenComplete() override;

	private:
		FDataScanResult ScanData();
		bool ProcessCurrentWindow();
		bool FindExistingChunk(const TMap<uint64, TSet<FGuid>>& ChunkLookup, TMap<FGuid, FSHAHash>& ChunkShaHashes, uint64 ChunkHash, const FRollingHash<WindowSize>& ChunkBuffer, FGuid& OutMatchedChunk);
		bool FindExistingChunk(const TMap<uint64, TSet<FGuid>>& ChunkLookup, TMap<FGuid, FSHAHash>& ChunkShaHashes, uint64 ChunkHash, const TArray<uint8>& ChunkBuffer, FGuid& OutMatchedChunk);

	private:
		const uint64 DataStartOffset;
		TArray<uint8> Data;
		FCloudEnumerationRef CloudEnumeration;
		FDataMatcherRef DataMatcher;
		FStatsCollectorRef StatsCollector;
		FThreadSafeBool bIsComplete;
		FThreadSafeBool bShouldAbort;
		TFuture<FDataScanResult> FutureResult;
		volatile int64* StatCreatedScanners;
		volatile int64* StatRunningScanners;
		volatile int64* StatCpuTime;
		volatile int64* StatConsumeBytesTime;
		volatile int64* StatFindMatchTime;
		volatile int64* StatDataMatchTime;
		volatile int64* StatChunkWriterTime;
		volatile int64* StatHashCollisions;
		volatile int64* StatChunkDataChecks;
		volatile int64* StatChunkDataMatches;
		volatile int64* StatMissingChunks;
		volatile int64* StatMatchedData;
		volatile int64* StatExtraData;

	public:
		static FThreadSafeCounter NumIncompleteScanners;
		static FThreadSafeCounter NumRunningScanners;
	};

	FScopeCounter::FScopeCounter(FThreadSafeCounter* InCounter)
		: Counter(InCounter)
	{
		Counter->Increment();
	}

	FScopeCounter::~FScopeCounter()
	{
		Counter->Decrement();
	}

	FDataStructure::FDataStructure(const uint64 DataOffset)
	{
		DataStructure.AddZeroed();
		DataStructure.Top().DataOffset = DataOffset;
		DataStructure.Top().ChunkGuid = NewChunkGuid = FGuid::NewGuid();
	}

	FDataStructure::~FDataStructure()
	{
	}

	const FGuid& FDataStructure::GetCurrentChunkId() const
	{
		return NewChunkGuid;
	}

	void FDataStructure::PushKnownChunk(const FGuid& PotentialMatch, uint32 NumDataInWindow)
	{
		if (DataStructure.Top().PartSize > 0)
		{
			// Add for matched
			DataStructure.AddUninitialized();
			// Add for next
			DataStructure.AddUninitialized();

			// Fill out info
			FChunkPart& PreviousChunkPart = DataStructure[DataStructure.Num() - 3];
			FChunkPart& MatchedChunkPart = DataStructure[DataStructure.Num() - 2];
			FChunkPart& NextChunkPart = DataStructure[DataStructure.Num() - 1];

			MatchedChunkPart.DataOffset = PreviousChunkPart.DataOffset + PreviousChunkPart.PartSize;
			MatchedChunkPart.PartSize = NumDataInWindow;
			MatchedChunkPart.ChunkOffset = 0;
			MatchedChunkPart.ChunkGuid = PotentialMatch;

			NextChunkPart.DataOffset = MatchedChunkPart.DataOffset + MatchedChunkPart.PartSize;
			NextChunkPart.ChunkGuid = PreviousChunkPart.ChunkGuid;
			NextChunkPart.ChunkOffset = PreviousChunkPart.ChunkOffset + PreviousChunkPart.PartSize;
			NextChunkPart.PartSize = 0;
		}
		else
		{
			// Add for next
			DataStructure.AddZeroed();

			// Fill out info
			FChunkPart& MatchedChunkPart = DataStructure[DataStructure.Num() - 2];
			FChunkPart& NextChunkPart = DataStructure[DataStructure.Num() - 1];

			NextChunkPart.ChunkOffset = MatchedChunkPart.ChunkOffset;
			NextChunkPart.ChunkGuid = NewChunkGuid;
			NextChunkPart.DataOffset = MatchedChunkPart.DataOffset + NumDataInWindow;

			MatchedChunkPart.PartSize = NumDataInWindow;
			MatchedChunkPart.ChunkOffset = 0;
			MatchedChunkPart.ChunkGuid = PotentialMatch;
		}
	}

	void FDataStructure::PushUnknownByte()
	{
		DataStructure.Top().PartSize++;
	}

	void FDataStructure::RemapCurrentChunk(const FGuid& NewId)
	{
		for (auto& DataPiece : DataStructure)
		{
			if (DataPiece.ChunkGuid == NewChunkGuid)
			{
				DataPiece.ChunkGuid = NewId;
			}
		}
		NewChunkGuid = NewId;
	}

	void FDataStructure::CompleteCurrentChunk()
	{
		DataStructure.AddZeroed();
		FChunkPart& PreviousPart = DataStructure[DataStructure.Num() - 2];

		// Create next chunk
		NewChunkGuid = FGuid::NewGuid();
		DataStructure.Top().DataOffset = PreviousPart.DataOffset + PreviousPart.PartSize;
		DataStructure.Top().ChunkGuid = NewChunkGuid;
	}

	TArray<FChunkPart> FDataStructure::GetFinalDataStructure()
	{
		if (DataStructure.Top().PartSize == 0)
		{
			DataStructure.Pop(false);
		}
		return MoveTemp(DataStructure);
	}

	FDataScannerImpl::FDataScannerImpl(const uint64 InDataOffset, const TArray<uint8>& InData, const FCloudEnumerationRef& InCloudEnumeration, const FDataMatcherRef& InDataMatcher, const FStatsCollectorRef& InStatsCollector)
		: DataStartOffset(InDataOffset)
		, Data(InData)
		, CloudEnumeration(InCloudEnumeration)
		, DataMatcher(InDataMatcher)
		, StatsCollector(InStatsCollector)
		, bIsComplete(false)
		, bShouldAbort(false)
	{
		// Create statistics
		StatCreatedScanners = StatsCollector->CreateStat(TEXT("Scanner: Created Scanners"), EStatFormat::Value);
		StatRunningScanners = StatsCollector->CreateStat(TEXT("Scanner: Running Scanners"), EStatFormat::Value);
		StatCpuTime = StatsCollector->CreateStat(TEXT("Scanner: CPU Time"), EStatFormat::Timer);
		StatConsumeBytesTime = StatsCollector->CreateStat(TEXT("Scanner: Consume Bytes Time"), EStatFormat::Timer);
		StatFindMatchTime = StatsCollector->CreateStat(TEXT("Scanner: Find Match Time"), EStatFormat::Timer);
		StatDataMatchTime = StatsCollector->CreateStat(TEXT("Scanner: Data Match Time"), EStatFormat::Timer);
		StatChunkWriterTime = StatsCollector->CreateStat(TEXT("Scanner: Chunk Writer Time"), EStatFormat::Timer);
		StatHashCollisions = StatsCollector->CreateStat(TEXT("Scanner: Hash Collisions"), EStatFormat::Value);
		StatChunkDataChecks = StatsCollector->CreateStat(TEXT("Scanner: Chunk Data Checks"), EStatFormat::Value);
		StatChunkDataMatches = StatsCollector->CreateStat(TEXT("Scanner: Chunk Data Matches"), EStatFormat::Value);
		StatMissingChunks = StatsCollector->CreateStat(TEXT("Scanner: Missing Chunks"), EStatFormat::Value);
		StatMatchedData = StatsCollector->CreateStat(TEXT("Scanner: Matched Data"), EStatFormat::DataSize);
		StatExtraData = StatsCollector->CreateStat(TEXT("Scanner: Extra Data"), EStatFormat::DataSize);
		// Queue thread
		NumIncompleteScanners.Increment();
		TFunction<FDataScanResult()> Task = [this]()
		{
			FDataScanResult Result = ScanData();
			FDataScannerImpl::NumIncompleteScanners.Decrement();
			return MoveTemp(Result);
		};
		FutureResult = Async(EAsyncExecution::ThreadPool, Task);
	}

	FDataScannerImpl::~FDataScannerImpl()
	{
		// Make sure the task is complete
		bShouldAbort = true;
		FutureResult.Wait();
	}

	bool FDataScannerImpl::IsComplete()
	{
		return bIsComplete;
	}

	FDataScanResult FDataScannerImpl::GetResultWhenComplete()
	{
		return MoveTemp(FutureResult.Get());
	}

	bool FDataScannerImpl::FindExistingChunk(const TMap<uint64, TSet<FGuid>>& ChunkLookup, TMap<FGuid, FSHAHash>& ChunkShaHashes, uint64 ChunkHash, const FRollingHash<WindowSize>& RollingHash, FGuid& OutMatchedChunk)
	{
		FStatsScopedTimer FindTimer(StatFindMatchTime);
		bool bFoundChunkMatch = false;
		if (ChunkLookup.Contains(ChunkHash))
		{
			FSHAHash ChunkSha;
			RollingHash.GetWindowData().GetShaHash(ChunkSha);
			for (FGuid& PotentialMatch : ChunkLookup.FindRef(ChunkHash))
			{
				// Use sha if we have it
				if (ChunkShaHashes.Contains(PotentialMatch))
				{
					if(ChunkSha == ChunkShaHashes[PotentialMatch])
					{
						bFoundChunkMatch = true;
						OutMatchedChunk = PotentialMatch;
						break;
					}
				}
				else
				{
					// Otherwise compare data
					TArray<uint8> SerialBuffer;
					FStatsScopedTimer DataMatchTimer(StatDataMatchTime);
					FStatsCollector::Accumulate(StatChunkDataChecks, 1);
					SerialBuffer.AddUninitialized(WindowSize);
					RollingHash.GetWindowData().Serialize(SerialBuffer.GetData());
					bool ChunkFound = false;
					if (DataMatcher->CompareData(PotentialMatch, ChunkHash, SerialBuffer, ChunkFound))
					{
						FStatsCollector::Accumulate(StatChunkDataMatches, 1);
						ChunkShaHashes.Add(PotentialMatch, ChunkSha);
						bFoundChunkMatch = true;
						OutMatchedChunk = PotentialMatch;
						break;
					}
					else if(!ChunkFound)
					{
						FStatsCollector::Accumulate(StatMissingChunks, 1);
					}
				}
				FStatsCollector::Accumulate(StatHashCollisions, 1);
			}
		}
		return bFoundChunkMatch;
	}

	bool FDataScannerImpl::FindExistingChunk(const TMap<uint64, TSet<FGuid>>& ChunkLookup, TMap<FGuid, FSHAHash>& ChunkShaHashes, uint64 ChunkHash, const TArray<uint8>& ChunkBuffer, FGuid& OutMatchedChunk)
	{
		FStatsScopedTimer FindTimer(StatFindMatchTime);
		bool bFoundChunkMatch = false;
		if (ChunkLookup.Contains(ChunkHash))
		{
			FSHAHash ChunkSha;
			FSHA1::HashBuffer(ChunkBuffer.GetData(), ChunkBuffer.Num(), ChunkSha.Hash);
			for (FGuid& PotentialMatch : ChunkLookup.FindRef(ChunkHash))
			{
				// Use sha if we have it
				if (ChunkShaHashes.Contains(PotentialMatch))
				{
					if(ChunkSha == ChunkShaHashes[PotentialMatch])
					{
						bFoundChunkMatch = true;
						OutMatchedChunk = PotentialMatch;
						break;
					}
				}
				else
				{
					// Otherwise compare data
					FStatsScopedTimer DataMatchTimer(StatDataMatchTime);
					FStatsCollector::Accumulate(StatChunkDataChecks, 1);
					bool ChunkFound = false;
					if (DataMatcher->CompareData(PotentialMatch, ChunkHash, ChunkBuffer, ChunkFound))
					{
						FStatsCollector::Accumulate(StatChunkDataMatches, 1);
						ChunkShaHashes.Add(PotentialMatch, ChunkSha);
						bFoundChunkMatch = true;
						OutMatchedChunk = PotentialMatch;
						break;
					}
					else if (!ChunkFound)
					{
						FStatsCollector::Accumulate(StatMissingChunks, 1);
					}
				}
				FStatsCollector::Accumulate(StatHashCollisions, 1);
			}
		}
		return bFoundChunkMatch;
	}

	FDataScanResult FDataScannerImpl::ScanData()
	{
		// Count running scanners
		FScopeCounter ScopeCounter(&NumRunningScanners);
		FStatsCollector::Accumulate(StatCreatedScanners, 1);
		FStatsCollector::Accumulate(StatRunningScanners, 1);

		// Init data
		FRollingHash<WindowSize> RollingHash;
		FChunkWriter ChunkWriter(FBuildPatchServicesModule::GetCloudDirectory(), StatsCollector);
		FDataStructure DataStructure(DataStartOffset);
		TMap<FGuid, FChunkInfo> ChunkInfoLookup;
		TArray<uint8> ChunkBuffer;
		TArray<uint8> NewChunkBuffer;
		uint32 PaddedZeros = 0;
		ChunkInfoLookup.Reserve(Data.Num() / WindowSize);
		ChunkBuffer.SetNumUninitialized(WindowSize);
		NewChunkBuffer.Reserve(WindowSize);

		// Get a copy of the chunk inventory
		TMap<uint64, TSet<FGuid>> ChunkInventory = CloudEnumeration->GetChunkInventory();
		TMap<FGuid, int64> ChunkFileSizes = CloudEnumeration->GetChunkFileSizes();
		TMap<FGuid, FSHAHash> ChunkShaHashes = CloudEnumeration->GetChunkShaHashes();

		// Loop over and process all data
		FGuid MatchedChunk;
		uint64 TempTimer;
		uint64 CpuTimer;
		FStatsCollector::AccumulateTimeBegin(CpuTimer);
		for (int32 idx = 0; (idx < Data.Num() || PaddedZeros < WindowSize) && !bShouldAbort; ++idx)
		{
			// Consume data
			const uint32 NumDataNeeded = RollingHash.GetNumDataNeeded();
			if (NumDataNeeded > 0)
			{
				FStatsScopedTimer ConsumeTimer(StatConsumeBytesTime);
				uint32 NumConsumedBytes = 0;
				if (idx < Data.Num())
				{
					NumConsumedBytes = FMath::Min<uint32>(NumDataNeeded, Data.Num() - idx);
					RollingHash.ConsumeBytes(&Data[idx], NumConsumedBytes);
					idx += NumConsumedBytes - 1;
				}
				// Zero Pad?
				if (NumConsumedBytes < NumDataNeeded)
				{
					TArray<uint8> Zeros;
					Zeros.AddZeroed(NumDataNeeded - NumConsumedBytes);
					RollingHash.ConsumeBytes(Zeros.GetData(), Zeros.Num());
					PaddedZeros = Zeros.Num();
				}
				check(RollingHash.GetNumDataNeeded() == 0);
				continue;
			}

			const uint64 NumDataInWindow = WindowSize - PaddedZeros;
			const uint64 WindowHash = RollingHash.GetWindowHash();
			// Try find match
			if (FindExistingChunk(ChunkInventory, ChunkShaHashes, WindowHash, RollingHash, MatchedChunk))
			{
				// Push the chunk to the structure
				DataStructure.PushKnownChunk(MatchedChunk, NumDataInWindow);
				FChunkInfo& ChunkInfo = ChunkInfoLookup.FindOrAdd(MatchedChunk);
				ChunkInfo.Hash = WindowHash;
				ChunkInfo.ShaHash = ChunkShaHashes[MatchedChunk];
				ChunkInfo.IsNew = false;
				FStatsCollector::Accumulate(StatMatchedData, NumDataInWindow);
				// Clear matched window
				RollingHash.Clear();
				// Decrement idx to include current byte in next window
				--idx;
			}
			else
			{
				// Collect unrecognized bytes
				NewChunkBuffer.Add(RollingHash.GetWindowData().Bottom());
				DataStructure.PushUnknownByte();
				if (NumDataInWindow == 1)
				{
					NewChunkBuffer.AddZeroed(WindowSize - NewChunkBuffer.Num());
				}
				if (NewChunkBuffer.Num() == WindowSize)
				{
					const uint64 NewChunkHash = FRollingHash<WindowSize>::GetHashForDataSet(NewChunkBuffer.GetData());
					if (FindExistingChunk(ChunkInventory, ChunkShaHashes, NewChunkHash, NewChunkBuffer, MatchedChunk))
					{
						DataStructure.RemapCurrentChunk(MatchedChunk);
						FChunkInfo& ChunkInfo = ChunkInfoLookup.FindOrAdd(MatchedChunk);
						ChunkInfo.Hash = NewChunkHash;
						ChunkInfo.ShaHash = ChunkShaHashes[MatchedChunk];
						ChunkInfo.IsNew = false;
						FStatsCollector::Accumulate(StatMatchedData, WindowSize);
					}
					else
					{
						FStatsScopedTimer ChunkWriterTimer(StatChunkWriterTime);
						const FGuid& NewChunkGuid = DataStructure.GetCurrentChunkId();
						FStatsCollector::AccumulateTimeEnd(StatCpuTime, CpuTimer);
						ChunkWriter.QueueChunk(NewChunkBuffer.GetData(), NewChunkGuid, NewChunkHash);
						FStatsCollector::AccumulateTimeBegin(CpuTimer);
						FChunkInfo& ChunkInfo = ChunkInfoLookup.FindOrAdd(NewChunkGuid);
						ChunkInfo.Hash = NewChunkHash;
						ChunkInfo.IsNew = true;
						FSHA1::HashBuffer(NewChunkBuffer.GetData(), NewChunkBuffer.Num(), ChunkInfo.ShaHash.Hash);
						ChunkShaHashes.Add(NewChunkGuid, ChunkInfo.ShaHash);
						FStatsCollector::Accumulate(StatExtraData, NewChunkBuffer.Num());
					}
					DataStructure.CompleteCurrentChunk();
					NewChunkBuffer.Empty(WindowSize);
				}

				// Roll byte into window
				if (idx < Data.Num())
				{
					RollingHash.RollForward(Data[idx]);
				}
				else
				{
					RollingHash.RollForward(0);
					++PaddedZeros;
				}
			}
		}

		// Collect left-overs
		if (NewChunkBuffer.Num() > 0)
		{
			NewChunkBuffer.AddZeroed(WindowSize - NewChunkBuffer.Num());
			const uint64 NewChunkHash = FRollingHash<WindowSize>::GetHashForDataSet(NewChunkBuffer.GetData());
			if (FindExistingChunk(ChunkInventory, ChunkShaHashes, NewChunkHash, NewChunkBuffer, MatchedChunk))
			{
				// Setup chunk info for a match
				DataStructure.RemapCurrentChunk(MatchedChunk);
				FChunkInfo& ChunkInfo = ChunkInfoLookup.FindOrAdd(MatchedChunk);
				ChunkInfo.Hash = NewChunkHash;
				ChunkInfo.ShaHash = ChunkShaHashes[MatchedChunk];
				ChunkInfo.IsNew = false;
			}
			else
			{
				// Save the final chunk if no match
				FStatsScopedTimer ChunkWriterTimer(StatChunkWriterTime);
				const FGuid& NewChunkGuid = DataStructure.GetCurrentChunkId();
				FStatsCollector::AccumulateTimeEnd(StatCpuTime, CpuTimer);
				ChunkWriter.QueueChunk(NewChunkBuffer.GetData(), NewChunkGuid, NewChunkHash);
				FStatsCollector::AccumulateTimeBegin(CpuTimer);
				FChunkInfo& ChunkInfo = ChunkInfoLookup.FindOrAdd(NewChunkGuid);
				ChunkInfo.Hash = NewChunkHash;
				ChunkInfo.IsNew = true;
				FSHA1::HashBuffer(NewChunkBuffer.GetData(), NewChunkBuffer.Num(), ChunkInfo.ShaHash.Hash);
				ChunkShaHashes.Add(NewChunkGuid, ChunkInfo.ShaHash);
				FStatsCollector::Accumulate(StatExtraData, NewChunkBuffer.Num());
			}
		}
		FStatsCollector::AccumulateTimeEnd(StatCpuTime, CpuTimer);

		// Wait for the chunk writer to finish, and fill out chunk file sizes
		FStatsCollector::AccumulateTimeBegin(TempTimer);
		ChunkWriter.NoMoreChunks();
		ChunkWriter.WaitForThread();
		ChunkWriter.GetChunkFilesizes(ChunkFileSizes);
		FStatsCollector::AccumulateTimeEnd(StatChunkWriterTime, TempTimer);

		// Fill out chunk file sizes
		FStatsCollector::AccumulateTimeBegin(CpuTimer);
		for (auto& ChunkInfo : ChunkInfoLookup)
		{
			ChunkInfo.Value.ChunkFileSize = ChunkFileSizes[ChunkInfo.Key];
		}

		// Empty data to save RAM
		Data.Empty();
		FStatsCollector::AccumulateTimeEnd(StatCpuTime, CpuTimer);

		FStatsCollector::Accumulate(StatRunningScanners, -1);
		bIsComplete = true;
		return FDataScanResult(
			MoveTemp(DataStructure.GetFinalDataStructure()),
			MoveTemp(ChunkInfoLookup));
	}

	FThreadSafeCounter FDataScannerImpl::NumIncompleteScanners;
	FThreadSafeCounter FDataScannerImpl::NumRunningScanners;

	int32 FDataScannerCounter::GetNumIncompleteScanners()
	{
		return FDataScannerImpl::NumIncompleteScanners.GetValue();
	}

	int32 FDataScannerCounter::GetNumRunningScanners()
	{
		return FDataScannerImpl::NumRunningScanners.GetValue();
	}

	FDataScannerRef FDataScannerFactory::Create(const uint64 DataOffset, const TArray<uint8>& Data, const FCloudEnumerationRef& CloudEnumeration, const FDataMatcherRef& DataMatcher, const FStatsCollectorRef& StatsCollector)
	{
		return MakeShareable(new FDataScannerImpl(DataOffset, Data, CloudEnumeration, DataMatcher, StatsCollector));
	}
}

#endif
