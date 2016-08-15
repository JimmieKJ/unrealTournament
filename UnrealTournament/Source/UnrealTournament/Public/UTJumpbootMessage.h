// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTJumpbootMessage.generated.h"

UCLASS(CustomConstructor, Abstract)
class UNREALTOURNAMENT_API UUTJumpbootMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UUTJumpbootMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("PickupMessage"));
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 1.0f;
		ActivateText = NSLOCTEXT("UUTJumpbootMessage", "JumpbootActivate", "Press [jump] again to activate jumpboots.");
		FontSizeIndex = 0;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText ActivateText;

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override
	{
		return (Switch == 0) ? ActivateText : FText::GetEmpty();
	}
};

