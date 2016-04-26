// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CookStats.h"

#if ENABLE_COOK_STATS
#include "ThreadingBase.h"
#endif

/**
 * Usage stats for the derived data cache nodes. At the end of the app or commandlet, the DDC
 * can be asked to gather usage stats for each of the nodes in the DDC graph, which are accumulated
 * into a TMap of Name->Stats. The Stats portion is this class.
 * 
 * The class exposes various high-level routines to time important aspects of the DDC, mostly
 * focusing on performance of GetCachedData, PutCachedData, and CachedDataProbablyExists. This class
 * will track time taken, calls made, hits, misses, bytes processed, and do it for two buckets:
 * 1) the main thread and 2) all other threads.  The reason is because any time spent in the DDC on the
 * main thread is considered meaningful, as DDC access is generally expected to be async from helper threads.
 * 
 * The class goes through a bit of trouble to use thread-safe access calls for task-thread usage, and 
 * simple, fast accumulators for game-thread usage, since it's guaranteed to not be written to concurrently.
 * The class also limits itself to checking the thread once at construction.
 * 
 * Usage would be something like this in a concrete FDerivedDataBackendInterface implementation:
 *   class MyBackend : public FDerivedDataBackendInterface
 *   {
 *       FDerivedDataCacheUsageStats UsageStats;
 *   public:
 *       <override CachedDataProbablyExists>
 *       { 
 *           auto Timer = UsageStats.TimeExists();
 *           ...
 *       }
 *       <override GetCachedData>
 *       {    
 *           auto Timer = UsageStats.TimeGet();
 *           ...
 *           <if it's a cache hit> Timer.AddHit(DataSize);
 *           // Misses are automatically tracked
 *       }
 *       <override PutCachedData>
 *       {
 *           auto Timer = UsageStats.TimePut();
 *           ...
 *           <if the data will really be Put> Timer.AddHit(DataSize);
 *           // Misses are automatically tracked
 *       }
 *       <override GatherUsageStats>
 *       {
 *           // Add this node's UsageStats to the usage map. Your Key name should be UNIQUE to the entire graph (so use the file name, or pointer to this if you have to).
 *           UsageStatsMap.Add(FString::Printf(TEXT("%s: <Some unique name for this node instance>"), *GraphPath), UsageStats);
 *       }
*   }
 */
class FDerivedDataCacheUsageStats
{
#if ENABLE_COOK_STATS
public:
	/** 
	 * Struct to hold stats for a Get/Put/Exists call. 
	 * The sub-structs and stuff look intimidating, but it is to unify the concept
	 * of accumulating any stat on the game or other thread, using raw int64 on the 
	 * main thread, and thread safe accumulators on other threads.
	 * 
	 * Each call type that will be tracked will track call counts, call cycles, and bytes processed,
	 * grouped by hits and misses. Some stats will by definition be zero (ie, no Miss will ever track non-zero bytes processed)
	 * but it limits the branching in the tracking code.
	 */
	struct CallStats
	{
		/** One group of accumulators for hits and misses. */
		enum class EHitOrMiss : uint8
		{
			Hit,
			Miss,
			MaxValue,
		};

		/** Each hit or miss will contain these accumulator stats. */
		enum class EStatType : uint8
		{
			Counter,
			Cycles,
			Bytes,
			MaxValue,
		};

		/** Contains a pair of accumulators, one for the game thread, one for the other threads. */
		struct GameAndOtherThreadAccumulator
		{
			/** Accumulates a stat. Uses thread safe primitives for non-game thread accumulation. */
			void Accumulate(int64 Value, bool bIsInGameThread)
			{
				if (bIsInGameThread)
				{
					GameThread += Value;
				}
				else if (Value != 0)
				{
					OtherThread.Add(Value);
				}
			}
			/** Access the accumulated values (exposed for more uniform access methds to each accumulator). */
			int64 GetAccumulatedValue(bool bIsInGameThread) const
			{
				return bIsInGameThread ? GameThread : OtherThread.GetValue();
			}

			int64 GameThread = 0l;
			FThreadSafeCounter64 OtherThread;
		};

