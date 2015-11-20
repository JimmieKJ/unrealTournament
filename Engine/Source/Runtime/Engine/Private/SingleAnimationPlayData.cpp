// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SingleAnimationPlayData.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/VertexAnim/VertexAnimation.h"


void FSingleAnimationPlayData::Initialize(UAnimSingleNodeInstance* Instance)
{
	Instance->SetAnimationAsset(AnimToPlay);
	Instance->SetPosition(SavedPosition, false);
	Instance->SetPlayRate(SavedPlayRate);
	Instance->SetPlaying(bSavedPlaying);
	Instance->SetLooping(bSavedLooping);
}

void FSingleAnimationPlayData::PopulateFrom(UAnimSingleNodeInstance* Instance)
{
	AnimToPlay = Instance->CurrentAsset;
	SavedPosition = Instance->CurrentTime;
	SavedPlayRate = Instance->PlayRate;
	bSavedPlaying = Instance->bPlaying;
	bSavedLooping = Instance->bLooping;
}

void FSingleAnimationPlayData::ValidatePosition()
{
	float Min = 0, Max = 0;

	if (AnimToPlay)
	{
		UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimToPlay);
		if (SequenceBase)
		{
			Max = SequenceBase->SequenceLength;
		}
	}
	else if (VertexAnimToPlay)
	{
		Max = VertexAnimToPlay->GetAnimLength();
	}

	SavedPosition = FMath::Clamp<float>(SavedPosition, Min, Max);
}