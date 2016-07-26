// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacterVoice.h"
#include "UTAnnouncer.h"

UUTCharacterVoice::UUTCharacterVoice(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("ConsoleMessage"));
	bIsStatusAnnouncement = false;
	bOptionalSpoken = true;

	// < 1000 is reserved for taunts
	SameTeamBaseIndex = 1000;
	FriendlyReactionBaseIndex = 2000;
	EnemyReactionBaseIndex = 2500;

	// 10000+ is reserved for status messages
	StatusBaseIndex = 10000;
	StatusOffsets.Add(StatusMessage::NeedBackup, 0);
	StatusOffsets.Add(StatusMessage::EnemyFCHere, 100);
	StatusOffsets.Add(StatusMessage::AreaSecure, 200);
	StatusOffsets.Add(StatusMessage::IGotFlag, 300);
	StatusOffsets.Add(StatusMessage::DefendFlag, 400);
	StatusOffsets.Add(StatusMessage::DefendFC, 500);
	StatusOffsets.Add(StatusMessage::GetFlagBack, 600);
	StatusOffsets.Add(StatusMessage::ImOnDefense, 700);
	StatusOffsets.Add(StatusMessage::ImGoingIn, 800);
	StatusOffsets.Add(StatusMessage::ImOnOffense, 900);
	StatusOffsets.Add(StatusMessage::SpreadOut, 1000);
	StatusOffsets.Add(StatusMessage::BaseUnderAttack, 1100);

	TauntText = NSLOCTEXT("UTCharacterVoice", "Taunt", "{PlayerName}: {TauntMessage}");
	StatusTextFormat = NSLOCTEXT("UTCharacterVoice", "StatusFormat", "{PlayerName} @ {LastKnownLocation}: {TauntMessage}");

	BridgeLines.EnemyFCText = NSLOCTEXT("UTCharacterVoice", "BridgeEnemyFC", "Enemy Flag Carrier is on Bridge!");;
	BridgeLines.FriendlyFCText = NSLOCTEXT("UTCharacterVoice", "BridgeFriendlyFC", "I'm on Bridge!");;
	BridgeLines.SecureText = NSLOCTEXT("UTCharacterVoice", "BridgeSecure", "Bridge is secure.");;
	BridgeLines.SecureText = NSLOCTEXT("UTCharacterVoice", "BridgeUndefended", "Bridge is undefended!");;

	RiverLines.EnemyFCText = NSLOCTEXT("UTCharacterVoice", "RiverEnemyFC", "Enemy Flag Carrier is in River!");;
	RiverLines.FriendlyFCText = NSLOCTEXT("UTCharacterVoice", "RiverFriendlyFC", "I'm in River!");;
	RiverLines.SecureText = NSLOCTEXT("UTCharacterVoice", "RiverSecure", "River is secure.");;
	RiverLines.SecureText = NSLOCTEXT("UTCharacterVoice", "RiverUndefended", "River is undefended!");;

	AntechamberLines.EnemyFCText = NSLOCTEXT("UTCharacterVoice", "AntechamberEnemyFC", "Enemy Flag Carrier is in Antechamber!");;
	AntechamberLines.FriendlyFCText = NSLOCTEXT("UTCharacterVoice", "AntechamberFriendlyFC", "I'm in Antechamber!");;
	AntechamberLines.SecureText = NSLOCTEXT("UTCharacterVoice", "AntechamberSecure", "Antechamber is secure.");;
	AntechamberLines.SecureText = NSLOCTEXT("UTCharacterVoice", "AntechamberUndefended", "Antechamber is undefended!");;

	ThroneRoomLines.EnemyFCText = NSLOCTEXT("UTCharacterVoice", "ThroneRoomEnemyFC", "Enemy Flag Carrier is in ThroneRoom!");;
	ThroneRoomLines.FriendlyFCText = NSLOCTEXT("UTCharacterVoice", "ThroneRoomFriendlyFC", "I'm in ThroneRoom!");;
	ThroneRoomLines.SecureText = NSLOCTEXT("UTCharacterVoice", "ThroneRoomSecure", "ThroneRoom is secure.");;
	ThroneRoomLines.SecureText = NSLOCTEXT("UTCharacterVoice", "ThroneRoomUndefended", "ThroneRoom is undefended!");;

	CourtyardLines.EnemyFCText = NSLOCTEXT("UTCharacterVoice", "CourtyardEnemyFC", "Enemy Flag Carrier is in Courtyard!");;
	CourtyardLines.FriendlyFCText = NSLOCTEXT("UTCharacterVoice", "CourtyardFriendlyFC", "I'm in Courtyard!");;
	CourtyardLines.SecureText = NSLOCTEXT("UTCharacterVoice", "CourtyardSecure", "Courtyard is secure.");;
	CourtyardLines.SecureText = NSLOCTEXT("UTCharacterVoice", "CourtyardUndefended", "Courtyard is undefended!");;

	StablesLines.EnemyFCText = NSLOCTEXT("UTCharacterVoice", "StablesEnemyFC", "Enemy Flag Carrier is in Stables!");;
	StablesLines.FriendlyFCText = NSLOCTEXT("UTCharacterVoice", "StablesFriendlyFC", "I'm in Stables!");;
	StablesLines.SecureText = NSLOCTEXT("UTCharacterVoice", "StablesSecure", "Stables are secure.");;
	StablesLines.SecureText = NSLOCTEXT("UTCharacterVoice", "StablesUndefended", "Stables are undefended!");;

	DefenderBaseLines.EnemyFCText = NSLOCTEXT("UTCharacterVoice", "DefenderBaseEnemyFC", "Enemy Flag Carrier is in our base!");;
	DefenderBaseLines.FriendlyFCText = NSLOCTEXT("UTCharacterVoice", "DefenderBaseFriendlyFC", "I'm going in!");;
	DefenderBaseLines.SecureText = NSLOCTEXT("UTCharacterVoice", "DefenderBaseSecure", "Base is secure.");;
	DefenderBaseLines.SecureText = NSLOCTEXT("UTCharacterVoice", "DefenderBaseUndefended", "Base is undefended!");;
}

