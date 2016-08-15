// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"
#include "UTTranslocatorMessage.generated.h"

UCLASS(CustomConstructor, Abstract)
class UNREALTOURNAMENT_API UUTTranslocatorMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UUTTranslocatorMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("PickupMessage"));
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 4.0f;
		FontSizeIndex = 0;
		DisruptedText = NSLOCTEXT("UTTranslocatorMessage", "TranslocatorDisrupted", "Translocator Disk Disrupted - Regenerating.");
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText DisruptedText;

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override
	{
		return DisruptedText;
	}
};

