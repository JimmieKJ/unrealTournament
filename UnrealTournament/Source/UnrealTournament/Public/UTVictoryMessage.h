// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTVictoryMessage.generated.h"

UCLASS()
class UUTVictoryMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText YouHaveWonText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText YouHaveLostText;

	virtual void ClientReceive(const FClientReceiveData& ClientData) const OVERRIDE
	{
		Super::ClientReceive(ClientData);
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (PC != NULL && PC->StatusAnnouncer != NULL)
		{
			PC->StatusAnnouncer->PlayAnnouncement(GetClass(), ClientData.MessageIndex, ClientData.OptionalObject);
		}
	}
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const OVERRIDE
	{
		return (Switch == 0) ? FName(TEXT("WonMatch")) : FName(TEXT("LostMatch"));
	}
	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const OVERRIDE;
};

