// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/** 
 * ChartCreation
 *
 */

#pragma once

#define DO_CHARTING 1
#if DO_CHARTING

/*-----------------------------------------------------------------------------
	FPS chart support.
-----------------------------------------------------------------------------*/

/** Helper structure for FPS chart entries. */
struct FFPSChartEntry
{
	/** Number of frames in bucket. */
	int32		Count;
	/** Cumulative time spent in bucket. */
	double	CummulativeTime;

	FFPSChartEntry()
		: Count(0)
		, CummulativeTime(0.0)
	{
	}
};

//@todo this is a leftover from the old stats system, which used enums
enum FPSChartStats
{
	STAT_FPSChartFirstStat,
	STAT_FPSChart_0_5 = STAT_FPSChartFirstStat,
	STAT_FPSChart_5_10,
	STAT_FPSChart_10_15,
	STAT_FPSChart_15_20,
	STAT_FPSChart_20_25,
	STAT_FPSChart_25_30,
	STAT_FPSChart_30_40,
	STAT_FPSChart_40_50,
	STAT_FPSChart_50_60,
	STAT_FPSChart_60_70,
	STAT_FPSChart_70_80,
	STAT_FPSChart_80_90,
	STAT_FPSChart_90_100,
	STAT_FPSChart_100_110,
	STAT_FPSChart_110_120,
	STAT_FPSChart_120_INF,
	STAT_FPSChartLastBucketStat,

	/** Hitch stats */
	STAT_FPSChart_FirstHitchStat,
	STAT_FPSChart_Hitch_5000_Plus = STAT_FPSChart_FirstHitchStat,
	STAT_FPSChart_Hitch_2500_5000,
	STAT_FPSChart_Hitch_2000_2500,
	STAT_FPSChart_Hitch_1500_2000,
	STAT_FPSChart_Hitch_1000_1500,
	STAT_FPSChart_Hitch_750_1000,
	STAT_FPSChart_Hitch_500_750,
	STAT_FPSChart_Hitch_300_500,
	STAT_FPSChart_Hitch_200_300,
	STAT_FPSChart_Hitch_150_200,
	STAT_FPSChart_Hitch_100_150,
	STAT_FPSChart_Hitch_60_100,
	STAT_FPSChart_Hitch_30_60,
	STAT_FPSChart_LastHitchBucketStat,
	STAT_FPSChart_TotalHitchCount,
};

/** Start time of current FPS chart.										*/
extern ENGINE_API double GFPSChartStartTime;
/** FPS chart information. Time spent for each bucket and count.			*/
extern ENGINE_API FFPSChartEntry GFPSChart[STAT_FPSChartLastBucketStat - STAT_FPSChartFirstStat];
/** The total GPU time taken to render all frames. In seconds.				*/
extern ENGINE_API double GTotalGPUTime;

/** Helper structure for hitch entries. */
struct FHitchChartEntry
{
	/** Number of hitches */
	int32 HitchCount;

	/** How many hitches were bound by the game thread? */
	int32 GameThreadBoundHitchCount;

	/** How many hitches were bound by the render thread? */
	int32 RenderThreadBoundHitchCount;

	/** How many hitches were bound by the GPU? */
	int32 GPUBoundHitchCount;

	/** Time spent hitching */
	double FrameTimeSpentInBucket;

	FHitchChartEntry()
		: HitchCount(0)
		, GameThreadBoundHitchCount(0)
		, RenderThreadBoundHitchCount(0)
		, GPUBoundHitchCount(0)
		, FrameTimeSpentInBucket(0.0)
	{
	}
};


/** Hitch histogram.  How many times the game experienced hitches of various lengths. */
extern ENGINE_API FHitchChartEntry GHitchChart[ STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat ];

#endif // DO_CHARTING
