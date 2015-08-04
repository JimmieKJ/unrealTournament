// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
}

FText UUTCharacterVoice::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	FFormatNamedArguments Args;
	if (!RelatedPlayerState_1)
	{
		UE_LOG(UT, Warning, TEXT("Character voice w/ no playerstate index %d"), Switch);
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
	else if (Switch >= StatusBaseIndex)
	{
		Switch -= StatusBaseIndex;
		if (Switch == GetStatusIndex(StatusMessage::NeedBackup))
		{
			if (NeedBackupMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return NeedBackupMessages[FMath::RandRange(0, NeedBackupMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::EnemyFCHere))
		{
			if (EnemyFCHereMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return EnemyFCHereMessages[FMath::RandRange(0, EnemyFCHereMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::AreaSecure))
		{
			if (AreaSecureMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return AreaSecureMessages[FMath::RandRange(0, AreaSecureMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::IGotFlag))
		{
			if (IGotFlagMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return IGotFlagMessages[FMath::RandRange(0, IGotFlagMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::DefendFlag))
		{
			if (DefendFlagMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return DefendFlagMessages[FMath::RandRange(0, DefendFlagMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::DefendFC))
		{
			if (DefendFCMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return DefendFCMessages[FMath::RandRange(0, DefendFCMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::GetFlagBack))
		{
			if (GetFlagBackMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return GetFlagBackMessages[FMath::RandRange(0, GetFlagBackMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::ImOnDefense))
		{
			if (ImOnDefenseMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return ImOnDefenseMessages[FMath::RandRange(0, ImOnDefenseMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::ImGoingIn))
		{
			if (ImGoingInMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return ImGoingInMessages[FMath::RandRange(0, ImGoingInMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::ImOnOffense))
		{
			if (ImOnOffenseMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return ImOnOffenseMessages[FMath::RandRange(0, ImOnOffenseMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::BaseUnderAttack))
		{
			if (BaseUnderAttackMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return BaseUnderAttackMessages[FMath::RandRange(0, BaseUnderAttackMessages.Num() - 1)].SpeechText;
		}
		if (Switch == GetStatusIndex(StatusMessage::SpreadOut))
		{
			if (SpreadOutMessages.Num() == 0)
			{
				return FText::GetEmpty();
			}
			return SpreadOutMessages[FMath::RandRange(0, SpreadOutMessages.Num() - 1)].SpeechText;
		}
	}
	else
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("UTCharacterVoice", "Taunt", "{PlayerName}: {TauntMessage}"), Args);
}

FName UUTCharacterVoice::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return NAME_Custom;
}

USoundBase* UUTCharacterVoice::GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const
{
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
	else if (Switch >= StatusBaseIndex)
	{
		Switch -= StatusBaseIndex;
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
	return !Cast<AUTPlayerController>(ClientData.LocalPC) || ((AUTPlayerController*)(ClientData.LocalPC))->bHearsTaunts;
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

