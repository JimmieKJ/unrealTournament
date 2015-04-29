// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUInGameHomePanel.h"
#include "SUMatchPanel.h"
#include "UTLobbyMatchInfo.h"

#if !UE_SERVER

class FMatchData
{
public:
	TWeakObjectPtr<AUTLobbyMatchInfo> Info;
	TSharedPtr<SUMatchPanel> Panel;

	FMatchData(AUTLobbyMatchInfo* inInfo, TSharedPtr<SUMatchPanel> inPanel)
	{
		Info = inInfo;
		Panel = inPanel;
	}
	
	static TSharedRef<FMatchData> Make(AUTLobbyMatchInfo* inInfo, TSharedPtr<SUMatchPanel> inPanel)
	{
		return MakeShareable( new FMatchData( inInfo, inPanel ) );
	}
};

class FPlayerData
{
public:
	TWeakObjectPtr<AUTPlayerState> PlayerState;
	
	FPlayerData(AUTPlayerState* inPlayerState)
	{
		PlayerState = inPlayerState;
	}

	static TSharedRef<FPlayerData> Make(AUTPlayerState* inPlayerState)
	{
		return MakeShareable( new FPlayerData(inPlayerState));
	}
};



class SULobbyInfoPanel : public SUInGameHomePanel
{
public:
	virtual void ConstructPanel(FVector2D ViewportSize);

protected:

	AUTPlayerState* GetOwnerPlayerState();

	// Will be true if we are showing the match dock.  It will be set to false when the owner enters a match 
	bool bShowingMatchDock;

	// Holds the last match state if the player owner has a current match.  It's used to detect state changes and rebuild the menu as needed.
	FName LastMatchState;

	TArray<TSharedPtr<FMatchData>> MatchData;
	TSharedPtr<SHorizontalBox> MatchPanelDock;
	
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	
	virtual TSharedRef<SWidget> BuildDefaultMatchMessage();

	void OwnerCurrentMatchChanged(AUTLobbyPlayerState* LobbyPlayerState);
	bool AlreadyTrackingMatch(AUTLobbyMatchInfo* TestMatch);
	bool MatchStillValid(FMatchData* Match);

	void ShowMatchSetupPanel();
	void ShowMatchDock();

	virtual void BuildChatDestinationMenu();

	virtual void BuildMatchPanel();
	virtual TSharedRef<SWidget> BuildMatchMenu();

	virtual FReply MatchButtonClicked();
	virtual FReply ReadyButtonClicked();

	FText GetMatchButtonText() const;
	FText GetReadyButtonText() const;
	bool GetReadyButtonEnabled() const;
	TSharedRef<SWidget> BuildNewMatchButton();

	FString GetMatchCount() const;

	TSharedRef<SWidget> BuildChatArea();

	TArray< TSharedPtr< FPlayerData > > UserList;

	TSharedPtr<class SSplitter> Splitter;
	TSharedPtr<class SRichTextBlock> ChatDisplay;
	TSharedPtr<class SScrollBox> ChatScroller;
	TSharedPtr<class SListView <TSharedPtr<FPlayerData>>> UserListView;
	TSharedPtr<class SVerticalBox> MatchArea;
	TSharedPtr<STextBlock> MatchPlayerList;

	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FPlayerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	int32 LastChatCount;

	TWeakObjectPtr<AUTLobbyMatchInfo> WatchedMatch;

	virtual void UpdateUserList();
	void UpdateChatText();

	float GetReverseScale() const;

	FText GetMatchPlayerListText() const;
	FText GetServerNameText() const;

	virtual void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);

};

#endif