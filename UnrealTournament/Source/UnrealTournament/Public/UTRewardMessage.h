// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTRewardMessage.generated.h"

UCLASS(CustomConstructor, Abstract)
class UUTRewardMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UUTRewardMessage(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		MessageArea = FName(TEXT("DeathMessage"));
		Importance = 0.8f;
		bIsSpecial = true;
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 4.0f;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText MessageText;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FName Announcement;

	virtual void ClientReceive(const FClientReceiveData& ClientData) const override
	{
		Super::ClientReceive(ClientData);
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (PC != NULL && PC->RewardAnnouncer != NULL)
		{
			PC->RewardAnnouncer->PlayAnnouncement(GetClass(), ClientData.MessageIndex, ClientData.OptionalObject);
		}
	}
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		return Announcement;
	}
	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override
	{
		return MessageText;
	}
};

