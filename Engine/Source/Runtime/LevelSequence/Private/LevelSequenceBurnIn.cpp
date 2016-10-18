// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequenceBurnIn.h"

ULevelSequenceBurnIn::ULevelSequenceBurnIn( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}

void ULevelSequenceBurnIn::TakeSnapshotsFrom(ALevelSequenceActor& InActor)
{
	LevelSequenceActor = &InActor;
	if (ensure(InActor.SequencePlayer))
	{
		InActor.SequencePlayer->OnSequenceUpdated().AddUObject(this, &ULevelSequenceBurnIn::OnSequenceUpdated);
		InActor.SequencePlayer->TakeFrameSnapshot(FrameInformation);
	}
}

void ULevelSequenceBurnIn::OnSequenceUpdated(const ULevelSequencePlayer& Player, float CurrentTime, float PreviousTime)
{
	Player.TakeFrameSnapshot(FrameInformation);
}