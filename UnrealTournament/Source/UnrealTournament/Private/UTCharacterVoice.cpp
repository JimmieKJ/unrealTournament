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
	FontSizeIndex = -1;
	Lifetime = 6.0f;
	bPlayDuringIntermission = false;

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
	StatusOffsets.Add(StatusMessage::ImOnOffense, 900);
	StatusOffsets.Add(StatusMessage::SpreadOut, 1000);

	StatusOffsets.Add(StatusMessage::ImGoingIn, KEY_CALLOUTS + 100);
	StatusOffsets.Add(StatusMessage::BaseUnderAttack, KEY_CALLOUTS + 200);
	StatusOffsets.Add(StatusMessage::Incoming, KEY_CALLOUTS + 250);
	// only game volume speech after FIRSTGAMEVOLUMESPEECH = KEY_CALLOUTS + 299 
	StatusOffsets.Add(GameVolumeSpeechType::GV_Bridge, KEY_CALLOUTS + 300);
	StatusOffsets.Add(GameVolumeSpeechType::GV_River, KEY_CALLOUTS + 400);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Antechamber, KEY_CALLOUTS + 500);
	StatusOffsets.Add(GameVolumeSpeechType::GV_ThroneRoom, KEY_CALLOUTS + 600);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Courtyard, KEY_CALLOUTS + 700);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Stables, KEY_CALLOUTS + 800);
	StatusOffsets.Add(GameVolumeSpeechType::GV_AntechamberHigh, KEY_CALLOUTS + 1000);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Tower, KEY_CALLOUTS + 1100);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Creek, KEY_CALLOUTS + 1200);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Temple, KEY_CALLOUTS + 1300);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Cave, KEY_CALLOUTS + 1400);
	StatusOffsets.Add(GameVolumeSpeechType::GV_BaseCamp, KEY_CALLOUTS + 1500);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Sniper, KEY_CALLOUTS + 1600);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Arena, KEY_CALLOUTS + 1700);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Bonsaii, KEY_CALLOUTS + 1800);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Cliffs, KEY_CALLOUTS + 1900);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Core, KEY_CALLOUTS + 2000);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Crossroads, KEY_CALLOUTS + 2100);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Vents, KEY_CALLOUTS + 2200);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Pipes, KEY_CALLOUTS + 2300);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Ramp, KEY_CALLOUTS + 2400);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Hinge, KEY_CALLOUTS + 2500);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Tree, KEY_CALLOUTS + 2600);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Tunnel, KEY_CALLOUTS + 2700);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Falls, KEY_CALLOUTS + 2800);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Fort, KEY_CALLOUTS + 2900);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Fountain, KEY_CALLOUTS + 3000);
	StatusOffsets.Add(GameVolumeSpeechType::GV_GateHouse, KEY_CALLOUTS + 3100);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Overlook, KEY_CALLOUTS + 3200);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Ruins, KEY_CALLOUTS + 3300);
	StatusOffsets.Add(GameVolumeSpeechType::GV_SniperTower, KEY_CALLOUTS + 3400);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Flak, KEY_CALLOUTS + 3500);
	StatusOffsets.Add(GameVolumeSpeechType::GV_Waterfall, KEY_CALLOUTS + 3600);
	// only game volume speech before LASTGAMEVOLUMESPEECH = KEY_CALLOUTS + 5000 

	StatusOffsets.Add(StatusMessage::EnemyRally, KEY_CALLOUTS + 5000);
	StatusOffsets.Add(StatusMessage::FindFC, KEY_CALLOUTS + 5001);
	StatusOffsets.Add(StatusMessage::LastLife, KEY_CALLOUTS + 5002);
	StatusOffsets.Add(StatusMessage::EnemyLowLives, KEY_CALLOUTS + 5003);
	StatusOffsets.Add(StatusMessage::EnemyThreePlayers, KEY_CALLOUTS + 5004);
	StatusOffsets.Add(StatusMessage::NeedRally, KEY_CALLOUTS + 5006);
	StatusOffsets.Add(StatusMessage::NeedHealth, KEY_CALLOUTS + 5007);
	StatusOffsets.Add(StatusMessage::BehindYou, KEY_CALLOUTS + 5008);
	StatusOffsets.Add(StatusMessage::RedeemerSpotted, KEY_CALLOUTS + 5009);
	StatusOffsets.Add(StatusMessage::GetTheFlag, KEY_CALLOUTS + 5010);
	StatusOffsets.Add(StatusMessage::RallyNow, KEY_CALLOUTS + 5011);

	//FIRSTPICKUPSPEECH = KEY_CALLOUTS + 5099;
	StatusOffsets.Add(PickupSpeechType::RedeemerPickup, KEY_CALLOUTS + 5100);
	StatusOffsets.Add(PickupSpeechType::UDamagePickup, KEY_CALLOUTS + 5200);
	StatusOffsets.Add(PickupSpeechType::ShieldbeltPickup, KEY_CALLOUTS + 5300);
	// LASTPICKUPSPEECH = KEY_CALLOUTS + 9999;


	StatusOffsets.Add(StatusMessage::RedeemerKills, KEY_CALLOUTS + 10000);	

	TauntText = NSLOCTEXT("UTCharacterVoice", "Taunt", ": {TauntMessage}");
	StatusTextFormat = NSLOCTEXT("UTCharacterVoice", "StatusFormat", " at {LastKnownLocation}: {TauntMessage}");

	FallbackLines.Add(FName(TEXT("Bridge")), GameVolumeSpeechType::GV_Bridge);
	FallbackLines.Add(FName(TEXT("River")), GameVolumeSpeechType::GV_River);
	FallbackLines.Add(FName(TEXT("Tower")), GameVolumeSpeechType::GV_Tower);
	FallbackLines.Add(FName(TEXT("Creek")), GameVolumeSpeechType::GV_Creek);
	FallbackLines.Add(FName(TEXT("Temple")), GameVolumeSpeechType::GV_Temple);
	FallbackLines.Add(FName(TEXT("Cave")), GameVolumeSpeechType::GV_Cave);
	FallbackLines.Add(FName(TEXT("Antechamber")), GameVolumeSpeechType::GV_Antechamber);
	FallbackLines.Add(FName(TEXT("Courtyard")), GameVolumeSpeechType::GV_Courtyard);
	FallbackLines.Add(FName(TEXT("Stables")), GameVolumeSpeechType::GV_Stables);
}

