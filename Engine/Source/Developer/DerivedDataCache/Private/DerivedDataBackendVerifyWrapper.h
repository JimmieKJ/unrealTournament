// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
/**
 * FDerivedDataBackendVerifyWrapper, a wrapper for derived data that verifies the cache is bit-wise identical by failing all gets and verifying the puts
**/
class FDerivedDataBackendVerifyWrapper : public FDerivedDataBackendInterface
{
public:

	/**
	 * Constructor
	 *
	 * @param	InInnerBackend		Backend to use for storage, my responsibilities are about async puts
	 * @param	InbFixProblems		if true, fix any problems found
	 */
	FDerivedDataBackendVerifyWrapper(FDerivedDataBackendInterface* InInnerBackend, bool InbFixProblems)
		: bFixProblems(InbFixProblems)
		, InnerBackend(InInnerBackend)
	{
		check(InnerBackend);
	}

	virtual bool IsWritable() override
	{
		return true;
	}

	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) override
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if (AlreadyTested.Contains(FString(CacheKey)))
		{
			return true;
		}
		return false;
	}
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData) override
	{
		bool bAlreadyTested = false;
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			if (AlreadyTested.Contains(FString(CacheKey)))
			{
				bAlreadyTested = true;
			}
		}
		if (bAlreadyTested)
		{
			return InnerBackend->GetCachedData(CacheKey, OutData);
		}
		return false;
	}
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& InData, bool bPutEvenIfExists) override
	{
		bool bAlreadyTested = false;
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			if (AlreadyTested.Contains(FString(CacheKey)))
			{
				bAlreadyTested = true;
			}
			else
			{
				AlreadyTested.Add(FString(CacheKey));
			}
		}
		if (!bAlreadyTested)
		{
			TArray<uint8> OutData;
			bool bSuccess = InnerBackend->GetCachedData(CacheKey, OutData);
			if (bSuccess)
			{
				if (OutData != InData)
				{
					UE_LOG(LogDerivedDataCache, Error, TEXT("Verify: Cached data differs from newly generated data %s."), CacheKey);
					FString Cache = FString(FPaths::GameSavedDir()) / TEXT("VerifyDDC") / CacheKey + TEXT(".fromcache");
					FFileHelper::SaveArrayToFile(OutData, *Cache);
					FString Verify = FString(FPaths::GameSavedDir()) / TEXT("VerifyDDC") / CacheKey + TEXT(".verify");;
					FFileHelper::SaveArrayToFile(InData, *Verify);
					if (bFixProblems)
					{
						UE_LOG(LogDerivedDataCache, Display, TEXT("Verify: Wrote newly generated data to the cache."), CacheKey);
						InnerBackend->PutCachedData(CacheKey, InData, true);
					}
				}
				else
				{
					UE_LOG(LogDerivedDataCache, Log, TEXT("Verify: Cached data exists and matched %s."), CacheKey);
				}
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("Verify: Cached data didn't exist %s."), CacheKey);
				InnerBackend->PutCachedData(CacheKey, InData, bPutEvenIfExists);
			}
		}
	}
	virtual void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) override
	{
		InnerBackend->RemoveCachedData(CacheKey, bTransient);
	}
private:
	/** If problems are encountered, do we fix them?						*/
	bool											bFixProblems;
	/** Object used for synchronization via a scoped lock						*/
	FCriticalSection								SynchronizationObject;
	/** Set of cache keys we already tested **/
	TSet<FString>									AlreadyTested;
	/** Backend to service the actual requests */
	FDerivedDataBackendInterface*					InnerBackend;
};
