// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "BuildPatchServicesPrivatePCH.h"

#include "Core/BlockStructure.h"
#include "Generation/DataScanner.h"
#include "Generation/BuildStreamer.h"
#include "Generation/CloudEnumeration.h"
#include "Generation/ManifestBuilder.h"
#include "Generation/FileAttributesParser.h"

using namespace BuildPatchServices;

DECLARE_LOG_CATEGORY_EXTERN(LogPatchGeneration, Log, All);
DEFINE_LOG_CATEGORY(LogPatchGeneration);

namespace PatchGenerationHelpers
{
	bool SerialiseIntersection(const FBlockStructure& ByteStructure, const FBlockStructure& Intersection, FBlockStructure& SerialRanges)
	{
		FBlockStructure ActualIntersection = ByteStructure.Intersect(Intersection);
		uint64 ByteOffset = 0;
		const FBlockEntry* ByteBlock = ByteStructure.GetHead();
		const FBlockEntry* IntersectionBlock = ActualIntersection.GetHead();
		while (ByteBlock != nullptr && IntersectionBlock != nullptr)
		{
			uint64 ByteBlockEnd = ByteBlock->GetOffset() + ByteBlock->GetSize();
			if (ByteBlockEnd <= IntersectionBlock->GetOffset())
			{
				ByteOffset += ByteBlock->GetSize();
				ByteBlock = ByteBlock->GetNext();
				continue;
			}
			else
			{
				// Intersection must overlap.
				check(IntersectionBlock->GetOffset() >= ByteBlock->GetOffset());
				check(ByteBlockEnd >= (IntersectionBlock->GetOffset() + IntersectionBlock->GetSize()));
				uint64 Chop = IntersectionBlock->GetOffset() - ByteBlock->GetOffset();
				ByteOffset += Chop;
				SerialRanges.Add(ByteOffset, IntersectionBlock->GetSize());
				ByteOffset += ByteBlock->GetSize() - Chop;
				IntersectionBlock = IntersectionBlock->GetNext();
				ByteBlock = ByteBlock->GetNext();
			}
		}
		return true;
	}

	uint64 CountSerialBytes(const FBlockStructure& Structure)
	{
		uint64 Count = 0;
		const FBlockEntry* Block = Structure.GetHead();
		while (Block != nullptr)
		{
			Count += Block->GetSize();
			Block = Block->GetNext();
		}
		return Count;
	}
}

namespace BuildPatchServices
{
	struct FScannerDetails
	{
		FScannerDetails(int32 InLayer, uint64 InLayerOffset, bool bInIsFinalScanner, uint64 InPaddingSize, TArray<uint8> InData, FBlockStructure InStructure, const ICloudEnumerationRef& CloudEnumeration, const FStatsCollectorRef& StatsCollector)
			: Layer(InLayer)
			, LayerOffset(InLayerOffset)
			, bIsFinalScanner(bInIsFinalScanner)
			, PaddingSize(InPaddingSize)
			, Data(MoveTemp(InData))
			, Structure(MoveTemp(InStructure))
			, Scanner(FDataScannerFactory::Create(Data, CloudEnumeration, StatsCollector))
		{}

		int32 Layer;
		uint64 LayerOffset;
		bool bIsFinalScanner;
		uint64 PaddingSize;
		TArray<uint8> Data;
		FBlockStructure Structure;
		IDataScannerRef Scanner;
	};
}

