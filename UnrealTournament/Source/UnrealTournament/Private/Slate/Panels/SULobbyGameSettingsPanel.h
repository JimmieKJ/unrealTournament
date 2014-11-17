// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMatchPanel.h"
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
	 *	Called when this widget needs to refresh it's options.  This is only called on clients that are not the match owner.
	 **/
	virtual void RefreshOptions();

	/**
	 *	Called to commit the local set of options.  This will replicate from the host to the server then out to the clients
	 **/
	virtual void CommitOptions();


protected:

	// Holds a cached list of player data and the slot they belong too.  
	TArray<TSharedPtr<FMatchPlayerData>> PlayerData;

	// Will be true if this is the host's setup window
	bool bIsHost;

	UPROPERTY()
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;

	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	TSharedPtr<SULobbyMatchSetupPanel> ParentPanel;

	TSharedPtr<SOverlay> PanelContents;
	TSharedPtr<SHorizontalBox> PlayerListBox;

	// Can be override in children to construct their contents.  PlayerOwner, MatchInfo and DefaultGameMode will all be filled out by now.
	virtual TSharedRef<SWidget> ConstructContents();

	virtual void BuildPlayerList(float DeltaTime);

	FOnMatchInfoOptionsChanged OnMatchOptionsChangedDelegate;

	FText GetReadyButtonText() const;
	void OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender);
	FReply Ready();

	FText GetMatchMessage() const;

};

#endif