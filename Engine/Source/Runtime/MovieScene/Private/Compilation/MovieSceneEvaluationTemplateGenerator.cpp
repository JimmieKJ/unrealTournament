// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Compilation/MovieSceneEvaluationTemplateGenerator.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"

namespace 
{
	void AddPtrsToGroup(FMovieSceneEvaluationGroup& Group, bool bRequiresImmediateFlush, TArray<FMovieSceneEvaluationFieldSegmentPtr>& InitPtrs, TArray<FMovieSceneEvaluationFieldSegmentPtr>& EvalPtrs)
	{
		if (!InitPtrs.Num() && !EvalPtrs.Num())
		{
			return;
		}

		FMovieSceneEvaluationGroupLUTIndex Index;
		
		Index.bRequiresImmediateFlush = bRequiresImmediateFlush;

		Index.LUTOffset = Group.SegmentPtrLUT.Num();
		Index.NumInitPtrs = InitPtrs.Num();
		Index.NumEvalPtrs = EvalPtrs.Num();

		Group.LUTIndices.Add(Index);
		Group.SegmentPtrLUT.Append(InitPtrs);
		Group.SegmentPtrLUT.Append(EvalPtrs);

		InitPtrs.Reset();
		EvalPtrs.Reset();
	}
}

FMovieSceneEvaluationTemplateGenerator::FMovieSceneEvaluationTemplateGenerator(UMovieSceneSequence& InSequence, FMovieSceneEvaluationTemplate& OutTemplate, FMovieSceneSequenceTemplateStore& InStore)
	: SourceSequence(InSequence)
	, Template(OutTemplate)
	, TransientArgs(*this, InStore)
{
}

void FMovieSceneEvaluationTemplateGenerator::AddLegacyTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack)
{
	AddOwnedTrack(MoveTemp(InTrackTemplate), SourceTrack);
	Template.bHasLegacyTrackInstances = true;
}

void FMovieSceneEvaluationTemplateGenerator::AddOwnedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack)
{
	// Add the track to the template
	Template.AddTrack(SourceTrack.GetSignature(), MoveTemp(InTrackTemplate));
	CompiledSignatures.Add(SourceTrack.GetSignature());
}

void FMovieSceneEvaluationTemplateGenerator::AddSharedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, FMovieSceneSharedDataId SharedId, const UMovieSceneTrack& SourceTrack)
{
	FSharedPersistentDataKey Key(SharedId, FMovieSceneEvaluationOperand(MovieSceneSequenceID::Root, InTrackTemplate.GetObjectBindingID()));

	if (AddedSharedTracks.Contains(Key))
	{
		return;
	}

	AddedSharedTracks.Add(Key);
	AddOwnedTrack(MoveTemp(InTrackTemplate), SourceTrack);
}

void FMovieSceneEvaluationTemplateGenerator::AddExternalSegments(TRange<float> RootRange, TArrayView<const FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrs)
{
	if (RootRange.IsEmpty())
	{
		return;
	}

	SegmentData.Reserve(SegmentData.Num() + SegmentPtrs.Num());

	for (const FMovieSceneEvaluationFieldSegmentPtr& SegmentPtr : SegmentPtrs)
	{
		if (!ExternalSegmentLookup.Contains(SegmentPtr))
		{
			ExternalSegmentLookup.Add(SegmentPtr, TrackLUT.Num());
			TrackLUT.Add(SegmentPtr);
		}

		SegmentData.Add(FMovieSceneSectionData(RootRange, ExternalSegmentLookup.FindRef(SegmentPtr)));
	}
}

FMovieSceneSequenceTransform FMovieSceneEvaluationTemplateGenerator::GetSequenceTransform(FMovieSceneSequenceIDRef InSequenceID) const
{
	if (InSequenceID == MovieSceneSequenceID::Root)
	{
		return FMovieSceneSequenceTransform();
	}

	const FMovieSceneSubSequenceData* Data = Template.Hierarchy.FindSubData(InSequenceID);
	return ensure(Data) ? Data->RootToSequenceTransform : FMovieSceneSequenceTransform();
}

