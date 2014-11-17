// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class FSimpleListData
{
public: 
	FString DisplayText;
	FLinearColor DisplayColor;

	FSimpleListData(FString inDisplayText, FLinearColor inDisplayColor)
		: DisplayText(inDisplayText)
		, DisplayColor(inDisplayColor)
	{
	};

	static TSharedRef<FSimpleListData> Make( FString inDisplayText, FLinearColor inDisplayColor)
	{
		return MakeShareable( new FSimpleListData( inDisplayText, inDisplayColor ) );
	}
};

class SUChatPanel : public SUWPanel
{
public:
	virtual void ConstructPanel(FVector2D ViewportSize);
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);


protected:

	// Lists of users on the server
	TArray< TSharedPtr< FSimpleListData > > UserList;
	
	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FSimpleListData> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	// The Chat destination
	virtual FText GetChatDestinationTag(FName Destination);
	FName ChatDestination;

	// Make sure to set this to true if this info panel is used in a lobby game
	bool bIsLobbyGame;

	// Make sure to set this to true if this is a team game
	bool bIsTeamGame;


	bool bUserListVisible;

	TSharedPtr<class SOverlay> NonChatPanel;

	TSharedPtr<class SButton> ChatDestinationButton;
	TSharedPtr<class SButton> UserButton;
	TSharedPtr<class SEditableTextBox> ChatText;
	TSharedPtr<class SBox> PlayerListBox;
	TSharedPtr<class SRichTextBlock> ChatDisplay;
	TSharedPtr<class SScrollBox> ChatScroller;
	TSharedPtr<class SImage> UserListTic;
	TSharedPtr<class SSplitter> Splitter;
	TSharedPtr<class SListView <TSharedPtr<FSimpleListData>>> UserListView;

	void ChatTextChanged(const FText& NewText);
	void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);
	FReply ChatDestinationChanged();
	FReply UserListToggle();
	FText GetChatButtonText() const;

	int32 LastChatCount;

	virtual void BuildNonChatPanel();
	virtual void TickNonChatPanel(float DeltaTime);
};

#endif