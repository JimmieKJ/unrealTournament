// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTLastSecondMessage.h"
#include "GameFramework/LocalMessage.h"


UUTLastSecondMessage::UUTLastSecondMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bIsUnique = true;
	Importance = 0.8f;
	bIsSpecial = true;
	Lifetime = 3.0f;
	MessageArea = FName(TEXT("GameMessages"));
	bIsConsoleMessage = false;

	bIsStatusAnnouncement = false;
}

FName UUTLastSecondMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return (Switch == 0) ? FName(TEXT("Denied")) : FName(TEXT("LastSecondSave"));
}

void UUTLastSecondMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	Super::ClientReceive(ClientData);
	if (ClientData.RelatedPlayerState_1 != NULL && ClientData.LocalPC == ClientData.RelatedPlayerState_1->GetOwner())
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (PC != NULL && PC->RewardAnnouncer != NULL)
		{
			PC->RewardAnnouncer->PlayAnnouncement(GetClass(), ClientData.MessageIndex, ClientData.OptionalObject);
		}
	}
}


