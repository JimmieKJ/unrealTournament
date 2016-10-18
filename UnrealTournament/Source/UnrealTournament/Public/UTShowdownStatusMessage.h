// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTShowdownStatusMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTShowdownStatusMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()
public:


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName FinalLife;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText FinalLifeMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText LivesRemainingPrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText LivesRemainingPostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		TArray<FName> LivesRemaining;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
};