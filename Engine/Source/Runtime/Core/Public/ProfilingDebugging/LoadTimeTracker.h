// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
* Declarations for LoadTimer which helps get load times for various parts of the game.
*/

#pragma once
#define ENABLE_LOADTIME_TRACKING 1
#define ENABLE_LOADTIME_RAW_TIMINGS 0

#include "ScopedTimers.h"

/** High level load time tracker utility (such as initial engine startup or game specific timings) */
class CORE_API FLoadTimeTracker
{
public:
	static FLoadTimeTracker& Get()
	{
		static FLoadTimeTracker Singleton;
		return Singleton;
	}

	/** Adds a scoped time for a given label.  Records each instance individually */
	void ReportScopeTime(double ScopeTime, const FName ScopeLabel);

	/** Prints out total time and individual times */
	void DumpHighLevelLoadTimes() const;

	static void DumpHighLevelLoadTimesStatic()
	{
		Get().DumpHighLevelLoadTimes();
	}

	const TMap<FName, TArray<double>>& GetData() const
	{
		return TimeInfo;
	}

	void ResetHighLevelLoadTimes();

	/** Prints out raw load times for individual timers */
	void DumpRawLoadTimes() const;

	void ResetRawLoadTimes();

#if ENABLE_LOADTIME_RAW_TIMINGS

	/** Raw Timers */
	double CreateAsyncPackagesFromQueueTime;
	double ProcessAsyncLoadingTime;
	double ProcessLoadedPackagesTime;
	double SerializeTaggedPropertiesTime;
	double CreateLinkerTime;
	double FinishLinkerTime;
	double CreateImportsTime;
	double CreateExportsTime;
	double PreLoadObjectsTime;
	double PostLoadObjectsTime;
	double PostLoadDeferredObjectsTime;
	double FinishObjectsTime;
	double MaterialPostLoad;
	double MaterialInstancePostLoad;
	double SerializeInlineShaderMaps;
	double MaterialSerializeTime;
	double MaterialInstanceSerializeTime;
	double AsyncLoadingTime;

	double LinkerLoad_CreateLoader;
	double LinkerLoad_SerializePackageFileSummary;
	double LinkerLoad_SerializeNameMap;
	double LinkerLoad_SerializeGatherableTextDataMap;
	double LinkerLoad_SerializeImportMap;
	double LinkerLoad_SerializeExportMap;
	double LinkerLoad_StartTextureAllocation;
	double LinkerLoad_FixupImportMap;
	double LinkerLoad_RemapImports;
	double LinkerLoad_FixupExportMap;
	double LinkerLoad_SerializeDependsMap;
	double LinkerLoad_CreateExportHash;
	double LinkerLoad_FindExistingExports;
	double LinkerLoad_FinalizeCreation;

	double Package_FinishLinker;
	double Package_LoadImports;
	double Package_CreateImports;
	double Package_CreateLinker;
	double Package_FinishTextureAllocations;
	double Package_CreateExports;
	double Package_PreLoadObjects;
	double Package_PostLoadObjects;
	double Package_Tick;
	double Package_CreateAsyncPackagesFromQueue;

	double Package_Temp1;
	double Package_Temp2;
	double Package_Temp3;
	double Package_Temp4;

	double TickAsyncLoading_ProcessLoadedPackages;


	double LinkerLoad_SerializeNameMap_ProcessingEntries;
#endif

private:
	TMap<FName, TArray<double>> TimeInfo;
private:
	FLoadTimeTracker();
};

#if ENABLE_LOADTIME_TRACKING
#define ACCUM_LOADTIME(TimerName, Time) FLoadTimeTracker::Get().ReportScopeTime(Time, FName(TimerName));
#else
#define ACCUM_LOADTIME(TimerName, Time)
#endif

#if ENABLE_LOADTIME_RAW_TIMINGS
#define SCOPED_LOADTIMER(TimerName) FScopedDurationTimer DurationTimer_##TimerName(FLoadTimeTracker::Get().TimerName);
#else
#define SCOPED_LOADTIMER(TimerName)
#endif