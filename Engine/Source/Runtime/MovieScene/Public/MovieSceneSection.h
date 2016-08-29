// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyParams.h"
#include "MovieSceneSection.generated.h"


/**
 * Base class for movie scene sections
 */
UCLASS(abstract, MinimalAPI)
class UMovieSceneSection
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Calls Modify if this section can be modified, i.e. can't be modified if it's locked
	 *
	 * @return Returns whether this section is locked or not
	 */
	virtual MOVIESCENE_API bool TryModify(bool bAlwaysMarkDirty=true);

	/**
	 * @return The start time of the section
	 */
	float GetStartTime() const
	{
		return StartTime;
	}

	/**
	 * @return The end time of the section
	 */
	float GetEndTime() const
	{
		return EndTime;
	}
	
	/**
	 * @return The size of the time range of the section
	 */
	float GetTimeSize() const
	{
		return EndTime - StartTime;
	}

	/**
	 * Sets a new end time for this section
	 * 
	 * @param InEndTime	The new end time
	 */
	void SetStartTime(float NewStartTime)
	{ 
		if (TryModify())
		{
			StartTime = NewStartTime;
		}
	}

	/**
	 * Sets a new end time for this section
	 * 
	 * @param InEndTime	The new end time
	 */
	void SetEndTime(float NewEndTime)
	{ 
		if (TryModify())
		{
			EndTime = NewEndTime;
		}
	}
	
	/**
	 * @return The range of times of the section
	 */
	TRange<float> GetRange() const 
	{
		// Use the single value constructor for zero sized ranges because it creates a range that is inclusive on both upper and lower
		// bounds which isn't considered "empty".  Use the standard constructor for non-zero sized ranges so that they work well when
		// calculating overlap with other non-zero sized ranges.
		return (StartTime == EndTime)
			? TRange<float>(StartTime)
			: TRange<float>(StartTime, TRangeBound<float>::Inclusive(EndTime));
	}
	
	/**
	 * Sets a new range of times for this section
	 * 
	 * @param NewRange	The new range of times
	 */
	void SetRange(TRange<float> NewRange)
	{
		check(NewRange.HasLowerBound() && NewRange.HasUpperBound());

		if (TryModify())
		{
			StartTime = NewRange.GetLowerBoundValue();
			EndTime = NewRange.GetUpperBoundValue();
		}
	}

	/**
	 * Returns whether or not a provided position in time is within the timespan of the section 
	 *
	 * @param Position	The position to check
	 * @return true if the position is within the timespan, false otherwise
	 */
	bool IsTimeWithinSection(float Position) const 
	{
		return Position >= StartTime && Position <= EndTime;
	}

	/**
	 * Moves the section by a specific amount of time
	 *
	 * @param DeltaTime	The distance in time to move the curve
	 * @param KeyHandles The key handles to operate on
	 */
	virtual void MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
	{
		if (TryModify())
		{
			StartTime += DeltaTime;
			EndTime += DeltaTime;
		}
	}
	
	/**
	 * Dilates the section by a specific factor
	 *
	 * @param DilationFactor The multiplier which scales this section
	 * @param bFromStart Whether to dilate from the beginning or end (whichever stays put)
	 * @param KeyHandles The key handles to operate on
	 */
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
	{
		if (TryModify())
		{
			StartTime = (StartTime - Origin) * DilationFactor + Origin;
			EndTime = (EndTime - Origin) * DilationFactor + Origin;
		}
	}
	
	/**
	 * Split a section in two at the split time
	 *
	 * @param SplitTime The time at which to split
	 * @return The newly created split section
	 */
	virtual MOVIESCENE_API UMovieSceneSection* SplitSection(float SplitTime);

	/**
	 * Trim a section at the trim time
	 *
	 * @param TrimTime The time at which to trim
	 * @param bTrimLeft Whether to trim left or right
	 */
	virtual MOVIESCENE_API void TrimSection(float TrimTime, bool bTrimLeft);

	/**
	 * Get the key handles for the keys on the curves within this section
	 *
	 * @param OutKeyHandles Will contain the key handles of the keys on the curves within this section
	 * @param TimeRange Optional time range that the keys must be in (default = all)
	 */
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const { };

	/**
	 * Get the data structure representing the specified key.
	 *
	 * @param KeyHandle The handle of the key.
	 * @return The key's data structure representation, or nullptr if key not found or no structure available.
	 */
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(const TArray<FKeyHandle>& KeyHandles)
	{
		return nullptr;
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
	
	/** Sets this section's priority over overlapping sections (higher wins) */
	void SetOverlapPriority(int32 NewPriority)
	{
		OverlapPriority = NewPriority;
	}

	/** Gets this section's priority over overlapping sections (higher wins) */
	int32 GetOverlapPriority() const
	{
		return OverlapPriority;
	}

	/**
	 * Adds a key to a rich curve, finding an existing key to modify or adding a new one.
	 *
	 * @param InCurve The curve to add keys to.
	 * @param Time The time where the key should be added.
	 * @param Value The value at the given time.
	 * @param Interpolation The key interpolation to use.
	 * @param bUnwindRotation Unwind rotation.
	 */
	void MOVIESCENE_API AddKeyToCurve(FRichCurve& InCurve, float Time, float Value, EMovieSceneKeyInterpolation Interpolation, const bool bUnwindRotation = false);

	/**
	 * Sets the default value for a curve.
	 *
	 * @param InCurve The curve to set a default value on.
	 * @param Value The value to use as the default.
	 */
	void MOVIESCENE_API SetCurveDefault(FRichCurve& InCurve, float Value);

	/**
	 * Checks to see if this section overlaps with an array of other sections
	 * given an optional time and track delta.
	 *
	 * @param Sections Section array to check against.
	 * @param TrackDelta Optional offset to this section's track index.
	 * @param TimeDelta Optional offset to this section's time delta.
	 * @return The first section that overlaps, or null if there is no overlap.
	 */
	virtual MOVIESCENE_API const UMovieSceneSection* OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta = 0, float TimeDelta = 0.f) const;
	
	/**
	 * Places this section at the first valid row at the specified time. Good for placement upon creation.
	 *
	 * @param Sections Sections that we can not overlap with.
	 * @param InStartTime The new start time.
	 * @param InEndTime The new end time.
	 * @param bAllowMultipleRows If false, it will move the section in the time direction to make it fit, rather than the row direction.
	 */
	virtual MOVIESCENE_API void InitialPlacement(const TArray<UMovieSceneSection*>& Sections, float InStartTime, float InEndTime, bool bAllowMultipleRows);

	/** Whether or not this section is active. */
	void SetIsActive(bool bInIsActive) { bIsActive = bInIsActive; }
	bool IsActive() const { return bIsActive; }

	/** Whether or not this section is locked. */
	void SetIsLocked(bool bInIsLocked) { bIsLocked = bInIsLocked; }
	bool IsLocked() const { return bIsLocked; }

	/** Whether or not this section is infinite. An infinite section will draw the entire width of the track. StartTime and EndTime will be ignored but not discarded. */
	void SetIsInfinite(bool bInIsInfinite) { bIsInfinite = bInIsInfinite; }
	bool IsInfinite() const { return bIsInfinite; }

	/** Gets the time for the key referenced by the supplied key handle. */
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const PURE_VIRTUAL( UAISenseEvent::GetKeyTime, return TOptional<float>(); );

	/** Sets the time for the key referenced by the supplied key handle. */
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) PURE_VIRTUAL( UAISenseEvent::SetKeyTime, );

private:

	/** The start time of the section */
	UPROPERTY(EditAnywhere, Category="Section")
	float StartTime;

	/** The end time of the section */
	UPROPERTY(EditAnywhere, Category="Section")
	float EndTime;

	/** The row index that this section sits on */
	UPROPERTY()
	int32 RowIndex;

	/** This section's priority over overlapping sections */
	UPROPERTY()
	int32 OverlapPriority;

	/** Toggle whether this section is active/inactive */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsActive : 1;

	/** Toggle whether this section is locked/unlocked */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsLocked : 1;

	/** Toggle to set this section to be infinite */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsInfinite : 1;
};
