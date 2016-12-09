// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScene3DTransformTemplate.h"

#include "Sections/MovieScene3DTransformSection.h"
#include "Evaluation/MovieSceneTemplateCommon.h"
#include "MovieSceneCommonHelpers.h"

DECLARE_CYCLE_STAT(TEXT("Transform Track Evaluate"), MovieSceneEval_TransformTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Transform Track Token Execute"), MovieSceneEval_TransformTrack_TokenExecute, STATGROUP_MovieSceneEval);

/** A movie scene execution token that stores a specific transform, and an operand */
struct F3DTransformTrackExecutionToken
	: IMovieSceneExecutionToken
	, F3DTransformTrackToken
{
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_TransformTrack_TokenExecute)

		for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = Object.Get();
			USceneComponent* SceneComponent = ObjectPtr ? MovieSceneHelpers::SceneComponentFromRuntimeObject(ObjectPtr) : nullptr;

			if (SceneComponent)
			{
				Player.SavePreAnimatedState(*SceneComponent, FMobilityTokenProducer::GetAnimTypeID(), FMobilityTokenProducer());
				Player.SavePreAnimatedState(*SceneComponent, F3DTransformTokenProducer::GetAnimTypeID(), F3DTransformTokenProducer());
	
				SceneComponent->SetMobility(EComponentMobility::Movable);

				Apply(*SceneComponent, Context.GetDelta());
			}
		}
	}
};

FMovieScene3DTransformSectionTemplate::FMovieScene3DTransformSectionTemplate(const UMovieScene3DTransformSection& Section)
{
	TranslationCurve[0]	= Section.GetTranslationCurve(EAxis::X);
	TranslationCurve[1]	= Section.GetTranslationCurve(EAxis::Y);
	TranslationCurve[2]	= Section.GetTranslationCurve(EAxis::Z);

	RotationCurve[0]	= Section.GetRotationCurve(EAxis::X);
	RotationCurve[1]	= Section.GetRotationCurve(EAxis::Y);
	RotationCurve[2]	= Section.GetRotationCurve(EAxis::Z);

	ScaleCurve[0]		= Section.GetScaleCurve(EAxis::X);
	ScaleCurve[1]		= Section.GetScaleCurve(EAxis::Y);
	ScaleCurve[2]		= Section.GetScaleCurve(EAxis::Z);
}

void FMovieScene3DTransformSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_TransformTrack_Evaluate)

	F3DTransformTrackExecutionToken Token;

	const float Time = Context.GetTime();

	Token.Translation.X = 	TranslationCurve[0].Eval(Time);
	Token.Translation.Y = 	TranslationCurve[1].Eval(Time);
	Token.Translation.Z = 	TranslationCurve[2].Eval(Time);

	Token.Rotation.Roll = 	RotationCurve[0].Eval(Time);
	Token.Rotation.Pitch = 	RotationCurve[1].Eval(Time);
	Token.Rotation.Yaw = 	RotationCurve[2].Eval(Time);

	Token.Scale.X = 		ScaleCurve[0].Eval(Time);
	Token.Scale.Y = 		ScaleCurve[1].Eval(Time);
	Token.Scale.Z = 		ScaleCurve[2].Eval(Time);

	ExecutionTokens.Add(Token);
}
