// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTJumpbootMessage.generated.h"

UCLASS(CustomConstructor, Abstract)
class UNREALTOURNAMENT_API UUTJumpbootMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UUTJumpbootMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		MessageArea = FName(TEXT("PickupMessage"));
		bIsSpecial = true;
		bIsUnique = true;
		bIsConsoleMessage = false;
		Lifetime = 1.0f;
		ActivateText = NSLOCTEXT("UUTJumpbootMessage", "JumpbootActivate", "Press [jump] again to activate jumpboots.");
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText ActivateText;

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override
	{
		return (Switch == 0) ? ActivateText : FText::GetEmpty();
	}

	virtual bool UseLargeFont(int32 MessageIndex) const override
	{
		return false;
	}
};

