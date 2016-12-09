// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationTrack.h"

FMovieSceneEvaluationTrack::FMovieSceneEvaluationTrack()
{
}

FMovieSceneEvaluationTrack::FMovieSceneEvaluationTrack(const FGuid& InObjectBindingID)
	: ObjectBindingID(InObjectBindingID)
	, EvaluationPriority(1000)
	, EvaluationMethod(EEvaluationMethod::Static)
	, bEvaluateInPreroll(true)
	, bEvaluateInPostroll(true)
{
}

void FMovieSceneEvaluationTrack::DefineAsSingleTemplate(FMovieSceneEvalTemplatePtr&& InTemplate)
{
	ChildTemplates.Reset(1);
	Segments.Reset(1);

	ChildTemplates.Add(MoveTemp(InTemplate));
	
	int32 SegmentIndex = 0;
	Segments.Add(FMovieSceneSegment(TRange<float>::All(), TArrayView<int32>(&SegmentIndex, 1)));
}

int32 FMovieSceneEvaluationTrack::AddChildTemplate(FMovieSceneEvalTemplatePtr&& InTemplate)
{
	return ChildTemplates.Add(MoveTemp(InTemplate));
}

void FMovieSceneEvaluationTrack::SetSegments(TArray<FMovieSceneSegment>&& InSegments)
{
	Segments = MoveTemp(InSegments);

	ValidateSegments();
}

void FMovieSceneEvaluationTrack::ValidateSegments()
{
	// We don't remove segments as this may break ptrs that have been set up in the evaluation field. Instead we just remove invalid track indices.

	for (FMovieSceneSegment& Segment : Segments)
	{
		for (int32 Index = Segment.Impls.Num() - 1; Index >= 0; --Index)
		{
			int32 TemplateIndex = Segment.Impls[Index].ImplIndex;
			if (!HasChildTemplate(TemplateIndex))
			{
				Segment.Impls.RemoveAt(Index, 1, false);
			}
		}
	}
}

void FMovieSceneEvaluationTrack::SetupOverrides()
{
	for (FMovieSceneEvalTemplatePtr& ChildTemplate : ChildTemplates)
	{
		if (ChildTemplate.IsValid())
		{
			ChildTemplate->SetupOverrides();
		}
	}

	if (TrackTemplate.IsValid())
	{
		TrackTemplate->SetupOverrides();
	}
}

void FMovieSceneEvaluationTrack::Initialize(int32 SegmentIndex, const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	if (!TrackTemplate.IsValid() || !TrackTemplate->HasCustomInitialize())
	{
		DefaultInitialize(SegmentIndex, Operand, Context, PersistentData, Player);
	}
	else
	{
		TrackTemplate->Initialize(*this, SegmentIndex, Operand, Context, PersistentData, Player);
	}
}

void FMovieSceneEvaluationTrack::DefaultInitialize(int32 SegmentIndex, const FMovieSceneEvaluationOperand& Operand, FMovieSceneContext Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	const FMovieSceneSegment& Segment = Segments[SegmentIndex];
	for (const FSectionEvaluationData& EvalData : Segment.Impls)
	{
		const FMovieSceneEvalTemplate& Template = GetChildTemplate(EvalData.ImplIndex);

		if (Template.RequiresInitialization())
		{
			PersistentData.DeriveSectionKey(EvalData.ImplIndex);
			
			Context.OverrideTime(EvalData.ForcedTime);

			Template.Initialize(
				Operand,
				Context,
				PersistentData,
				Player);
		}
	}
}

void FMovieSceneEvaluationTrack::Evaluate(int32 SegmentIndex, const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	ExecutionTokens.SetCompletionMode(EMovieSceneCompletionMode::KeepState);

	if (!TrackTemplate.IsValid() || !TrackTemplate->HasCustomEvaluate())
	{
		DefaultEvaluate(SegmentIndex, Operand, Context, PersistentData, ExecutionTokens);
	}
	else
	{
		TrackTemplate->Evaluate(*this, SegmentIndex, Operand, Context, PersistentData, ExecutionTokens);
	}
}

