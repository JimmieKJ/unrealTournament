// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieScene3DAttachTemplate.generated.h"

class UMovieScene3DAttachSection;

USTRUCT()
struct FMovieScene3DAttachSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieScene3DAttachSectionTemplate() {}
	FMovieScene3DAttachSectionTemplate(const UMovieScene3DAttachSection& Section);

	/** GUID of the parent we should attach to */
	UPROPERTY()
	FGuid AttachGuid;

	/** Name of the socket to attach to */
	UPROPERTY()
	FName AttachSocketName;

	/** Name of the component to attach to */
	UPROPERTY()
	FName AttachComponentName;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
