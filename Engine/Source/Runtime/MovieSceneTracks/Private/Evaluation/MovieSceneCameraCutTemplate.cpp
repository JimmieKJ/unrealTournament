// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneCameraCutTemplate.h"
#include "Sections/MovieSceneCameraCutSection.h"

DECLARE_CYCLE_STAT(TEXT("Camera Cut Track Token Execute"), MovieSceneEval_CameraCutTrack_TokenExecute, STATGROUP_MovieSceneEval);

struct FCameraCutTrackData : IPersistentEvaluationData
{
	TWeakObjectPtr<> LastLockedCamera;
};

/** A movie scene pre-animated token that stores a pre-animated camera cut */
struct FCameraCutPreAnimatedToken : IMovieScenePreAnimatedGlobalToken
{
	virtual void RestoreState(IMovieScenePlayer& Player) override
	{
		Player.UpdateCameraCut(nullptr, nullptr);
	}
};

/** A movie scene execution token that applies camera cuts */
struct FCameraCutExecutionToken : IMovieSceneExecutionToken
{
	FGuid CameraGuid;

	FCameraCutExecutionToken(const FGuid& InCameraGuid)
		: CameraGuid(InCameraGuid)
	{}

	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FCameraCutExecutionToken>();
	}
	
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_CameraCutTrack_TokenExecute)

		FMovieSceneEvaluationOperand CameraOperand(Operand.SequenceID, CameraGuid);
		
		TArrayView<TWeakObjectPtr<>> Objects = Player.FindBoundObjects(CameraOperand);
		if (!Objects.Num())
		{
			return;
		}

		// Only ever deal with one camera
		UObject* CameraObject = Objects[0].Get();

		struct FProducer : IMovieScenePreAnimatedGlobalTokenProducer
		{
			virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
			{
				return FCameraCutPreAnimatedToken();
			}
		} Producer;

		Player.SavePreAnimatedState(GetAnimTypeID(), Producer);

		FCameraCutTrackData& CameraCutCache = PersistentData.GetOrAddTrackData<FCameraCutTrackData>();
		if (CameraCutCache.LastLockedCamera.Get() != CameraObject)
		{
			Player.UpdateCameraCut(CameraObject, CameraCutCache.LastLockedCamera.Get(), Context.HasJumped());
			CameraCutCache.LastLockedCamera = CameraObject;
		}
		else if (CameraObject)
		{
			Player.UpdateCameraCut(CameraObject, nullptr, Context.HasJumped());
		}
	}
};


FMovieSceneCameraCutSectionTemplate::FMovieSceneCameraCutSectionTemplate(const UMovieSceneCameraCutSection& Section)
	: CameraGuid(Section.GetCameraGuid())
{
}

void FMovieSceneCameraCutSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	ExecutionTokens.Add(FCameraCutExecutionToken(CameraGuid));
}
