// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RuntimeAssetCacheBackend.h"

/**
 * File system implementation of runtime asset cache backend.
 */
class FRuntimeAssetCacheFilesystemBackend : public FRuntimeAssetCacheBackend
{
public:
	FRuntimeAssetCacheFilesystemBackend();

	/** FRuntimeAssetCacheBackend interface implementation. */
	virtual bool RemoveCacheEntry(FName Bucket, const TCHAR* CacheKey) override;
	virtual bool ClearCache() override;
	virtual bool ClearCache(FName Bucket) override;
	virtual FRuntimeAssetCacheBucket* PreLoadBucket(FName BucketName, int32 BucketSize) override;
protected:
	virtual FArchive* CreateReadArchive(FName Bucket, const TCHAR* CacheKey) override;
	virtual FArchive* CreateWriteArchive(FName Bucket, const TCHAR* CacheKey) override;
	/** End of FRuntimeAssetCacheBackend interface implementation. */

private:

	/** File system path to runtime asset cache. */
	FString PathToRAC;
};
