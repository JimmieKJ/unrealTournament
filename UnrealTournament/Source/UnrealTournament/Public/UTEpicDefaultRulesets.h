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
			TEXT("Casual"), 
			TEXT("Deathmatch"), 
			TEXT("Standard free-for-all Deathmatch."), 
			TEXT("Standard free-for-all deathmatch.\n\n<UT.Hub.RulesText_Small>FragLimit : 30</>\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 6 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-Tuba,DM-NickTest1,DM-Chill,DM-ASDF,DM-Focus,DM-Temple,DM-Lea"), 
			6, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_DM.GB_DM'"), 
			TEXT("DM"),
			TEXT("?GoalScore=30?TimeLimit=10"));

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("BigDM"), 
			TEXT("Big"), 
			TEXT("Big Deathmatch"), 
			TEXT("Deathmatch with an insane # of players on big maps."), 
			TEXT("Deathmatch with big player counts on big maps!\n\n<UT.Hub.RulesText_Small>FragLimit : 50</>\n<UT.Hub.RulesText_Small>TimeLimit : 15 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 16 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-SidCastle,DM-Cannon,DM-Deadfall,DM-Spacer"), 
			16, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_LargeDM.GB_LargeDM'"), 
			TEXT("DM"),
			TEXT("?GoalScore=50?TimeLimit=15"));

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("TDM"), 
			TEXT("TeamPlay"), 
			TEXT("Team Deathmatch"), 
			TEXT("Two teams trying to kill each other."), 
			TEXT("Team-Deathmatch pits two teams against each other!\n\n<UT.Hub.RulesText_Small>FragLimit : 100</>\n<UT.Hub.RulesText_Small>TimeLimit : 15 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 12 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-SidCastle,DM-Cannon,DM-Deadfall,DM-Spacer"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_TDM.GB_TDM'"), 
			TEXT("TDM"),
			TEXT("?GoalScore=100?TimeLimit=15"));

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("DUEL"), 
			TEXT("Competitive"), 
			TEXT("Duel"), 
			TEXT("Two players enter, one player leaves!"), 
			TEXT("You against one other opponent!\n\n<UT.Hub.RulesText_Small>FragLimit : 20</>\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : OFF</>\n\nMaximum of 2 players allowed!"),
			3, 
			TEXT("DM-asdOutpost23,DM-Tuba,DM-NickTest1,DM-Chill,DM-ASDF,DM-Focus,DM-Temple,DM-Lea"), 
			2, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_Duel.GB_Duel'"), 
			TEXT("Duel"),
			TEXT("?GoalScore=20?TimeLimit=10"));

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("CTF"), 
			TEXT("Casual,TeamPlay"), 
			TEXT("Capture the Flag"), 
			TEXT("Two teams trying to steal each other flags."), 
			TEXT("Try to steal the enemy flag and return it home.!\n\n<UT.Hub.RulesText_Small>TimeLimit : 2x 7 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 12 players allowed!"),
			6, 
			TEXT("CTF-Blank,CTF-Outside,CTF-FaceTest,CTF-BigRock,CTF-Dam,CTF-Crashsite"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_CTF.GB_CTF'"), 
			TEXT("CTF"),
			TEXT("?TimeLimit=14"));

		if (NewRuleset) Storage.Add(NewRuleset);


		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("BIGCTF"), 
			TEXT("Big"), 
			TEXT("Big Capture the Flag"), 
			TEXT("Two teams trying to steal each other flags with insane player counts."), 
			TEXT("Try to steal the enemy flag and return it home.!\n\n<UT.Hub.RulesText_Small>TimeLimit : 2x 7 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 20 players allowed!"),
			6, 
			TEXT("CTF-Blank,CTF-Outside,CTF-FaceTest,CTF-BigRock,CTF-Dam,CTF-Crashsite"), 
			20, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_LargeCTF.GB_LargeCTF'"), 
			TEXT("CTF"),
			TEXT("?TimeLimit=14"));

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("iDM"), 
			TEXT("Instagib"), 
			TEXT("Instagib DM"), 
			TEXT("One hit one kill deathmatch."), 
			TEXT("One hit one kill Deathmatch.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>FragLimit : 30</>\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 6 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-Tuba,DM-NickTest1,DM-Chill,DM-ASDF,DM-Focus,DM-Temple,DM-Lea"), 
			6, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibDM.GB_InstagibDM'"), 
			TEXT("DM"),
			TEXT("?GoalScore=30?TimeLimit=10?Mutator=Instagib"));

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("iTDM"), 
			TEXT("Instagib"), 
			TEXT("Instagib TDM"), 
			TEXT("One hit one kill team deathmatch."), 
			TEXT("One hit one kill Deathmatch.\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>FragLimit : 30</>\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 6 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-SidCastle,DM-Cannon,DM-Deadfall,DM-Spacer"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibDuel.GB_InstagibDuel'"), 
			TEXT("TDM"),
			TEXT("?GoalScore=100?TimeLimit=15?Mutator=Instagib"));

		if (NewRuleset) Storage.Add(NewRuleset);

		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("iDuel"), 
			TEXT("Competitive,Instagib"), 
			TEXT("Instagib Duel"), 
			TEXT("Two players enter, one player leaves -- Instagib style."), 
			TEXT("You against one other opponent!\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>FragLimit : 20</>\n<UT.Hub.RulesText_Small>TimeLimit : 10 minutes</>\n<UT.Hub.RulesText_Small>Mercy Rule : OFF</>\n\nMaximum of 2 players allowed!"),
			6, 
			TEXT("DM-Outpost23,DM-SidCastle,DM-Cannon,DM-Deadfall,DM-Spacer"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibDuel.GB_InstagibDuel'"), 
			TEXT("Duel"),
			TEXT("?GoalScore=20?TimeLimit=10?Mutator=Instagib"));

		if (NewRuleset) Storage.Add(NewRuleset);


		NewRuleset = UUTEpicDefaultRulesets::AddDefaultRuleset(Owner, 
			TEXT("iCTF"), 
			TEXT("Instagib"), 
			TEXT("Instgib CTF"), 
			TEXT("One hit one kill CTF."), 
			TEXT("Try to steal the enemy flag and return it home while avoiding even a single hit!\n\n<UT.Hub.RulesText_Small>Mutators : Instagib</>\n<UT.Hub.RulesText_Small>TimeLimit : 2x 7 minute halfs</>\n<UT.Hub.RulesText_Small>Mercy Rule : On</>\n\nMaximum of 12 players allowed!"),
			6, 
			TEXT("CTF-Blank,CTF-Outside,CTF-FaceTest,CTF-BigRock,CTF-Dam,CTF-Crashsite"), 
			12, 
			TEXT("Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_InstagibCTF.GB_InstagibCTF'"), 
			TEXT("CTF"),
			TEXT("?TimeLimit=14?Mutator=Instagib"));

		if (NewRuleset) Storage.Add(NewRuleset);
	}


	static AUTReplicatedGameRuleset* AddDefaultRuleset(AActor* Owner, FString inUniqueTag, FString inCategoryList, FString inTitle, FString inTooltip, FString inDescription, int32 inMapPlaylistSize,
												FString inMapPlaylist, int32 inMaxPlayers, FString inDisplayTexture, FString inGameMode, FString inGameOptions)
	{
		FActorSpawnParameters Params;
		Params.Name = FName(*inUniqueTag);
		Params.Owner = Owner;
		AUTReplicatedGameRuleset* NewReplicatedRuleset = Owner->GetWorld()->SpawnActor<AUTReplicatedGameRuleset>(Params);
		if (NewReplicatedRuleset)
		{
			NewReplicatedRuleset->UniqueTag = inUniqueTag;
			NewReplicatedRuleset->Title = inTitle;
			NewReplicatedRuleset->Tooltip = inTooltip;
			NewReplicatedRuleset->Description = inDescription;
			NewReplicatedRuleset->MapPlaylistSize = inMapPlaylistSize;
			NewReplicatedRuleset->MaxPlayers = inMaxPlayers;
			NewReplicatedRuleset->DisplayTexture = inDisplayTexture;
			NewReplicatedRuleset->GameMode = inGameMode;
			NewReplicatedRuleset->GameOptions = inGameOptions;

			TArray<FString> StrArray;
			inCategoryList.ParseIntoArray(&StrArray, TEXT(","), true);
			for (int i=0;i<StrArray.Num();i++)
			{
				NewReplicatedRuleset->Categories.Add(FName(*StrArray[i]));
			}

			inMapPlaylist.ParseIntoArray(&StrArray, TEXT(","), true);
			for (int i=0;i<StrArray.Num();i++)
			{
				NewReplicatedRuleset->MapPlaylist.Add(StrArray[i]);
			}

			return NewReplicatedRuleset;
		}

		return NULL;
	}


};



