// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#if WITH_BUILDPATCHGENERATION

#include "BuildPatchServicesPrivatePCH.h"

#include "CloudEnumeration.h"

namespace BuildPatchServices
{
	class FCloudEnumerationImpl
		: public FCloudEnumeration
	{
	public:
		FCloudEnumerationImpl(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold);
		virtual ~FCloudEnumerationImpl();

		virtual TSet<FGuid> GetChunkSet(uint64 ChunkHash) const override;
		virtual TMap<uint64, TSet<FGuid>> GetChunkInventory() const override;
		virtual TMap<FGuid, int64> GetChunkFileSizes() const override;
		virtual TMap<FGuid, FSHAHash> GetChunkShaHashes() const override;
	private:
		void EnumerateCloud();
		void EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest);

	private:
		const FString CloudDirectory;
		const FDateTime ManifestAgeThreshold;
		mutable FCriticalSection InventoryCS;
		TMap<uint64, TSet<FGuid>> ChunkInventory;
		TMap<FGuid, int64> ChunkFileSizes;
		TMap<FGuid, FSHAHash> ChunkShaHashes;
		TMap<FSHAHash, TSet<FGuid>> FileInventory;
		uint64 NumChunksFound;
		uint64 NumFilesFound;
		TFuture<void> Future;
	};

	FCloudEnumerationImpl::FCloudEnumerationImpl(const FString& InCloudDirectory, const FDateTime& InManifestAgeThreshold)
		: CloudDirectory(InCloudDirectory)
		, ManifestAgeThreshold(InManifestAgeThreshold)
		, NumChunksFound(0)
		, NumFilesFound(0)
	{
		TFunction<void()> Task = [this]() { EnumerateCloud(); };
		Future = Async(EAsyncExecution::Thread, Task);
	}

	FCloudEnumerationImpl::~FCloudEnumerationImpl()
	{
	}

	TSet<FGuid> FCloudEnumerationImpl::GetChunkSet(uint64 ChunkHash) const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkInventory.FindRef(ChunkHash);
		}
	}


	TMap<uint64, TSet<FGuid>> FCloudEnumerationImpl::GetChunkInventory() const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkInventory;
		}
	}

	TMap<FGuid, int64> FCloudEnumerationImpl::GetChunkFileSizes() const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkFileSizes;
		}
	}

	TMap<FGuid, FSHAHash> FCloudEnumerationImpl::GetChunkShaHashes() const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkShaHashes;
		}
	}

	void FCloudEnumerationImpl::EnumerateCloud()
	{
		FScopeLock ScopeLock(&InventoryCS);

		IFileManager& FileManager = IFileManager::Get();
		FString JSONOutput;
		TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy< TCHAR > > > DebugWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy< TCHAR > >::Create(&JSONOutput);
		DebugWriter->WriteObjectStart();

		// Find all manifest files
		if (FileManager.DirectoryExists(*CloudDirectory))
		{
			const double StartEnumerate = FPlatformTime::Seconds();
			TArray<FString> AllManifests;
			GLog->Logf(TEXT("FCloudEnumeration: Enumerating Manifests from %s"), *CloudDirectory);
			FileManager.FindFiles(AllManifests, *(CloudDirectory / TEXT("*.manifest")), true, false);
			const double EnumerateTime = FPlatformTime::Seconds() - StartEnumerate;
			GLog->Logf(TEXT("FCloudEnumeration: Found %d manifests in %.1f seconds"), AllManifests.Num(), EnumerateTime);
			int NumSkipped = 0;

			// Load all manifest files
			const double StartLoadAllManifest = FPlatformTime::Seconds();
			for (const auto& ManifestFile : AllManifests)
			{
				// Determine chunks from manifest file
				const FString ManifestFilename = CloudDirectory / ManifestFile;
				if (IFileManager::Get().GetTimeStamp(*ManifestFilename) < ManifestAgeThreshold)
				{
					++NumSkipped;
					GLog->Logf(TEXT("FCloudEnumeration: Skipping %s as it is too old to reuse"), *ManifestFile);
					continue;
				}
				FBuildPatchAppManifestRef BuildManifest = MakeShareable(new FBuildPatchAppManifest());
				const double StartLoadManifest = FPlatformTime::Seconds();
				if (BuildManifest->LoadFromFile(ManifestFilename))
				{
					const double LoadManifestTime = FPlatformTime::Seconds() - StartLoadManifest;
					GLog->Logf(TEXT("FCloudEnumeration: Loaded %s in %.1f seconds"), *ManifestFile, LoadManifestTime);
					EnumerateManifestData(BuildManifest);
				}
				else
				{
					GLog->Logf(TEXT("FCloudEnumeration: WARNING: Could not read Manifest file. Data recognition will suffer (%s)"), *ManifestFilename);
				}
			}
			const double LoadAllManifestTime = FPlatformTime::Seconds() - StartLoadAllManifest;
			GLog->Logf(TEXT("FCloudEnumeration: Used %d manifests to enumerate %llu chunks in %.1f seconds"), AllManifests.Num() - NumSkipped, NumChunksFound, LoadAllManifestTime);
		}
		else
		{
			GLog->Logf(TEXT("FCloudEnumeration: Cloud directory does not exist: %s"), *CloudDirectory);
		}
	}

	void FCloudEnumerationImpl::EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest)
	{
		TArray<FGuid> DataList;
		Manifest->GetDataList(DataList);
		if (!Manifest->IsFileDataManifest())
		{
			FSHAHashData DataShaHash;
			uint64 DataChunkHash;
			for (const auto& DataGuid : DataList)
			{
				if (Manifest->GetChunkHash(DataGuid, DataChunkHash))
				{
					if (DataChunkHash != 0)
					{
						TSet<FGuid>& HashChunkList = ChunkInventory.FindOrAdd(DataChunkHash);
						if (!HashChunkList.Contains(DataGuid))
						{
							++NumChunksFound;
							HashChunkList.Add(DataGuid);
							ChunkFileSizes.Add(DataGuid, Manifest->GetDataSize(DataGuid));
						}
					}
					else
					{
						GLog->Logf(TEXT("FCloudEnumeration: INFO: Ignored an existing chunk %s with a failed hash value of zero to avoid performance problems while chunking"), *DataGuid.ToString());
					}
				}
				else
				{
					GLog->Logf(TEXT("FCloudEnumeration: WARNING: Missing chunk hash for %s in manifest %s %s"), *DataGuid.ToString(), *Manifest->GetAppName(), *Manifest->GetVersionString());
				}
				if (Manifest->GetChunkShaHash(DataGuid, DataShaHash))
				{
					FSHAHash& ShaHash = ChunkShaHashes.FindOrAdd(DataGuid);
					FMemory::Memcpy(ShaHash.Hash, DataShaHash.Hash, FSHA1::DigestSize);
				}
			}
		}
		else
		{
			GLog->Logf(TEXT("FCloudEnumeration: INFO: Ignoring non-chunked manifest %s %s"), *Manifest->GetAppName(), *Manifest->GetVersionString());
		}
	}

	FCloudEnumerationRef FCloudEnumerationFactory::Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold)
	{
		return MakeShareable(new FCloudEnumerationImpl(CloudDirectory, ManifestAgeThreshold));
	}
}

#endif
