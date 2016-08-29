// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SingleAnimationPlayData.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequenceBase.h"


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
	AnimToPlay = Instance->GetCurrentAsset();
	SavedPosition = Instance->GetCurrentTime();
	SavedPlayRate = Instance->GetPlayRate();
	bSavedPlaying = Instance->IsPlaying();
	bSavedLooping = Instance->IsLooping();
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

	SavedPosition = FMath::Clamp<float>(SavedPosition, Min, Max);
}