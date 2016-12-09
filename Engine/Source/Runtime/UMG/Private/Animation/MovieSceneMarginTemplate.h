// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Layout/Margin.h"
#include "MovieSceneMarginTemplate.generated.h"

class UMovieSceneMarginSection;
class UMovieScenePropertyTrack;

USTRUCT()
struct FMovieSceneMarginSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneMarginSectionTemplate(){}
	FMovieSceneMarginSectionTemplate(const UMovieSceneMarginSection& Section, const UMovieScenePropertyTrack& Track);

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
		PropertyData.SetupCachedTrack<FMargin>(PersistentData);
	}
	virtual void Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override
	{
		PropertyData.SetupCachedFrame<FMargin>(Operand, PersistentData, Player);
	}

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieScenePropertySectionData PropertyData;

	UPROPERTY()
	FRichCurve TopCurve;

	UPROPERTY()
	FRichCurve LeftCurve;

	UPROPERTY()
	FRichCurve RightCurve;

	UPROPERTY()
	FRichCurve BottomCurve;
};
