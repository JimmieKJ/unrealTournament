// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneMarginTemplate.h"

#include "Animation/MovieSceneMarginSection.h"
#include "Tracks/MovieScenePropertyTrack.h"

FMovieSceneMarginSectionTemplate::FMovieSceneMarginSectionTemplate(const UMovieSceneMarginSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, TopCurve(Section.GetTopCurve())
	, LeftCurve(Section.GetLeftCurve())
	, RightCurve(Section.GetRightCurve())
	, BottomCurve(Section.GetBottomCurve())
{
}

void FMovieSceneMarginSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	TCachedSectionData<FMargin>& SectionData = PersistentData.GetSectionData<TCachedSectionData<FMargin>>();

	float Time = Context.GetTime();
	for (TCachedValue<FMargin>& ObjectAndValue : SectionData.ObjectsAndValues)
	{
		FMargin& Margin = ObjectAndValue.Value;

		Margin.Top = TopCurve.Eval(Time, Margin.Top);
		Margin.Left = LeftCurve.Eval(Time, Margin.Left);
		Margin.Right = RightCurve.Eval(Time, Margin.Right);
		Margin.Bottom = BottomCurve.Eval(Time, Margin.Bottom);
	}

	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<FMargin>());
}