FText UUTCharacterVoice::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	bool bStatusMessage = false;

	FFormatNamedArguments Args;
	if (!RelatedPlayerState_1)
	{
		UE_LOG(UT, Warning, TEXT("Character voice w/ no playerstate index %d"), Switch);
		return FText::GetEmpty();
	}

	UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (GS != NULL && GS->GetBotSpeech() < ((Switch >= StatusBaseIndex) ? BSO_StatusTextOnly : BSO_All))
	{ 
		return FText::GetEmpty();
	}

	Args.Add("PlayerName", FText::AsCultureInvariant(RelatedPlayerState_1->PlayerName));
	if (TauntMessages.Num() > Switch)
	{
		Args.Add("TauntMessage", TauntMessages[Switch].SpeechText);
	}
	else if (SameTeamMessages.Num() > Switch - SameTeamBaseIndex)
	{
		Args.Add("TauntMessage", SameTeamMessages[Switch - SameTeamBaseIndex].SpeechText);
	}
	else if (FriendlyReactions.Num() > Switch - FriendlyReactionBaseIndex)
	{
		Args.Add("TauntMessage", FriendlyReactions[Switch - FriendlyReactionBaseIndex].SpeechText);
	}
	else if (EnemyReactions.Num() > Switch - EnemyReactionBaseIndex)
	{
		Args.Add("TauntMessage", EnemyReactions[Switch - EnemyReactionBaseIndex].SpeechText);
	}
	else if (Switch == ACKNOWLEDGE_SWITCH_INDEX )
	{
		bStatusMessage = true;
		Args.Add("TauntMessage", AcknowledgeMessages[FMath::RandRange(0, AcknowledgeMessages.Num() - 1)].SpeechText);
	}
	else if (Switch == NEGATIVE_SWITCH_INDEX )
	{
		bStatusMessage = true;
		Args.Add("TauntMessage", NegativeMessages[FMath::RandRange(0, NegativeMessages.Num() - 1)].SpeechText);
	}
	else if (Switch == GOT_YOUR_BACK_SWITCH_INDEX)
	{
		bStatusMessage = true;
		Args.Add("TauntMessage", GotYourBackMessages[FMath::RandRange(0, GotYourBackMessages.Num() - 1)].SpeechText);
	}
	else if (Switch == UNDER_HEAVY_ATTACK_SWITCH_INDEX)
	{
		bStatusMessage = true;
		Args.Add("TauntMessage", UnderHeavyAttackMessages[FMath::RandRange(0, UnderHeavyAttackMessages.Num() - 1)].SpeechText);
	}
	else if (Switch == ATTACK_THEIR_BASE_SWITCH_INDEX)
	{
		bStatusMessage = true;
		Args.Add("TauntMessage", AttackTheirBaseMessages[FMath::RandRange(0, AttackTheirBaseMessages.Num() - 1)].SpeechText);
	}
	else if (Switch >= StatusBaseIndex)
	{
		bStatusMessage = true;
		if (Switch == GetStatusIndex(StatusMessage::NeedBackup))
		{
			if (NeedBackupMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", NeedBackupMessages[FMath::RandRange(0, NeedBackupMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::EnemyFCHere))
		{
			if (EnemyFCHereMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", EnemyFCHereMessages[FMath::RandRange(0, EnemyFCHereMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::AreaSecure))
		{
			if (AreaSecureMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", AreaSecureMessages[FMath::RandRange(0, AreaSecureMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::IGotFlag))
		{
			if (IGotFlagMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", IGotFlagMessages[FMath::RandRange(0, IGotFlagMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::DefendFlag))
		{
			if (DefendFlagMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", DefendFlagMessages[FMath::RandRange(0, DefendFlagMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::DefendFC))
		{
			if (DefendFCMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", DefendFCMessages[FMath::RandRange(0, DefendFCMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::GetFlagBack))
		{
			if (GetFlagBackMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", GetFlagBackMessages[FMath::RandRange(0, GetFlagBackMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::ImOnDefense))
		{
			if (ImOnDefenseMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", ImOnDefenseMessages[FMath::RandRange(0, ImOnDefenseMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::ImGoingIn))
		{
			if (ImGoingInMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", ImGoingInMessages[FMath::RandRange(0, ImGoingInMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::ImOnOffense))
		{
			if (ImOnOffenseMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", ImOnOffenseMessages[FMath::RandRange(0, ImOnOffenseMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::BaseUnderAttack))
		{
			if (BaseUnderAttackMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", BaseUnderAttackMessages[FMath::RandRange(0, BaseUnderAttackMessages.Num() - 1)].SpeechText);
		}
		if (Switch == GetStatusIndex(StatusMessage::SpreadOut))
		{
			if (SpreadOutMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			Args.Add("TauntMessage", SpreadOutMessages[FMath::RandRange(0, SpreadOutMessages.Num() - 1)].SpeechText);
		}
	}
	else
	{
		return FText::GetEmpty();
	}

	if (OptionalObject != nullptr)
	{
		AUTGameVolume* GameVolume = Cast<AUTGameVolume>(OptionalObject);
		if (bStatusMessage && GameVolume != nullptr)
		{
			Args.Add("LastKnownLocation", GameVolume->VolumeName);
		}
		else
		{
			bStatusMessage = false;
		}
	}
	else
	{
		bStatusMessage = false;
	}

	return bStatusMessage ? FText::Format(StatusTextFormat, Args) : FText::Format(TauntText, Args);
}

FName UUTCharacterVoice::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	return NAME_Custom;
}

USoundBase* UUTCharacterVoice::GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const
{
//	UE_LOG(UT,Log,TEXT("Announcement: %i"),Switch);

	if (TauntMessages.Num() > Switch)
	{
		return TauntMessages[Switch].SpeechSound;
	}
	else if (SameTeamMessages.Num() > Switch - SameTeamBaseIndex)
	{
		return SameTeamMessages[Switch - SameTeamBaseIndex].SpeechSound;
	}
	else if (FriendlyReactions.Num() > Switch - FriendlyReactionBaseIndex)
	{
		return FriendlyReactions[Switch - FriendlyReactionBaseIndex].SpeechSound;
	}
	else if (EnemyReactions.Num() > Switch - EnemyReactionBaseIndex)
	{
		return EnemyReactions[Switch - EnemyReactionBaseIndex].SpeechSound;
	}
	else if (Switch == ACKNOWLEDGE_SWITCH_INDEX )
	{
		return AcknowledgeMessages[FMath::RandRange(0, AcknowledgeMessages.Num() - 1)].SpeechSound;
	}
	else if (Switch == NEGATIVE_SWITCH_INDEX )
	{
		return NegativeMessages[FMath::RandRange(0, NegativeMessages.Num() - 1)].SpeechSound;
	}
	else if (Switch == GOT_YOUR_BACK_SWITCH_INDEX)
	{
		return GotYourBackMessages[FMath::RandRange(0, GotYourBackMessages.Num() - 1)].SpeechSound;
	}
	else if (Switch == UNDER_HEAVY_ATTACK_SWITCH_INDEX)
	{
		return UnderHeavyAttackMessages[FMath::RandRange(0, UnderHeavyAttackMessages.Num() - 1)].SpeechSound;
	}
	else if (Switch == ATTACK_THEIR_BASE_SWITCH_INDEX)
	{
		return AttackTheirBaseMessages[FMath::RandRange(0, AttackTheirBaseMessages.Num() - 1)].SpeechSound;
	}
	else if (Switch >= StatusBaseIndex)
	{
		if (Switch == GetStatusIndex(StatusMessage::NeedBackup))
		{
			if (NeedBackupMessages.Num() == 0)
			{
				return NULL;
			}
			return NeedBackupMessages[FMath::RandRange(0, NeedBackupMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::EnemyFCHere))
		{
			if (EnemyFCHereMessages.Num() == 0)
			{
				return NULL;
			}
			return EnemyFCHereMessages[FMath::RandRange(0, EnemyFCHereMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::AreaSecure))
		{
			if (AreaSecureMessages.Num() == 0)
			{
				return NULL;
			}
			return AreaSecureMessages[FMath::RandRange(0, AreaSecureMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::IGotFlag))
		{
			if (IGotFlagMessages.Num() == 0)
			{
				return NULL;
			}
			return IGotFlagMessages[FMath::RandRange(0, IGotFlagMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::DefendFlag))
		{
			if (DefendFlagMessages.Num() == 0)
			{
				return NULL;
			}
			return DefendFlagMessages[FMath::RandRange(0, DefendFlagMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::DefendFC))
		{
			if (DefendFCMessages.Num() == 0)
			{
				return NULL;
			}
			return DefendFCMessages[FMath::RandRange(0, DefendFCMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::GetFlagBack))
		{
			if (GetFlagBackMessages.Num() == 0)
			{
				return NULL;
			}
			return GetFlagBackMessages[FMath::RandRange(0, GetFlagBackMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::ImOnDefense))
		{
			if (ImOnDefenseMessages.Num() == 0)
			{
				return NULL;
			}
			return ImOnDefenseMessages[FMath::RandRange(0, ImOnDefenseMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::ImGoingIn))
		{
			if (ImGoingInMessages.Num() == 0)
			{
				return NULL;
			}
			return ImGoingInMessages[FMath::RandRange(0, ImGoingInMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::ImOnOffense))
		{
			if (ImOnOffenseMessages.Num() == 0)
			{
				return NULL;
			}
			return ImOnOffenseMessages[FMath::RandRange(0, ImOnOffenseMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::BaseUnderAttack))
		{
			if (BaseUnderAttackMessages.Num() == 0)
			{
				return NULL;
			}
			return BaseUnderAttackMessages[FMath::RandRange(0, BaseUnderAttackMessages.Num() - 1)].SpeechSound;
		}
		if (Switch == GetStatusIndex(StatusMessage::SpreadOut))
		{
			if (SpreadOutMessages.Num() == 0)
			{
				return NULL;
			}
			return SpreadOutMessages[FMath::RandRange(0, SpreadOutMessages.Num() - 1)].SpeechSound;
		}
	}
	return NULL;
}

void UUTCharacterVoice::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
}

bool UUTCharacterVoice::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	if (ClientData.RelatedPlayerState_1 && ClientData.RelatedPlayerState_1->bIsABot)
	{
		UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
		if (GS != NULL && GS->GetBotSpeech() < BSO_All)
		{
			return false;
		}
		return !Cast<AUTPlayerController>(ClientData.LocalPC) || ((AUTPlayerController*)(ClientData.LocalPC))->bHearsTaunts;
	}
	else
	{
		return true;
	}
}

bool UUTCharacterVoice::InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	return (GetClass() == OtherMessageClass) && (Switch >= 1000) && (OtherSwitch < 1000);
}

bool UUTCharacterVoice::CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	return (GetClass() != OtherMessageClass) || (Switch < 1000);
}

float UUTCharacterVoice::GetAnnouncementPriority(int32 Switch) const
{
	return 0.f;
}

int32 UUTCharacterVoice::GetStatusIndex(FName NewStatus) const
{
	return StatusBaseIndex + StatusOffsets.FindRef(NewStatus);
}