		/** Make it easier to update an accumulator by providing strongly typed access to the 2D array. */
		void Accumulate(EHitOrMiss HitOrMiss, EStatType StatType, int64 Value, bool bIsInGameThread)
		{
			Accumulators[(uint8)HitOrMiss][(uint8)StatType].Accumulate(Value, bIsInGameThread);
		}

		/** Make it easier to access an accumulator using a uniform, stronly typed interface. */
		int64 GetAccumulatedValue(EHitOrMiss HitOrMiss, EStatType StatType, bool bIsInGameThread) const
		{
			return Accumulators[(uint8)HitOrMiss][(uint8)StatType].GetAccumulatedValue(bIsInGameThread);
		}
	
	private:
		/** The actual accumulators. All access should be from the above public functions. */
		GameAndOtherThreadAccumulator Accumulators[(uint8)EHitOrMiss::MaxValue][(uint8)EStatType::MaxValue];
	};

	/**
	 * used to accumulated cycles to the thread-safe counter.
	 * Will also accumulate hit/miss stats in the dtor as well.
	 * If AddHit is not called, it will assume a hit. If AddMiss is called, it will convert any previous hit call to a miss.
	 */
	class FScopedStatsCounter
	{
	public:
		/** Starts the time, tracks the underlying stat it will update. */
		explicit FScopedStatsCounter(CallStats& InStats)
			: Stats(InStats)
			, StartTime(FPlatformTime::Seconds())
			, bIsInGameThread(IsInGameThread())
		{
		}
		/** Ends the timer. Flushes the stats to the underlying stats object. */
		~FScopedStatsCounter()
		{
			// Can't safely use FPlatformTime::Cycles() because we might be timing long durations.
			const int64 CyclesUsed = int64((FPlatformTime::Seconds() - StartTime) / FPlatformTime::GetSecondsPerCycle());
			if (bTrackHitOrMissCount)
			{
				Stats.Accumulate(HitOrMiss, CallStats::EStatType::Counter, 1l, bIsInGameThread);
			}
			Stats.Accumulate(HitOrMiss, CallStats::EStatType::Cycles, CyclesUsed, bIsInGameThread);
			Stats.Accumulate(HitOrMiss, CallStats::EStatType::Bytes, BytesProcessed, bIsInGameThread);
		}
		
		/** Call to indicate a Get or Put "cache hit". Exists calls by definition don't have hits or misses. */
		void AddHit(int64 InBytesProcessed)
		{
			HitOrMiss = CallStats::EHitOrMiss::Hit;
			BytesProcessed += InBytesProcessed;
		}

		/** Call to indicate a Get or Put "cache miss". This is optional, as a Miss is assumed by the timer. Exists calls by definition don't have hits or misses. */
		void AddMiss()
		{
			HitOrMiss = CallStats::EHitOrMiss::Miss;
			BytesProcessed = 0;
		}
		/** Used in rare case of async nodes to track sync work without necessarily adding a hit/miss, since we won't know until the async task runs, but we want to track cycles used. */
		void UntrackHitOrMiss()
		{
			bTrackHitOrMissCount = false;
		}

	private:
		CallStats& Stats;
		double StartTime;
		int64 BytesProcessed = 0;
		bool bIsInGameThread;
		bool bTrackHitOrMissCount = true;
		CallStats::EHitOrMiss HitOrMiss = CallStats::EHitOrMiss::Miss;
	};

	/** Call this at the top of the CachedDataProbablyExists override. auto Timer = TimeProbablyExists(); */
	FScopedStatsCounter TimeProbablyExists()
	{
		return FScopedStatsCounter(ExistsStats);
	}

	/** Call this at the top of the GetCachedData override. auto Timer = TimeGet(); Use AddHit on the returned type to track a cache hit. */
	FScopedStatsCounter TimeGet()
	{
		return FScopedStatsCounter(GetStats);
	}

	/** Call this at the top of the PutCachedData override. auto Timer = TimePut(); Use AddHit on the returned type to track a cache hit. */
	FScopedStatsCounter TimePut()
	{
		return FScopedStatsCounter(PutStats);
	}

	// expose these publicly for low level access. These should really never be accessed directly except when finished accumulating them.
	CallStats GetStats;
	CallStats PutStats;
	CallStats ExistsStats;
#endif
};
