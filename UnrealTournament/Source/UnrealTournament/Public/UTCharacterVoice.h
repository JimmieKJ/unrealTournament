// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLocalMessage.h"
#include "UTCharacterVoice.generated.h"

USTRUCT()
struct FCharacterSpeech
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	FText SpeechText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	USoundBase* SpeechSound;
};

UCLASS()
class UNREALTOURNAMENT_API UUTCharacterVoice : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	TArray<FCharacterSpeech> TauntMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> SameTeamMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> FriendlyReactions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> EnemyReactions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> NeedBackupMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> EnemyFCHereMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> AreaSecureMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> IGotFlagMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> DefendFlagMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> DefendFCMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> GetFlagBackMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> ImOnDefenseMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> ImGoingInMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> ImOnOffenseMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> BaseUnderAttackMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> SpreadOutMessages;

	/** Index offset for sending Same Team messages. */
	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 SameTeamBaseIndex;

	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 FriendlyReactionBaseIndex;

	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 EnemyReactionBaseIndex;

	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 StatusBaseIndex;

	/** map of status index offsets. */
	TMap< FName, float > StatusOffsets;

	virtual int32 GetStatusIndex(FName NewStatus) const;

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual USoundBase* GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
	virtual bool InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const override;
	virtual bool CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const override;
	virtual float GetAnnouncementPriority(int32 Switch) const override;
};
