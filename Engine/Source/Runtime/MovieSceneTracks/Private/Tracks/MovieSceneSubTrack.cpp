// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneSubTrack.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneSubSection.h"
#include "MovieScene.h"
#include "Compilation/MovieSceneSegmentCompiler.h"


#define LOCTEXT_NAMESPACE "MovieSceneSubTrack"


/* UMovieSceneSubTrack interface
 *****************************************************************************/
UMovieSceneSubTrack::UMovieSceneSubTrack( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(180, 0, 40, 65);
#endif
}

UMovieSceneSubSection* UMovieSceneSubTrack::AddSequence(UMovieSceneSequence* Sequence, float StartTime, float Duration, const bool& bInsertSequence)
{
	Modify();

	// If inserting, push all existing sections out to make space for the new one
	if (bInsertSequence)
	{
		// If there's a shot that starts at the same time as this new shot, push the shots forward to make space for this one
		bool bPushShotsForward = false;
		for (auto Section : Sections)
		{
			float SectionStartTime = Section->GetStartTime();
			if (FMath::IsNearlyEqual(SectionStartTime, StartTime))
			{
				bPushShotsForward = true;
				break;
			}
		}
		if (bPushShotsForward)
		{
			for (auto Section : Sections)
			{
				float SectionStartTime = Section->GetStartTime();
				if (SectionStartTime >= StartTime)
				{
					Section->SetStartTime(SectionStartTime + Duration);
					Section->SetEndTime(Section->GetEndTime() + Duration);
				}
			}
		}
		// Otherwise, see if there's a shot after the start time and clamp the duration to that next shot
		else
		{
			bool bFoundAfterShot = false;
			float MinDiff = FLT_MAX;
			for (auto Section : Sections)
			{
				float SectionStartTime = Section->GetStartTime();
				if (SectionStartTime > StartTime)
				{
					bFoundAfterShot = true;
					float Diff = SectionStartTime - StartTime;
					MinDiff = FMath::Min(MinDiff, Diff);
				}
			}

			if (MinDiff != FLT_MAX)
			{
				Duration = MinDiff;
			}
		}
	}
	// Otherwise, append this to the end of the existing sequences
	else
	{
		if (Sections.Num())
		{
			StartTime = Sections[0]->GetEndTime();

			for (const auto& Section : Sections)
			{
				StartTime = FMath::Max(StartTime, Section->GetEndTime());
			}
		}
	}

	UMovieSceneSubSection* NewSection = CastChecked<UMovieSceneSubSection>(CreateNewSection());
	{
		NewSection->SetSequence(Sequence);
		NewSection->SetStartTime(StartTime);
		NewSection->SetEndTime(StartTime + Duration);
	}

	Sections.Add(NewSection);

	return NewSection;
}

UMovieSceneSubSection* UMovieSceneSubTrack::AddSequenceToRecord()
{
	UMovieScene* MovieScene = CastChecked<UMovieScene>(GetOuter());
	TRange<float> PlaybackRange = MovieScene->GetPlaybackRange();

	int32 MaxRowIndex = 0;
	for (auto Section : Sections)
	{
		MaxRowIndex = FMath::Max(Section->GetRowIndex(), MaxRowIndex);
	}

	UMovieSceneSubSection* NewSection = CastChecked<UMovieSceneSubSection>(CreateNewSection());
	{
		NewSection->SetRowIndex(MaxRowIndex + 1);
		NewSection->SetAsRecording(true);
		NewSection->SetStartTime(PlaybackRange.GetLowerBoundValue());
		NewSection->SetEndTime(PlaybackRange.GetUpperBoundValue());
	}

	Sections.Add(NewSection);

	return NewSection;
}

