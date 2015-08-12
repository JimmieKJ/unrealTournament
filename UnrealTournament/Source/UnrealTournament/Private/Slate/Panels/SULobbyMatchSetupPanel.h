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

class UNREALTOURNAMENT_API SULobbyMatchSetupPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SULobbyMatchSetupPanel)
		: _bIsHost(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_ARGUMENT( TWeakObjectPtr<AUTLobbyMatchInfo>, MatchInfo )
		SLATE_ARGUMENT( bool, bIsHost )
		SLATE_EVENT(FOnRulesetUpdated, OnRulesetUpdated);

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

	float BlinkyTimer;
	int32 Dots;

	TSharedPtr<STextBlock> StatusTextBlock;
	FText GetStatusText() const;

	FOnMatchInfoUpdated OnMatchInfoUpdatedDelegate;
	virtual void OnMatchInfoUpdated();

	FOnRulesetUpdated OnRulesetUpdatedDelegate;
	virtual void OnRulesetUpdated();

	// Screenshots..	
	/** bushes used for the map list */
	FSlateDynamicImageBrush* MapScreenshot;

	FText GetMatchRulesTitle() const;
	FText GetMatchRulesDescription() const;

	TSharedRef<SWidget> AddChangeButton();
	TSharedRef<SWidget> AddActionButtons();
	bool CanChooseGame() const;
	FReply ChooseGameClicked();

	ECheckBoxState GetLimitRankState() const;
	ECheckBoxState GetAllowSpectatingState() const;
	ECheckBoxState GetAllowJIPState() const;

	TSharedPtr<SUWGameSetupDialog> SetupDialog;

	void OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed);
	const FSlateBrush* GetGameModeBadge() const;

	FReply CancelDownloadClicked();
	EVisibility CancelButtonVisible() const;

	const FSlateBrush* SULobbyMatchSetupPanel::GetMapImage() const;
	FSlateDynamicImageBrush* DefaultLevelScreenshot;
protected:
	TWeakObjectPtr<AUTReplicatedMapInfo> LastMapInfo;

	FText GetStartMatchText() const;
	FText GetMapName() const;
	FReply StartMatchClicked();
	FReply LeaveMatchClicked();

	FOnRulesetUpdated RulesetUpdatedDelegate;

};




#endif