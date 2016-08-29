// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "DerivedDataCacheUsageStats.h"

/** 
 * A simple thread safe, memory based backend. This is used for Async puts and the boot cache.
**/
class FMemoryDerivedDataBackend : public FDerivedDataBackendInterface
{
public:
	FMemoryDerivedDataBackend(int64 InMaxCacheSize = -1)
		: MaxCacheSize(InMaxCacheSize)
		, bDisabled( false )
		, CurrentCacheSize( SerializationSpecificDataSize )
		, bMaxSizeExceeded(false)
	{
	}
	~FMemoryDerivedDataBackend()
	{
		Disable();
	}

	/** return true if this cache is writable **/
	virtual bool IsWritable() override
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return !bDisabled;
	}

	/**
	 * Synchronous test for the existence of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				true if the data probably will be found, this can't be guaranteed because of concurrency in the backends, corruption, etc
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) override
	{
		COOK_STAT(auto Timer = UsageStats.TimeProbablyExists());
		FScopeLock ScopeLock(&SynchronizationObject);
		if (bDisabled)
		{
			return false;
		}
		// to avoid constant error reporting in async put due to restricted cache size, 
		// we report true if the max size has been exceeded
		if (bMaxSizeExceeded)
		{
			return true;
		}

		bool Result = CacheItems.Contains(FString(CacheKey));
		if (Result)
		{
			COOK_STAT(Timer.AddHit(0));
		}
		return Result;
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
		COOK_STAT(auto Timer = UsageStats.TimeGet());
		FScopeLock ScopeLock(&SynchronizationObject);
		if (!bDisabled)
		{
			FCacheValue* Item = CacheItems.FindRef(FString(CacheKey));
			if (Item)
			{
				OutData = Item->Data;
				Item->Age = 0;
				check(OutData.Num());
				COOK_STAT(Timer.AddHit(OutData.Num()));
				return true;
			}
		}
		OutData.Empty();
		return false;
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
		COOK_STAT(auto Timer = UsageStats.TimePut());
		FScopeLock ScopeLock(&SynchronizationObject);
		
		if (bDisabled || bMaxSizeExceeded)
		{
			return;
		}
		
		FString Key(CacheKey);
		FCacheValue* Item = CacheItems.FindRef(FString(CacheKey));
		if (Item)
		{
			//check(Item->Data == InData); // any second attempt to push data should be identical data
		}
		else
		{
			FCacheValue* Val = new FCacheValue(InData);
			int32 CacheValueSize = CalcCacheValueSize(Key, *Val);

			// check if we haven't exceeded the MaxCacheSize
			if (MaxCacheSize > 0 && (CurrentCacheSize + CacheValueSize) > MaxCacheSize)
			{
				delete Val;
				UE_LOG(LogDerivedDataCache, Display, TEXT("Failed to cache data. Maximum cache size reached. CurrentSize %d kb / MaxSize: %d kb"), CurrentCacheSize / 1024, MaxCacheSize / 1024);
				bMaxSizeExceeded = true;
			}
			else
			{
				COOK_STAT(Timer.AddHit(InData.Num()));
				CacheItems.Add(Key, Val);
				CalcCacheValueSize(Key, *Val);

				CurrentCacheSize += CacheValueSize;
			}
		}
	}

	virtual void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) override
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if (bDisabled || bTransient)
		{
			return;
		}
		FString Key(CacheKey);
		FCacheValue* Item = NULL;
		if (CacheItems.RemoveAndCopyValue(Key, Item))
		{
			CurrentCacheSize -= CalcCacheValueSize(Key, *Item);
			bMaxSizeExceeded = false;

			check(Item);
			delete Item;
		}
		else
		{
			check(!Item);
		}
	}

	/**
	 * Save the cache to disk
	 * @param	Filename	Filename to save
	 * @return	true if file was saved sucessfully
	 */
	bool SaveCache(const TCHAR* Filename)
	{
		double StartTime = FPlatformTime::Seconds();
		TAutoPtr<FArchive> SaverArchive(IFileManager::Get().CreateFileWriter(Filename, FILEWRITE_EvenIfReadOnly));
		if (!SaverArchive.IsValid())
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("Could not save memory cache %s."), Filename);
			return false;
		}

		FArchive& Saver = *SaverArchive;
		uint32 Magic = MemCache_Magic64;
		Saver << Magic;
		const int64 DataStartOffset = Saver.Tell();
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			check(!bDisabled);
			for (TMap<FString, FCacheValue*>::TIterator It(CacheItems); It; ++It )
			{
				Saver << It.Key();
				Saver << It.Value()->Age;
				Saver << It.Value()->Data;
			}
		}
		const int64 DataSize = Saver.Tell(); // Everything except the footer
		int64 Size = DataSize;
		uint32 Crc = MemCache_Magic64; // Crc takes more time than I want to spend  FCrc::MemCrc_DEPRECATED(&Buffer[0], Size);
		Saver << Size;
		Saver << Crc;

		check(SerializationSpecificDataSize + DataSize <= MaxCacheSize || MaxCacheSize <= 0);

		UE_LOG(LogDerivedDataCache, Log, TEXT("Saved boot cache %4.2fs %lldMB %s."), float(FPlatformTime::Seconds() - StartTime), DataSize / (1024 * 1024), Filename);
		return true;
	}
	/**
	 * Load the cache from disk
	 * @param	Filename	Filename to load
	 * @return	true if file was loaded sucessfully
	 */
	bool LoadCache(const TCHAR* Filename)
	{
		double StartTime = FPlatformTime::Seconds();
		const int64 FileSize = IFileManager::Get().FileSize(Filename);
		if (FileSize < 0)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Could not find memory cache %s."), Filename);
			return false;
		}
		// We test 3 * uint32 which is the old format (< SerializationSpecificDataSize). We'll test
		// agains SerializationSpecificDataSize later when we read the magic number from the cache.
		if (FileSize < sizeof(uint32) * 3)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("Memory cache was corrputed (short) %s."), Filename);
			return false;
		}
		if (FileSize > MaxCacheSize*2 && MaxCacheSize > 0)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("Refusing to load DDC cache %s. Size exceeds doubled MaxCacheSize."), Filename);
			return false;
		}

		TAutoPtr<FArchive> LoderArchive(IFileManager::Get().CreateFileReader(Filename));
		if (!LoderArchive.IsValid())
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Could not read memory cache %s."), Filename);
			return false;
		}

		FArchive& Loader = *LoderArchive;
		uint32 Magic = 0;
		Loader << Magic;
		if (Magic != MemCache_Magic && Magic != MemCache_Magic64)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("Memory cache was corrputed (magic) %s."), Filename);
			return false;
		}
		// Check the file size again, this time against the correct minimum size.
		if (Magic == MemCache_Magic64 && FileSize < SerializationSpecificDataSize)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("Memory cache was corrputed (short) %s."), Filename);
			return false;
		}
		// Calculate expected DataSize based on the magic number (footer size difference)
		const int64 DataSize = FileSize - (Magic == MemCache_Magic64 ? (SerializationSpecificDataSize - sizeof(uint32)) : (sizeof(uint32) * 2));		
		Loader.Seek(DataSize);
		int64 Size = 0;
		uint32 Crc = 0;
		if (Magic == MemCache_Magic64)
		{
			Loader << Size;
		}
		else
		{
			uint32 Size32 = 0;
			Loader << Size32;
			Size = (int64)Size32;
		}
		Loader << Crc;
		if (Size != DataSize)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("Memory cache was corrputed (size) %s."), Filename);
			return false;
		}
		if ((Crc != MemCache_Magic && Crc != MemCache_Magic64) || Crc != Magic)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Memory cache was corrputed (crc) %s."), Filename);
			return false;
		}
		// Seek to data start offset (skip magic number)
		Loader.Seek(sizeof(uint32));
		{
			TArray<uint8> Working;
			FScopeLock ScopeLock(&SynchronizationObject);
			check(!bDisabled);
			while (Loader.Tell() < DataSize)
			{
				FString Key;
				int32 Age;
				Loader << Key;
				Loader << Age;
				Age++;
				Loader << Working;
				if (Age < MaxAge)
				{
					CacheItems.Add(Key, new FCacheValue(Working, Age));
				}
				Working.Reset();
			}
			// these are just a double check on ending correctly
			if (Magic == MemCache_Magic64)
			{
				Loader << Size;
			}
			else
			{
				uint32 Size32 = 0;
				Loader << Size32;
				Size = (int64)Size32;
			}
			Loader << Crc;
		}
		
		CurrentCacheSize = FileSize;
		CacheFilename = Filename;
		UE_LOG(LogDerivedDataCache, Log, TEXT("Loaded boot cache %4.2fs %lldMB %s."), float(FPlatformTime::Seconds() - StartTime), DataSize / (1024 * 1024), Filename);
		return true;
	}
	/**
	 * Disable cache and ignore all subsequent requests
	 */
	void Disable()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		bDisabled = true;
		for (TMap<FString,FCacheValue*>::TIterator It(CacheItems); It; ++It )
		{
			delete It.Value();
		}
		CacheItems.Empty();

		CurrentCacheSize = SerializationSpecificDataSize;
	}

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath) override
	{
		COOK_STAT(UsageStatsMap.Add(FString::Printf(TEXT("%s: %s.%s"), *GraphPath, TEXT("MemoryBackend"), *CacheFilename), UsageStats));
	}

