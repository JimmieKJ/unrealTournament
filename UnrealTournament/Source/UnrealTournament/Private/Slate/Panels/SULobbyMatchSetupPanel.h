// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMatchPanel.h"
#include "UTLobbyMatchInfo.h"
#include "UTLocalPlayer.h"
#include "TAttributeProperty.h"
#include "../Widgets/SUTComboButton.h"

#if !UE_SERVER

class SUWGameSetupDialog;


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


class SULobbyMatchSetupPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SULobbyMatchSetupPanel)
		: _bIsHost(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_ARGUMENT( TWeakObjectPtr<AUTLobbyMatchInfo>, MatchInfo )
		SLATE_ARGUMENT( bool, bIsHost )

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	/** We need to build the player list each frame.  */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:

	// Will be true if this is the host's setup window
	bool bIsHost;

	UPROPERTY()
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;

	// Pointer to the MatchInfo governing this panel
	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	// Holds the current Settings data.
	TSharedPtr<SVerticalBox> GameRulesPanel;

	// Builds the options that are only available to the host
	TSharedRef<SWidget> BuildHostOptionWidgets();

	// Builds the game rules panel
	void BuildGameRulesPanel();

	void JoinAnyTimeChanged(ECheckBoxState NewState);
	void AllowSpectatorChanged(ECheckBoxState NewState);
	void RankCeilingChanged(ECheckBoxState NewState);

	// Holds a cached list of player data and the slot they belong too.  
	TArray<TSharedPtr<FMatchPlayerData>> PlayerData;

	TSharedPtr<SHorizontalBox> PlayerListBox;
	virtual void BuildPlayerList(float DeltaTime);
	TSharedRef<SWidget> BuildELOBadgeForPlayer(TWeakObjectPtr<AUTPlayerState> PlayerState);

	void OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender);

	float BlinkyTimer;
	int Dots;

	TSharedPtr<STextBlock> StatusText;
	FText GetStatusText() const;

	FOnMatchInfoUpdated OnMatchInfoUpdatedDelegate;
	virtual void OnMatchInfoUpdated();

	// Screenshots..	
	/** bushes used for the map list */
	TArray<FSlateDynamicImageBrush*> MaplistScreenshots;

	FText GetMatchRulesTitle() const;
	FText GetMatchRulesDescription() const;

	TSharedRef<SWidget> AddChangeButton();
	FReply ChooseGameClicked();

	TSharedPtr<SVerticalBox> MapListPanel;
	virtual void BuildMapList();

	ECheckBoxState GetLimitRankState() const;
	ECheckBoxState GetAllowSpectatingState() const;
	ECheckBoxState GetAllowJIPState() const;

	TSharedPtr<SUWGameSetupDialog> SetupDialog;

	void OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed);
	const FSlateBrush* GetGameModeBadge() const;
};




#endif