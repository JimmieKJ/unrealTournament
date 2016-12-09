// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneFadeTemplate.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "MovieSceneSequence.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "GameFramework/PlayerController.h"

DECLARE_CYCLE_STAT(TEXT("Fade Track Token Execute"), MovieSceneEval_FadeTrack_TokenExecute, STATGROUP_MovieSceneEval);

struct FFadeTrackToken
{
	FFadeTrackToken(float InFadeValue, const FLinearColor& InFadeColor, bool bInFadeAudio)
		: FadeValue(InFadeValue)
		, FadeColor(InFadeColor)
		, bFadeAudio(bInFadeAudio)
	{}

	float FadeValue;
	FLinearColor FadeColor;
	bool bFadeAudio;

	void Apply(IMovieScenePlayer& Player)
	{
		// Set editor preview/fade
		EMovieSceneViewportParams ViewportParams;
		ViewportParams.SetWhichViewportParam = (EMovieSceneViewportParams::SetViewportParam)(EMovieSceneViewportParams::SVP_FadeAmount | EMovieSceneViewportParams::SVP_FadeColor);
		ViewportParams.FadeAmount = FadeValue;
		ViewportParams.FadeColor = FadeColor;

		TMap<FViewportClient*, EMovieSceneViewportParams> ViewportParamsMap;
		Player.GetViewportSettings(ViewportParamsMap);
		for (auto ViewportParamsPair : ViewportParamsMap)
		{
			ViewportParamsMap[ViewportParamsPair.Key] = ViewportParams;
		}
		Player.SetViewportSettings(ViewportParamsMap);

		// Set runtime fade
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				UWorld* World = Context.World();
				if (World != nullptr)
				{
					APlayerController* PlayerController = World->GetGameInstance()->GetFirstLocalPlayerController();
					if (PlayerController != nullptr && PlayerController->PlayerCameraManager && !PlayerController->PlayerCameraManager->IsPendingKill())
					{
						PlayerController->PlayerCameraManager->SetManualCameraFade(FadeValue, FadeColor, bFadeAudio);
					}
				}
			}
		}
	}
};

struct FFadePreAnimatedGlobalToken : FFadeTrackToken, IMovieScenePreAnimatedGlobalToken
{
	FFadePreAnimatedGlobalToken(float InFadeValue, const FLinearColor& InFadeColor, bool bInFadeAudio)
		: FFadeTrackToken(InFadeValue, InFadeColor, bInFadeAudio)
	{}

	virtual void RestoreState(IMovieScenePlayer& Player) override
	{
		Apply(Player);
	}
};

struct FFadePreAnimatedGlobalTokenProducer : IMovieScenePreAnimatedGlobalTokenProducer
{
	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
	{
		return FFadePreAnimatedGlobalToken(0.f, FLinearColor::Black, false);
	}
};

/** A movie scene execution token that applies fades */
struct FFadeExecutionToken : IMovieSceneExecutionToken, FFadeTrackToken
{
	FFadeExecutionToken(float InFadeValue, const FLinearColor& InFadeColor, bool bInFadeAudio)
		: FFadeTrackToken(InFadeValue, InFadeColor, bInFadeAudio)
	{}

	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FFadeExecutionToken>();
	}
	
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_FadeTrack_TokenExecute)

		Player.SavePreAnimatedState(GetAnimTypeID(), FFadePreAnimatedGlobalTokenProducer());

		Apply(Player);
	}
};

FMovieSceneFadeSectionTemplate::FMovieSceneFadeSectionTemplate(const UMovieSceneFadeSection& Section)
	: FadeCurve(Section.GetFloatCurve())
	, FadeColor(Section.FadeColor)
	, bFadeAudio(Section.bFadeAudio)
{
}

void FMovieSceneFadeSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	float FadeValue = FadeCurve.Eval(Context.GetTime());
	ExecutionTokens.Add(FFadeExecutionToken(FadeValue, FadeColor, bFadeAudio));
}
