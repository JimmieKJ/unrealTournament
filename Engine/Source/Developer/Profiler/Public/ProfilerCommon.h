// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ProfilerCommon.h: Declares the profiler common classes, structures, typedef etc.
=============================================================================*/

#pragma once
DECLARE_LOG_CATEGORY_EXTERN(Profiler, Log, All);

/*-----------------------------------------------------------------------------
	Type definitions
-----------------------------------------------------------------------------*/

/** Type definition for shared pointers to instances of FGraphDataSource. */
typedef TSharedPtr<class FGraphDataSource> FGraphDataSourcePtr;

/** Type definition for shared references to const instances of FGraphDataSource. */
typedef TSharedRef<const class FGraphDataSource> FGraphDataSourceRefConst;

/** Type definition for weak references to instances of FGraphDataSource. */
typedef TWeakPtr<class FGraphDataSource> FGraphDataSourceWeak;


/** Type definition for shared pointers to instances of FCombinedGraphDataSource. */
typedef TSharedPtr<class FCombinedGraphDataSource> FCombinedGraphDataSourcePtr;

/** Type definition for shared references to instances of FCombinedGraphDataSource. */
typedef TSharedRef<class FCombinedGraphDataSource> FCombinedGraphDataSourceRef;

/** Type definition for shared pointers to instances of IDataProvider. */
typedef TSharedPtr<class IDataProvider> IDataProviderPtr;

/** Type definition for shared references to instances of IDataProvider. */
typedef TSharedRef<class IDataProvider> IDataProviderRef;

/** Type definition for shared pointers to instances of FProfilerSession. */
typedef TSharedPtr<class FProfilerSession> FProfilerSessionPtr;

/** Type definition for shared references to instances of FProfilerSession. */
typedef TSharedRef<class FProfilerSession> FProfilerSessionRef;

/** Type definition for shared references to const instances of FProfilerSession. */
typedef TSharedRef<const class FProfilerSession> FProfilerSessionRefConst;

/** Type definition for weak references to instances of FProfilerSession. */
typedef TWeakPtr<class FProfilerSession> FProfilerSessionWeak;

/** Type definition for shared pointers to instances of FProfilerStatMetaData. */
typedef TSharedPtr<class FProfilerStatMetaData> FProfilerStatMetaDataPtr;

/** Type definition for shared references to instances of FProfilerStatMetaData. */
typedef TSharedRef<class FProfilerStatMetaData> FProfilerStatMetaDataRef;

class FProfilerStat;
class FProfilerGroup;
class FProfilerStatMetaData;
class FProfilerAggregatedStat;

#define DEBUG_PROFILER_PERFORMANCE 0

#if DEBUG_PROFILER_PERFORMANCE==1
#define PROFILER_SCOPE_LOG_TIME(arg0,arg1) SCOPE_LOG_TIME(arg0,arg1)
#else
#define PROFILER_SCOPE_LOG_TIME(arg0,arg1)
#endif

/** Time spent on graph drawing. */
DECLARE_CYCLE_STAT_EXTERN( TEXT("DataGraphOnPaint"),	STAT_DG_OnPaint,			STATGROUP_Profiler, );

/** Time spent on handling profiler data. */
DECLARE_CYCLE_STAT_EXTERN( TEXT("ProfilerHandleData"),	STAT_PM_HandleProfilerData,	STATGROUP_Profiler, );

/** Time spent on ticking profiler manager. */
DECLARE_CYCLE_STAT_EXTERN( TEXT("ProfilerTick"),		STAT_PM_Tick,				STATGROUP_Profiler, );

/** Number of bytes allocated by all profiler sessions. */
DECLARE_MEMORY_STAT_EXTERN( TEXT("ProfilerMemoryUsage"),STAT_PM_MemoryUsage,		STATGROUP_Profiler, );

/*-----------------------------------------------------------------------------
	Enumerators
-----------------------------------------------------------------------------*/

/** Enumerates graph styles. */
namespace EGraphStyles
{
	enum Type
	{
		/** Line graph. */
		Line,

		/** Combined graph. */
		Combined,

		/** More to come in future. */
		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};
}

/*-----------------------------------------------------------------------------
	Basic structures
-----------------------------------------------------------------------------*/

class FProfilerHelper
{
public:
	/** Shorten a name for stats display. */
	static FString ShortenName( const FString& NameToShorten, const int32 Limit = 16 )
	{
		FString Result(NameToShorten);
		if (Result.Len() > Limit)
		{
			Result = FString(TEXT("...")) + Result.Right(Limit);
		}
		return Result;
	}
};

/** Implements binary search for the various cases. */
class FBinaryFindIndex
{
public:
	template<class T>
	static int32 LessEqual( const TArray<T>& Array, const T& Item, const int32 FirstIndex = 0, const int32 LastIndex = INDEX_NONE )
	{
		const int32 LocalLastIndex = LastIndex == INDEX_NONE ? Array.Num() : LastIndex;
		int32 Lenght = LocalLastIndex - FirstIndex;
		int32 Middle = Lenght / 2;
		int32 Offset = FirstIndex;

		while( Middle > 0 )
		{
			Middle = Lenght / 2;
			if( Array[Offset + Middle] <= Item )
			{
				Offset += Middle;
			}
			Lenght -= Middle;
		}
		return Offset;
	}

	template<class T>
	static int32 GreaterEqual( const TArray<T>& Array, const T& Item, const int32 FirstIndex = 0, const int32 LastIndex = INDEX_NONE )
	{
		const int32 LocalLastIndex = LastIndex == INDEX_NONE ? Array.Num() : LastIndex;
		int32 Lenght = LocalLastIndex - FirstIndex;
		int32 Middle = Lenght / 2;
		int32 Offset = FirstIndex;
		int32 Edge = 0;

		while( Middle > 0 )
		{
			Middle = Lenght / 2;
			if( Array[Offset + Middle] >= Item )
			{
				Edge = 0;
			}
			else
			{
				Edge = 1;
				Offset += Middle;		
			}

			Lenght -= Middle;
		}
		return Offset+Edge;
	}
};
