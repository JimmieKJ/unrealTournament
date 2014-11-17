// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTCountDownMessage.h"
#include "GameFramework/LocalMessage.h"


UUTCountDownMessage::UUTCountDownMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsUnique = true;
	bOptionalSpoken = true;
	Lifetime = 0.95;
	MessageArea = FName(TEXT("GameMessages"));

	bIsStatusAnnouncement = true;

	CountDownText = NSLOCTEXT("UTTimerMessage","MatBeginCountdown","Match begins in {Count}...");
}

void UUTCountDownMessage::GetArgs(FFormatNamedArguments& Args, int32 Switch, bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	Super::GetArgs(Args, Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	Args.Add(TEXT("Count"), FText::AsNumber(Switch));
}

FText UUTCountDownMessage::GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const
{
	return GetDefault<UUTCountDownMessage>(GetClass())->CountDownText;
}
