// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "SUWindowsStyle.h"
#include "UTServerBeaconClient.h"

#if !UE_SERVER
/*
	Holds data about a server.  
*/

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
	}

	static TSharedRef<FServerData> Make( FString inName, FString inIP, FString inBeaconIP, FString inGameMode, FString inMap, int32 inNumPlayers, int32 inNumSpecators, int32 inMaxPlayers, FString inVersion, uint32 inPing, int32 inFlags)
	{
		return MakeShareable( new FServerData( inName, inIP, inBeaconIP, inGameMode, inMap, inNumPlayers, inNumSpecators, inMaxPlayers, inVersion, inPing, inFlags ) );
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

namespace BrowserState
{
	// Compressed Texture Formats
	static FName NAME_NotLoggedIn(TEXT("NotLoggedIn"));
	static FName NAME_ServerIdle(TEXT("ServerIdle"));
	static FName NAME_AuthInProgress(TEXT("AuthInProgress"));
	static FName NAME_RequestInProgress(TEXT("RequrestInProgress"));
}

class SUWServerBrowser : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWServerBrowser)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);


protected:

	FName BrowserState;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineSessionPtr OnlineSessionInterface;

	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TSharedPtr<class SButton> RefreshButton;
	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class SComboButton> GameFilter;
	TSharedPtr<class STextBlock> GameFilterText;

	TSharedPtr< SListView< TSharedPtr<FServerData> > > InternetServerList;

	TArray< TSharedPtr< FServerData > > FilteredServers;

	TArray< TSharedPtr< FServerData > > InternetServers;
	TArray< TSharedPtr< FServerData > > LanServers;

	UPROPERTY()
	TArray< AUTServerBeaconClient* > QoSQueries;

	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	
	virtual FReply OnRefreshClick();
	virtual void RefreshServers();

	void OnFindSessionsComplete(bool bWasSuccessful);
	virtual void SetBrowserState(FName NewBrowserState);
	virtual void CleanupQoS();

	virtual void OnListMouseButtonDoubleClick(TSharedPtr<FServerData> SelectedServer);
	virtual void OnSort(EColumnSortPriority::Type, const FName&, EColumnSortMode::Type);
	virtual FReply OnJoinClick(bool bSpectate);
	virtual void ConnectTo(FServerData ServerData,bool bSpectate);

	virtual void AddGameFilters();
	virtual FReply OnGameFilterSelection(FString Filter);

	void SortServers(FName ColumnName);
	virtual void FilterServers();

	FPlayerOnlineStatusChangedDelegate PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

	TSharedPtr<class SHeaderRow> HeaderRow;
	bool bRequireSave;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

private:
	bool bAutoRefresh;
	bool bDescendingSort;
	FName CurrentSortColumn;

	FOnFindSessionsCompleteDelegate OnFindSessionCompleteDelegate;
	TSharedPtr<class FUTOnlineGameSearchBase> SearchSettings;

};

#endif