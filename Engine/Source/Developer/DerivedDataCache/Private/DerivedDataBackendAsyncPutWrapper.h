// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Thread safe set helper
**/
struct FThreadSet
{
	FCriticalSection	SynchronizationObject;
	TSet<FString>		FilesInFlight;

	void Add(const FString& Key)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		check(Key.Len());
		FilesInFlight.Add(Key);
	}
	void Remove(const FString& Key)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		FilesInFlight.Remove(Key);
	}
	bool Exists(const FString& Key)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FilesInFlight.Contains(Key);
	}
};
/** 
	* Async task to handle the fire and forget async put
**/
class FCachePutAsyncWorker
{
public:
	/** Cache Key for the put to InnerBackend **/
	FString								CacheKey;
	/** Data for the put to InnerBackend **/
	TArray<uint8>						Data;
	/** Backend to use for storage, my responsibilities are about async puts **/
	FDerivedDataBackendInterface*		InnerBackend;
	/** Memory based cache to clear once the put is finished **/
	FDerivedDataBackendInterface*		InflightCache;
	/** We remember outstanding puts so that we don't do them redundantly **/
	FThreadSet*							FilesInFlight;
	/**If true, then do not attempt skip the put even if CachedDataProbablyExists returns true **/
	bool								bPutEvenIfExists;

	/** Constructor
	*/
	FCachePutAsyncWorker(const TCHAR* InCacheKey, const TArray<uint8>* InData, FDerivedDataBackendInterface* InInnerBackend, bool InbPutEvenIfExists, FDerivedDataBackendInterface* InInflightCache, FThreadSet* InInFilesInFlight)
		: CacheKey(InCacheKey)
		, Data(*InData)
		, InnerBackend(InInnerBackend)
		, InflightCache(InInflightCache)
		, FilesInFlight(InInFilesInFlight)
		, bPutEvenIfExists(InbPutEvenIfExists)
	{
		check(InnerBackend);
	}
		
	/** Call the inner backend and when that completes, remove the memory cache */
	void DoWork()
	{
		bool bOk = true;
		InnerBackend->PutCachedData(*CacheKey, Data, bPutEvenIfExists);
		if (InflightCache)
		{
			if (!InnerBackend->CachedDataProbablyExists(*CacheKey))
			{
				// retry
				InnerBackend->PutCachedData(*CacheKey, Data, false);
				if (!InnerBackend->CachedDataProbablyExists(*CacheKey))
				{
					UE_LOG(LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendAsyncPutWrapper: Put failed, keeping in memory copy %s."),*CacheKey);
					bOk = false;
				}
			}
		}
		if (bOk && InflightCache)
		{
			InflightCache->RemoveCachedData(*CacheKey, /*bTransient=*/ false); // we can remove this from the temp cache, since the real cache will hit now
		}
		FilesInFlight->Remove(CacheKey);
		FDerivedDataBackend::Get().AddToAsyncCompletionCounter(-1);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FCachePutAsyncWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	/** Indicates to the thread pool that this task is abandonable */
	bool CanAbandon()
	{
		return true;
	}

	/** Abandon routine, we need to remove the item from the in flight cache because something might be waiting for that */
	void Abandon()
	{
		if (InflightCache)
		{
			InflightCache->RemoveCachedData(*CacheKey, /*bTransient=*/ false); // we can remove this from the temp cache, since the real cache will hit now
		}
		FilesInFlight->Remove(CacheKey);
		FDerivedDataBackend::Get().AddToAsyncCompletionCounter(-1);
	}
};


/** 
 * A backend wrapper that coordinates async puts. This means that a Get will hit an in-memory cache while the async put is still in flight.
**/
class FDerivedDataBackendAsyncPutWrapper : public FDerivedDataBackendInterface
{
public:

	/**
	 * Constructor
	 *
	 * @param	InInnerBackend		Backend to use for storage, my responsibilities are about async puts
	 * @param	bCacheInFlightPuts	if true, cache in-flight puts in a memory cache so that they hit immediately
	 */
	FDerivedDataBackendAsyncPutWrapper(FDerivedDataBackendInterface* InInnerBackend, bool bCacheInFlightPuts)
		: InnerBackend(InInnerBackend)
		, InflightCache(bCacheInFlightPuts ? (new FMemoryDerivedDataBackend) : NULL)
	{
		check(InnerBackend);
	}

	/** return true if this cache is writable **/
	virtual bool IsWritable() override
	{
		return InnerBackend->IsWritable();
	}

	/**
	 * Synchronous test for the existence of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				true if the data probably will be found, this can't be guaranteed because of concurrency in the backends, corruption, etc
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) override
	{
		return (InflightCache && InflightCache->CachedDataProbablyExists(CacheKey)) || InnerBackend->CachedDataProbablyExists(CacheKey);
	}
	/**
	 * Synchronous retrieve of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer to receive the results, if any were found
	 * @return				true if any data was found, and in this case OutData is non-empty
	 */
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData) override
	{
		if (InflightCache && InflightCache->GetCachedData(CacheKey, OutData))
		{
			return true;
		}
		bool bSuccess = InnerBackend->GetCachedData(CacheKey, OutData);
		return bSuccess;
	}
	/**
	 * Asynchronous, fire-and-forget placement of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	InData		Buffer containing the data to cache, can be destroyed after the call returns, immediately
	 * @param	bPutEvenIfExists	If true, then do not attempt skip the put even if CachedDataProbablyExists returns true
	 */
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& InData, bool bPutEvenIfExists) override
	{
		if (!InnerBackend->IsWritable())
		{
			return; // no point in continuing down the chain
		}
		if (FilesInFlight.Exists(CacheKey))
		{
			return; // if it is already on its way, we don't need to send it again
		}
		if (InflightCache)
		{
			if (InflightCache->CachedDataProbablyExists(CacheKey))
			{
				return; // if it is already on its way, we don't need to send it again
			}
			InflightCache->PutCachedData(CacheKey, InData, true); // temp copy stored in memory while the async task waits to complete
		}
		FilesInFlight.Add(CacheKey);
		FDerivedDataBackend::Get().AddToAsyncCompletionCounter(1);
		(new FAutoDeleteAsyncTask<FCachePutAsyncWorker>(CacheKey, &InData, InnerBackend, bPutEvenIfExists, InflightCache.GetOwnedPointer(), &FilesInFlight))->StartBackgroundTask();
	}

	virtual void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) override
	{
		if (!InnerBackend->IsWritable())
		{
			return; // no point in continuing down the chain
		}
		while (FilesInFlight.Exists(CacheKey))
		{
			FPlatformProcess::Sleep(0.0f); // this is an exception condition (corruption), spin and wait for it to clear
		}
		if (InflightCache)
		{
			InflightCache->RemoveCachedData(CacheKey, bTransient);
		}
		InnerBackend->RemoveCachedData(CacheKey, bTransient);
	}
private:

	/** Backend to use for storage, my responsibilities are about async puts **/
	FDerivedDataBackendInterface*					InnerBackend;
	/** Memory based cache to deal with gets that happen while an async put is still in flight **/
	TScopedPointer<FDerivedDataBackendInterface>	InflightCache;
	/** We remember outstanding puts so that we don't do them redundantly **/
	FThreadSet										FilesInFlight;
};



