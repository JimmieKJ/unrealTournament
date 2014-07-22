// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTFirstBloodMessage.generated.h"

UCLASS(CustomConstructor)
class UUTFirstBloodMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	TArray<FText> AnnouncementText;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	TArray<FName> AnnouncementNames;

	UUTFirstBloodMessage(const FPostConstructInitializeProperties& PCIP)
		: Super(PCIP)
	{
		MessageArea = FName(TEXT("DeathMessage"));

		AnnouncementText.Add(NSLOCTEXT("UTFirstBloodMessage", "AnnouncementText[0]", "First Blood!"));
		AnnouncementNames.Add(TEXT("FirstBlood"));

		Importance = 0.9f;
		bIsSpecial = true;
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 4.0f;
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
		return AnnouncementNames.Num() > 0 ? AnnouncementNames[FMath::Clamp<int32>(Switch, 0, AnnouncementNames.Num() - 1)] : NAME_None;
	}
	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		return AnnouncementText.Num() > 0 ? AnnouncementText[FMath::Clamp<int32>(Switch, 0, AnnouncementText.Num() - 1)] : FText();
	}
};