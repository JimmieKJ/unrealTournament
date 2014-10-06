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

class SUMidGameInfoPanel : public SUWPanel
{
	virtual void BuildPage(FVector2D ViewportSize);
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual void OnShowPanel();


protected:

	TArray< TSharedPtr< FSimpleListData > > UserList;
	
	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FSimpleListData> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	virtual FText GetDestinationTag(FName Destination);

	FName ChatDestination;

	bool bIsLobbyGame;
	bool bIsTeamGame;

	bool bUserListVisible;

	TSharedPtr<class SButton> ChatDestinationButton;
	TSharedPtr<class SButton> UserButton;
	TSharedPtr<class SEditableTextBox> ChatText;
	TSharedPtr<class SBox> PlayerListBox;
	TSharedPtr<class STextBlock> ServerName;
	TSharedPtr<class STextBlock> ServerRules;
	TSharedPtr<class STextBlock> ServerMOTD;
	TSharedPtr<class SRichTextBlock> ChatDisplay;
	TSharedPtr<class SScrollBox> ChatScroller;
	TSharedPtr<class SImage> UserListTic;
	TSharedPtr<class SSplitter> Splitter;



	void ConsoleCommand(FString Command);

	void ChatTextChanged(const FText& NewText);
	void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);
	FReply ChatDestinationChanged();
	FReply UserListToggle();
	FText GetChatButtonText();

	int32 LastChatCount;

};

#endif