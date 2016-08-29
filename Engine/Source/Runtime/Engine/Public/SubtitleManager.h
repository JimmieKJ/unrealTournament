// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once


#define SUBTITLE_SCREEN_DEPTH_FOR_3D 0.1f

/**
 * A collection of subtitles, rendered at a certain priority.
 */
class FActiveSubtitle
{
public:
	FActiveSubtitle( int32 InIndex, float InPriority, bool InbSplit, bool InbSingleLine, TArray<FSubtitleCue> InSubtitles )
		:	Index( InIndex )
		,	Priority( InPriority )
		,	bSplit( InbSplit )
		,	bSingleLine( InbSingleLine )
		,	Subtitles( MoveTemp(InSubtitles) )
	{}

	/** Index into the Subtitles array for the currently active subtitle in this set. */
	int32						Index;
	/** Priority for this set of subtitles, used by FSubtitleManager to determine which subtitle to play. */
	float					Priority;
	/** true if the subtitles have come in pre split, or after they have been split programmatically. */
	bool					bSplit;
	/** true if the subtitles should be displayed as a sequence of single lines */
	bool					bSingleLine;
	/** A set of subtitles. */
	TArray<FSubtitleCue>	Subtitles;
};

struct FQueueSubtitleParams
{
	FQueueSubtitleParams(const TArray<FSubtitleCue>& InSubtitles)
		: Subtitles(InSubtitles)
	{ }

	uint64 AudioComponentID;
	TWeakObjectPtr<UWorld> WorldPtr;
	PTRINT WaveInstance;
	float SubtitlePriority;
	float Duration;
	uint8 bManualWordWrap : 1;
	uint8 bSingleLine : 1;
	const TArray<FSubtitleCue>& Subtitles;
};


/**
 * Subtitle manager.  Handles prioritization and rendering of subtitles.
 */
class FSubtitleManager
{
public:
	/**
	 * Kills all the subtitles
	 */
	void		KillAllSubtitles( void );

	/**
	 * Kills all the subtitles associated with the subtitle ID
	 */
	void		KillSubtitles( PTRINT SubtitleID );

	/**
	 * Trim the SubtitleRegion to the safe 80% of the canvas 
	 */
	void		TrimRegionToSafeZone( FCanvas * Canvas, FIntRect & SubtitleRegion );

	/**
	 * If any of the active subtitles need to be split into multiple lines, do so now
	 * - caveat - this assumes the width of the subtitle region does not change while the subtitle is active
	 */
	void		SplitLinesToSafeZone( FCanvas* Canvas, FIntRect & SubtitleRegion );

	/**
	 * Find the highest priority subtitle from the list of currently active ones
	 */
	PTRINT		FindHighestPrioritySubtitle( float CurrentTime );

	/**
	 * Add an array of subtitles to the active list
	 *
	 * @param  SubTitleID - the controlling id that groups sets of subtitles together
	 * @param  Priority - used to prioritize subtitles; higher values have higher priority.  Subtitles with a priority 0.0f are not displayed.
	 * @param  StartTime - float of seconds that the subtitles start at
	 * @param  SoundDuration - time in seconds after which the subtitles do not display
	 * @param  Subtitles - TArray of lines of subtitle and time offset to play them
	 */
	void		QueueSubtitles( PTRINT SubtitleID, float Priority, bool bManualWordWrap, bool bSingleLine, float SoundDuration, const TArray<FSubtitleCue>& Subtitles, float InStartTime );

	static void QueueSubtitles(const FQueueSubtitleParams& QueueSubtitlesParams);

	/**
	 * Draws a subtitle at the specified pixel location.
	 */
	void		DisplaySubtitle( FCanvas *Canvas, FActiveSubtitle* Subtitle, FIntRect & Parms, const FLinearColor& Color );

	/**
	 * Display the currently queued subtitles and cleanup after they have finished rendering
	 *
	 * @param  Canvas - where to render the subtitles
	 * @param  CurrentTime - current world time
	 */
	ENGINE_API void		DisplaySubtitles( FCanvas *InCanvas, FIntRect & SubtitleRegion, float InAudioTimeSeconds );

	/** @return true if there are any active subtitles */
	bool		HasSubtitles( void ) 
	{ 
		return ActiveSubtitles.Num() > 0; 
	}

	/**
	 * @return The height of the currently rendered subtitles, 0 if none are rendering
	 */
	float GetCurrentSubtitlesHeight( void )
	{
		return CurrentSubtitleHeight;
	}

	/**
	 * Returns the singleton subtitle manager instance, creating it if necessary.
	 */
	ENGINE_API static FSubtitleManager* GetSubtitleManager( void );

private:
	/**
	 * The set of active, prioritized subtitles.
	 */
	TMap<PTRINT, FActiveSubtitle>	ActiveSubtitles;

	/**
	 * The current height of the subtitles
	 */
	float CurrentSubtitleHeight;
};
