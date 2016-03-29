// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneSpawnable.h"
#include "IMovieSceneSpawnRegister.h"

/** Spawn register used by sequencer to override default behaviour of other implementations */
template<typename MixinType>
struct TSequencerSpawnRegister : public MixinType
{
	/** Implement CanDestroyExpiredObject to take into account FMovieSceneSpawnable::ShouldIgnoreOwnershipInEditor */
	virtual bool CanDestroyExpiredObject(const FGuid& ObjectId, ESpawnOwnership Ownership, FMovieSceneSequenceInstance& Instance) override
	{
		if (!MixinType::CanDestroyExpiredObject(ObjectId, Ownership, Instance))
		{
			return false;
		}
		
		// Find the spawnable
		UMovieSceneSequence* Sequence = Instance.GetSequence();
		UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;

		if (!MovieScene)
		{
			return true;
		}

		const FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectId);
		return !Spawnable || !Spawnable->ShouldIgnoreOwnershipInEditor();
	}
};
