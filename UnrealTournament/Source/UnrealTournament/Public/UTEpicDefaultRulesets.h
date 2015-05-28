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
class UUTEpicDefaultRulesets : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(Config)
	TArray<FRuleCategoryData> RuleCategories;

	static void GetDefaultRules(AActor* Owner, TArray<TWeakObjectPtr<AUTReplicatedGameRuleset>>& Storage)
	{
		if (!Owner) return;

		AUTReplicatedGameRuleset* NewRuleset;
		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner,
			TEXT("Deathmatch"),
			TEXT("Deathmatch"),
			TEXT("Free For All"),
			TEXT("Standard free-for-all Deathmatch."), 
			TEXT("Standard free-for-all deathmatch.\n\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\nMaximum of 6 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-Tuba,DM-NickTest1,DM-Chill,DM-ASDF,DM-Focus,DM-Temple,DM-Lea"), 
			6, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_DM.GB_DM'"), 
			TEXT("DM"),
			TEXT("/Script/UnrealTournament.UTDMGameMode"),
			TEXT("?TimeLimit=10?GoalScore=0"),
			false);

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("BigDM"), 
			TEXT("Deathmatch"), 
			TEXT("Big Deathmatch"), 
			TEXT("Deathmatch with an insane # of players on big maps."), 
			TEXT("Deathmatch with big player counts on big maps!\n\n<UT.Hub.RulesText_Small>TimeLimit : 20 minutes</>\n\nMaximum of 16 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-DeckTest,DM-SidCastle,DM-Cannon,DM-Deadfall,DM-Spacer"), 
			16, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_LargeDM.GB_LargeDM'"), 
			TEXT("DM"),
			TEXT("/Script/UnrealTournament.UTDMGameMode"),
			TEXT("?TimeLimit=10?GoalScore=0"),
			false);

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("TDM"), 
			TEXT("TeamPlay"), 
			TEXT("Team Deathmatch"), 
			TEXT("Two teams trying to kill each other."), 
			TEXT("Team-Deathmatch pits two teams against each other!\n\n<UT.Hub.RulesText_Small>TimeLimit : 20 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 12 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-DeckTest,DM-SidCastle,DM-Cannon,DM-Deadfall,DM-Spacer"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_TDM.GB_TDM'"), 
			TEXT("TDM"),
			TEXT("/Script/UnrealTournament.UTTeamDMGameMode"),
			TEXT("?TimeLimit=20?GoalScore=0"),
			true);

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("DUEL"), 
			TEXT("Deathmatch"), 
			TEXT("Duel"), 
			TEXT("Two players enter, one player leaves!"), 
			TEXT("You against one other opponent!\n\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : OFF</>\n\nMaximum of 2 players allowed!"),
			3, 
			TEXT("DM-Outpost23,DM-Tuba,DM-DeckTest,DM-NickTest1,DM-Chill,DM-ASDF,DM-Focus,DM-Temple,DM-Lea,DM-Solo"), 
			2, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_Duel.GB_Duel'"), 
			TEXT("Duel"),
			TEXT("/Script/UnrealTournament.UTDuelGame"),
			TEXT("?TimeLimit=10?GoalScore=0"),
			false);

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("CTF"), 
			TEXT("TeamPlay"), 
			TEXT("Capture the Flag"), 
			TEXT("Two teams trying to steal each other flags."), 
			TEXT("Try to steal the enemy flag and return it home.!\n\n<UT.Hub.RulesText_Small>TimeLimit : 2x 10 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 12 players allowed!"),
			6, 
			TEXT("CTF-Blank,CTF-Outside,CTF-FaceTest,CTF-BigRock,CTF-Dam,CTF-Crashsite,CTF-Mine"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_CTF.GB_CTF'"), 
			TEXT("CTF"),
			TEXT("/Script/UnrealTournament.UTCTFGameMode"),
			TEXT("?TimeLimit=20?GoalScore=0"),
			true);

		if (NewRuleset) Storage.Add(NewRuleset);


		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("BIGCTF"), 
			TEXT("TeamPlay"), 
			TEXT("Big Capture the Flag"), 
			TEXT("Two teams trying to steal each other flags with insane player counts."), 
			TEXT("Try to steal the enemy flag and return it home.!\n\n<UT.Hub.RulesText_Small>TimeLimit : 2x 10 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 20 players allowed!"),
			6, 
			TEXT("CTF-Blank,CTF-Outside,CTF-FaceTest,CTF-BigRock,CTF-Dam,CTF-Crashsite,CTF-Mine"), 
			20, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_LargeCTF.GB_LargeCTF'"), 
			TEXT("CTF"),
			TEXT("/Script/UnrealTournament.UTCTFGameMode"),
			TEXT("?TimeLimit=20?GoalScore=0"),
			true);

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("iDM"), 
			TEXT("Instagib"), 
			TEXT("Instagib DM"), 
			TEXT("One hit one kill deathmatch."), 
			TEXT("One hit one kill Deathmatch.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n\nMaximum of 6 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-Tuba,DM-NickTest1,DM-Chill,DM-ASDF,DM-Focus,DM-Temple,DM-Lea"), 
			6, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibDM.GB_InstagibDM'"), 
			TEXT("DM"),
			TEXT("/Script/UnrealTournament.UTDMGameMode"),
			TEXT("?TimeLimit=10?GoalScore=0?Mutator=Instagib"),
			false);

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("iTDM"), 
			TEXT("Instagib"), 
			TEXT("Instagib TDM"), 
			TEXT("One hit one kill team deathmatch."), 
			TEXT("One hit one kill Deathmatch.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 20 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 6 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-SidCastle,DM-Cannon,DM-Deadfall,DM-Spacer"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibDuel.GB_InstagibDuel'"), 
			TEXT("TDM"),
			TEXT("/Script/UnrealTournament.UTTeamDMGameMode"),
			TEXT("?TimeLimit=20?GoalScore=0?Mutator=Instagib"),
			true);

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("iCTF"), 
			TEXT("Instagib"), 
			TEXT("Instagib CTF"), 
			TEXT("One hit one kill CTF."), 
			TEXT("Try to steal the enemy flag and return it home while avoiding even a single hit!\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 2x 10 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 12 players allowed!"),
			6, 
			TEXT("CTF-Blank,CTF-Outside,CTF-FaceTest,CTF-BigRock,CTF-Dam,CTF-Crashsite,CTF-Mine"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibCTF.GB_InstagibCTF'"), 
			TEXT("CTF"),
			TEXT("/Script/UnrealTournament.UTCTFGameMode"),
			TEXT("?TimeLimit=20?GoalScore=0?Mutator=Instagib"),
			true);

		if (NewRuleset) Storage.Add(NewRuleset);
	}


	static AUTReplicatedGameRuleset* AddDefaultRuleset(AActor* Owner, FString inUniqueTag, FString inCategoryList, FString inTitle, FString inTooltip, FString inDescription, int32 inMapPlaylistSize,
												FString inMapPlaylist, int32 inMaxPlayers, FString inDisplayTexture, FString inGameMode, FString inGameModeClass, FString inGameOptions, bool bTeamGame)
	{
		FActorSpawnParameters Params;
		Params.Owner = Owner;
		AUTReplicatedGameRuleset* NewReplicatedRuleset = Owner->GetWorld()->SpawnActor<AUTReplicatedGameRuleset>(Params);
		if (NewReplicatedRuleset)
		{
			NewReplicatedRuleset->UniqueTag = inUniqueTag;
			NewReplicatedRuleset->Title = inTitle;
			NewReplicatedRuleset->Tooltip = inTooltip;
			NewReplicatedRuleset->Description = inDescription;
			NewReplicatedRuleset->MapPlaylistSize = inMapPlaylistSize;
			NewReplicatedRuleset->MinPlayersToStart = 2;
			NewReplicatedRuleset->MaxPlayers = inMaxPlayers;
			NewReplicatedRuleset->DisplayTexture = inDisplayTexture;
			NewReplicatedRuleset->GameMode = inGameMode;
			NewReplicatedRuleset->GameModeClass = inGameModeClass;
			NewReplicatedRuleset->GameOptions = inGameOptions;
			NewReplicatedRuleset->bTeamGame = bTeamGame;


			TArray<FString> StrArray;
			inCategoryList.ParseIntoArray(StrArray, TEXT(","), true);
			for (int32 i=0;i<StrArray.Num();i++)
			{
				NewReplicatedRuleset->Categories.Add(FName(*StrArray[i]));
			}

			inMapPlaylist.ParseIntoArray(StrArray, TEXT(","), true);
			for (int32 i=0;i<StrArray.Num();i++)
			{
				NewReplicatedRuleset->MapPlaylist.Add(StrArray[i]);
			}

			return NewReplicatedRuleset;
		}

		return NULL;
	}


};



