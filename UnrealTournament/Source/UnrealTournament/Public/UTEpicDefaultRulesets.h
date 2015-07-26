// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTReplicatedGameRuleset.h"
#include "UTEpicDefaultRulesets.generated.h"

// Eventually, this class needs to be a file pulled from the MCP so we can update
// our rulesets on the fly.

USTRUCT()
struct FRuleCategoryData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName CategoryName;

	UPROPERTY()
	FString CategoryButtonText;

	FRuleCategoryData()
		: CategoryName(NAME_None)
		, CategoryButtonText(TEXT(""))
	{
	}
};

UCLASS(Config=Rules)
class UNREALTOURNAMENT_API UUTEpicDefaultRulesets : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(Config)
	TArray<FRuleCategoryData> RuleCategories;

	// Holds the complete list of rules allowed in a Hub.  
	UPROPERTY(Config)
	TArray<FString> AllowedRulesets;

	static void GetEpicRulesets(TArray<FString>& Rules)
	{
		Rules.Add(EEpicDefaultRuleTags::Deathmatch);
		Rules.Add(EEpicDefaultRuleTags::BigDM);
		Rules.Add(EEpicDefaultRuleTags::TDM);
		Rules.Add(EEpicDefaultRuleTags::DUEL);
		Rules.Add(EEpicDefaultRuleTags::SHOWDOWN);
		Rules.Add(EEpicDefaultRuleTags::CTF);
		Rules.Add(EEpicDefaultRuleTags::BIGCTF);
		Rules.Add(EEpicDefaultRuleTags::iDM);
		Rules.Add(EEpicDefaultRuleTags::iTDM);
		Rules.Add(EEpicDefaultRuleTags::iCTF);
		Rules.Add(EEpicDefaultRuleTags::iCTFT);
	}

	static void InsureEpicDefaults(UUTGameRuleset* NewRuleset)
	{
		// TODO: This should pull from a file that is pushed from the MCP if the MCP is available


		if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::Deathmatch, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Deathmatch"));

			NewRuleset->Title = TEXT("Deathmatch");
			NewRuleset->Tooltip = TEXT("Standard free-for-all Deathmatch.");
			NewRuleset->Description = TEXT("Standard free-for-all deathmatch.\n\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Maximum players : 6</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 6;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_DM.GB_DM'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTDMGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=10?GoalScore=0");
			NewRuleset->bTeamGame = false;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/DM-Outpost23";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Spacer";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Vortex";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Cannon";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Chill";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-SidCastle";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Deadfall";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Temple";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Focus";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Overlord";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-NickTest1";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Solo";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Decktest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Tuba";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/EpicInternal/Lea/DM-Lea";

			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/DM-Outpost23";

		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::BigDM, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Deathmatch"));

			NewRuleset->Title = TEXT("Big Deathmatch");
			NewRuleset->Tooltip = TEXT("Deathmatch with large player counts on big maps.");
			NewRuleset->Description = TEXT("Deathmatch with large player counts on big maps.\n\n<UT.Hub.RulesText_Small>TimeLimit : 20 minutes</>\n<UT.Hub.RulesText_Small>Maximum players: 16</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 16;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_LargeDM.GB_LargeDM'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTDMGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=10?GoalScore=0");
			NewRuleset->bTeamGame = false;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/DM-Outpost23";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Spacer";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Vortex";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Cannon";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Chill";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-SidCastle";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Deadfall";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Temple";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Focus";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Overlord";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Decktest";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/DM-Outpost23";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::TDM, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("TeamPlay"));

			NewRuleset->Title = TEXT("Team Deathmatch");
			NewRuleset->Tooltip = TEXT("Red versus blue team deathmatch.");
			NewRuleset->Description = TEXT("Red versus blue team deathmatch.\n\n<UT.Hub.RulesText_Small>TimeLimit : 20 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n<UT.Hub.RulesText_Small>Maximum players: 10</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 10;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_TDM.GB_TDM'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTTeamDMGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=20?GoalScore=0");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/DM-Outpost23";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Spacer";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Vortex";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Cannon";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Chill";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-SidCastle";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Deadfall";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Temple";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Focus";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Overlord";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-NickTest1";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Solo";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Decktest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Tuba";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/DM-Outpost23";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::DUEL, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Deathmatch"));

			NewRuleset->Title = TEXT("Duel");
			NewRuleset->Tooltip = TEXT("One vs one test of deathmatch skill.");
			NewRuleset->Description = TEXT("One vs one test of deathmatch skill.\n\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : OFF</>\n<UT.Hub.RulesText_Small>Maximum players: 2</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 2;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_Duel.GB_Duel'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTDuelGame");
			NewRuleset->GameOptions = TEXT("?TimeLimit=10?GoalScore=0");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps =							"/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps +	",/Game/RestrictedAssets/Maps/WIP/DM-Solo";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Decktest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Tuba";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::SHOWDOWN, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Deathmatch"));

			NewRuleset->Title = TEXT("Showdown");
			NewRuleset->Tooltip = TEXT("New School one vs one test of deathmatch skill.");
			NewRuleset->Description = TEXT("New School one vs one test of deathmatch skill.\n\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : OFF</>\n<UT.Hub.RulesText_Small>Maximum players : 2</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 2;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_Duel.GB_Duel'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTShowdownGame");
			NewRuleset->GameOptions = TEXT("?Timelimit=2?GoalScore=5");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Solo";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Decktest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Tuba";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::CTF, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("TeamPlay"));

			NewRuleset->Title = TEXT("Capture the Flag");
			NewRuleset->Tooltip = TEXT("Capture the Flag.");
			NewRuleset->Description = TEXT("Capture the Flag, with guns.\n\n<UT.Hub.RulesText_Small>TimeLimit : 2x 10 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n<UT.Hub.RulesText_Small>Maximum players : 10</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 10;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_CTF.GB_CTF'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTCTFGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=20?GoalScore=0");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/WIP/CTF-Outside";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Blank";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-BigRock";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Dam";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-FaceTest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Volcano";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Lance";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Mine";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-CrashSite";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/WIP/CTF-Outside";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::BIGCTF, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("TeamPlay"));

			NewRuleset->Title = TEXT("Big Capture the Flag");
			NewRuleset->Tooltip = TEXT("Capture the Flag with large teams.");
			NewRuleset->Description = TEXT("Capture the Flag with large teams.\n\n<UT.Hub.RulesText_Small>TimeLimit : 2x 10 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n<UT.Hub.RulesText_Small>Maximum players : 20</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 20;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_LargeCTF.GB_LargeCTF'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTCTFGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=20?GoalScore=0");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps =							"/Game/RestrictedAssets/Maps/WIP/CTF-BigRock";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps +	",/Game/RestrictedAssets/Maps/WIP/CTF-Dam";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps +	",/Game/RestrictedAssets/Maps/WIP/CTF-FaceTest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Volcano";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Lance";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-CrashSite";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/WIP/CTF-BigRock";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::iDM, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Instagib"));

			NewRuleset->Title = TEXT("Instagib DM");
			NewRuleset->Tooltip = TEXT("One hit one kill Deathmatch.");
			NewRuleset->Description = TEXT("One hit one kill Deathmatch.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Maximum players : 10</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 10;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibDM.GB_InstagibDM'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTDMGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=10?GoalScore=0?Mutator=Instagib");
			NewRuleset->bTeamGame = false;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/DM-Outpost23";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Spacer";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Vortex";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Cannon";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Chill";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-SidCastle";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Deadfall";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Temple";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Focus";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Overlord";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-NickTest1";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Solo";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Decktest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Tuba";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/DM-Outpost23";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::iTDM, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Instagib"));

			NewRuleset->Title = TEXT("Instagib TDM");
			NewRuleset->Tooltip = TEXT("One hit one kill Team Deathmatch.");
			NewRuleset->Description = TEXT("One hit one kill Team Deathmatch.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 20 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n<UT.Hub.RulesText_Small>Maximum players : 16</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 16;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibDuel.GB_InstagibDuel'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTTeamDMGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=20?GoalScore=0?Mutator=Instagib");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/DM-Outpost23";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Spacer";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Vortex";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Cannon";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Chill";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-SidCastle";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Deadfall";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Temple";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Focus";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Overlord";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-NickTest1";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Solo";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Decktest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-Tuba";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/DM-ASDF";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/DM-Outpost23";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::iCTF, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Instagib"));

			NewRuleset->Title = TEXT("Instagib CTF");
			NewRuleset->Tooltip = TEXT("Instagib CTF");
			NewRuleset->Description = TEXT("Capture the Flag with Instagib rifles.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 2x 10 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\n<UT.Hub.RulesText_Small>Maximum players : 16</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 16;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibCTF.GB_InstagibCTF'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTCTFGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=20?GoalScore=0?Mutator=Instagib");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/WIP/CTF-Outside";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Blank";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-BigRock";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Dam";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-FaceTest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Volcano";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Lance";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Mine";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-CrashSite";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/WIP/CTF-Outside";
		}
		else if (NewRuleset->UniqueTag.Equals(EEpicDefaultRuleTags::iCTFT, ESearchCase::IgnoreCase))
		{
			NewRuleset->Categories.Empty(); 
			NewRuleset->Categories.Add(TEXT("Instagib"));

			NewRuleset->Title = TEXT("Translocator iCTF");
			NewRuleset->Tooltip = TEXT("Translocator iCTF");
			NewRuleset->Description = TEXT("Capture the Flag with Instagib rifles and Translocators.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 2x 10 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n<UT.Hub.RulesText_Small>Maximum players : 16</>");
			NewRuleset->MinPlayersToStart = 2;
			NewRuleset->MaxPlayers = 16;
			NewRuleset->DisplayTexture = TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibCTF.GB_InstagibCTF'");
			NewRuleset->GameMode = TEXT("/Script/UnrealTournament.UTCTFGameMode");
			NewRuleset->GameOptions = TEXT("?TimeLimit=20?GoalScore=0?Mutator=Instagib,AddTrans");
			NewRuleset->bTeamGame = true;

			NewRuleset->MaxMapsInList=16;

			NewRuleset->EpicMaps = "/Game/RestrictedAssets/Maps/WIP/CTF-Outside";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Blank";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-BigRock";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Dam";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-FaceTest";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Volcano";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Lance";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-Mine";
			NewRuleset->EpicMaps = NewRuleset->EpicMaps + ",/Game/RestrictedAssets/Maps/WIP/CTF-CrashSite";
			NewRuleset->DefaultMap = "/Game/RestrictedAssets/Maps/WIP/CTF-Outside";
		}
	}
};



