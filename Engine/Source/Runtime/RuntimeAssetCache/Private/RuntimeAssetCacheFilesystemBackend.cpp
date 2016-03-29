// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RuntimeAssetCachePrivatePCH.h"
#include "RuntimeAssetCacheFilesystemBackend.h"
#include "RuntimeAssetCacheEntryMetadata.h"
#include "RuntimeAssetCacheBucket.h"

FArchive* FRuntimeAssetCacheFilesystemBackend::CreateReadArchive(FName Bucket, const TCHAR* CacheKey)
{
	FString Path = FPaths::Combine(*PathToRAC, *Bucket.ToString(), CacheKey);
	return IFileManager::Get().CreateFileReader(*Path);
}

FArchive* FRuntimeAssetCacheFilesystemBackend::CreateWriteArchive(FName Bucket, const TCHAR* CacheKey)
{
	FString Path = FPaths::Combine(*PathToRAC, *Bucket.ToString(), CacheKey);
	return IFileManager::Get().CreateFileWriter(*Path);
}

FRuntimeAssetCacheFilesystemBackend::FRuntimeAssetCacheFilesystemBackend()
{
	GConfig->GetString(TEXT("RuntimeAssetCache"), TEXT("PathToRAC"), PathToRAC, GEngineIni);
	PathToRAC = FPaths::GameSavedDir() / PathToRAC;
}

bool FRuntimeAssetCacheFilesystemBackend::RemoveCacheEntry(const FName Bucket, const TCHAR* CacheKey)
{
	FString Path = FPaths::Combine(*PathToRAC, *Bucket.ToString(), CacheKey);
	return IFileManager::Get().Delete(*Path);
}

bool FRuntimeAssetCacheFilesystemBackend::ClearCache()
{
	return IFileManager::Get().DeleteDirectory(*PathToRAC, false, true);
}

bool FRuntimeAssetCacheFilesystemBackend::ClearCache(FName Bucket)
{
	return IFileManager::Get().DeleteDirectory(*FPaths::Combine(*PathToRAC, *Bucket.ToString()), false, true);
}

FRuntimeAssetCacheBucket* FRuntimeAssetCacheFilesystemBackend::PreLoadBucket(FName BucketName, int32 BucketSize)
{
	FString Path = FPaths::Combine(*PathToRAC, *BucketName.ToString());
	FRuntimeAssetCacheBucket* Result = new FRuntimeAssetCacheBucket(BucketSize);

	class FRuntimeAssetCacheFilesystemBackendDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		FRuntimeAssetCacheFilesystemBackendDirectoryVisitor(FRuntimeAssetCacheBucket* InBucket, FName InBucketName, FRuntimeAssetCacheFilesystemBackend* InBackend)
			: Bucket(InBucket)
			, BucketName(InBucketName)
			, Backend(InBackend)
		{ }

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory)
			{
				return true;
			}
			FString CacheKey = FPaths::GetBaseFilename(FilenameOrDirectory);
			FArchive* Ar = Backend->CreateReadArchive(BucketName, *CacheKey);
			FCacheEntryMetadata* Metadata = Backend->PreloadMetadata(Ar);
			Bucket->AddMetadataEntry(*FPaths::GetBaseFilename(FilenameOrDirectory), Metadata, true);
			delete Ar;
			return true;
		}

	private:
		FRuntimeAssetCacheBucket* Bucket;
		FName BucketName;
		FRuntimeAssetCacheFilesystemBackend* Backend;
	} Visitor(Result, BucketName, this);
	IFileManager::Get().IterateDirectory(*Path, Visitor);

	return Result;
}
