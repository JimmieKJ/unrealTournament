// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Abstract base class of animation composite base
 * This contains Composite Section data and some necessary interface to make this work
 */

#include "AnimSequenceBase.h"
#include "AnimCompositeBase.generated.h"

class UAnimSequence;

/** Struct defining a RootMotionExtractionStep.
 * When extracting RootMotion we can encounter looping animations (wrap around), or different animations.
 * We break those up into different steps, to help with RootMotion extraction, 
 * as we can only extract a contiguous range per AnimSequence.
 */
USTRUCT()
struct FRootMotionExtractionStep
{
	GENERATED_USTRUCT_BODY()

	/** AnimSequence ref */
	UPROPERTY()
	UAnimSequence * AnimSequence;

	/** Start position to extract root motion from. */
	UPROPERTY()
	float StartPosition;

	/** End position to extract root motion to. */
	UPROPERTY()
	float EndPosition;

	FRootMotionExtractionStep() 
		: AnimSequence(NULL)
		, StartPosition(0.f)
		, EndPosition(0.f)
		{
		}

	FRootMotionExtractionStep(UAnimSequence * InAnimSequence, float InStartPosition, float InEndPosition) 
		: AnimSequence(InAnimSequence)
		, StartPosition(InStartPosition)
		, EndPosition(InEndPosition)
	{
	}
};

/** this is anim segment that defines what animation and how **/
USTRUCT()
struct FAnimSegment
{
	GENERATED_USTRUCT_BODY()

	/** Anim Reference to play - only allow AnimSequence or AnimComposite **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimSegment)
	UAnimSequenceBase* AnimReference;

	/** Start Pos within this AnimCompositeBase */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=AnimSegment)
	float	StartPos;

	/** Time to start playing AnimSequence at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimSegment)
	float	AnimStartTime;

	/** Time to end playing the AnimSequence at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimSegment)
	float	AnimEndTime;

	/** Playback speed of this animation. If you'd like to reverse, set -1*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimSegment)
	float	AnimPlayRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimSegment)
	int32		LoopingCount;

	FAnimSegment()
		: AnimReference(NULL)
		, StartPos(0.f)
		, AnimStartTime(0.f)
		, AnimEndTime(0.f)
		, AnimPlayRate(1.f)
		, LoopingCount(1)
	{
	}

	/** Ensures PlayRate is non Zero */
	float GetValidPlayRate() const
	{
		float SeqPlayRate = AnimReference ? AnimReference->RateScale : 1.0f;
		float FinalPlayRate = SeqPlayRate * AnimPlayRate;
		return (FMath::IsNearlyZero(FinalPlayRate) ? 1.f : FinalPlayRate);
	}

	float GetLength() const
	{
		return (float(LoopingCount) * (AnimEndTime - AnimStartTime)) / FMath::Abs(GetValidPlayRate());
	}

	bool IsInRange(float CurPos) const
	{
		return ((CurPos >= StartPos) && (CurPos <= (StartPos + GetLength())));
	}

	/**
	 * Get Animation Data, for now have weight to be controlled here, in the future, it will be controlled in Track
	 */
	UAnimSequenceBase* GetAnimationData(float PositionInTrack, float& PositionInAnim, float& Weight) const;

	/** Converts 'Track Position' to position on AnimSequence.
	 * Note: doesn't check that position is in valid range, must do that before calling this function! */
	float ConvertTrackPosToAnimPos(const float& TrackPosition) const;

	/** 
	 * Retrieves AnimNotifies between two Track time positions. ]PreviousTrackPosition, CurrentTrackPosition]
	 * Between PreviousTrackPosition (exclusive) and CurrentTrackPosition (inclusive).
	 * Supports playing backwards (CurrentTrackPosition<PreviousTrackPosition).
	 * Only supports contiguous range, does NOT support looping and wrapping over.
	 */
	void GetAnimNotifiesFromTrackPositions(const float& PreviousTrackPosition, const float& CurrentTrackPosition, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const;
	
	/** 
	 * Given a Track delta position [StartTrackPosition, EndTrackPosition]
	 * See if this AnimSegment overlaps any of it, and if it does, break it up into RootMotionExtractionSteps.
	 * Supports animation playing forward and backward. Track segment should be a contiguous range, not wrapping over due to looping.
	 */
	void GetRootMotionExtractionStepsForTrackRange(TArray<FRootMotionExtractionStep> & RootMotionExtractionSteps, const float StartPosition, const float EndPosition) const;
};

/** This is list of anim segments for this track 
 * For now this is only one TArray, but in the future 
 * we should define more transition/blending behaviors
 **/
USTRUCT()
struct FAnimTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimTrack, EditFixedSize)
	TArray<FAnimSegment>	AnimSegments;

	FAnimTrack() {}
	ENGINE_API float GetLength() const;
	bool IsAdditive() const;
	bool IsRotationOffsetAdditive() const;

	ENGINE_API int32 GetTrackAdditiveType() const;

	/** Returns whether any of the animation sequences this track uses has root motion */
	bool HasRootMotion() const;

	/** 
	 * Given a Track delta position [StartTrackPosition, EndTrackPosition]
	 * See if any AnimSegment overlaps any of it, and if it does, break it up into RootMotionExtractionPieces.
	 * Supports animation playing forward and backward. Track segment should be a contiguous range, not wrapping over due to looping.
	 */
	void GetRootMotionExtractionStepsForTrackRange(TArray<FRootMotionExtractionStep> & RootMotionExtractionSteps, const float StartTrackPosition, const float EndTrackPosition) const;

	/** Ensure segment times are correctly formed (no gaps and no extra time at the end of the anim reference) */
	void ValidateSegmentTimes();

	/** Gets the index of the segment at the given absolute montage time. */	
	int32 GetSegmentIndexAtTime(float InTime);

	/** Get the segment at the given absolute montage time */
	FAnimSegment* GetSegmentAtTime(float InTime);

#if WITH_EDITOR
	bool GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences) const;
	void ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap);

	/** Moves anim segments so that there are no gaps between one finishing
	 *  and the next starting, preserving the order of AnimSegments
	 */
	ENGINE_API void CollapseAnimSegments();

	/** Sorts AnimSegments based on the start time of each segment */
	ENGINE_API void SortAnimSegments();
#endif
};

UCLASS(abstract, MinimalAPI)
class UAnimCompositeBase : public UAnimSequenceBase
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	/** Set Sequence Length */
	ENGINE_API void SetSequenceLength(float InSequenceLength);
#endif

	// Extracts root motion from the supplied FAnimTrack between the Start End range specified
	ENGINE_API void ExtractRootMotionFromTrack(const FAnimTrack &SlotAnimTrack, float StartTrackPosition, float EndTrackPosition, FRootMotionMovementParams &RootMotion) const;
};