FName UUTCharacterVoice::GetFallbackLines(FName InName) const
{
	return FallbackLines.FindRef(InName);
}

bool UUTCharacterVoice::IsOptionalSpoken(int32 MessageIndex) const
{
	return bOptionalSpoken && (MessageIndex < StatusBaseIndex+KEY_CALLOUTS);
}

int32 UUTCharacterVoice::GetDestinationIndex(int32 MessageIndex) const
{
	return (MessageIndex < 1000) ? 4 : 6;
}

FText UUTCharacterVoice::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	// @TOOD FIXMESTEVE option to turn these one
	return FText::GetEmpty();

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
	FCharacterSpeech PickedSpeech = GetCharacterSpeech(Switch);
	Args.Add("PlayerName", FText::AsCultureInvariant(RelatedPlayerState_1->PlayerName));
	Args.Add("TauntMessage", PickedSpeech.SpeechText);

	bool bStatusMessage = (Switch >= StatusBaseIndex) && OptionalObject && Cast<AUTGameVolume>(OptionalObject);
	if (bStatusMessage)
	{
		Args.Add("LastKnownLocation", Cast<AUTGameVolume>(OptionalObject)->VolumeName);
	}
	return bStatusMessage ? FText::Format(StatusTextFormat, Args) : FText::Format(TauntText, Args);
}

FName UUTCharacterVoice::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	return NAME_Custom;
}

USoundBase* UUTCharacterVoice::GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	FCharacterSpeech PickedSpeech = GetCharacterSpeech(Switch);
	return PickedSpeech.SpeechSound;
}

