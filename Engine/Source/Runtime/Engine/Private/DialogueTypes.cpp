// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/DialogueVoice.h"
bool operator==(const FDialogueContext& LHS, const FDialogueContext& RHS)
{
	return	LHS.Speaker == RHS.Speaker &&
		LHS.Targets == RHS.Targets;
}

bool operator!=(const FDialogueContext& LHS, const FDialogueContext& RHS)
{
	return	LHS.Speaker != RHS.Speaker ||
		LHS.Targets != RHS.Targets;
}

FDialogueContext::FDialogueContext() : Speaker(NULL)
{
	Targets.AddZeroed();
}

FString FDialogueContext::GetLocalizationKey() const
{
	FString Key;
	if( Speaker )
	{
		Key = Speaker->LocalizationGUID.ToString();
	}

	TArray<FString> UniqueSortedTargets;
	for(const UDialogueVoice* Target : Targets)
	{
		if( Target )
		{
			UniqueSortedTargets.AddUnique( Target->LocalizationGUID.ToString() );
		}
	}
	UniqueSortedTargets.Sort( TLess<FString>() );
	for(const FString& TargetKey : UniqueSortedTargets)
	{
		Key = Key + TEXT(" ") + TargetKey;
	}
	return Key;
}

FDialogueWaveParameter::FDialogueWaveParameter() : DialogueWave(NULL)
{

}

UDialogueTypes::UDialogueTypes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}