bool UMovieSceneSubTrack::ContainsSequence(const UMovieSceneSequence& Sequence, bool Recursively) const
{
	for (const auto& Section : Sections)
	{
		const auto SubSection = CastChecked<UMovieSceneSubSection>(Section);

		// is the section referencing the sequence?
		const UMovieSceneSequence* SubSequence = SubSection->GetSequence();

		if (SubSequence == &Sequence)
		{
			return true;
		}

		if (!Recursively)
		{
			continue;
		}

		// does the section have sub-tracks referencing the sequence?
		const UMovieScene* SubMovieScene = SubSequence->GetMovieScene();

		if (SubMovieScene == nullptr)
		{
			continue;
		}

		UMovieSceneSubTrack* SubSubTrack = SubMovieScene->FindMasterTrack<UMovieSceneSubTrack>();

		if ((SubSubTrack != nullptr) && SubSubTrack->ContainsSequence(Sequence))
		{
			return true;
		}
	}

	return false;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneSubTrack::AddSection(UMovieSceneSection& Section)
{
	if (Section.IsA<UMovieSceneSubSection>())
	{
		Sections.Add(&Section);
	}
}


UMovieSceneSection* UMovieSceneSubTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSubSection>(this, NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneSubTrack::GetAllSections() const 
{
	return Sections;
}


TRange<float> UMovieSceneSubTrack::GetSectionBoundaries() const
{
	TArray<TRange<float>> Bounds;

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		Bounds.Add(Sections[SectionIndex]->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


bool UMovieSceneSubTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneSubTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


void UMovieSceneSubTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


void UMovieSceneSubTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}


bool UMovieSceneSubTrack::SupportsMultipleRows() const
{
	return true;
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneSubTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Subscenes");
}
#endif

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneSubTrack::GetTrackCompilerRules() const
{
	return TInlineValue<FMovieSceneSegmentCompilerRules>();
}

struct FSubTrackRemapper
{
	FSubTrackRemapper(const FMovieSceneTrackCompilerArgs& InArgs, const TArray<UMovieSceneSection*>& InSections)
		: Args(InArgs)
		, Sections(InSections)
	{
	}

	/** Remap the specified segment, generated from the main sub track, into the outer template's generator */
	void RemapSegment(const FMovieSceneSegment& Segment)
	{
		TSet<FMovieSceneEvaluationFieldSegmentPtr> AllSegmentPtrs;

		// Remap, then add all segment ptrs to the master sequence
		for (const FSectionEvaluationData& EvalData : Segment.Impls)
		{
			const int32 SectionIndex = EvalData.ImplIndex;

			// Remap the sequence in the generator, if we haven't already
			if (!SectionDataCache.Contains(SectionIndex) && !RemapSubSection(SectionIndex))
			{
				// We failed to even remap the track. Leave this ptr out of the segment
				continue;
			}

			FSectionData* Cache = SectionDataCache.Find(SectionIndex);
			check(Cache);

			const UMovieSceneSubSection* SubSection = CastChecked<const UMovieSceneSubSection>(Sections[SectionIndex]);

			// Remap the tracks contained within this sequence only within the range of the section
			FMovieSceneSequenceTransform InnerSequenceTransform = Args.Generator.GetSequenceTransform(Cache->RemappedID);

			// Add preroll section
			if (SubSection->Parameters.PrerollTime > 0.f && Segment.Range.GetLowerBound().IsClosed())
			{
				TRange<float> PrerollRange = TRange<float>(Segment.Range.GetLowerBoundValue() - SubSection->Parameters.PrerollTime, TRangeBound<float>::FlipInclusion(Segment.Range.GetLowerBound()));
				if (!PrerollRange.IsEmpty())
				{
					// Iterate all ptrs that exist in the preroll range
					TRange<int32> OverlappingSegments = Cache->Template->EvaluationField.OverlapRange(PrerollRange * InnerSequenceTransform);
					for (int32 Index = OverlappingSegments.GetLowerBoundValue(); Index < OverlappingSegments.GetUpperBoundValue(); ++Index)
					{
						AllSegmentPtrs.Reset();

						TRange<float> InnerSegmentRange = Cache->Template->EvaluationField.Ranges[Index] * InnerSequenceTransform.Inverse();
						TRange<float> OverlappingRange = TRange<float>::Intersection(InnerSegmentRange, PrerollRange);
						if (!OverlappingRange.IsEmpty())
						{
							RemapPrerollGroup(*Cache, *Cache->Template, Index, AllSegmentPtrs);
							Args.Generator.AddExternalSegments(OverlappingRange, AllSegmentPtrs.Array());
						}
					}
				}
			}

			// Add main evaluation section
			{
				TRange<int32> OverlappingSegments = Cache->Template->EvaluationField.OverlapRange(Segment.Range * InnerSequenceTransform);
				for (int32 Index = OverlappingSegments.GetLowerBoundValue(); Index < OverlappingSegments.GetUpperBoundValue(); ++Index)
				{
					AllSegmentPtrs.Reset();

					TRange<float> InnerSegmentRange = Cache->Template->EvaluationField.Ranges[Index] * InnerSequenceTransform.Inverse();
					TRange<float> OverlappingRange = TRange<float>::Intersection(InnerSegmentRange, Segment.Range);
					if (!OverlappingRange.IsEmpty())
					{
						RemapEvaluationGroup(*Cache, Cache->Template->EvaluationField.Groups[Index], AllSegmentPtrs);
						Args.Generator.AddExternalSegments(OverlappingRange, AllSegmentPtrs.Array());
					}
				}
			}

			// Add postroll section
			if (SubSection->Parameters.PostrollTime > 0.f && Segment.Range.GetUpperBound().IsClosed())
			{
				TRange<float> PostrollRange = TRange<float>(TRangeBound<float>::FlipInclusion(Segment.Range.GetUpperBound()), Segment.Range.GetUpperBoundValue() + SubSection->Parameters.PostrollTime);
				if (!PostrollRange.IsEmpty())
				{
					// Iterate all ptrs that exist in the preroll range
					TRange<int32> OverlappingSegments = Cache->Template->EvaluationField.OverlapRange(PostrollRange * InnerSequenceTransform);
					for (int32 Index = OverlappingSegments.GetLowerBoundValue(); Index < OverlappingSegments.GetUpperBoundValue(); ++Index)
					{
						AllSegmentPtrs.Reset();

						TRange<float> InnerSegmentRange = Cache->Template->EvaluationField.Ranges[Index] * InnerSequenceTransform.Inverse();
						TRange<float> OverlappingRange = TRange<float>::Intersection(InnerSegmentRange, PostrollRange);
						if (!OverlappingRange.IsEmpty())
						{
							RemapPostrollGroup(*Cache, *Cache->Template, Index, AllSegmentPtrs);
							Args.Generator.AddExternalSegments(OverlappingRange, AllSegmentPtrs.Array());
						}
					}
				}
			}
		}
	}

	bool RemapSubSection(int32 SectionIndex)
	{
		const UMovieSceneSubSection* SubSection = CastChecked<const UMovieSceneSubSection>(Sections[SectionIndex]);
		UMovieSceneSequence* SubSequence = SubSection->GetSequence();
		if (!SubSequence)
		{
			return false;
		}

		FMovieSceneEvaluationTemplate& Template = Args.SubSequenceStore.GetCompiledTemplate(*SubSequence);
		
		FMovieSceneSequenceTransform RootToSequenceTransform =
			FMovieSceneSequenceTransform(SubSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue() + SubSection->Parameters.StartOffset) *		// Inner play offset
			FMovieSceneSequenceTransform(0.f, SubSection->Parameters.TimeScale) *		// Inner play rate
			FMovieSceneSequenceTransform(-SubSection->GetStartTime());					// Outer section start time

#if WITH_EDITORONLY_DATA
		TRange<float> InnerSectionRange(
			SubSection->GetStartTime() * RootToSequenceTransform,
			SubSection->GetEndTime() * RootToSequenceTransform
			);
		FMovieSceneSubSequenceData SubData(*SubSequence, SubSection->GetSequenceID(), *SubSection->GetPathNameInMovieScene(), InnerSectionRange);
#else
		FMovieSceneSubSequenceData SubData(*SubSequence, SubSection->GetSequenceID());
#endif

		// Construct the sub sequence data for this sub section
		SubData.RootToSequenceTransform = RootToSequenceTransform;

		// Remap it into the template generator
		FMovieSceneSequenceID RemappedID = Args.Generator.GenerateSequenceID(SubData, MovieSceneSequenceID::Root);

		FSectionData Cache(RemappedID, &Template);

		// For editor previews, we remap all sub sequences regardless of whether they are actually used in the sub sequence or not
		if (Args.Params.bForEditorPreview)
		{
			for (auto& Pair : Cache.Template->Hierarchy.AllSubSequenceData())
			{
				FMovieSceneSequenceID ChildID = Pair.Key;
				RemapSubSequence(Cache, ChildID);
			}
		}

		SectionDataCache.Add(SectionIndex, MoveTemp(Cache));
		return true;
	}

private:

	struct FSectionData
	{
		FSectionData(FMovieSceneSequenceIDRef InRemappedID, const FMovieSceneEvaluationTemplate* InTemplate) : RemappedID(InRemappedID), Template(InTemplate) {}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
		FSectionData(FSectionData&&) = default;
		FSectionData& operator=(FSectionData&&) = default;
#else
		FSectionData(FSectionData&& RHS)
		{
			*this = MoveTemp(RHS);
		}
		FSectionData& operator=(FSectionData&& RHS)
		{
			RemappedID = MoveTemp(RHS.RemappedID);
			Template = MoveTemp(RHS.Template);
			ChildrenRemappedIDs = MoveTemp(RHS.ChildrenRemappedIDs);
			return *this;
		}
#endif
		/** The ID of this section's sequence in the master sequecne */
		FMovieSceneSequenceID RemappedID;
		/** The compiled template (stored in the sequence itself) */
		const FMovieSceneEvaluationTemplate* Template;
		/** A map of remapped sequence IDs originating from this sequence's external tracks*/
		TMap<FMovieSceneSequenceID, FMovieSceneSequenceID> ChildrenRemappedIDs;
	};

	void RemapPrerollGroup(FSectionData& Cache, const FMovieSceneEvaluationTemplate& Template, int32 EvaluationFieldIndex, TSet<FMovieSceneEvaluationFieldSegmentPtr>& AllSegmentPtrs)
	{
		const FMovieSceneEvaluationGroup& Group = Template.EvaluationField.Groups[EvaluationFieldIndex];

		for (const FMovieSceneEvaluationGroupLUTIndex& LUTIndex : Group.LUTIndices)
		{
			const int32 First = LUTIndex.LUTOffset;
			const int32 Last = First + LUTIndex.NumInitPtrs + LUTIndex.NumEvalPtrs;

			for (int32 Index = First; Index < Last; ++Index)
			{
				FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = Group.SegmentPtrLUT[Index];

				const FMovieSceneEvaluationTrack* Track = FindTrack(SegmentPtr, Cache);
				if (!ensure(Track) || !Track->ShouldEvaluateInPreroll())
				{
					continue;
				}

				SegmentPtr.SequenceID = RemapSubSequence(Cache, SegmentPtr.SequenceID);
				AllSegmentPtrs.Add(SegmentPtr);
			}
		}
	}

	void RemapEvaluationGroup(FSectionData& Cache, const FMovieSceneEvaluationGroup& Group, TSet<FMovieSceneEvaluationFieldSegmentPtr>& AllSegmentPtrs)
	{
		for (const FMovieSceneEvaluationGroupLUTIndex& LUTIndex : Group.LUTIndices)
		{
			const int32 First = LUTIndex.LUTOffset;
			const int32 Last = First + LUTIndex.NumInitPtrs + LUTIndex.NumEvalPtrs;

			for (int32 Index = First; Index < Last; ++Index)
			{
				FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = Group.SegmentPtrLUT[Index];
				SegmentPtr.SequenceID = RemapSubSequence(Cache, SegmentPtr.SequenceID);
				AllSegmentPtrs.Add(SegmentPtr);
			}
		}
	}

	void RemapPostrollGroup(FSectionData& Cache, const FMovieSceneEvaluationTemplate& Template, int32 EvaluationFieldIndex, TSet<FMovieSceneEvaluationFieldSegmentPtr>& AllSegmentPtrs)
	{
		const FMovieSceneEvaluationGroup& Group = Template.EvaluationField.Groups[EvaluationFieldIndex];

		for (const FMovieSceneEvaluationGroupLUTIndex& LUTIndex : Group.LUTIndices)
		{
			const int32 First = LUTIndex.LUTOffset;
			const int32 Last = First + LUTIndex.NumInitPtrs + LUTIndex.NumEvalPtrs;

			for (int32 Index = First; Index < Last; ++Index)
			{
				FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = Group.SegmentPtrLUT[Index];

				const FMovieSceneEvaluationTrack* Track = FindTrack(SegmentPtr, Cache);
				if (!ensure(Track) || !Track->ShouldEvaluateInPostroll())
				{
					continue;
				}

				SegmentPtr.SequenceID = RemapSubSequence(Cache, SegmentPtr.SequenceID);
				AllSegmentPtrs.Add(SegmentPtr);
			}
		}
	}

	FMovieSceneSequenceID RemapSubSequence(FSectionData& Cache, FMovieSceneSequenceID SourceID)
	{
		// Root tracks use the cache's remapped ID
		if (SourceID == MovieSceneSequenceID::Root)
		{
			return Cache.RemappedID;
		}

		// Nested tracks tracks - may need to remap the outer sequence
		if (Cache.ChildrenRemappedIDs.Contains(SourceID))
		{
			return Cache.ChildrenRemappedIDs.FindRef(SourceID);
		}

		// Remap this into the root
		const FMovieSceneSubSequenceData* SubData = Cache.Template->Hierarchy.FindSubData(SourceID);
		const FMovieSceneSequenceHierarchyNode* HierarchyNode = Cache.Template->Hierarchy.FindNode(SourceID);
		check(SubData && HierarchyNode);

		FMovieSceneSubSequenceData SubDataCopy = *SubData;

		// Accumulate the parent's sequence transform
		SubDataCopy.RootToSequenceTransform = SubDataCopy.RootToSequenceTransform * Args.Generator.GetSequenceTransform(Cache.RemappedID);

		FMovieSceneSequenceID RemappedParentID = HierarchyNode->ParentID == MovieSceneSequenceID::Root ? Cache.RemappedID : RemapSubSequence(Cache, HierarchyNode->ParentID);

		// Remap this sequence within the main generator, under this section as a parent
		FMovieSceneSequenceID RemappedID = Args.Generator.GenerateSequenceID(SubDataCopy, RemappedParentID);
		Cache.ChildrenRemappedIDs.Add(SourceID, RemappedID);

		return RemappedID;
	}

	const FMovieSceneEvaluationTrack* FindTrack(FMovieSceneEvaluationFieldTrackPtr TrackPtr, const FSectionData& SectionData) const
	{
		if (TrackPtr.SequenceID == MovieSceneSequenceID::Root)
		{
			return SectionData.Template->FindTrack(TrackPtr.TrackIdentifier);
		}
		return nullptr;
	}

private:

	/** Compilation parameters */
	const FMovieSceneTrackCompilerArgs Args;

	/** Array of sections to compile */
	const TArray<UMovieSceneSection*>& Sections;

	/** Cached information pertaining to each section's sequence */
	TMap<int32, FSectionData> SectionDataCache;
};

void UMovieSceneSubTrack::GenerateTemplate(const FMovieSceneTrackCompilerArgs& Args) const
{
	// We compile sub tracks by generating a track evaluation field, and remapping child tracks from the inner sequence to the outer's timeline
	TInlineValue<FMovieSceneSegmentCompilerRules> TrackRules = GetTrackCompilerRules();
	TInlineValue<FMovieSceneSegmentCompilerRules> RowRules = GetRowCompilerRules();

	// Generate track rows
	FMovieSceneTrackCompiler::FRows TrackRows(Sections, RowRules.GetPtr());

	// Compile track rows into an evaluation field based on the supplied compiler rules
	FMovieSceneTrackCompiler Compiler;
	FMovieSceneTrackEvaluationField EvaluationField = Compiler.Compile(TrackRows.Rows, TrackRules.GetPtr());

	// Remap each segment in the final eval field into the outer template generator.
	// This ensures we only reference segments that are actually going to be evaluated in the sub section
	FSubTrackRemapper Remapper(Args, Sections);

	// This is only required in editor, where we need to navigate sections that aren't necessarily evaluated in the master sequence (outside of an inner playback range, or don't contain any tracks)
	const bool bAggressivelyRemapSections = Args.Params.bForEditorPreview;
	if (bAggressivelyRemapSections)
	{
		// Ensure all sub data is remapped to the master
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			Remapper.RemapSubSection(SectionIndex);
		}
	}

	for (const FMovieSceneSegment& Segment : EvaluationField.Segments)
	{
		Remapper.RemapSegment(Segment);
	}
}


#undef LOCTEXT_NAMESPACE
