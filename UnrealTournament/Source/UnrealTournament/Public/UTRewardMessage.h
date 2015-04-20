// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"
#include "UTRewardMessage.generated.h"

UCLASS(CustomConstructor, Abstract)
class UNREALTOURNAMENT_API UUTRewardMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UUTRewardMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		MessageArea = FName(TEXT("DeathMessage"));
		Importance = 0.8f;
		bIsSpecial = true;
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 3.0f;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText MessageText;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FName Announcement;

	virtual FLinearColor GetMessageColor(int32 MessageIndex) const
	{
		return FLinearColor::Red;
	}

	virtual float GetScaleInSize(int32 MessageIndex) const
	{
		return 3.f;
	}

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

