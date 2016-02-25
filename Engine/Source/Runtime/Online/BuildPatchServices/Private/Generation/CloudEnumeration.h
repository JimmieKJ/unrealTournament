// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_BUILDPATCHGENERATION

namespace BuildPatchServices
{
	class FCloudEnumeration
	{
	public:
		virtual TSet<FGuid> GetChunkSet(uint64 ChunkHash) const = 0;
		virtual TMap<uint64, TSet<FGuid>> GetChunkInventory() const = 0;
		virtual TMap<FGuid, int64> GetChunkFileSizes() const = 0;
		virtual TMap<FGuid, FSHAHash> GetChunkShaHashes() const = 0;
	};

	typedef TSharedRef<FCloudEnumeration, ESPMode::ThreadSafe> FCloudEnumerationRef;
	typedef TSharedPtr<FCloudEnumeration, ESPMode::ThreadSafe> FCloudEnumerationPtr;

	class FCloudEnumerationFactory
	{
	public:
		static FCloudEnumerationRef Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold);
	};
}

#endif