bool FBuildDataGenerator::GenerateChunksManifestFromDirectory(const FBuildPatchSettings& Settings)
{
	uint64 StartTime = FStatsCollector::GetCycles();

	// Create a manifest details.
	FManifestDetails ManifestDetails;
	ManifestDetails.AppId = Settings.AppId;
	ManifestDetails.AppName = Settings.AppName;
	ManifestDetails.BuildVersion = Settings.BuildVersion;
	ManifestDetails.LaunchExe = Settings.LaunchExe;
	ManifestDetails.LaunchCommand = Settings.LaunchCommand;
	ManifestDetails.PrereqName = Settings.PrereqName;
	ManifestDetails.PrereqPath = Settings.PrereqPath;
	ManifestDetails.PrereqArgs = Settings.PrereqArgs;
	ManifestDetails.CustomFields = Settings.CustomFields;

	// Check for the required output filename.
	if (Settings.OutputFilename.IsEmpty())
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Manifest OutputFilename was not provided"));
		return false;
	}

	// Load the required file attributes.
	if (!Settings.AttributeListFile.IsEmpty())
	{
		FFileAttributesParserRef FileAttributesParser = FFileAttributesParserFactory::Create();
		if (!FileAttributesParser->ParseFileAttributes(Settings.AttributeListFile, ManifestDetails.FileAttributesMap))
		{
			UE_LOG(LogPatchGeneration, Error, TEXT("Attributes list file did not parse %s"), *Settings.AttributeListFile);
			return false;
		}
	}

	// Create stat collector.
	FStatsCollectorRef StatsCollector = FStatsCollectorFactory::Create();

	// Enumerate Chunks.
	const FDateTime Cutoff = Settings.bShouldHonorReuseThreshold ? FDateTime::UtcNow() - FTimespan::FromDays(Settings.DataAgeThreshold) : FDateTime::MinValue();
	ICloudEnumerationRef CloudEnumeration = FCloudEnumerationFactory::Create(FBuildPatchServicesModule::GetCloudDirectory(), Cutoff, StatsCollector);

	// Create a build streamer.
	FBuildStreamerRef BuildStream = FBuildStreamerFactory::Create(Settings.RootDirectory, Settings.IgnoreListFile, StatsCollector);

	// Create a chunk writer.
	FChunkWriter ChunkWriter(FBuildPatchServicesModule::GetCloudDirectory(), StatsCollector);

	// Output to log for builder info.
	UE_LOG(LogPatchGeneration, Log, TEXT("Running Chunks Patch Generation for: %u:%s %s"), Settings.AppId, *Settings.AppName, *Settings.BuildVersion);

	// Create the manifest builder.
	IManifestBuilderRef ManifestBuilder = FManifestBuilderFactory::Create(ManifestDetails);

	// Used to store data read lengths.
	uint32 ReadLen = 0;

	// The last time we logged out data processed.
	double LastProgressLog = FPlatformTime::Seconds();
	const double TimeGenStarted = LastProgressLog;

	// 32.5 chunk size data buffer, per scanner.
	const uint64 ScannerDataSize = FBuildPatchData::ChunkDataSize*32.5;
	const uint64 ScannerOverlapSize = FBuildPatchData::ChunkDataSize - 1;
	TArray<uint8> DataBuffer;

	// Setup Generation stats.
	auto* StatTotalTime = StatsCollector->CreateStat(TEXT("Generation: Total Time"), EStatFormat::Timer);

	// List of created scanners.
	TArray<TAutoPtr<FScannerDetails>> Scanners;

	// Tracking info per layer for rescanning.
	TMap<int32, FChunkMatch> LayerToLastChunkMatch;
	TMap<int32, uint64> LayerToProcessedCount;
	TMap<int32, uint64> LayerToScannerCount;
	TMap<int32, uint64> LayerToDataOffset;
	TMap<int32, TArray<uint8>> LayerToUnknownLayerData;
	TMap<int32, FBlockStructure> LayerToUnknownLayerStructure;
	TMap<int32, FBlockStructure> LayerToUnknownBuildStructure;

	// For future investigation.
	TArray<FChunkMatch> RejectedChunkMatches;
	FBlockStructure BuildSpaceRejectedStructure;

	// Keep a copy of the new chunk inventory.
	TMap<uint64, TSet<FGuid>> ChunkInventory;
	TMap<FGuid, FSHAHash> ChunkShaHashes;
	TSet<FGuid> ChunksReferenced;

	// Loop through all data.
	bool bHasUnknownData = true;
	while (!BuildStream->IsEndOfData() || Scanners.Num() > 0 || bHasUnknownData)
	{
		bool bScannerQueueFull = FDataScannerCounter::GetNumIncompleteScanners() > FDataScannerCounter::GetNumRunningScanners();
		if (!bScannerQueueFull)
		{
			// Create a scanner from new build data?
			if (!BuildStream->IsEndOfData())
			{
				// Keep the overlap data from previous scanner.
				int32 PreviousSize = DataBuffer.Num();
				uint64& DataOffset = LayerToDataOffset.FindOrAdd(0);
				if (PreviousSize > 0)
				{
					check(PreviousSize > ScannerOverlapSize);
					uint8* CopyTo = DataBuffer.GetData();
					uint8* CopyFrom = CopyTo + (PreviousSize - ScannerOverlapSize);
					FMemory::Memcpy(CopyTo, CopyFrom, ScannerOverlapSize);
					DataBuffer.SetNum(ScannerOverlapSize, false);
					DataOffset += ScannerDataSize - ScannerOverlapSize;
				}

				// Grab some data from the build stream.
				PreviousSize = DataBuffer.Num();
				DataBuffer.SetNumUninitialized(ScannerDataSize);
				ReadLen = BuildStream->DequeueData(DataBuffer.GetData() + PreviousSize, ScannerDataSize - PreviousSize);
				DataBuffer.SetNum(PreviousSize + ReadLen, false);

				// Pad scanner data if end of build
				uint64 PadSize = BuildStream->IsEndOfData() ? ScannerOverlapSize : 0;
				DataBuffer.AddZeroed(PadSize);

				// Create data processor.
				FBlockStructure Structure;
				Structure.Add(DataOffset, DataBuffer.Num() - PadSize);
				Scanners.Emplace(new FScannerDetails(0, DataOffset, BuildStream->IsEndOfData(), PadSize, DataBuffer, MoveTemp(Structure), CloudEnumeration, StatsCollector));
				LayerToScannerCount.FindOrAdd(0)++;
			}
		}

		// Grab any completed scanners?
		while (Scanners.Num() > 0 && Scanners[0]->Scanner->IsComplete())
		{
			FScannerDetails& Scanner = *Scanners[0];

			// Get the scan result.
			TArray<FChunkMatch> ChunkMatches = Scanner.Scanner->GetResultWhenComplete();

			// Create structures to track results in required spaces.
			FBlockStructure LayerSpaceUnknownStructure;
			FBlockStructure LayerSpaceKnownStructure;
			FBlockStructure BuildSpaceUnknownStructure;
			FBlockStructure BuildSpaceKnownStructure;
			BuildSpaceUnknownStructure.Add(Scanner.Structure);
			LayerSpaceUnknownStructure.Add(Scanner.LayerOffset, Scanner.Data.Num() - Scanner.PaddingSize);

			// Grab the last match from previous layer scanner, to handle to overlap.
			FChunkMatch* LayerLastChunkMatch = LayerToLastChunkMatch.Find(Scanner.Layer);
			if (LayerLastChunkMatch != nullptr)
			{
				// Track the last match in this range's layer structure.
				LayerSpaceUnknownStructure.Remove(LayerLastChunkMatch->DataOffset, FBuildPatchData::ChunkDataSize);
				LayerSpaceKnownStructure.Add(LayerLastChunkMatch->DataOffset, FBuildPatchData::ChunkDataSize);
				LayerSpaceKnownStructure.Remove(0, Scanner.LayerOffset);

				// Assert there is one or zero blocks
				check(LayerSpaceKnownStructure.GetHead() == LayerSpaceKnownStructure.GetFoot());

				// Track the last match in this range's build structure.
				if (LayerSpaceKnownStructure.GetHead() != nullptr)
				{
					uint64 FirstByte = LayerSpaceKnownStructure.GetHead()->GetOffset() - Scanner.LayerOffset;
					uint64 Count = LayerSpaceKnownStructure.GetHead()->GetSize();
					FBlockStructure LastChunkBuildSpace;
					if (Scanner.Structure.SelectSerialBytes(FirstByte, Count, LastChunkBuildSpace) != Count)
					{
						// Fatal error, translated last chunk should be in range.
						UE_LOG(LogPatchGeneration, Error, TEXT("Translated last chunk match was not within scanner's range."));
						return false;
					}
					BuildSpaceUnknownStructure.Remove(LastChunkBuildSpace);
					BuildSpaceKnownStructure.Add(LastChunkBuildSpace);
				}
			}

			// Process each chunk that this scanner matched.
			for (int32 MatchIdx = 0; MatchIdx < ChunkMatches.Num(); ++MatchIdx)
			{
				FChunkMatch& Match = ChunkMatches[MatchIdx];

				// Translate to build space.
				FBlockStructure BuildSpaceChunkStructure;
				const uint64 BytesFound = Scanner.Structure.SelectSerialBytes(Match.DataOffset, FBuildPatchData::ChunkDataSize, BuildSpaceChunkStructure);
				const bool bFoundOk = Scanner.bIsFinalScanner || BytesFound == FBuildPatchData::ChunkDataSize;
				if (!bFoundOk)
				{
					// Fatal error if the scanner returned a matched range that doesn't fit inside it's data.
					UE_LOG(LogPatchGeneration, Error, TEXT("Chunk match was not within scanner's data structure."));
					return false;
				}

				uint64 LayerOffset = Scanner.LayerOffset + Match.DataOffset;
				if (LayerLastChunkMatch == nullptr || ((LayerLastChunkMatch->DataOffset + FBuildPatchData::ChunkDataSize) <= LayerOffset))
				{
					// Accept the match.
					LayerLastChunkMatch = &LayerToLastChunkMatch.Add(Scanner.Layer, FChunkMatch(LayerOffset, Match.ChunkGuid));

					// Track data from this scanner.
					LayerSpaceUnknownStructure.Remove(LayerOffset, FBuildPatchData::ChunkDataSize);
					LayerSpaceKnownStructure.Add(LayerOffset, FBuildPatchData::ChunkDataSize);

					// Process the chunk in build space.
					BuildSpaceUnknownStructure.Remove(BuildSpaceChunkStructure);
					BuildSpaceKnownStructure.Add(BuildSpaceChunkStructure);
					ManifestBuilder->AddChunkMatch(Match.ChunkGuid, BuildSpaceChunkStructure);
					ChunksReferenced.Add(Match.ChunkGuid);
				}
				else
				{
					// Currently we don't use overlapping chunks, but we should save that info to drive improvement investigation.
					RejectedChunkMatches.Add(Match);
					BuildSpaceRejectedStructure.Add(BuildSpaceChunkStructure);
				}
			}
			// Remove padding from known structure.
			LayerSpaceKnownStructure.Remove(Scanner.LayerOffset + Scanner.Data.Num() - Scanner.PaddingSize, Scanner.PaddingSize);

			// Collect unknown data into buffers and spaces.
			TArray<uint8>& UnknownLayerData = LayerToUnknownLayerData.FindOrAdd(Scanner.Layer);
			FBlockStructure& UnknownLayerStructure = LayerToUnknownLayerStructure.FindOrAdd(Scanner.Layer);
			FBlockStructure& UnknownBuildStructure = LayerToUnknownBuildStructure.FindOrAdd(Scanner.Layer);

			// Check sanity of tracked data
			check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
				&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));

			// Remove from unknown data buffer what we now know. This covers newly recognised data from previous overlaps.
			FBlockStructure NowKnownDataStructure;
			if (PatchGenerationHelpers::SerialiseIntersection(UnknownLayerStructure, LayerSpaceKnownStructure, NowKnownDataStructure))
			{
				const FBlockEntry* NowKnownDataBlock = NowKnownDataStructure.GetFoot();
				while (NowKnownDataBlock != nullptr)
				{
					UnknownLayerData.RemoveAt(NowKnownDataBlock->GetOffset(), NowKnownDataBlock->GetSize(), false);
					NowKnownDataBlock = NowKnownDataBlock->GetPrevious();
				}
			}
			else
			{
				// Fatal error if we could not convert to serial space.
				UE_LOG(LogPatchGeneration, Error, TEXT("Could not convert new blocks to serial space."));
				return false;
			}
			UnknownLayerStructure.Remove(LayerSpaceKnownStructure);
			UnknownBuildStructure.Remove(BuildSpaceKnownStructure);

			// Check sanity of tracked data
			check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
				&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));

			// Mark the number of bytes we know to be accurate. This stays one scanner behind.
			LayerToProcessedCount.FindOrAdd(Scanner.Layer) = UnknownLayerData.Num();

			// Add new unknown data to buffer and structures.
			LayerSpaceUnknownStructure.Remove(UnknownLayerStructure);
			const FBlockEntry* LayerSpaceUnknownBlock = LayerSpaceUnknownStructure.GetHead();
			while (LayerSpaceUnknownBlock != nullptr)
			{
				const int32 ScannerDataOffset = LayerSpaceUnknownBlock->GetOffset() - Scanner.LayerOffset;
				const int32 BlockSize = LayerSpaceUnknownBlock->GetSize();
				check(ScannerDataOffset >= 0 && (ScannerDataOffset + BlockSize) <= (Scanner.Data.Num() - Scanner.PaddingSize));
				UnknownLayerData.Append(&Scanner.Data[ScannerDataOffset], BlockSize);
				LayerSpaceUnknownBlock = LayerSpaceUnknownBlock->GetNext();
			}
			UnknownLayerStructure.Add(LayerSpaceUnknownStructure);
			UnknownBuildStructure.Add(BuildSpaceUnknownStructure);

			// Check sanity of tracked data
			check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
				&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));

			// If this is the final scanner then mark all unknown data ready for processing.
			if (Scanner.bIsFinalScanner)
			{
				LayerToProcessedCount.FindOrAdd(Scanner.Layer) = UnknownLayerData.Num();
			}

			// Remove scanner from list.
			LayerToScannerCount.FindOrAdd(Scanner.Layer)--;
			Scanners.RemoveAt(0);
		}

		// Process some unknown data for each layer
		for (TPair<int32, TArray<uint8>>& UnknownLayerDataPair : LayerToUnknownLayerData)
		{
			// Gather info for the layer.
			const int32 Layer = UnknownLayerDataPair.Key;
			uint64& ProcessedCount = LayerToProcessedCount[Layer];
			TArray<uint8>& UnknownLayerData = UnknownLayerDataPair.Value;
			FBlockStructure& UnknownLayerStructure = LayerToUnknownLayerStructure[Layer];
			FBlockStructure& UnknownBuildStructure = LayerToUnknownBuildStructure[Layer];

			// Check if this final data for the layer.
			bool bIsFinalData = BuildStream->IsEndOfData();
			for (int32 LayerIdx = 0; LayerIdx < Layer; ++LayerIdx)
			{
				bIsFinalData = bIsFinalData && LayerToUnknownLayerData.FindRef(LayerIdx).Num() == 0;
				bIsFinalData = bIsFinalData && LayerToScannerCount.FindRef(LayerIdx) == 0;
			}
			bIsFinalData = bIsFinalData && LayerToScannerCount.FindRef(Layer) == 0;

			// Use large enough unknown data runs to make new chunks.
			if (UnknownLayerData.Num() > 0)
			{
				const FBlockEntry* UnknownLayerBlock = UnknownLayerStructure.GetHead();
				bool bSingleBlock = UnknownLayerBlock != nullptr && UnknownLayerBlock->GetNext() == nullptr;
				uint64 ByteOffset = 0;
				while (UnknownLayerBlock != nullptr)
				{
					uint64 ByteEnd = ByteOffset + UnknownLayerBlock->GetSize();
					if (ByteEnd > ProcessedCount)
					{
						// Clamp size by setting end to processed count or offset. Unsigned subtraction is unsafe.
						ByteEnd = FMath::Max<uint64>(ProcessedCount, ByteOffset);
					}
					// Make a new chunk if large enough block, or it's a final single block.
					if ((ByteEnd - ByteOffset) >= FBuildPatchData::ChunkDataSize || (bSingleBlock && bIsFinalData))
					{
						// Chunk needs padding?
						check(UnknownLayerData.Num() > ByteOffset);
						uint64 SizeOfData = FMath::Min<uint64>(FBuildPatchData::ChunkDataSize, UnknownLayerData.Num() - ByteOffset);
						if (SizeOfData < FBuildPatchData::ChunkDataSize)
						{
							UnknownLayerData.SetNumZeroed(ByteOffset + FBuildPatchData::ChunkDataSize);
						}

						// Create data for new chunk.
						uint8* NewChunkData = &UnknownLayerData[ByteOffset];
						FGuid NewChunkGuid = FGuid::NewGuid();
						uint64 NewChunkHash = FRollingHash<FBuildPatchData::ChunkDataSize>::GetHashForDataSet(NewChunkData);
						FSHAHash NewChunkSha;
						FSHA1::HashBuffer(NewChunkData, FBuildPatchData::ChunkDataSize, NewChunkSha.Hash);

						// Save it out.
						ChunkWriter.QueueChunk(NewChunkData, NewChunkGuid, NewChunkHash);
						ChunkShaHashes.Add(NewChunkGuid, NewChunkSha);
						ChunkInventory.FindOrAdd(NewChunkHash).Add(NewChunkGuid);

						// Add to manifest builder.
						FBlockStructure BuildSpaceChunkStructure;
						if (UnknownBuildStructure.SelectSerialBytes(ByteOffset, SizeOfData, BuildSpaceChunkStructure) != SizeOfData)
						{
							// Fatal error if the scanner returned a matched range that doesn't fit inside it's data.
							UE_LOG(LogPatchGeneration, Error, TEXT("New chunk was not within build space structure."));
							return false;
						}
						ManifestBuilder->AddChunkMatch(NewChunkGuid, BuildSpaceChunkStructure);
						ChunksReferenced.Add(NewChunkGuid);

						// Remove from tracking.
						UnknownLayerData.RemoveAt(ByteOffset, FBuildPatchData::ChunkDataSize, false);
						FBlockStructure LayerSpaceChunkStructure;
						if (UnknownLayerStructure.SelectSerialBytes(ByteOffset, SizeOfData, LayerSpaceChunkStructure) != SizeOfData)
						{
							// Fatal error if the scanner returned a matched range that doesn't fit inside it's data.
							UE_LOG(LogPatchGeneration, Error, TEXT("New chunk was not within layer space structure."));
							return false;
						}
						UnknownLayerStructure.Remove(LayerSpaceChunkStructure);
						UnknownBuildStructure.Remove(BuildSpaceChunkStructure);
						check(ProcessedCount >= SizeOfData);
						ProcessedCount -= SizeOfData;

						// Check sanity of tracked data
						check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
							&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));
						check(SizeOfData >= FBuildPatchData::ChunkDataSize || UnknownLayerData.Num() == 0);

						// UnknownLayerBlock is now potentially invalid due to mutation of UnknownLayerStructure so we must loop from beginning.
						UnknownLayerBlock = UnknownLayerStructure.GetHead();
						ByteOffset = 0;
						continue;
					}
					ByteOffset += UnknownLayerBlock->GetSize();
					UnknownLayerBlock = UnknownLayerBlock->GetNext();
				}
			}

			// Use unknown data to make new scanners.
			if (UnknownLayerData.Num() > 0)
			{
				bScannerQueueFull = FDataScannerCounter::GetNumIncompleteScanners() > FDataScannerCounter::GetNumRunningScanners();

				// Make sure there are enough bytes available for a scanner, plus a chunk, so that we know no more chunks will get
				// made from this sequential unknown data.
				uint64 RequiredScannerBytes = ScannerDataSize + FBuildPatchData::ChunkDataSize;
				// We make a scanner when there's enough data, or if we are at the end of data for this layer.
				bool bShouldMakeScanner = (ProcessedCount >= RequiredScannerBytes) || bIsFinalData;
				if (!bScannerQueueFull && bShouldMakeScanner)
				{
					// Create data processor.
					const uint64 SizeToTake = FMath::Min<uint64>(ScannerDataSize, UnknownLayerData.Num());
					bool bIsFinalScanner = bIsFinalData && SizeToTake == UnknownLayerData.Num();
					TArray<uint8> ScannerData;
					ScannerData.Append(UnknownLayerData.GetData(), SizeToTake);
					// Pad scanner data if end of build.
					uint64 PadSize = bIsFinalScanner ? ScannerOverlapSize : 0;
					ScannerData.AddZeroed(PadSize);
					// Grab structure for scanner.
					FBlockStructure BuildStructure;
					if (UnknownBuildStructure.SelectSerialBytes(0, SizeToTake, BuildStructure) != SizeToTake)
					{
						// Fatal error if the tracked results do not align correctly.
						UE_LOG(LogPatchGeneration, Error, TEXT("Tracked unknown build data inconsistent."));
						return false;
					}
					uint64& DataOffset = LayerToDataOffset.FindOrAdd(Layer + 1);
					Scanners.Emplace(new FScannerDetails(Layer + 1, DataOffset, bIsFinalScanner, PadSize, MoveTemp(ScannerData), BuildStructure, CloudEnumeration, StatsCollector));
					LayerToScannerCount.FindOrAdd(Layer + 1)++;

					// Remove data we just pulled out from tracking, minus the overlap.
					uint64 RemoveSize = bIsFinalScanner ? SizeToTake : SizeToTake - ScannerOverlapSize;
					DataOffset += RemoveSize;
					FBlockStructure LayerStructure;
					if (UnknownLayerStructure.SelectSerialBytes(0, RemoveSize, LayerStructure) != RemoveSize)
					{
						// Fatal error if the tracked results do not align correctly.
						UE_LOG(LogPatchGeneration, Error, TEXT("Tracked unknown layer data inconsistent."));
						return false;
					}
					BuildStructure.Empty();
					if (UnknownBuildStructure.SelectSerialBytes(0, RemoveSize, BuildStructure) != RemoveSize)
					{
						// Fatal error if the tracked results do not align correctly.
						UE_LOG(LogPatchGeneration, Error, TEXT("Tracked unknown build data inconsistent."));
						return false;
					}
					UnknownLayerData.RemoveAt(0, RemoveSize, false);
					UnknownLayerStructure.Remove(LayerStructure);
					UnknownBuildStructure.Remove(BuildStructure);
					check(ProcessedCount >= RemoveSize);
					ProcessedCount -= RemoveSize;

					// Check sanity of tracked data.
					check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
						&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));
					// Final scanner must have emptied data.
					check(PadSize == 0 || UnknownLayerData.Num() == 0);
				}
			}
		}

		// Log collected stats.
		GLog->FlushThreadedLogs();
		FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
		StatsCollector->LogStats(10.0f);

		// Set whether we are still processing unknown data.
		bHasUnknownData = false;
		for (TPair<int32, TArray<uint8>>& UnknownLayerDataPair : LayerToUnknownLayerData)
		{
			if (UnknownLayerDataPair.Value.Num() > 0)
			{
				bHasUnknownData = true;
				break;
			}
		}

		// Sleep to allow other threads.
		FPlatformProcess::Sleep(0.01f);
	}

	// Inform no more chunks.
	ChunkWriter.NoMoreChunks();
	// Wait for chunk writer.
	ChunkWriter.WaitForThread();

	// Collect chunk info for the manifest builder.
	TMap<FGuid, FChunkInfoData> ChunkInfoMap;
	TMap<FGuid, int64> ChunkFileSizes = CloudEnumeration->GetChunkFileSizes();
	ChunkWriter.GetChunkFilesizes(ChunkFileSizes);
	for (const TPair<uint64, TSet<FGuid>>& ChunkInventoryPair : CloudEnumeration->GetChunkInventory())
	{
		TSet<FGuid>& ChunkSet = ChunkInventory.FindOrAdd(ChunkInventoryPair.Key);
		ChunkSet = ChunkSet.Union(ChunkInventoryPair.Value);
	}
	ChunkShaHashes.Append(CloudEnumeration->GetChunkShaHashes());
	bool bFoundAllChunkInfo = true;
	for (const TPair<uint64, TSet<FGuid>>& ChunkInventoryPair : ChunkInventory)
	{
		for (const FGuid& ChunkGuid : ChunkInventoryPair.Value)
		{
			if (ChunkShaHashes.Contains(ChunkGuid) && ChunkFileSizes.Contains(ChunkGuid))
			{
				FChunkInfoData& ChunkInfo = ChunkInfoMap.FindOrAdd(ChunkGuid);
				ChunkInfo.Guid = ChunkGuid;
				ChunkInfo.Hash = ChunkInventoryPair.Key;
				FMemory::Memcpy(ChunkInfo.ShaHash.Hash, ChunkShaHashes[ChunkGuid].Hash, FSHA1::DigestSize);
				ChunkInfo.FileSize = ChunkFileSizes[ChunkGuid];
				ChunkInfo.GroupNumber = FCrc::MemCrc32(&ChunkGuid, sizeof(FGuid)) % 100;
			}
			else if (ChunksReferenced.Contains(ChunkGuid))
			{
				bFoundAllChunkInfo = false;
				UE_LOG(LogPatchGeneration, Error, TEXT("Missing info for chunk %s. HasSha:%d HasFileSize:%d."), *ChunkGuid.ToString(), ChunkShaHashes.Contains(ChunkGuid), ChunkFileSizes.Contains(ChunkGuid));
			}
		}
	}
	if (bFoundAllChunkInfo == false)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Cannot continue without all required chunk info."));
		return false;
	}

	// Finalize the manifest data.
	TArray<FChunkInfoData> ChunkInfoList;
	ChunkInfoMap.GenerateValueArray(ChunkInfoList);
	if (ManifestBuilder->FinalizeData(BuildStream->GetAllFiles(), MoveTemp(ChunkInfoList)) == false)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Finalizing manifest failed."));
		return false;
	}

	// Produce final stats log.
	FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
	StatsCollector->LogStats();
	uint64 EndTime = FStatsCollector::GetCycles();
	UE_LOG(LogPatchGeneration, Log, TEXT("Completed in %s."), *FPlatformTime::PrettyTime(FStatsCollector::CyclesToSeconds(EndTime - StartTime)));

	// Save manifest out to the cloud directory.
	FString OutputFilename = FBuildPatchServicesModule::GetCloudDirectory() / Settings.OutputFilename;
	if (ManifestBuilder->SaveToFile(OutputFilename) == false)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Saving manifest failed."));
		return false;
	}
	UE_LOG(LogPatchGeneration, Log, TEXT("Saved manifest to %s."), *OutputFilename);

	return true;
}