FCharacterSpeech UUTCharacterVoice::GetCharacterSpeech(int32 Switch) const
{
	FCharacterSpeech EmptySpeech;
	EmptySpeech.SpeechSound = nullptr;
	EmptySpeech.SpeechText = FText::GetEmpty();

	if (TauntMessages.Num() > Switch)
	{
		return TauntMessages[Switch];
	}
	else if (SameTeamMessages.Num() > Switch - SameTeamBaseIndex)
	{
		return SameTeamMessages[Switch - SameTeamBaseIndex];
	}
	else if (FriendlyReactions.Num() > Switch - FriendlyReactionBaseIndex)
	{
		return FriendlyReactions[Switch - FriendlyReactionBaseIndex];
	}
	else if (EnemyReactions.Num() > Switch - EnemyReactionBaseIndex)
	{
		return EnemyReactions[Switch - EnemyReactionBaseIndex];
	}
	else if (Switch == ACKNOWLEDGE_SWITCH_INDEX )
	{
		return AcknowledgeMessages[FMath::RandRange(0, AcknowledgeMessages.Num() - 1)];
	}
	else if (Switch == NEGATIVE_SWITCH_INDEX )
	{
		return NegativeMessages[FMath::RandRange(0, NegativeMessages.Num() - 1)];
	}
	else if (Switch == GOT_YOUR_BACK_SWITCH_INDEX)
	{
		return GotYourBackMessages[FMath::RandRange(0, GotYourBackMessages.Num() - 1)];
	}
	else if (Switch == UNDER_HEAVY_ATTACK_SWITCH_INDEX)
	{
		return UnderHeavyAttackMessages[FMath::RandRange(0, UnderHeavyAttackMessages.Num() - 1)];
	}
	else if (Switch == ATTACK_THEIR_BASE_SWITCH_INDEX)
	{
		return AttackTheirBaseMessages[FMath::RandRange(0, AttackTheirBaseMessages.Num() - 1)];
	}
	else if (Switch >= StatusBaseIndex)
	{
		if (Switch < StatusBaseIndex + KEY_CALLOUTS)
		{
			if (Switch == GetStatusIndex(StatusMessage::NeedBackup))
			{
				if (NeedBackupMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return NeedBackupMessages[FMath::RandRange(0, NeedBackupMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::EnemyFCHere))
			{
				if (EnemyFCHereMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return EnemyFCHereMessages[FMath::RandRange(0, EnemyFCHereMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::AreaSecure))
			{
				if (AreaSecureMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return AreaSecureMessages[FMath::RandRange(0, AreaSecureMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::IGotFlag))
			{
				if (IGotFlagMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return IGotFlagMessages[FMath::RandRange(0, IGotFlagMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::DefendFlag))
			{
				if (DefendFlagMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return DefendFlagMessages[FMath::RandRange(0, DefendFlagMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::DefendFC))
			{
				if (DefendFCMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return DefendFCMessages[FMath::RandRange(0, DefendFCMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::GetFlagBack))
			{
				if (GetFlagBackMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return GetFlagBackMessages[FMath::RandRange(0, GetFlagBackMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::ImOnDefense))
			{
				if (ImOnDefenseMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return ImOnDefenseMessages[FMath::RandRange(0, ImOnDefenseMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::ImOnOffense))
			{
				if (ImOnOffenseMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return ImOnOffenseMessages[FMath::RandRange(0, ImOnOffenseMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::SpreadOut))
			{
				if (SpreadOutMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return SpreadOutMessages[FMath::RandRange(0, SpreadOutMessages.Num() - 1)];
			}
		}
		else
		{
			if (Switch == GetStatusIndex(StatusMessage::ImGoingIn))
			{
				if (ImGoingInMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return ImGoingInMessages[FMath::RandRange(0, ImGoingInMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::BaseUnderAttack))
			{
				if (BaseUnderAttackMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return BaseUnderAttackMessages[FMath::RandRange(0, BaseUnderAttackMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::Incoming))
			{
				if (IncomingMessages.Num() == 0)
				{
					return EmptySpeech;
				}
				return IncomingMessages[FMath::RandRange(0, IncomingMessages.Num() - 1)];
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Bridge) / 100)
			{
				return GetGVLine(BridgeLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Bridge));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_River) / 100)
			{
				return GetGVLine(RiverLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_River));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Antechamber) / 100)
			{
				return GetGVLine(AntechamberLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Antechamber));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_ThroneRoom) / 100)
			{
				return GetGVLine(ThroneRoomLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_ThroneRoom));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Courtyard) / 100)
			{
				return GetGVLine(CourtyardLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Courtyard));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Stables) / 100)
			{
				return GetGVLine(StablesLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Stables));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_AntechamberHigh) / 100)
			{
				return GetGVLine(AntechamberHighLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_AntechamberHigh));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Tower) / 100)
			{
				return GetGVLine(TowerLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Tower));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Creek) / 100)
			{
				return GetGVLine(CreekLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Creek));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Temple) / 100)
			{
				return GetGVLine(TempleLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Temple));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Cave) / 100)
			{
				return GetGVLine(CaveLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Cave));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_BaseCamp) / 100)
			{
				return GetGVLine(BaseCampLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_BaseCamp));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Sniper) / 100)
			{
				return GetGVLine(SniperLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Sniper));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Arena) / 100)
			{
				return GetGVLine(ArenaLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Arena));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Bonsaii) / 100)
			{
				return GetGVLine(BonsaiiLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Bonsaii));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Cliffs) / 100)
			{
				return GetGVLine(CliffsLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Cliffs));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Core) / 100)
			{
				return GetGVLine(CoreLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Core));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Crossroads) / 100)
			{
				return GetGVLine(CrossroadsLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Crossroads));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Vents) / 100)
			{
				return GetGVLine(VentsLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Vents));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Pipes) / 100)
			{
				return GetGVLine(PipesLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Pipes));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Ramp) / 100)
			{
				return GetGVLine(RampLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Ramp));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Hinge) / 100)
			{
				return GetGVLine(HingeLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Hinge));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Tree) / 100)
			{
				return GetGVLine(TreeLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Tree));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Tunnel) / 100)
			{
				return GetGVLine(TunnelLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Tunnel));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Falls) / 100)
			{
				return GetGVLine(WaterfallLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Falls));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Fort) / 100)
			{
				return GetGVLine(FortLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Fort));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Fountain) / 100)
			{
				return GetGVLine(FountainLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Fountain));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_GateHouse) / 100)
			{
				return GetGVLine(GateHouseLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_GateHouse));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Overlook) / 100)
			{
				return GetGVLine(OverlookLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Overlook));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Ruins) / 100)
			{
				return GetGVLine(RuinsLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Ruins));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_SniperTower) / 100)
			{
				return GetGVLine(SniperTowerLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_SniperTower));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Flak) / 100)
			{
				return GetGVLine(FlakLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Flak));
			}
			else if (Switch / 100 == GetStatusIndex(GameVolumeSpeechType::GV_Waterfall) / 100)
			{
				return GetGVLine(WaterfallLines, Switch - GetStatusIndex(GameVolumeSpeechType::GV_Waterfall));
			}
			else if (Switch / 100 == GetStatusIndex(PickupSpeechType::UDamagePickup) / 100)
			{
				return (Switch - GetStatusIndex(PickupSpeechType::UDamagePickup) == 0) ? UDamageAvailableLine : UDamagePickupLine;
			}
			else if (Switch / 100 == GetStatusIndex(PickupSpeechType::ShieldbeltPickup) / 100)
			{
				return (Switch - GetStatusIndex(PickupSpeechType::ShieldbeltPickup) == 0) ? ShieldbeltAvailableLine : ShieldbeltPickupLine;
			}
			else if (Switch / 100 == GetStatusIndex(PickupSpeechType::RedeemerPickup) / 100)
			{
				int32 Index = Switch - GetStatusIndex(PickupSpeechType::RedeemerPickup);
				if (Index == 2)
				{
					if (DroppedRedeemerMessages.Num() == 0)
					{
						return EmptySpeech;
					}
					return DroppedRedeemerMessages[FMath::RandRange(0, DroppedRedeemerMessages.Num() - 1)];
				}
				return (Index == 0) ? RedeemerAvailableLine : RedeemerPickupLine;
			}
			else if (Switch == GetStatusIndex(StatusMessage::EnemyRally))
			{
				return (EnemyRallyMessages.Num() == 0) ? EmptySpeech : EnemyRallyMessages[FMath::RandRange(0, EnemyRallyMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::RallyNow))
			{
				return (RallyNowMessages.Num() == 0) ? EmptySpeech : RallyNowMessages[FMath::RandRange(0, RallyNowMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::FindFC))
			{
				return (FindFCMessages.Num() == 0) ? EmptySpeech : FindFCMessages[FMath::RandRange(0, FindFCMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::LastLife))
			{
				return (LastLifeMessages.Num() == 0) ? EmptySpeech : LastLifeMessages[FMath::RandRange(0, LastLifeMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::EnemyLowLives))
			{
				return (EnemyLowLivesMessages.Num() == 0) ? EmptySpeech : EnemyLowLivesMessages[FMath::RandRange(0, EnemyLowLivesMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::EnemyThreePlayers))
			{
				return (EnemyThreePlayersMessages.Num() == 0) ? EmptySpeech : EnemyThreePlayersMessages[FMath::RandRange(0, EnemyThreePlayersMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::NeedRally))
			{
				return (NeedRallyMessages.Num() == 0) ? EmptySpeech : NeedRallyMessages[FMath::RandRange(0, NeedRallyMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::NeedHealth))
			{
				return (NeedHealthMessages.Num() == 0) ? EmptySpeech : NeedHealthMessages[FMath::RandRange(0, NeedHealthMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::BehindYou))
			{
				return (BehindYouMessages.Num() == 0) ? EmptySpeech : BehindYouMessages[FMath::RandRange(0, BehindYouMessages.Num() - 1)];
			}
			else if (Switch == GetStatusIndex(StatusMessage::GetTheFlag))
			{
				return (GetTheFlagMessages.Num() == 0) ? EmptySpeech : GetTheFlagMessages[FMath::RandRange(0, GetTheFlagMessages.Num() - 1)];
			}
			else if (Switch/100 == GetStatusIndex(StatusMessage::RedeemerKills)/100)
			{
				int32 Index = FMath::Min(4, Switch - GetStatusIndex(StatusMessage::RedeemerKills));
				return (RedeemerKillMessages.Num() > Index) ? RedeemerKillMessages[Index] : EmptySpeech;
			}
			else if (Switch == GetStatusIndex(StatusMessage::RedeemerSpotted))
			{
				return (RedeemerSpottedMessages.Num() == 0) ? EmptySpeech : RedeemerSpottedMessages[FMath::RandRange(0, RedeemerSpottedMessages.Num() - 1)];
			}
		}
	}
	return EmptySpeech;
}

FCharacterSpeech UUTCharacterVoice::GetGVLine(const FGameVolumeSpeech& GVLines, int32 SwitchIndex) const
{
	FCharacterSpeech EmptySpeech;
	EmptySpeech.SpeechSound = nullptr;
	EmptySpeech.SpeechText = FText::GetEmpty();
	switch(SwitchIndex)
	{
	case 0: return (GVLines.EnemyFCSpeech.Num() > 0) ? GVLines.EnemyFCSpeech[FMath::RandRange(0, GVLines.EnemyFCSpeech.Num() - 1)] : EmptySpeech;
	case 1: return (GVLines.FriendlyFCSpeech.Num() > 0) ? GVLines.FriendlyFCSpeech[FMath::RandRange(0, GVLines.FriendlyFCSpeech.Num() - 1)] : EmptySpeech;
	case 2: return (GVLines.SecureSpeech.Num() > 0) ? GVLines.SecureSpeech[FMath::RandRange(0, GVLines.SecureSpeech.Num() - 1)] : EmptySpeech;
	case 3: return (GVLines.UndefendedSpeech.Num() > 0) ? GVLines.UndefendedSpeech[FMath::RandRange(0, GVLines.UndefendedSpeech.Num() - 1)] : EmptySpeech;
	}
	return EmptySpeech;
}

void UUTCharacterVoice::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
}

bool UUTCharacterVoice::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	if (ClientData.RelatedPlayerState_1 && ClientData.RelatedPlayerState_1->bIsABot && (TauntMessages.Num() > ClientData.MessageIndex))
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

bool UUTCharacterVoice::IsFlagLocationUpdate(int32 Switch) const
{
	return (Switch > StatusBaseIndex + FIRSTGAMEVOLUMESPEECH) && (Switch < StatusBaseIndex + LASTGAMEVOLUMESPEECH) && (Switch % 10 < 2);
}

bool UUTCharacterVoice::IsPickupUpdate(int32 Switch) const
{
	return (Switch > StatusBaseIndex + FIRSTPICKUPSPEECH) && (Switch < StatusBaseIndex + LASTPICKUPSPEECH);
}

bool UUTCharacterVoice::InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const
{
	if (AnnouncementInfo.Switch == GetStatusIndex(StatusMessage::Incoming))
	{
		return (AnnouncementInfo.MessageClass == OtherAnnouncementInfo.MessageClass)
			|| OtherAnnouncementInfo.MessageClass->GetDefaultObject<UUTLocalMessage>()->IsOptionalSpoken(OtherAnnouncementInfo.Switch)
			|| (OtherAnnouncementInfo.MessageClass->GetDefaultObject<UUTLocalMessage>()->GetAnnouncementPriority(OtherAnnouncementInfo) < 1.f);
	}
	if ((AnnouncementInfo.MessageClass == OtherAnnouncementInfo.MessageClass) && (AnnouncementInfo.Switch >= StatusBaseIndex + KEY_CALLOUTS))
	{
		bool bNewIsFlagLocUpdate = IsFlagLocationUpdate(AnnouncementInfo.Switch);
		bool bOldIsFlagLocUpdate = IsFlagLocationUpdate(OtherAnnouncementInfo.Switch);
		if ((OtherAnnouncementInfo.Switch < StatusBaseIndex + KEY_CALLOUTS) || (bNewIsFlagLocUpdate && (bOldIsFlagLocUpdate || IsPickupUpdate(OtherAnnouncementInfo.Switch))))
		{
			return true;
		}
	}
	return false;
}

bool UUTCharacterVoice::CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	if (GetClass() == OtherMessageClass)
	{
		if (Switch < StatusBaseIndex + KEY_CALLOUTS)
		{
			return (OtherSwitch >= StatusBaseIndex + KEY_CALLOUTS);
		}
		if ((OtherSwitch < StatusBaseIndex + FIRSTGAMEVOLUMESPEECH) || (Switch < StatusBaseIndex + FIRSTGAMEVOLUMESPEECH))
		{
			return false;
		}
		if ((OtherSwitch >= StatusBaseIndex + KEY_CALLOUTS) && (OtherSwitch < StatusBaseIndex + LASTGAMEVOLUMESPEECH) && (Switch < StatusBaseIndex + LASTGAMEVOLUMESPEECH) && (OtherSwitch % 10 < 2) && (Switch % 10 < 2))
		{
			return true;
		}
		return false;
	}
	else
	{
		return (Switch < StatusBaseIndex + KEY_CALLOUTS);
	}
}

float UUTCharacterVoice::GetAnnouncementPriority(const FAnnouncementInfo AnnouncementInfo) const
{
	if (AnnouncementInfo.Switch == GetStatusIndex(StatusMessage::Incoming))
	{
		return 1.1f;
	}
	if (IsFlagLocationUpdate(AnnouncementInfo.Switch))
	{
		return 0.7f;
	}
	return (AnnouncementInfo.Switch >= StatusBaseIndex + KEY_CALLOUTS) ? 0.5f : 0.1f;
}

int32 UUTCharacterVoice::GetStatusIndex(FName NewStatus) const
{
	return StatusBaseIndex + StatusOffsets.FindRef(NewStatus);
}

