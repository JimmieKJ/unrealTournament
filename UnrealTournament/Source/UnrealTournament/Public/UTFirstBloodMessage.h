// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTFirstBloodMessage.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTFirstBloodMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FName FirstBloodAnnouncement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText FirstBloodLocalText;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText FirstBloodRemoteText;

	UUTFirstBloodMessage(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("Spree"));

		FirstBloodLocalText = NSLOCTEXT("UTFirstBloodMessage", "FirstBloodLocalText", "First Blood!");
		FirstBloodRemoteText = NSLOCTEXT("UTFirstBloodMessage", "FirstBloodRemoteText", "{Player1Name} drew First Blood!");

		FirstBloodAnnouncement = FName(TEXT("FirstBlood"));

		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 2.0f;
		AnnouncementDelay = 0.5f;
		ScaleInSize = 3.f;
	}

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return FLinearColor::Red;
	}

	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override
	{
		return IsLocalForAnnouncement(ClientData, true, false);
	}

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override
	{
		return FirstBloodAnnouncement;
	}

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		if (bTargetsPlayerState1)
		{
			return FirstBloodLocalText;
		}
			
		return FirstBloodRemoteText;
	}
};