void FMovieSceneEvaluationTrack::DefaultEvaluate(int32 SegmentIndex, const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	switch (EvaluationMethod)
	{
	case EEvaluationMethod::Static:
		EvaluateStatic(SegmentIndex, Operand, Context, PersistentData, ExecutionTokens);
		break;
	case EEvaluationMethod::Swept:
		EvaluateSwept(SegmentIndex, Operand, Context, PersistentData, ExecutionTokens);
		break;
	}
}

void FMovieSceneEvaluationTrack::EvaluateStatic(int32 SegmentIndex, const FMovieSceneEvaluationOperand& Operand, FMovieSceneContext Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	for (const FSectionEvaluationData& EvalData : GetSegment(SegmentIndex).Impls)
	{
		const FMovieSceneEvalTemplate& Template = GetChildTemplate(EvalData.ImplIndex);
		
		Context.OverrideTime(EvalData.ForcedTime);

		PersistentData.DeriveSectionKey(EvalData.ImplIndex);
		ExecutionTokens.SetSectionIdentifier(EvalData.ImplIndex);
		ExecutionTokens.SetCompletionMode(Template.GetCompletionMode());
		ExecutionTokens.SetContext(Context);

		Template.Evaluate(
			Operand,
			Context,
			PersistentData,
			ExecutionTokens);
	}
}

namespace
{
	bool IntersectSegmentRanges(const FMovieSceneSegment& Segment, TRange<float> TraversedRange, TMap<int32, TRange<float>>& ImplToAccumulatedRange)
	{
		TRange<float> Intersection = TRange<float>::Intersection(Segment.Range, TraversedRange);
		if (Intersection.IsEmpty())
		{
			return false;
		}

		for (const FSectionEvaluationData& EvalData : Segment.Impls)
		{
			TRange<float>* AccumulatedRange = ImplToAccumulatedRange.Find(EvalData.ImplIndex);
			if (!AccumulatedRange)
			{
				ImplToAccumulatedRange.Add(EvalData.ImplIndex, Intersection);
			}
			else
			{
				*AccumulatedRange = TRange<float>::Hull(*AccumulatedRange, Intersection);
			}
		}

		return true;
	}
}

void FMovieSceneEvaluationTrack::EvaluateSwept(int32 SegmentIndex, const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	TRange<float> TraversedRange = Context.GetRange();

	// Accumulate the relevant ranges that each section intersects with the evaluated range
	TMap<int32, TRange<float>> ImplToAccumulatedRange;

	// Search backwards from the current segment for any segments intersecting the traversed range
	{
		int32 PreviousIndex = SegmentIndex;
		while(--PreviousIndex >= 0 && IntersectSegmentRanges(Segments[PreviousIndex], TraversedRange, ImplToAccumulatedRange));
	}

	// Obviously the current segment intersects otherwise we wouldn't be in here
	IntersectSegmentRanges(Segments[SegmentIndex], TraversedRange, ImplToAccumulatedRange);

	// Search forwards from the current segment for any segments intersecting the traversed range
	{
		int32 NextIndex = SegmentIndex;
		while(++NextIndex < Segments.Num() && IntersectSegmentRanges(Segments[NextIndex], TraversedRange, ImplToAccumulatedRange));
	}

	for (auto& Pair : ImplToAccumulatedRange)
	{
		const int32 SectionIndex = Pair.Key;
		TRange<float> EvaluationRange = Pair.Value;
		const FMovieSceneEvalTemplate& Template = GetChildTemplate(SectionIndex);

		PersistentData.DeriveSectionKey(SectionIndex);
		ExecutionTokens.SetSectionIdentifier(SectionIndex);
		ExecutionTokens.SetCompletionMode(Template.GetCompletionMode());
		ExecutionTokens.SetContext(Context);

		Template.EvaluateSwept(
			Operand,
			Context.Clamp(EvaluationRange),
			PersistentData,
			ExecutionTokens);
	}
}
