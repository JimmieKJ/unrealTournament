// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Slate/WidgetTransform.h"
#include "MovieScene2DTransformTemplate.generated.h"

class UMovieScene2DTransformSection;
class UMovieScenePropertyTrack;

USTRUCT()
struct FMovieScene2DTransformSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieScene2DTransformSectionTemplate(){}
	FMovieScene2DTransformSectionTemplate(const UMovieScene2DTransformSection& Section, const UMovieScenePropertyTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override
	{
		return *StaticStruct();
	}
	virtual void SetupOverrides() override
	{
		EnableOverrides(RequiresSetupFlag | RequiresInitializeFlag);
	}
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override
	{
		PropertyData.SetupCachedTrack<FWidgetTransform>(PersistentData);
	}
	virtual void Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override
	{
		PropertyData.SetupCachedFrame<FWidgetTransform>(Operand, PersistentData, Player);
	}

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieScenePropertySectionData PropertyData;

	/** Translation curves */
	UPROPERTY()
	FRichCurve Translation[2];
	
	/** Rotation curve */
	UPROPERTY()
	FRichCurve Rotation;

	/** Scale curves */
	UPROPERTY()
	FRichCurve Scale[2];

	/** Shear curve */
	UPROPERTY()
	FRichCurve Shear[2];
};