private:
	/** Name of the cache file loaded (if any). */
	FString CacheFilename;
	FDerivedDataCacheUsageStats UsageStats;

	struct FCacheValue
	{
		int32 Age;
		TArray<uint8> Data;
		FCacheValue(const TArray<uint8>& InData, int32 InAge = 0)
			: Age(InAge)
			, Data(InData)
		{
		}
	};

	FORCEINLINE int32 CalcCacheValueSize(const FString& Key, const FCacheValue& Val)
	{
		return (Key.Len() + 1) * sizeof(TCHAR) + sizeof(Val.Age) + Val.Data.Num();
	}

	/** Set of files that are being written to disk asynchronously. */
	TMap<FString, FCacheValue*> CacheItems;
	/** Maximum size the cached items can grow up to ( in bytes ) */
	int64 MaxCacheSize;
	/** When set to true, this cache is disabled...ignore all requests. */
	bool bDisabled;
	/** Object used for synchronization via a scoped lock						*/
	FCriticalSection	SynchronizationObject;
	/** Current estimated cache size in bytes */
	int64 CurrentCacheSize;
	/** Indicates that the cache max size has been exceeded. This is used to avoid
		warning spam after the size has reached the limit. */
	bool bMaxSizeExceeded;
	enum 
	{
		/** Magic number to use in header */
		MemCache_Magic = 0x0cac0ddc,
		/** Magic number to use in header (new, > 2GB size compatible) */
		MemCache_Magic64 = 0x0cac1ddc,
		/** Oldest cache items to keep */
		MaxAge = 3,
		/** Size of data that is stored in the cachefile apart from the cache entries (64 bit size). */
		SerializationSpecificDataSize = sizeof(uint32)	// Magic
									  + sizeof(int64)	// Size
									  + sizeof(uint32), // CRC
	};
};