FMovieSceneSequenceID FMovieSceneEvaluationTemplateGenerator::GenerateSequenceID(FMovieSceneSubSequenceData SequenceData, FMovieSceneSequenceIDRef ParentID)
{
	FMovieSceneSequenceHierarchyNode* ParentNode = Template.Hierarchy.FindNode(ParentID);
	checkf(ParentNode, TEXT("Cannot generate a sequence ID for a ParentID that doesn't yet exist"));

	FMovieSceneSequenceID ThisID = SequenceData.DeterministicSequenceID;

	if (const FMovieSceneSubSequenceData* ParentSubSequenceData = Template.Hierarchy.FindSubData(ParentID))
	{
#if WITH_EDITORONLY_DATA
		// Clamp this sequence's valid play range by its parent's valid play range
		TRange<float> ParentPlayRangeChildSpace = ParentSubSequenceData->ValidPlayRange * (SequenceData.RootToSequenceTransform * ParentSubSequenceData->RootToSequenceTransform.Inverse());
		SequenceData.ValidPlayRange = TRange<float>::Intersection(ParentPlayRangeChildSpace, SequenceData.ValidPlayRange);
#endif
		// Determine its ID from its parent's
		ThisID = SequenceData.DeterministicSequenceID.AccumulateParentID(ParentSubSequenceData->DeterministicSequenceID);
	}

	// Ensure we have a unique ID. This should never happen in reality.
	while(!ensureMsgf(!Template.Hierarchy.FindNode(ThisID), TEXT("CRC collision on deterministic hashes. Manually hashing a random new one.")))
	{
		ThisID = ThisID.AccumulateParentID(ThisID);
	}

	SequenceData.DeterministicSequenceID = ThisID;

	ParentNode->Children.Add(ThisID);
	Template.Hierarchy.Add(SequenceData, ThisID, ParentID);

	return ThisID;
}

void FMovieSceneEvaluationTemplateGenerator::FlushGroupImmediately(FName EvaluationGroup)
{
	ImmediateFlushGroups.Add(EvaluationGroup);
}

void FMovieSceneEvaluationTemplateGenerator::Generate(FMovieSceneTrackCompilationParams InParams)
{
	Template.Hierarchy = FMovieSceneSequenceHierarchy();

	// Generate templates for each track in the movie scene
	UMovieScene* MovieScene = SourceSequence.GetMovieScene();

	TransientArgs.Params = InParams;

	if (UMovieSceneTrack* Track = MovieScene->GetCameraCutTrack())
	{
		ProcessTrack(*Track);
	}

	for (UMovieSceneTrack* Track : MovieScene->GetMasterTracks())
	{
		ProcessTrack(*Track);
	}

	for (const FMovieSceneBinding& ObjectBinding : MovieScene->GetBindings())
	{
		for (UMovieSceneTrack* Track : ObjectBinding.GetTracks())
		{
			ProcessTrack(*Track, ObjectBinding.GetObjectGuid());
		}
	}

	// Remove references to old tracks
	RemoveOldTrackReferences();

	// Add all the tracks in *this* sequence (these exist after any sub section ptrs, not that its important for this algorithm)
	for (auto& Pair : Template.GetTracks())
	{
		TArrayView<const FMovieSceneSegment> Segments = Pair.Value.GetSegments();

		// Add the segment range data to the master collection for overall compilation
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			SegmentData.Add(FMovieSceneSectionData(Segments[SegmentIndex].Range, TrackLUT.Num()));
			TrackLUT.Add(FMovieSceneEvaluationFieldSegmentPtr(MovieSceneSequenceID::Root, Pair.Key, SegmentIndex));
		}
	}

	TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*> SequenceIdToTemplate;
	for (auto& Pair : Template.Hierarchy.AllSubSequenceData())
	{
		if (UMovieSceneSequence* Sequence = Pair.Value.Sequence)
		{
			SequenceIdToTemplate.Add(Pair.Key, &TransientArgs.SubSequenceStore.GetCompiledTemplate(*Sequence));
		}
	}

	// Compile the new evaluation field
	TArray<FMovieSceneSegment> NewSegments = FMovieSceneSegmentCompiler().Compile(SegmentData);
	UpdateEvaluationField(NewSegments, TrackLUT, SequenceIdToTemplate);
}

void FMovieSceneEvaluationTemplateGenerator::ProcessTrack(const UMovieSceneTrack& InTrack, const FGuid& ObjectId)
{
	FGuid Signature = InTrack.GetSignature();

	// See if this track signature already exists in the ledger, if it does, we don't need to regenerate it
	if (Template.FindTracks(Signature).Num())
	{
		CompiledSignatures.Add(Signature);
		return;
	}

	TransientArgs.ObjectBindingId = ObjectId;

	// Potentially expensive generation is required
	InTrack.GenerateTemplate(TransientArgs);
}

void FMovieSceneEvaluationTemplateGenerator::RemoveOldTrackReferences()
{
	TArray<FGuid> SignaturesToRemove;

	// Go through the template ledger, and remove anything that is no longer referenced
	for (auto& Pair : Template.GetLedger().TrackSignatureToTrackIdentifier)
	{
		if (!CompiledSignatures.Contains(Pair.Key))
		{
			SignaturesToRemove.Add(Pair.Key);
		}
	}

	// Remove the signatures, updating entries in the evaluation field as we go
	for (const FGuid& Signature : SignaturesToRemove)
	{
		Template.RemoveTrack(Signature);
	}
}

