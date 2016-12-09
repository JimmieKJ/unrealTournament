// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTrack.h"
#include "Evaluation/MovieSceneSegment.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"

#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Evaluation/MovieSceneLegacyTrackInstanceTemplate.h"

UMovieSceneTrack::UMovieSceneTrack(const FObjectInitializer& InInitializer)
	: Super(InInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(127, 127, 127, 0);
#endif
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedPtr<IMovieSceneTrackInstance> UMovieSceneTrack::CreateLegacyInstance() const
{
	// Ugly const cast required due to lack of const-correctness in old system
	return const_cast<UMovieSceneTrack*>(this)->CreateInstance();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneTrack::GetRowCompilerRules() const
{
	// By default we only evaluate the section with the highest Z-Order if they overlap on the same row
	struct FDefaultCompilerRules : FMovieSceneSegmentCompilerRules
	{
		virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
		{
			MovieSceneSegmentCompiler::BlendSegmentHighPass(Segment, SourceData);
		}
	};
	return FDefaultCompilerRules();
}

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneTrack::GetTrackCompilerRules() const
{
	struct FRoundToNearstSectionRules : FMovieSceneSegmentCompilerRules
	{
		virtual TOptional<FMovieSceneSegment> InsertEmptySpace(const TRange<float>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment) const
		{
			return MovieSceneSegmentCompiler::EvaluateNearestSegment(Range, PreviousSegment, NextSegment);
		}
	};

	// Evaluate according to bEvaluateNearestSection preference
	if (EvalOptions.bCanEvaluateNearestSection && EvalOptions.bEvaluateNearestSection)
	{
		return FRoundToNearstSectionRules();
	}
	else
	{
		return FMovieSceneSegmentCompilerRules();
	}
}

void UMovieSceneTrack::GenerateTemplate(const FMovieSceneTrackCompilerArgs& Args) const
{
	FMovieSceneEvaluationTrack NewTrackTemplate(Args.ObjectBindingId);

	// Legacy path
	if (CreateLegacyInstance().IsValid())
	{
		NewTrackTemplate.DefineAsSingleTemplate(FMovieSceneLegacyTrackInstanceTemplate(this));
		Args.Generator.AddLegacyTrack(MoveTemp(NewTrackTemplate), *this);
		return;
	}

	if (Compile(NewTrackTemplate, Args) != EMovieSceneCompileResult::Success)
	{
		return;
	}

	Args.Generator.AddOwnedTrack(MoveTemp(NewTrackTemplate), *this);
}

struct FSegmentRemapper
{
	void ProcessSegments(const TArray<FMovieSceneSegment>& SourceSegments, FMovieSceneEvaluationTrack& OutTrack, const TFunctionRef<FMovieSceneEvalTemplatePtr(int32)>& TemplateFactory)
	{
		NewSegments.Reset(SourceSegments.Num());
		NewIndices.Reset();

		for (const FMovieSceneSegment& Segment : SourceSegments)
		{
			AddSegment(Segment, OutTrack, TemplateFactory);
		}

		OutTrack.SetSegments(MoveTemp(NewSegments));
	}

	void AddSegment(const FMovieSceneSegment& SourceSegment, FMovieSceneEvaluationTrack& OutTrack, const TFunctionRef<FMovieSceneEvalTemplatePtr(int32)>& TemplateFactory)
	{
		FMovieSceneSegment NewSegment;
		NewSegment.Range = SourceSegment.Range;

		for (const FSectionEvaluationData& EvalData : SourceSegment.Impls)
		{
			EnsureIndexIsValid(EvalData.ImplIndex);

			// Ensure all our templates have been added to the track
			if (NewIndices[EvalData.ImplIndex] == NotCreatedYet)
			{
				FMovieSceneEvalTemplatePtr NewTemplate = TemplateFactory(EvalData.ImplIndex);
				if (NewTemplate.IsValid())
				{
					NewIndices[EvalData.ImplIndex] = OutTrack.AddChildTemplate(MoveTemp(NewTemplate));
				}
				else
				{
					NewIndices[EvalData.ImplIndex] = NullTemplate;
				}
			}

			if (NewIndices[EvalData.ImplIndex] == NullTemplate)
			{
				continue;
			}

			FSectionEvaluationData NewData = EvalData;
			NewData.ImplIndex = NewIndices[EvalData.ImplIndex];
			NewSegment.Impls.Add(NewData);
		}

		if (NewSegment.Impls.Num())
		{
			NewSegments.Add(NewSegment);
		}
	}

private:

	static const int32 NotCreatedYet;
	static const int32 NullTemplate;

	TArray<FMovieSceneSegment> NewSegments;
	TArray<int32> NewIndices;

	void EnsureIndexIsValid(int32 InSourceIndex)
	{
		NewIndices.Reserve(InSourceIndex + 1);
		for (int32 Index = NewIndices.Num(); Index < InSourceIndex + 1; ++Index)
		{
			NewIndices.Add(NotCreatedYet);
		}
	}
};

const int32 FSegmentRemapper::NotCreatedYet = -1;
const int32 FSegmentRemapper::NullTemplate = -2;

EMovieSceneCompileResult UMovieSceneTrack::Compile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const
{
	OutTrack.SetPreAndPostrollConditions(EvalOptions.bEvaluateInPreroll, EvalOptions.bEvaluateInPostroll);

	EMovieSceneCompileResult Result = CustomCompile(OutTrack, Args);

	if (Result == EMovieSceneCompileResult::Unimplemented)
	{
		// Default implementation
		const TArray<UMovieSceneSection*>& AllSections = GetAllSections();
		
		TInlineValue<FMovieSceneSegmentCompilerRules> RowCompilerRules = GetRowCompilerRules();
		FMovieSceneTrackCompiler::FRows TrackRows(AllSections, RowCompilerRules.GetPtr(nullptr));

		FMovieSceneTrackCompiler Compiler;
		FMovieSceneTrackEvaluationField EvaluationField = Compiler.Compile(TrackRows.Rows, GetTrackCompilerRules().GetPtr(nullptr));

		FSegmentRemapper Remapper;
		Remapper.ProcessSegments(EvaluationField.Segments, OutTrack,
			[&](int32 SectionIndex){
				FMovieSceneEvalTemplatePtr NewTemplate = CreateTemplateForSection(*AllSections[SectionIndex]);
				if (NewTemplate.IsValid())
				{
					NewTemplate->SetCompletionMode(AllSections[SectionIndex]->EvalOptions.CompletionMode);
				}
				return NewTemplate;
			}
		);

		Result = EMovieSceneCompileResult::Success;
	}

	if (Result == EMovieSceneCompileResult::Success)
	{
		PostCompile(OutTrack, Args);
	}

	return Result;
}

FMovieSceneEvalTemplatePtr UMovieSceneTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return InSection.GenerateTemplate();
}
