// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
 * code everywhere.  And we can have consistent naming for all our files.
 *
 */

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogHistograms, Log, All);

 /** Fairly generic histogram for values that have natural lower bound and possibly no upper bound, e.g. frame time */
struct CORE_API FHistogram
{
	/** Inits histogram with linear, equally sized bins */
	void InitLinear(double MinTime, double MaxTime, double BinSize);

	/** Inits histogram to mimic our existing hitch buckets */
	void InitHitchTracking();

	/** Resets measurements, without resetting the configured bins. */
	void Reset();

	/** Adds an observed measurement. */
	void AddMeasurement(double Value);

	/** Prints histogram contents to the log. */
	void DumpToLog(const FString& HistogramName);

	/** Populates array commonly used in analytics events, adding two pairs per bin (count and sum). */
	void DumpToAnalytics(const FString& ParamNamePrefix, TArray<TPair<FString, double>>& OutParamArray);

	/** Gets number of bins. */
	inline int32 GetNumBins() const {
		return Bins.Num();
	}

	/** Gets lower bound of the bin, i.e. minimum value that goes into it. */
	inline double GetBinLowerBound(int IdxBin) const {
		return Bins[IdxBin].MinValue;
	}

	/** Gets upper bound of the bin, i.e. first value that does not go into it. */
	inline double GetBinUpperBound(int IdxBin) const {
		return Bins[IdxBin].UpperBound;
	}

	/** Gets number of observations in the bin. */
	inline int32 GetBinObservationsCount(int IdxBin) const {
		return Bins[IdxBin].Count;
	}

	/** Gets sum of observations in the bin. */
	inline double GetBinObservationsSum(int IdxBin) const {
		return Bins[IdxBin].Sum;
	}

	/** Sets the count of the indexed bin */
	void SetBinCountByIndex(int32 IdxBin, int32 Count);

	/** Sets the sum of the indexed bin */
	void SetBinSumByIndex(int IdxBin, double InSum);

	FORCEINLINE FHistogram operator-(const FHistogram& Other) const
	{
		FHistogram Tmp;
		for (int32 BinIndex = 0, NumBins = Bins.Num(); BinIndex < NumBins; BinIndex++)
		{
			Tmp.Bins.Add(Bins[BinIndex] - Other.Bins[BinIndex]);
		}
		return Tmp;
	}

protected:

	/** Bin */
	struct FBin
	{
		/** MinValue to be stored in the bin, inclusive. */
		double				MinValue;

		/** First value NOT to be stored in the bin. */
		double				UpperBound;

		/** Sum of all values that were put into this bin. */
		double				Sum;

		/** How many elements are in this bin. */
		int32				Count;

		FBin()
		{
		}

		/** Constructor for a pre-seeded bin */
		FBin(double MinInclusive, double MaxExclusive, int32 InSum, int32 InCount)
			: MinValue(MinInclusive)
			, UpperBound(MaxExclusive)
			, Sum(InSum)
			, Count(InCount)
		{
		}

		/** Constructor for any bin */
		FBin(double MinInclusive, double MaxExclusive)
			: MinValue(MinInclusive)
			, UpperBound(MaxExclusive)
			, Sum(0)
			, Count(0)
		{
		}

		/** Constructor for the last bin. */
		FBin(double MinInclusive)
			: MinValue(MinInclusive)
			, UpperBound(FLT_MAX)
			, Sum(0)
			, Count(0)
		{
		}

		FORCEINLINE FBin operator-(const FBin& Other) const
		{
			return FBin(MinValue, UpperBound, Sum - Other.Sum, Count - Other.Count);
		}
	};

	/** Bins themselves, should be continous in terms of [MinValue; UpperBound) and sorted ascending by MinValue. Last bin's UpperBound doesn't matter */
	TArray<FBin>			Bins;
};

enum EStreamingStatus
{
	LEVEL_Unloaded,
	LEVEL_UnloadedButStillAround,
	LEVEL_Loading,
	LEVEL_Loaded,
	LEVEL_MakingVisible,
	LEVEL_Visible,
	LEVEL_Preloading,
};


/**
 * This will get the version that should be used with the Automated Performance Testing
 * If one is passed in we use that otherwise we use FApp::GetEngineVersion.  This allows
 * us to have build machine built .exe and test them. 
 *
 * NOTE: had to use AutomatedBenchmarking as the parsing code is flawed and doesn't match
 *       on whole words.  so automatedperftestingChangelist was failing :-( 
 **/
CORE_API int32 GetChangeListNumberForPerfTesting();

/**
 * This will return
 *   the name specified with -BuildName=SomeName (needs to be a valid c++ name a-z A-Z _ 0-9 so we don't run into issues when we pass it around) or if not specified
 *   "CL%d" where %d is coming from GetChangeListNumberForPerfTesting()
 **/
CORE_API FString GetBuildNameForPerfTesting();

/**
 * This makes it so UnrealConsole will open up the memory profiler for us
 *
 * @param NotifyType has the <namespace>:<type> (e.g. UE_PROFILER!UE3STATS:)
 * @param FullFileName the File name to copy from the console
 **/
CORE_API void SendDataToPCViaUnrealConsole( const FString& NotifyType, const FString& FullFileName );


/** 
 * This will generate the profiling file name that will work with limited filename sizes on consoles.
 * We want a uniform naming convention so we will all just call this function.
 *
 * @param ProfilingType this is the type of profiling file this is
 * 
 **/
CORE_API FString CreateProfileFilename( const FString& InFileExtension, bool bIncludeDateForDirectoryName );

/**
* This will generate the profiling file name that will work with limited filename sizes on consoles.
* We want a uniform naming convention so we will all just call this function.
*
*
**/
CORE_API FString CreateProfileFilename(const FString& InFilename, const FString& InFileExtension, bool bIncludeDateForDirectoryName);

/** 
 * This will create the directories and the file name all in one function
 **/
CORE_API FString CreateProfileDirectoryAndFilename( const FString& InSubDirectoryName, const FString& InFileExtension );

#if WITH_ENGINE
/** Delegate type for getting current map name */
DECLARE_DELEGATE_RetVal(const FString, FGetMapNameDelegate);

/** Delegate used by CreateProfileFilename() and CreateProfileDirectoryAndFilename() to get current map name */
extern CORE_API FGetMapNameDelegate GGetMapNameDelegate;
#endif

