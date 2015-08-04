// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProfileItemMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTProfileItemMessage : public UUTLocalMessage
{
	GENERATED_BODY()
public:
	UUTProfileItemMessage(const FObjectInitializer& OI)
		: Super(OI)
	{
		MessageArea = FName(TEXT("ConsoleMessage"));
		bIsSpecial = true;

		Lifetime = 5.0f;
	}
	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
	{
		UUTProfileItem* Item = Cast<UUTProfileItem>(OptionalObject);
		if (Item == NULL)
		{
			return FText();
		}
		else
		{
			return FText::Format(NSLOCTEXT("UTProfileItemMessage", "GotItemHighScore", "Congratulations! You got the item {0} for having the highest score!"), Item->DisplayName);
		}
	}
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return FLinearColor::White;
	}
	virtual bool UseLargeFont(int32 MessageIndex) const override
	{
		return true;
	}
};