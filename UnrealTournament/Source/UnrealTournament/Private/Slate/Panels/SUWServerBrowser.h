// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUWPanel.h"
#include "UTLocalPlayer.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTServerBeaconClient.h"

#if !UE_SERVER
/*
	Holds data about a server.  
*/

class FServerRuleData 
{
public:
	FString Rule;
	FString Value;

	FServerRuleData(FString inRule, FString inValue)
		: Rule(inRule)
		, Value(inValue)
	{};

	static TSharedRef<FServerRuleData> Make( FString inRule, FString inValue)
	{
		return MakeShareable( new FServerRuleData( inRule, inValue ) );
	}
};


class SServerRuleRow : public SMultiColumnTableRow< TSharedPtr<FServerRuleData> >
{
public:
	
	SLATE_BEGIN_ARGS( SServerRuleRow )
		: _ServerRuleData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FServerRuleData>, ServerRuleData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		ServerRuleData = InArgs._ServerRuleData;
		
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FSlateFontInfo ItemEditorFont = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Small"); //::Get().GetFontStyle(TEXT("NormalFont"));

		FText ColumnText;
		if (ServerRuleData.IsValid())
		{
			if (ColumnName == FName(TEXT("Rule"))) ColumnText = FText::FromString(ServerRuleData->Rule);
			else if (ColumnName == FName(TEXT("Value"))) ColumnText = FText::FromString(ServerRuleData->Value);
		}
		else
		{
			ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}

		return SNew( STextBlock )
			.Font( ItemEditorFont )
			.Text( ColumnText );
	}

	
private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FServerRuleData> ServerRuleData;
};


class FServerPlayerData 
{
public:
	FString PlayerName;
	FString Score;
	FString Id;

	FServerPlayerData(FString inPlayer, FString inScore, FString inId)
		: PlayerName(inPlayer)
		, Score(inScore)
		, Id(inId)
	{};

	static TSharedRef<FServerPlayerData> Make( FString inPlayerName, FString inScore, FString inId)
	{
		return MakeShareable( new FServerPlayerData( inPlayerName, inScore, inId ) );
	}
};

class SServerPlayerRow : public SMultiColumnTableRow< TSharedPtr<FServerPlayerData> >
{
public:
	
	SLATE_BEGIN_ARGS( SServerPlayerRow )
		: _ServerPlayerData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FServerPlayerData>, ServerPlayerData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		ServerPlayerData = InArgs._ServerPlayerData;
		
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FSlateFontInfo ItemEditorFont = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Small"); //::Get().GetFontStyle(TEXT("NormalFont"));

		FText ColumnText;
		if (ServerPlayerData.IsValid())
		{
			if (ColumnName == FName(TEXT("Name"))) ColumnText = FText::FromString(ServerPlayerData->PlayerName);
			else if (ColumnName == FName(TEXT("Score"))) ColumnText = FText::FromString(ServerPlayerData->Score);
		}
		else
		{
			ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}

		return SNew( STextBlock )
			.Font( ItemEditorFont )
			.Text( ColumnText );
	}

	
private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FServerPlayerData> ServerPlayerData;
};



class FServerData
{
public: 
	// The Server's name
	FString Name;

	// The Server IP Address
	FString IP;

	// The Server Beacon IP Address
	FString BeaconIP;

	// The Game mode running on the server
	FString GameMode;

	// The Map
	FString Map;

	// Number of players on this server
	int32 NumPlayers;

	// Number of Spectators
	int32 NumSpectators;

	// Max. # of players allowed on this server
	int32 MaxPlayers;

	// What UT/UE4 version the server is running
	FString Version;

	// What is the player's current ping to this server
	uint32 Ping;

	// Server Flags
	int32 Flags;
	
	TArray<TSharedPtr<FServerRuleData>> Rules;
	TArray<TSharedPtr<FServerPlayerData>> Players;

	FServerData( FString inName, FString inIP, FString inBeaconIP, FString inGameMode, FString inMap, int32 inNumPlayers, int32 inNumSpecators, int32 inMaxPlayers, FString inVersion, uint32 inPing, int32 inFlags)
	: Name( inName )
	, IP( inIP )
	, BeaconIP( inBeaconIP )
	, GameMode( inGameMode )
	, Map ( inMap )
	, NumPlayers( inNumPlayers )
	, NumSpectators( inNumSpecators )
	, MaxPlayers( inMaxPlayers )
	, Version( inVersion ) 
	, Ping( inPing )
	, Flags( inFlags )
	{		
		Rules.Empty();
		Players.Empty();
	}

