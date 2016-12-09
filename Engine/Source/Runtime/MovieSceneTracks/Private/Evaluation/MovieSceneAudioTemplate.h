// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneAudioTemplate.generated.h"

class UAudioComponent;
class UMovieSceneAudioSection;
class USoundBase;

USTRUCT()
struct FMovieSceneAudioSectionTemplateData
{
	GENERATED_BODY()

	FMovieSceneAudioSectionTemplateData() {}
	FMovieSceneAudioSectionTemplateData(const UMovieSceneAudioSection& Section);

	/** Ensure that the sound is playing for the specified audio component and data */
	void EnsureAudioIsPlaying(UAudioComponent& AudioComponent, FPersistentEvaluationData& PersistentData, const FMovieSceneContext& Context, bool bAllowSpatialization, IMovieScenePlayer& Player) const;

	/** The sound cue or wave that this template plays. Not to be dereferenced on a background thread */
	UPROPERTY()
	USoundBase* Sound;

	/** The absolute time that the sound starts playing at */
	UPROPERTY()
	float AudioStartTime;
	
	/** The amount which this audio is time dilated by */
	UPROPERTY()
	float AudioDilationFactor;

	/** The volume the sound will be played with. */
	UPROPERTY()
	float AudioVolume;

	/** The row index of the section */
	UPROPERTY()
	int32 RowIndex;
};

USTRUCT()
struct FMovieSceneAudioSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneAudioSectionTemplate() {}
	FMovieSceneAudioSectionTemplate(const UMovieSceneAudioSection& Section);

	UPROPERTY()
	FMovieSceneAudioSectionTemplateData AudioData;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
