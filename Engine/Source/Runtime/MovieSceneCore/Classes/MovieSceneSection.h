// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.generated.h"

/**
 * Base class for movie scene sections
 */
UCLASS( abstract, MinimalAPI )
class UMovieSceneSection : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	/**
	 * @return The start time of the section
	 */
	float GetStartTime() const { return StartTime; }

	/**
	 * @return The end time of the section
	 */
	float GetEndTime() const { return EndTime; }
	
	/**
	 * @return The size of the time range of the section
	 */
	float GetTimeSize() const {return EndTime - StartTime;}

	/**
	 * Sets a new end time for this section
	 * 
	 * @param InEndTime	The new end time
	 */
	void SetStartTime( float NewStartTime ) { StartTime = NewStartTime; }

	/**
	 * Sets a new end time for this section
	 * 
	 * @param InEndTime	The new end time
	 */
	void SetEndTime( float NewEndTime ) { EndTime = NewEndTime; }
	
	/**
	 * @return The range of times of the section
	 */
	TRange<float> GetRange() const 
	{
		// Uses an inclusive range so that sections that start and end on the same value aren't considered empty ranges.
		return TRange<float>( TRange<float>::BoundsType::Inclusive( StartTime ), TRange<float>::BoundsType::Inclusive( EndTime ) );
	}
	
	/**
	 * Sets a new range of times for this section
	 * 
	 * @param NewRange	The new range of times
	 */
	void SetRange(TRange<float> NewRange)
	{
		check(NewRange.HasLowerBound() && NewRange.HasUpperBound());

		Modify();

		StartTime = NewRange.GetLowerBoundValue();
		EndTime = NewRange.GetUpperBoundValue();
	}

	/**
	 * Returns whether or not a provided position in time is within the timespan of the section 
	 *
	 * @param Position	The position to check
	 * @return true if the position is within the timespan, false otherwise
	 */
	bool IsTimeWithinSection( float Position ) const 
	{
		return Position >= StartTime && Position <= EndTime;
	}

	/**
	 * Moves the section by a specific amount of time
	 *
	 * @param DeltaTime	The distance in time to move the curve
	 */
	virtual void MoveSection( float DeltaTime )
	{
		Modify();

		StartTime += DeltaTime;
		EndTime += DeltaTime;
	}
	
	/**
	 * Dilates the section by a specific factor
	 *
	 * @param DilationFactor The multipler which scales this section
	 * @param bFromStart Whether to dilate from the beginning or end (whichever stays put)
	 */
	virtual void DilateSection( float DilationFactor, float Origin )
	{
		Modify();

		StartTime = (StartTime - Origin) * DilationFactor + Origin;
		EndTime = (EndTime - Origin) * DilationFactor + Origin;
	}
	
	/**
	 * Gets all snap times for this section
	 *
	 * @param OutSnapTimes The array of times we will to output
	 * @param bGetSectionBorders Gets the section borders in addition to any custom snap times
	 */
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const
	{
		if (bGetSectionBorders)
		{
			OutSnapTimes.Add(StartTime);
			OutSnapTimes.Add(EndTime);
		}
	}

	virtual void GetAllKeyTimes(TArray<float>& OutKeyTimes) const {}

	/** Sets this section's new row index */
	void SetRowIndex(int32 NewRowIndex) {RowIndex = NewRowIndex;}

	/** Gets the row index for this section */
	int32 GetRowIndex() const {return RowIndex;}
	
	/**
	 * Adds a key to a rich curve, finding an existing key to modify or adding a new one
	 *
	 * @param InCurve	The curve to add keys to
	 * @param Time		The time where the key should be added
	 * @param Value		The value at the given time
	 */
	void MOVIESCENECORE_API AddKeyToCurve( FRichCurve& InCurve, float Time, float Value );

	/**
	 * Checks to see if this section overlaps with an array of other sections
	 * given an optional time and track delta.
	 *
	 * @param Sections		Section array to check against
	 * @param TrackDelta	Optional offset to this section's track index
	 * @param TimeDelta		Optional offset to this section's time delta
	 * @return				The first section that overlaps, or null if there is no overlap
	 */
	virtual MOVIESCENECORE_API const UMovieSceneSection* OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta = 0, float TimeDelta = 0.f) const;
	
	/**
	 * Places this section at the first valid row at the specified time. Good for placement upon creation.
	 *
	 * @param Sections		Sections that we can not overlap with
	 * @param InStartTime	The new start time
	 * @param InEndTime		The new end time
	 * @param bAllowMultipleRows	If false, it will move the section in the time direction to make it fit, rather than the row direction
	 */
	virtual MOVIESCENECORE_API void InitialPlacement(const TArray<UMovieSceneSection*>& Sections, float InStartTime, float InEndTime, bool bAllowMultipleRows);

private:
	/** The start time of the section */
	UPROPERTY()
	float StartTime;

	/** The end time of the section */
	UPROPERTY()
	float EndTime;

	/** The row index that this section sits on */
	UPROPERTY()
	int32 RowIndex;
};
