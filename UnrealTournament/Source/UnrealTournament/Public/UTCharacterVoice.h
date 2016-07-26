// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

USTRUCT()
struct FGameVolumeSpeech
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FText EnemyFCText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		USoundBase* EnemyFCSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FText FriendlyFCText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		USoundBase* FriendlyFCSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FText SecureText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		USoundBase* SecureSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	FText UndefendedText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	USoundBase* UndefendedSound;
};

const int32 ACKNOWLEDGE_SWITCH_INDEX = 3000;
const int32 NEGATIVE_SWITCH_INDEX = 3500;
const int32 GOT_YOUR_BACK_SWITCH_INDEX = 4000;
const int32 UNDER_HEAVY_ATTACK_SWITCH_INDEX = 4500;
const int32 NEED_BACKUP_SWITCH_INDEX = 10000;
const int32 ATTACK_THEIR_BASE_SWITCH_INDEX = 5000;
const int32 ENEMY_FC_HERE_SWITCH_INDEX = 10100;
const int32 AREA_SECURE_SWITCH_INDEX = 10200;
const int32 GOT_FLAG_SWITCH_INDEX = 10300;
const int32 DEFEND_FLAG_SWITCH_INDEX = 10400;
const int32 DEFEND_FLAG_CARRIER_SWITCH_INDEX = 10500;
const int32 GET_FLAG_BACK_SWITCH_INDEX = 10600;
const int32 ON_DEFENSE_SWITCH_INDEX = 10700;
const int32 GOING_IN_SWITCH_INDEX = 10800;
const int32 ON_OFFENSE_SWITCH_INDEX = 10900;
const int32 SPREAD_OUT_SWITCH_INDEX = 11000;
const int32 BASE_UNDER_ATTACK_SWITCH_INDEX = 11100;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> AcknowledgeMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		TArray<FCharacterSpeech> NegativeMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	TArray<FCharacterSpeech> GotYourBackMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	TArray<FCharacterSpeech> UnderHeavyAttackMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
	TArray<FCharacterSpeech> AttackTheirBaseMessages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FGameVolumeSpeech BridgeLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FGameVolumeSpeech RiverLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FGameVolumeSpeech AntechamberLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FGameVolumeSpeech ThroneRoomLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FGameVolumeSpeech CourtyardLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FGameVolumeSpeech StablesLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FGameVolumeSpeech DefenderBaseLines;

	/** Index offset for sending Same Team messages. */
	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 SameTeamBaseIndex;

	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 FriendlyReactionBaseIndex;

	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 EnemyReactionBaseIndex;

	UPROPERTY(BlueprintReadOnly, Category = Voice)
		int32 StatusBaseIndex;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FText TauntText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Voice)
		FText StatusTextFormat;


	/** map of status index offsets. */
	TMap< FName, float > StatusOffsets;

	virtual int32 GetStatusIndex(FName NewStatus) const;

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual USoundBase* GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
	virtual bool InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const override;
	virtual bool CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const override;
	virtual float GetAnnouncementPriority(int32 Switch) const override;
};
