// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_BUILDPATCHGENERATION

#include "CloudEnumeration.h"
#include "DataMatcher.h"
#include "StatsCollector.h"

namespace BuildPatchServices
{
	struct FChunkPart
	{
		// Offset into provided data
		uint64 DataOffset;
		// Size of part
		uint64 PartSize;
		// Offset into chunk
		uint64 ChunkOffset;
		// Chunk identity
		FGuid ChunkGuid;
	};

	struct FChunkInfo
	{
		// The hash for this chunk
		uint64 Hash;
		// The SHA hash for this chunk
		FSHAHash ShaHash;
		// The size of this chunk file
		int64 ChunkFileSize;
		// Whether this chunk is new
		bool IsNew;
	};

	struct FDataScanResult
	{
		// The structure of the data
		TArray<FChunkPart> DataStructure;
		// Information about chunks in the data
		TMap<FGuid, FChunkInfo> ChunkInfo;

		FDataScanResult() {}
		~FDataScanResult() {}

		FDataScanResult(TArray<FChunkPart> InDataStructure, TMap<FGuid, FChunkInfo> InChunkInfo)
			: DataStructure(MoveTemp(InDataStructure))
			, ChunkInfo(MoveTemp(InChunkInfo))
		{}

		FDataScanResult(const FDataScanResult& CopyFrom)
			: DataStructure(CopyFrom.DataStructure)
			, ChunkInfo(CopyFrom.ChunkInfo)
		{}

		FDataScanResult(FDataScanResult&& MoveFrom)
			: DataStructure(MoveTemp(MoveFrom.DataStructure))
			, ChunkInfo(MoveTemp(MoveFrom.ChunkInfo))
		{}

		FORCEINLINE FDataScanResult& operator=(const FDataScanResult& CopyFrom)
		{
			DataStructure = CopyFrom.DataStructure;
			ChunkInfo = CopyFrom.ChunkInfo;
			return *this;
		}

		FORCEINLINE FDataScanResult& operator=(FDataScanResult&& MoveFrom)
		{
			DataStructure = MoveTemp(MoveFrom.DataStructure);
			ChunkInfo = MoveTemp(MoveFrom.ChunkInfo);
			return *this;
		}
	};

	class FDataScanner
	{
	public:
		virtual bool IsComplete() = 0;
		virtual FDataScanResult GetResultWhenComplete() = 0;
	};

	typedef TSharedRef<FDataScanner, ESPMode::ThreadSafe> FDataScannerRef;
	typedef TSharedPtr<FDataScanner, ESPMode::ThreadSafe> FDataScannerPtr;

	class FDataScannerCounter
	{
	public:
		static int32 GetNumIncompleteScanners();
		static int32 GetNumRunningScanners();
	};

	class FDataScannerFactory
	{
	public:
		static FDataScannerRef Create(const uint64 DataOffset, const TArray<uint8>& Data, const FCloudEnumerationRef& CloudEnumeration, const FDataMatcherRef& DataMatcher, const FStatsCollectorRef& StatsCollector);
	};
}

#endif
