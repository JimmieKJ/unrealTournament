// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "StatsCollector.h"

namespace BuildPatchServices
{
	class ICloudEnumeration
	{
	public:
		virtual TSet<FGuid> GetChunkSet(uint64 ChunkHash) const = 0;
		virtual TMap<uint64, TSet<FGuid>> GetChunkInventory() const = 0;
		virtual TMap<FGuid, int64> GetChunkFileSizes() const = 0;
		virtual TMap<FGuid, FSHAHash> GetChunkShaHashes() const = 0;
	};

	typedef TSharedRef<ICloudEnumeration, ESPMode::ThreadSafe> ICloudEnumerationRef;
	typedef TSharedPtr<ICloudEnumeration, ESPMode::ThreadSafe> ICloudEnumerationPtr;

	class FCloudEnumerationFactory
	{
	public:
		static ICloudEnumerationRef Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const FStatsCollectorRef& StatsCollector);
	};
}
