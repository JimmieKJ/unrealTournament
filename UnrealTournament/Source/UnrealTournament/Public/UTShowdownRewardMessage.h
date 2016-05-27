// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTShowdownRewardMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTShowdownRewardMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FName FinishIt;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName LastMan;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName OverCharge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName Termination;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName Annihilation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName FinalLife;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText FinishItMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText LastManMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText OverChargeMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText TerminationMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText AnnihilationMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText FinalLifeMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText LivesRemainingPrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText LivesRemainingPostfix;

	/** sound played when termination occurs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		USoundBase* TerminationSound;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
	virtual void ClientReceive(const FClientReceiveData& ClientData) const override;
	virtual float GetAnnouncementDelay(int32 Switch) override;
};