void FMovieSceneEvaluationTemplateGenerator::UpdateEvaluationField(const TArray<FMovieSceneSegment>& Segments, const TArray<FMovieSceneEvaluationFieldSegmentPtr>& Ptrs, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates)
{
	FMovieSceneEvaluationField& Field = Template.EvaluationField;

	Field = FMovieSceneEvaluationField();

	TArray<FMovieSceneEvaluationFieldSegmentPtr> AllTracksInSegment;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FMovieSceneSegment& Segment = Segments[Index];
		if (Segment.Impls.Num() == 0)
		{
			continue;
		}

		Field.Ranges.Add(Segment.Range);

		AllTracksInSegment.Reset();
		for (const FSectionEvaluationData& LUTData : Segment.Impls)
		{
			AllTracksInSegment.Add(Ptrs[LUTData.ImplIndex]);
		}

		// Sort the track ptrs, and define flush ranges
		AllTracksInSegment.Sort(
			[&](const FMovieSceneEvaluationFieldSegmentPtr& A, const FMovieSceneEvaluationFieldSegmentPtr& B){
				return SortPredicate(LookupTrack(A, Templates), LookupTrack(B, Templates));
			}
		);

		FMovieSceneEvaluationGroup& Group = Field.Groups[Field.Groups.Emplace()];

		TArray<FMovieSceneEvaluationFieldSegmentPtr> EvalPtrs;
		TArray<FMovieSceneEvaluationFieldSegmentPtr> InitPtrs;

		// Now iterate the tracks and insert indices for initialization and evaluation
		FName CurrentEvaluationGroup, LastEvaluationGroup;
		TOptional<uint32> LastPriority;

		for (const FMovieSceneEvaluationFieldSegmentPtr& Ptr : AllTracksInSegment)
		{
			const FMovieSceneEvaluationTrack* Track = LookupTrack(Ptr, Templates);
			if (!ensure(Track))
			{
				continue;
			}

			// If we're now in a different flush group, add the ptrs
			CurrentEvaluationGroup = Track->GetEvaluationGroup();
			if (CurrentEvaluationGroup != LastEvaluationGroup)
			{
				AddPtrsToGroup(Group, ImmediateFlushGroups.Contains(LastEvaluationGroup), InitPtrs, EvalPtrs);
			}
			LastEvaluationGroup = Track->GetEvaluationGroup();

			const bool bRequiresInitialization = Track->GetSegment(Ptr.SegmentIndex).Impls.ContainsByPredicate(
				[Track](FSectionEvaluationData EvalData)
				{
					return Track->GetChildTemplate(EvalData.ImplIndex).RequiresInitialization();
				}
			);

			if (bRequiresInitialization)
			{
				InitPtrs.Add(Ptr);
			}

			EvalPtrs.Add(Ptr);
		}

		AddPtrsToGroup(Group, ImmediateFlushGroups.Contains(LastEvaluationGroup), InitPtrs, EvalPtrs);

		// Copmpute meta data for this segment
		FMovieSceneEvaluationMetaData& MetaData = Field.MetaData[Field.MetaData.Emplace()];

		TArray<FMovieSceneSequenceID, TInlineAllocator<16>> ActiveSequenceIDs;
		for (const FMovieSceneEvaluationFieldSegmentPtr& SegmentPtr : Group.SegmentPtrLUT)
		{
			ActiveSequenceIDs.AddUnique(SegmentPtr.SequenceID);
		}
		MetaData.ActiveSequences = ActiveSequenceIDs;
		MetaData.ActiveSequences.Sort();
	}
}

const FMovieSceneEvaluationTrack* FMovieSceneEvaluationTemplateGenerator::LookupTrack(const FMovieSceneEvaluationFieldTrackPtr& InPtr, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates)
{
	if (InPtr.SequenceID == MovieSceneSequenceID::Root)
	{
		return Template.FindTrack(InPtr.TrackIdentifier);
	}
	else if (const FMovieSceneEvaluationTemplate* SubTemplate = Templates.FindRef(InPtr.SequenceID))
	{
		return SubTemplate->FindTrack(InPtr.TrackIdentifier);
	}
	ensure(false);
	return nullptr;
}

bool FMovieSceneEvaluationTemplateGenerator::SortPredicate(const FMovieSceneEvaluationTrack* A, const FMovieSceneEvaluationTrack* B) const
{
	if (!ensure(A && B))
	{
		return false;
	}

	if (A->GetEvaluationPriority() != B->GetEvaluationPriority())
	{
		return A->GetEvaluationPriority() > B->GetEvaluationPriority();
	}

	if (A->GetEvaluationGroup() != B->GetEvaluationGroup())
	{
		return A->GetEvaluationGroup() < B->GetEvaluationGroup();
	}

	return false;
}
