// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieScene2DTransformTemplate.h"

#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"

FMovieScene2DTransformSectionTemplate::FMovieScene2DTransformSectionTemplate(const UMovieScene2DTransformSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
{
	Translation[0] = Section.GetTranslationCurve(EAxis::X);
	Translation[1] = Section.GetTranslationCurve(EAxis::Y);

	Rotation = Section.GetRotationCurve();

	Scale[0] = Section.GetScaleCurve(EAxis::X);
	Scale[1] = Section.GetScaleCurve(EAxis::Y);

	Shear[0] = Section.GetShearCurve(EAxis::X);
	Shear[1] = Section.GetShearCurve(EAxis::Y);
}

void FMovieScene2DTransformSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	TCachedSectionData<FWidgetTransform>& SectionData = PersistentData.GetSectionData<TCachedSectionData<FWidgetTransform>>();

	float Time = Context.GetTime();
	for (TCachedValue<FWidgetTransform>& ObjectAndValue : SectionData.ObjectsAndValues)
	{
		FWidgetTransform& Value = ObjectAndValue.Value;

		Value.Translation.X 	= Translation[0].Eval(Time, Value.Translation.X);
		Value.Translation.Y 	= Translation[1].Eval(Time, Value.Translation.Y);

		Value.Scale.X 			= Scale[0].Eval(Time, Value.Scale.X);
		Value.Scale.Y 			= Scale[1].Eval(Time, Value.Scale.Y);

		Value.Shear.X 			= Shear[0].Eval(Time, Value.Shear.X);
		Value.Shear.Y 			= Shear[1].Eval(Time, Value.Shear.Y);

		Value.Angle 			= Rotation.Eval(Time, Value.Angle);
	}

	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<FWidgetTransform>());
}
