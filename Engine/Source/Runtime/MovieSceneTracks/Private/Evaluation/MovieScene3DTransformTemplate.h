// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieScene3DTransformTemplate.generated.h"

class UMovieScene3DTransformSection;

USTRUCT()
struct FMovieScene3DTransformSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieScene3DTransformSectionTemplate() {}
	FMovieScene3DTransformSectionTemplate(const UMovieScene3DTransformSection& Section);

	UPROPERTY()
	FRichCurve TranslationCurve[3];

	UPROPERTY()
	FRichCurve RotationCurve[3];

	UPROPERTY()
	FRichCurve ScaleCurve[3];

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
