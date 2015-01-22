// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "AudioDecompress.h"
#include "TargetPlatform.h"
#include "AudioDerivedData.h"
#include "SubtitleManager.h"
#include "Sound/DialogueVoice.h"

UDialogueVoice::UDialogueVoice(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LocalizationGUID( FGuid::NewGuid() )
{
}

// Begin UObject interface. 
bool UDialogueVoice::IsReadyForFinishDestroy()
{
	return true;
}

FName UDialogueVoice::GetExporterName()
{
	return NAME_None;
}

FString UDialogueVoice::GetDesc()
{
	FString SummaryString;
	{
		UByteProperty* GenderProperty = CastChecked<UByteProperty>(FindFieldChecked<UProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UDialogueVoice, Gender)));
		SummaryString += GenderProperty->Enum->GetEnumText(Gender).ToString();

		if( Plurality != EGrammaticalNumber::Singular )
		{
			UByteProperty* PluralityProperty = CastChecked<UByteProperty>( FindFieldChecked<UProperty>( GetClass(), GET_MEMBER_NAME_CHECKED(UDialogueVoice, Plurality)) );

			SummaryString += ", ";
			SummaryString += PluralityProperty->Enum->GetEnumText(Plurality).ToString();
		}
	}

	return FString::Printf( TEXT( "%s (%s)" ), *( GetName() ), *(SummaryString) );
}

void UDialogueVoice::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

void UDialogueVoice::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if( !bDuplicateForPIE )
	{
		LocalizationGUID = FGuid::NewGuid();
	}
}
// End UObject interface. 