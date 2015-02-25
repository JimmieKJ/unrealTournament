// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMatchPanel.h"
#include "SNumericEntryBox.h"
#include "UTLobbyMatchInfo.h"
#include "UTLocalPlayer.h"
#include "UTGameMode.h"
#include "SULobbyMatchSetupPanel.h"
#include "../Widgets/SUTComboButton.h"

#if !UE_SERVER

class AUTGameMode;


class FMatchPlayerData
{
public:
	TWeakObjectPtr<AUTLobbyPlayerState> PlayerState;
	TSharedPtr<SWidget> Button;
	TSharedPtr<SUTComboButton> ComboButton;
	TSharedPtr<SImage> ReadyImage;

	FMatchPlayerData(TWeakObjectPtr<AUTLobbyPlayerState> inPlayerState, TSharedPtr<SWidget> inButton, TSharedPtr<SUTComboButton> inComboButton, TSharedPtr<SImage> inReadyImage )
		: PlayerState(inPlayerState)
		, Button(inButton)
		, ComboButton(inComboButton)
		, ReadyImage(inReadyImage)
	{}

	static TSharedRef<FMatchPlayerData> Make(TWeakObjectPtr<AUTLobbyPlayerState> inPlayerState, TSharedPtr<SWidget> inButton, TSharedPtr<SUTComboButton> inComboButton, TSharedPtr<SImage> inReadyImage)
	{
		return MakeShareable( new FMatchPlayerData(inPlayerState, inButton, inComboButton, inReadyImage));
	}
};

class SULobbyGameSettingsPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SULobbyGameSettingsPanel)
		: _bIsHost(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_ARGUMENT( TWeakObjectPtr<AUTLobbyMatchInfo>, MatchInfo )
		SLATE_ARGUMENT( TWeakObjectPtr<AUTGameMode>, DefaultGameMode )
		SLATE_ARGUMENT( bool, bIsHost )

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	/** We need to build the player list each frame.  */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	/**
	 *	MapChanged is called on non-match host clients when their match map changees
	 **/
	virtual void MapChanged();

	/**
	 *	OptionsChanged is called on non-match host clients when their match options change
	 **/
	virtual void OptionsChanged();

	/**
	 *	Called to commit the local set of options.  This will replicate from the host to the server then out to the clients
	 **/
	virtual void CommitOptions();


protected:

	/** brush for drawing the level shot */
	FSlateDynamicImageBrush* LevelScreenshot;

	TArray< TSharedPtr<TAttributePropertyBase> > GameConfigProps;

	// Takes the MatchInfo->MatchOptions string and builds the needed widgets from it.
	virtual void BuildOptionsFromData();

	// Holds a cached list of player data and the slot they belong too.  
	TArray<TSharedPtr<FMatchPlayerData>> PlayerData;

	// Will be true if this is the host's setup window
	bool bIsHost;

	UPROPERTY()
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;

	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	TSharedPtr<SULobbyMatchSetupPanel> ParentPanel;
	TSharedPtr<SVerticalBox> OptionsPanel;
	TSharedPtr<SVerticalBox> MapPanel;

	TSharedPtr<SOverlay> PanelContents;
	TSharedPtr<SHorizontalBox> PlayerListBox;

	TSharedRef<SWidget> BuildMapsPanel();
	TSharedRef<SWidget> BuildMapsList();

	TSharedPtr<SListView< TSharedPtr<FAllowedMapData> >> MapList;

	// Can be override in children to construct their contents.  PlayerOwner, MatchInfo and DefaultGameMode will all be filled out by now.
	virtual TSharedRef<SWidget> ConstructContents();

	// Builds the game panel (Game Options and Maps)
	TSharedRef<SWidget> BuildGamePanel();

	// Build the Play List
	virtual void BuildPlayerList(float DeltaTime);

	FOnMatchInfoMapChanged OnMatchMapChangedDelegate;
	FOnMatchInfoOptionsChanged OnMatchOptionsChangedDelegate;

	void OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender);

	FText GetMatchMessage() const;

	float BlinkyTimer;
	int Dots;

	TSharedPtr<STextBlock> StatusText;
	FString GetMapText() const;

	virtual void OnGameOptionChanged();
	TSharedRef<ITableRow> GenerateMapListWidget(TSharedPtr<FAllowedMapData> InItem, const TSharedRef<STableViewBase>& OwningList);
	void OnMapListChanged(TSharedPtr<FAllowedMapData> SelectedItem, ESelectInfo::Type SelectInfo);

	TSharedRef<SWidget> BuildELOBadgeForPlayer(TWeakObjectPtr<AUTPlayerState> PlayerState);

};

#endif