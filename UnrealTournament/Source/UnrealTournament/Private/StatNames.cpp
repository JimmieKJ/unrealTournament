// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "StatNames.h"

const FName NAME_AttackerScore(TEXT("AttackerScore"));
const FName NAME_DefenderScore(TEXT("DefenderScore"));
const FName NAME_SupporterScore(TEXT("SupporterScore"));
const FName NAME_TeamKills(TEXT("TeamKills"));

const FName NAME_UDamageTime(TEXT("UDamageTime"));
const FName NAME_BerserkTime(TEXT("BerserkTime"));
const FName NAME_InvisibilityTime(TEXT("InvisibilityTime"));
const FName NAME_UDamageCount(TEXT("UDamageCount"));
const FName NAME_BerserkCount(TEXT("BerserkCount"));
const FName NAME_InvisibilityCount(TEXT("InvisibilityCount"));
const FName NAME_BootJumps(TEXT("BootJumps"));
const FName NAME_ShieldBeltCount(TEXT("ShieldBeltCount"));
const FName NAME_ArmorVestCount(TEXT("ArmorVestCount"));
const FName NAME_ArmorPadsCount(TEXT("ArmorPadsCount"));
const FName NAME_HelmetCount(TEXT("HelmetCount"));

const FName NAME_SkillRating(TEXT("SkillRating"));
const FName NAME_TDMSkillRating(TEXT("TDMSkillRating"));
const FName NAME_DMSkillRating(TEXT("DMSkillRating"));
const FName NAME_CTFSkillRating(TEXT("CTFSkillRating"));

const FName NAME_SkillRatingSamples(TEXT("SkillRatingSamples"));
const FName NAME_TDMSkillRatingSamples(TEXT("TDMSkillRatingSamples"));
const FName NAME_DMSkillRatingSamples(TEXT("DMSkillRatingSamples"));
const FName NAME_CTFSkillRatingSamples(TEXT("CTFSkillRatingSamples"));

const FName NAME_MatchesPlayed(TEXT("MatchesPlayed"));
const FName NAME_MatchesQuit(TEXT("MatchesQuit"));
const FName NAME_TimePlayed(TEXT("TimePlayed"));
const FName NAME_Wins(TEXT("Wins"));
const FName NAME_Losses(TEXT("Losses"));
const FName NAME_Kills(TEXT("Kills"));
const FName NAME_Deaths(TEXT("Deaths"));
const FName NAME_Suicides(TEXT("Suicides"));

const FName NAME_MultiKillLevel0(TEXT("MultiKillLevel0"));
const FName NAME_MultiKillLevel1(TEXT("MultiKillLevel1"));
const FName NAME_MultiKillLevel2(TEXT("MultiKillLevel2"));
const FName NAME_MultiKillLevel3(TEXT("MultiKillLevel3"));

const FName NAME_SpreeKillLevel0(TEXT("SpreeKillLevel0"));
const FName NAME_SpreeKillLevel1(TEXT("SpreeKillLevel1"));
const FName NAME_SpreeKillLevel2(TEXT("SpreeKillLevel2"));
const FName NAME_SpreeKillLevel3(TEXT("SpreeKillLevel3"));
const FName NAME_SpreeKillLevel4(TEXT("SpreeKillLevel4"));

const FName NAME_ImpactHammerKills(TEXT("ImpactHammerKills"));
const FName NAME_EnforcerKills(TEXT("EnforcerKills"));
const FName NAME_BioRifleKills(TEXT("BioRifleKills"));
const FName NAME_ShockBeamKills(TEXT("ShockBeamKills"));
const FName NAME_ShockCoreKills(TEXT("ShockCoreKills"));
const FName NAME_ShockComboKills(TEXT("ShockComboKills"));
const FName NAME_LinkKills(TEXT("LinkKills"));
const FName NAME_LinkBeamKills(TEXT("LinkBeamKills"));
const FName NAME_MinigunKills(TEXT("MinigunKills"));
const FName NAME_MinigunShardKills(TEXT("MinigunShardKills"));
const FName NAME_FlakShardKills(TEXT("FlakShardKills"));
const FName NAME_FlakShellKills(TEXT("FlakShellKills"));
const FName NAME_RocketKills(TEXT("RocketKills"));
const FName NAME_SniperKills(TEXT("SniperKills"));
const FName NAME_SniperHeadshotKills(TEXT("SniperHeadshotKills"));
const FName NAME_RedeemerKills(TEXT("RedeemerKills"));
const FName NAME_InstagibKills(TEXT("InstagibKills"));
const FName NAME_TelefragKills(TEXT("TelefragKills"));