	static TSharedRef<FServerData> Make( FString inName, FString inIP, FString inBeaconIP, FString inGameMode, FString inMap, int32 inNumPlayers, int32 inNumSpecators, int32 inMaxPlayers, FString inVersion, uint32 inPing, int32 inFlags)
	{
		return MakeShareable( new FServerData( inName, inIP, inBeaconIP, inGameMode, inMap, inNumPlayers, inNumSpecators, inMaxPlayers, inVersion, inPing, inFlags ) );
	}

	void AddPlayer(FString PlayerName, FString Score, FString PlayerId)
	{
		Players.Add(FServerPlayerData::Make(PlayerName, Score, PlayerId));
	}

	void AddRule(FString Rule, FString Value)
	{
		Rules.Add( FServerRuleData::Make(Rule, Value));
	}
};


/**
 * An Item Editor used by list testing.
 * It visualizes a string and edits its contents.
 */
class SServerBrowserRow : public SMultiColumnTableRow< TSharedPtr<FServerData> >
{
public:
	
	SLATE_BEGIN_ARGS( SServerBrowserRow )
		: _ServerData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FServerData>, ServerData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		ServerData = InArgs._ServerData;
		
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FSlateFontInfo ItemEditorFont = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Small"); //::Get().GetFontStyle(TEXT("NormalFont"));

		FText ColumnText;
		if (ServerData.IsValid())
		{
			if (ColumnName == FName(TEXT("ServerName"))) ColumnText = FText::FromString(ServerData->Name);
			else if (ColumnName == FName(TEXT("ServerIP"))) ColumnText = FText::FromString(ServerData->IP);
			else if (ColumnName == FName(TEXT("ServerGame"))) ColumnText = FText::FromString(ServerData->GameMode);
			else if (ColumnName == FName(TEXT("ServerMap"))) ColumnText = FText::FromString(ServerData->Map);
			else if (ColumnName == FName(TEXT("ServerVer"))) ColumnText = FText::FromString(ServerData->Version);
			else if (ColumnName == FName(TEXT("ServerNumPlayers"))) ColumnText = FText::Format(NSLOCTEXT("SUWServerBrowser","NumPlayers","{0}/{1}"), FText::AsNumber(ServerData->NumPlayers), FText::AsNumber(ServerData->MaxPlayers));
			else if (ColumnName == FName(TEXT("ServerNumSpecs"))) ColumnText = FText::AsNumber(ServerData->NumSpectators);
			else if (ColumnName == FName(TEXT("ServerPing"))) ColumnText = FText::AsNumber(ServerData->Ping);
			else if (ColumnName == FName(TEXT("ServerFlags"))) 
			{
				TSharedPtr<SHorizontalBox> IconBox;
				SAssignNew(IconBox, SHorizontalBox);
				
				if ( (ServerData->Flags & 0x01) != 0)
				{
					IconBox->AddSlot()
						[
							SNew( SImage )		
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.Lock"))
						];
				}

				return IconBox->AsShared();
			}
			
			else ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}
		else
		{
			ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}


		return SNew( STextBlock )
			.Font( ItemEditorFont )
			.Text( ColumnText );

	}

	
private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FServerData> ServerData;
};

struct FServerPingTracker 
{
	TSharedPtr<FServerData> Server;
	AUTServerBeaconClient* Beacon;

	FServerPingTracker(TSharedPtr<FServerData> inServer, AUTServerBeaconClient* inBeacon)
		: Server(inServer)
		, Beacon(inBeacon)
	{};
};

namespace BrowserState
{
	// Compressed Texture Formats
	static FName NAME_NotLoggedIn(TEXT("NotLoggedIn"));
	static FName NAME_ServerIdle(TEXT("ServerIdle"));
	static FName NAME_AuthInProgress(TEXT("AuthInProgress"));
	static FName NAME_RequestInProgress(TEXT("RequrestInProgress"));
}

class SUWServerBrowser : public SUWPanel
{
	virtual void BuildPage(FVector2D ViewportSize);	

protected:

	FName BrowserState;

	bool bShowingLobbies;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineSessionPtr OnlineSessionInterface;

