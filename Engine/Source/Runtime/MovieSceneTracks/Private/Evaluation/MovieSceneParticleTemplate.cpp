// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneParticleTemplate.h"
#include "Sections/MovieSceneParticleSection.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystemComponent.h"

DECLARE_CYCLE_STAT(TEXT("Particle Track Token Execute"), MovieSceneEval_ParticleTrack_TokenExecute, STATGROUP_MovieSceneEval);

struct FParticleKeyState : IPersistentEvaluationData
{
	FKeyHandle LastKeyHandle;
	FKeyHandle InvalidKeyHandle;
};

/** A movie scene execution token that stores a specific transform, and an operand */
struct FParticleTrackExecutionToken
	: IMovieSceneExecutionToken
{
	FParticleTrackExecutionToken(EParticleKey::Type InParticleKey, TOptional<FKeyHandle> InKeyHandle = TOptional<FKeyHandle>())
		: ParticleKey(InParticleKey), KeyHandle(InKeyHandle)
	{
	}

	static UParticleSystemComponent* GetComponentFromObject(UObject* Object)
	{
		if(AEmitter* Emitter = Cast<AEmitter>(Object))
		{
			return Emitter->GetParticleSystemComponent();
		}
		else
		{
			return Cast<UParticleSystemComponent>(Object);
		}
	}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ParticleTrack_TokenExecute)
		
		if (KeyHandle.IsSet())
		{
			PersistentData.GetOrAddSectionData<FParticleKeyState>().LastKeyHandle = KeyHandle.GetValue();
		}

		for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = Object.Get();
			UParticleSystemComponent* ParticleSystemComponent = GetComponentFromObject(ObjectPtr);

			if (ParticleSystemComponent)
			{
				if ( ParticleKey == EParticleKey::Activate)
				{
					if ( ParticleSystemComponent->IsActive() )
					{
						ParticleSystemComponent->SetActive(false, true);
					}
					ParticleSystemComponent->SetActive(true, true);
				}
				else if( ParticleKey == EParticleKey::Deactivate )
				{
					ParticleSystemComponent->SetActive(false, true);
				}
				else if ( ParticleKey == EParticleKey::Trigger )
				{
					ParticleSystemComponent->ActivateSystem(true);
				}
			}
		}
	}

	EParticleKey::Type ParticleKey;
	TOptional<FKeyHandle> KeyHandle;
};

FMovieSceneParticleSectionTemplate::FMovieSceneParticleSectionTemplate(const UMovieSceneParticleSection& Section)
	: ParticleKeys(Section.GetParticleCurve())
{
}

void FMovieSceneParticleSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const bool bPlaying = Context.GetDirection() == EPlayDirection::Forwards && Context.GetRange().Size<float>() >= 0.f && Context.GetStatus() == EMovieScenePlayerStatus::Playing;

	const FParticleKeyState* SectionData = PersistentData.FindSectionData<FParticleKeyState>();

	if (!bPlaying)
	{
		ExecutionTokens.Add(FParticleTrackExecutionToken(EParticleKey::Deactivate, SectionData ? SectionData->InvalidKeyHandle : TOptional<FKeyHandle>()));
	}
	else
	{
		FKeyHandle PreviousHandle = ParticleKeys.FindKeyBeforeOrAt(Context.GetTime());
		if (ParticleKeys.IsKeyHandleValid(PreviousHandle) && (!SectionData || SectionData->LastKeyHandle != PreviousHandle))
		{
			FParticleTrackExecutionToken NewToken(
				(EParticleKey::Type)ParticleKeys.GetKey(PreviousHandle).Value,
				PreviousHandle);

			ExecutionTokens.Add(MoveTemp(NewToken));
		}
	}
}
