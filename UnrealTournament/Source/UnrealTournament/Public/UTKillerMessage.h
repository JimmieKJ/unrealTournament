// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTKillerMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTKillerMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouKilledPrefixText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText YouKilledPostfixText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText KillAssistedPrefixText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText KillAssistedPostfixText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText SpecKilledText;

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
	virtual FText CombineEmphasisText(int32 CombinedMessageIndex, FText CombinedEmphasisText, FText OriginalEmphasisText) const override;
	virtual FText CombinePrefixText(int32 CombinedMessageIndex, FText OriginalPrefixText) const override;
	virtual FText CombineText(int32 CombinedMessageIndex, FText CombinedEmphasisText, FText OriginalCombinedText) const override;
};



