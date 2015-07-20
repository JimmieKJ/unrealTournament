// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedGameRuleset.h"
#include "UTReplicatedMapInfo.h"
#include "Net/UnrealNetwork.h"

#if !UE_SERVER
#include "Slate/SUWindowsStyle.h"
#endif

AUTReplicatedGameRuleset::AUTReplicatedGameRuleset(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;
	bNetLoadOnClient = false;
}

void AUTReplicatedGameRuleset::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTReplicatedGameRuleset, UniqueTag);
	DOREPLIFETIME(AUTReplicatedGameRuleset, Categories);
	DOREPLIFETIME(AUTReplicatedGameRuleset, Title);
	DOREPLIFETIME(AUTReplicatedGameRuleset, Tooltip);
	DOREPLIFETIME(AUTReplicatedGameRuleset, Description);
	DOREPLIFETIME(AUTReplicatedGameRuleset, MaxMapsInList);
	DOREPLIFETIME(AUTReplicatedGameRuleset, MapList);
	DOREPLIFETIME(AUTReplicatedGameRuleset, MinPlayersToStart);
	DOREPLIFETIME(AUTReplicatedGameRuleset, MaxPlayers);
	DOREPLIFETIME(AUTReplicatedGameRuleset, OptimalPlayers);
	DOREPLIFETIME(AUTReplicatedGameRuleset, DisplayTexture);
	DOREPLIFETIME(AUTReplicatedGameRuleset, bCustomRuleset);
	DOREPLIFETIME(AUTReplicatedGameRuleset, GameMode);
	DOREPLIFETIME(AUTReplicatedGameRuleset, bTeamGame);
}


int32 AUTReplicatedGameRuleset::AddMapAssetToMapList(const FAssetData& Asset)
{

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		AUTReplicatedMapInfo* MapInfo = GameState->CreateMapInfo(Asset);
		return MapList.Add(MapInfo);
	}

	return INDEX_NONE;
}


void AUTReplicatedGameRuleset::SetRules(UUTGameRuleset* NewRules, const TArray<FAssetData>& MapAssets)
{

	UniqueTag			= NewRules->UniqueTag;
	Categories			= NewRules->Categories;
	Title				= NewRules->Title;
	Tooltip				= NewRules->Tooltip;
	Description			= Fixup(NewRules->Description);
	MinPlayersToStart	= NewRules->MinPlayersToStart;
	MaxPlayers			= NewRules->MaxPlayers;
	bTeamGame			= NewRules->bTeamGame;



	MaxMapsInList = NewRules->MaxMapsInList;

	// First add the Epic maps.

	if (!NewRules->EpicMaps.IsEmpty())
	{
		TArray<FString> EpicMapList;
		NewRules->EpicMaps.ParseIntoArray(EpicMapList,TEXT(","), true);
		for (int32 i = 0 ; i < EpicMapList.Num(); i++)
		{
			FString MapName = EpicMapList[i];
			if ( !MapName.IsEmpty() )
			{
				if ( FPackageName::IsShortPackageName(MapName) )
				{
					FPackageName::SearchForPackageOnDisk(MapName, &MapName); 
				}

				// Look for the map in the asset registry...

				for (const FAssetData& Asset : MapAssets)
				{
					FString AssetPackageName = Asset.PackageName.ToString();
					if ( MapName.Equals(AssetPackageName, ESearchCase::IgnoreCase) )
					{
						// Found the asset data for this map.  Build the FMapListInfo.
						AddMapAssetToMapList(Asset);
						break;
					}
				}
			}
		}
	}

	// Now add the custom maps..

	for (int32 i = 0; i < NewRules->CustomMapList.Num(); i++)
	{
		//if (MaxMapsInList > 0 && MapList.Num() >= MaxMapsInList) break;

		// Look for the map in the asset registry...

		for (const FAssetData& Asset : MapAssets)
		{
			FString AssetPackageName = Asset.PackageName.ToString();
			if ( NewRules->CustomMapList[i].MapName.Equals(AssetPackageName, ESearchCase::IgnoreCase) )
			{
				// Found the asset data for this map.  Build the FMapListInfo.
				int32 Idx = AddMapAssetToMapList(Asset);
				MapList[Idx]->Redirect = NewRules->CustomMapList[i].Redirect;
				break;
			}
		}
	}

	RequiredPackages = NewRules->RequiredPackages;

	DisplayTexture = NewRules->DisplayTexture;
	GameMode = NewRules->GameMode;
	GameOptions = NewRules->GameOptions;

	BuildSlateBadge();

}

FString AUTReplicatedGameRuleset::Fixup(FString OldText)
{
	FString Final = OldText.Replace(TEXT("\\n"), TEXT("\n"), ESearchCase::IgnoreCase);
	Final = Final.Replace(TEXT("\\n"), TEXT("\n"), ESearchCase::IgnoreCase);

	return Final;
}


void AUTReplicatedGameRuleset::BuildSlateBadge()
{
#if !UE_SERVER
	BadgeTexture = LoadObject<UTexture2D>(nullptr, *DisplayTexture, nullptr, LOAD_None, nullptr);
	SlateBadge = MakeShareable( new FSlateDynamicImageBrush(BadgeTexture, FVector2D(256.0f, 256.0f), NAME_None) );
#endif
}

#if !UE_SERVER
const FSlateBrush* AUTReplicatedGameRuleset::GetSlateBadge() const
{
	if (SlateBadge.IsValid()) 
	{
		return SlateBadge.Get();
	}
	else
	{
		return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");	
	}
}
#endif

void AUTReplicatedGameRuleset::GotTag()
{
}

AUTGameMode* AUTReplicatedGameRuleset::GetDefaultGameModeObject()
{
	if (!GameMode.IsEmpty())
	{
		UClass* GModeClass = LoadClass<AUTGameMode>(NULL, *GameMode, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
		if (GModeClass)
		{
			AUTGameMode* DefaultGameModeObject = GModeClass->GetDefaultObject<AUTGameMode>();
			return DefaultGameModeObject;
		}
	}

	return NULL;
}