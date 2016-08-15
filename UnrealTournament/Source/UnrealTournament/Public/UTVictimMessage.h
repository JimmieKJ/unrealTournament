// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTVictimMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTVictimMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouWereKilledByPrefix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText YouWereKilledByPostfix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText RespawnedVictimText;

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
	virtual float GetLifeTime(int32 Switch) const override;
};



