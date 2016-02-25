// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#if WITH_BUILDPATCHGENERATION

#include "BuildPatchServicesPrivatePCH.h"

#include "DataMatcher.h"
#include "../BuildPatchGeneration.h"

namespace BuildPatchServices
{
	class FDataMatcherImpl
		: public FDataMatcher
	{
	public:
		FDataMatcherImpl(const FString& CloudDirectory);
		virtual ~FDataMatcherImpl();

		virtual bool CompareData(const FGuid& DataGuid, uint64 Hash, const TArray<uint8>& NewData, bool& ChunkFound) const override;

	private:
		const FString CloudDirectory;
	};

	FDataMatcherImpl::FDataMatcherImpl(const FString& InCloudDirectory)
		: CloudDirectory(InCloudDirectory)
	{

	}

	FDataMatcherImpl::~FDataMatcherImpl()
	{

	}

	bool FDataMatcherImpl::CompareData(const FGuid& DataGuid, uint64 Hash, const TArray<uint8>& NewData, bool& ChunkFound) const
	{
		static FCriticalSection LegacyNonThreadSafeInterface;
		FScopeLock ScopeLock(&LegacyNonThreadSafeInterface);

		bool bMatching = false;
		ChunkFound = false;

		// Read the file
		FString ChunkFilePath = FBuildPatchUtils::GetChunkNewFilename(EBuildPatchAppManifestVersion::GetLatestVersion(), CloudDirectory, DataGuid, Hash);
		TSharedRef< FBuildGenerationChunkCache::FChunkReader > ChunkReader = FBuildGenerationChunkCache::Get().GetChunkReader(ChunkFilePath);
		if (ChunkReader->IsValidChunk() && DataGuid == ChunkReader->GetChunkGuid())
		{
			ChunkFound = true;
			// Default true
			bMatching = true;
			// Compare per small block (for early outing!)
			const uint32 CompareSize = 64;
			uint8* ReadBuffer;
			uint32 NumCompared = 0;
			while (bMatching && ChunkReader->BytesLeft() > 0 && NumCompared < FBuildPatchData::ChunkDataSize)
			{
				const uint32 ReadLen = FMath::Min< uint32 >(CompareSize, ChunkReader->BytesLeft());
				ChunkReader->ReadNextBytes(&ReadBuffer, ReadLen);
				bMatching = FMemory::Memcmp(&NewData[NumCompared], ReadBuffer, ReadLen) == 0;
				NumCompared += ReadLen;
			}
		}

		return bMatching;
	}

	FDataMatcherRef FDataMatcherFactory::Create(const FString& CloudDirectory)
	{
		return MakeShareable(new FDataMatcherImpl(CloudDirectory));
	}
}

#endif