	TSharedPtr<class SButton> RefreshButton;
	TSharedPtr<class SButton> SpectateButton;
	TSharedPtr<class SButton> JoinButton;
	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class STextBlock> BrowserTypeText;
	TSharedPtr<class SComboButton> GameFilter;
	TSharedPtr<class STextBlock> GameFilterText;

	// The Slate widget that displays the current list
	TSharedPtr< SListView< TSharedPtr<FServerData> > > InternetServerList;
	TSharedPtr< SListView< TSharedPtr<FServerRuleData> > > RulesList;
	TSharedPtr< SListView< TSharedPtr<FServerPlayerData> > > PlayersList;
	
	// SList doesn't allow you to change the item source after construction, so
	// we have to maintain our own list and copy.  Yech!

	TArray< TSharedPtr< FServerRuleData > > RulesListSource;
	TArray< TSharedPtr< FServerPlayerData > > PlayersListSource;

	// The list being displays.  It only includes servers that have met the filter requirement.
	TArray< TSharedPtr< FServerData > > FilteredServers;

	// When there is a server in this list, then the game will attempt to ping them.  
	TArray< TSharedPtr< FServerData > > PingList;

	// The whole list of servers
	TArray< TSharedPtr< FServerData > > InternetServers;

	// The list of lan servers
	TArray< TSharedPtr< FServerData > > LanServers;

	// A list of "pending ping requests".
	TArray< FServerPingTracker> PingTrackers;

	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedRef<ITableRow> OnGenerateWidgetForRulesList( TSharedPtr<FServerRuleData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedRef<ITableRow> OnGenerateWidgetForPlayersList( TSharedPtr<FServerPlayerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	
	virtual FReply OnRefreshClick();
	virtual void RefreshServers();

	void OnFindSessionsComplete(bool bWasSuccessful);
	virtual void SetBrowserState(FName NewBrowserState);
	virtual void CleanupQoS();

	virtual void OnListMouseButtonDoubleClick(TSharedPtr<FServerData> SelectedServer);
	virtual void OnSort(EColumnSortPriority::Type, const FName&, EColumnSortMode::Type);
	virtual FReply OnJoinClick(bool bSpectate);
	virtual void ConnectTo(FServerData ServerData,bool bSpectate);

	virtual void OnRuleSort(EColumnSortPriority::Type, const FName&, EColumnSortMode::Type);
	virtual void OnPlayerSort(EColumnSortPriority::Type, const FName&, EColumnSortMode::Type);

	virtual void AddGameFilters();
	virtual FReply OnGameFilterSelection(FString Filter);

	void SortServers(FName ColumnName);
	virtual void FilterAllServers();
	virtual void FilterServer(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate = true);

	FPlayerOnlineStatusChangedDelegate PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

	TSharedPtr<class SHeaderRow> HeaderRow;
	TSharedPtr<class SHeaderRow> RulesHeaderRow;
	TSharedPtr<class SHeaderRow> PlayersHeaderRow;

	TSharedPtr<class SEditableTextBox> QuickFilterText;
	TSharedPtr<class SBox> InternetServerBrowser;
	TSharedPtr<class SBox> LobbyBrowser;

	TSharedPtr<class SSplitter> VertSplitter;
	TSharedPtr<class SSplitter> HorzSplitter;

	bool bRequireSave;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	void OnServerListSelectionChanged(TSharedPtr<FServerData> SelectedItem, ESelectInfo::Type SelectInfo);

protected:
	virtual void OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo);
	virtual void OnServerBeaconFailure(AUTServerBeaconClient* Sender);
	virtual void PingNextServer();
	virtual void PingServer(TSharedPtr<FServerData> ServerToPing);

	virtual void VertSplitterResized();
	virtual void HorzSplitterResized();

	virtual TSharedRef<SWidget> BuildServerBrowser();
	virtual TSharedRef<SWidget> BuildLobbyBrowser();

	virtual void OnQuickFilterTextCommited(const FText& NewText, ETextCommit::Type CommitType);
	virtual FReply BrowserTypeChanged();
private:
	bool bAutoRefresh;

	bool bDescendingSort;
	bool bDescendingRulesSort;
	bool bDescendingPlayersSort;
	
	FName CurrentSortColumn;
	FName CurrentRulesSortColumn;
	FName CurrentPlayersSortColumn;

	FOnFindSessionsCompleteDelegate OnFindSessionCompleteDelegate;
	TSharedPtr<class FUTOnlineGameSearchBase> SearchSettings;

};

#endif