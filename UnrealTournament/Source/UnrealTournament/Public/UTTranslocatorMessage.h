// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
		MessageArea = FName(TEXT("PickupMessage"));
		Importance = 0.8f;
		bIsSpecial = true;
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 4.0f;
		DisruptedText = NSLOCTEXT("UTTranslocatorMessage", "TranslocatorDisrupted", "Translocator Disk Disrupted - Regenerating.");
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText DisruptedText;

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override
	{
		return DisruptedText;
	}

	virtual bool UseLargeFont(int32 MessageIndex) const override
	{
		return false;
	}
};