const FName NAME_ImpactHammerDeaths(TEXT("ImpactHammerDeaths"));
const FName NAME_EnforcerDeaths(TEXT("EnforcerDeaths"));
const FName NAME_BioRifleDeaths(TEXT("BioRifleDeaths"));
const FName NAME_ShockBeamDeaths(TEXT("ShockBeamDeaths"));
const FName NAME_ShockCoreDeaths(TEXT("ShockCoreDeaths"));
const FName NAME_ShockComboDeaths(TEXT("ShockComboDeaths"));
const FName NAME_LinkDeaths(TEXT("LinkDeaths"));
const FName NAME_LinkBeamDeaths(TEXT("LinkBeamDeaths"));
const FName NAME_MinigunDeaths(TEXT("MinigunDeaths"));
const FName NAME_MinigunShardDeaths(TEXT("MinigunShardDeaths"));
const FName NAME_FlakShardDeaths(TEXT("FlakShardDeaths"));
const FName NAME_FlakShellDeaths(TEXT("FlakShellDeaths"));
const FName NAME_RocketDeaths(TEXT("RocketDeaths"));
const FName NAME_SniperDeaths(TEXT("SniperDeaths"));
const FName NAME_SniperHeadshotDeaths(TEXT("SniperHeadshotDeaths"));
const FName NAME_RedeemerDeaths(TEXT("RedeemerDeaths"));
const FName NAME_InstagibDeaths(TEXT("InstagibDeaths"));
const FName NAME_TelefragDeaths(TEXT("TelefragDeaths"));

const FName NAME_PlayerXP(TEXT("PlayerXP"));

const FName NAME_BestShockCombo(TEXT("BestShockCombo"));
const FName NAME_AmazingCombos(TEXT("AmazingCombos"));
const FName NAME_AirRox(TEXT("AirRox"));
const FName NAME_FlakShreds(TEXT("FlakShreds"));
const FName NAME_AirSnot(TEXT("AirSnot"));

const FName NAME_RunDist(TEXT("RunDist"));
const FName NAME_SprintDist(TEXT("SprintDist"));
const FName NAME_InAirDist(TEXT("InAirDist"));
const FName NAME_SwimDist(TEXT("SwimDist"));
const FName NAME_TranslocDist(TEXT("TranslocDist"));
const FName NAME_NumDodges(TEXT("NumDodges"));
const FName NAME_NumWallDodges(TEXT("NumWallDodges"));
const FName NAME_NumJumps(TEXT("NumJumps"));
const FName NAME_NumLiftJumps(TEXT("NumLiftJumps"));
const FName NAME_NumFloorSlides(TEXT("NumFloorSlides"));
const FName NAME_NumWallRuns(TEXT("NumWallRuns"));
const FName NAME_NumImpactJumps(TEXT("NumImpactJumps"));
const FName NAME_NumRocketJumps(TEXT("NumRocketJumps"));
const FName NAME_SlideDist(TEXT("SlideDist"));
const FName NAME_WallRunDist(TEXT("WallRunDist"));

const FName NAME_EnforcerShots(TEXT("EnforcerShots"));
const FName NAME_BioRifleShots(TEXT("BioRifleShots"));
const FName NAME_ShockRifleShots(TEXT("ShockRifleShots"));
const FName NAME_LinkShots(TEXT("LinkShots"));
const FName NAME_MinigunShots(TEXT("MinigunShots"));
const FName NAME_FlakShots(TEXT("FlakShots"));
const FName NAME_RocketShots(TEXT("RocketShots"));
const FName NAME_SniperShots(TEXT("SniperShots"));
const FName NAME_RedeemerShots(TEXT("RedeemerShots"));
const FName NAME_InstagibShots(TEXT("InstagibShots"));

const FName NAME_EnforcerHits(TEXT("EnforcerHits"));
const FName NAME_BioRifleHits(TEXT("BioRifleHits"));
const FName NAME_ShockRifleHits(TEXT("ShockRifleHits"));
const FName NAME_LinkHits(TEXT("LinkHits"));
const FName NAME_MinigunHits(TEXT("MinigunHits"));
const FName NAME_FlakHits(TEXT("FlakHits"));
const FName NAME_RocketHits(TEXT("RocketHits"));
const FName NAME_SniperHits(TEXT("SniperHits"));
const FName NAME_RedeemerHits(TEXT("RedeemerHits"));
const FName NAME_InstagibHits(TEXT("InstagibHits"));

