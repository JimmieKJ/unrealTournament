// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "SUMidGameInfoPanel.h"


#if !UE_SERVER

struct FCompareUsers
{
	FORCEINLINE bool operator()	( const TSharedPtr< FSimpleListData > A, const TSharedPtr< FSimpleListData > B ) const 
	{
		if (A->DisplayColor != B->DisplayColor)
		{
			if (A->DisplayColor != FLinearColor::Gray && B->DisplayColor == FLinearColor::Gray) return true;
			if (A->DisplayColor == FLinearColor::Gray && B->DisplayColor != FLinearColor::Gray) return false;
			return (A->DisplayColor == FLinearColor::Red);
		}
		return ( A->DisplayText> B->DisplayText);
	}
};


void SUMidGameInfoPanel::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	LastChatCount=0;
	AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	bIsTeamGame = (GS != NULL && GS->bTeamGame);

	bIsLobbyGame =  PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>() != NULL;

	ChatDestination = bIsLobbyGame ? ChatDestinations::Global : ChatDestinations::Local;

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()		// Server Info, MOTD
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()			// MOTD and Chat text
				.Padding(10,10,10,10)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[

					SNew(SVerticalBox)

					+SVerticalBox::Slot()		// Server Title
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					[
						SAssignNew(ServerName,STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.ServerTitle")
					]

					+SVerticalBox::Slot()		// Server Rules
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					[
						SAssignNew(ServerRules,STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.RulesText")
					]

					+SVerticalBox::Slot()		// Server MOTD
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					[
						SAssignNew(ServerMOTD,STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.GeneralText")
					]

					+SVerticalBox::Slot()		// Chat List
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SAssignNew( ChatList, SListView< TSharedPtr<FStoredChatMessage> > )
						// Tell the list view where to get its source data
						.ListItemsSource( &PlayerOwner->ChatArchive)
						// When the list view needs to generate a widget for some data item, use this method
						.OnGenerateRow( this, &SUMidGameInfoPanel::OnGenerateWidgetForChat )
						.SelectionMode(ESelectionMode::None)
					]

				]

				+SHorizontalBox::Slot()		// Player List
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Right)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UWindows.MidGameMenu.UserList.Background"))
					]
					+SOverlay::Slot()
					.Padding(10.0f)
					[
						SAssignNew(PlayerListBox,SBox)
						.WidthOverride(150)
						[
								SNew( SListView< TSharedPtr<FSimpleListData> > )
								// List view items are this tall
								.ItemHeight(24)
								// Tell the list view where to get its source data
								.ListItemsSource( &UserList)
								// When the list view needs to generate a widget for some data item, use this method
								.OnGenerateRow( this, &SUMidGameInfoPanel::OnGenerateWidgetForList )
								.SelectionMode(ESelectionMode::Single)
						]
					]
				]
			]

			+ SVerticalBox::Slot()		// Chat Entry
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SOverlay)

						+SOverlay::Slot()
						[
							SNew(SBox)
							.HeightOverride(31)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.MidGameMenu.Chatbar.Background"))
							]
						]
						+SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
								.Text(FText::FromString(TEXT(" >")))
							]
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Fill)
							[
								SAssignNew(ChatText, SEditableTextBox)
								.Style(SUWindowsStyle::Get(), "UWindows.MidGame.ChatEditBox")
								.OnTextChanged(this, &SUMidGameInfoPanel::ChatTextChanged)
								.OnTextCommitted(this, &SUMidGameInfoPanel::ChatTextCommited)
								.ClearKeyboardFocusOnCommit(false)
								.MinDesiredWidth(300.0f)
								.Text(FText::GetEmpty())
							]
						]
					]
							
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.AutoWidth()
					[
						SAssignNew(ChatDestinationButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.MidGame.ChatBarButton.First")
						.ContentPadding(FMargin(20.0f, 5.0f, 20.0f, 5.0))
						.Text(GetChatButtonText())
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
						.OnClicked(this, &SUMidGameInfoPanel::ChatDestinationChanged)
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.MidGame.ChatBarButton")
						.ContentPadding(FMargin(10.0f, 5.0f))
						.Text(NSLOCTEXT("Generic", "Game", "Users").ToString())
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
						.OnClicked(this, &SUMidGameInfoPanel::UserListToggle)
					]
				]
			]
		];
}

void SUMidGameInfoPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Clear the current user list
	UserList.Empty();

	// Grab a list of users
	AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS)
	{
		for (int32 i=0; i<GS->PlayerArray.Num();i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
			if (PS != NULL)
			{
				if (PS->bIsSpectator)
				{
					UserList.Add( FSimpleListData::Make(PS->PlayerName, FLinearColor::Gray));
				}
				else
				{
					if (bIsTeamGame)
					{
						UserList.Add( FSimpleListData::Make(PS->PlayerName, (PS->GetTeamNum() == 1 ? FLinearColor(0.75f, 0.83f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.83f, 0.75f, 1.0f))));
					}
					else
					{
						UserList.Add( FSimpleListData::Make(PS->PlayerName, FLinearColor::White));
					}
				}
			}
		}

		UserList.Sort(FCompareUsers());

		ServerName->SetText(GS->ServerName);
		ServerMOTD->SetText(GS->ServerMOTD);
		ServerRules->SetText(GS->ServerRules());
	}

	if (PlayerOwner->ChatArchive.Num() != LastChatCount)
	{
		LastChatCount = PlayerOwner->ChatArchive.Num();
		ChatList->RequestListRefresh();
		if (LastChatCount-1 < PlayerOwner->ChatArchive.Num())
		{
			ChatList->RequestScrollIntoView(PlayerOwner->ChatArchive[LastChatCount-1]);
		}
	}

}

TSharedRef<ITableRow> SUMidGameInfoPanel::OnGenerateWidgetForList( TSharedPtr<FSimpleListData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	FSlateFontInfo Font = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Chat");
	return SNew(STableRow<TSharedPtr<FSimpleListData>>, OwnerTable)
		.Style(SUWindowsStyle::Get(),"UWindows.Standard.MidGame.UserList.Row")
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(InItem->DisplayColor)
			.Font(Font)
			.Text(InItem->DisplayText)

		];
}

FText SUMidGameInfoPanel::GetDestinationTag(FName Destination)
{
	if (Destination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "TeamTag","Team");
	if (Destination == ChatDestinations::Local)		return NSLOCTEXT("Chat", "LocalTag","Local");
	if (Destination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "FriendsTag","Friends");
	if (Destination == ChatDestinations::Match)		return NSLOCTEXT("Chat", "MatchTag","Match");
	if (Destination == ChatDestinations::Lobby)		return NSLOCTEXT("Chat", "LobbyTag","Lobby");
	
	return NSLOCTEXT("Chat", "GlobalTag","Global");

}

TSharedRef<ITableRow> SUMidGameInfoPanel::OnGenerateWidgetForChat( TSharedPtr<FStoredChatMessage> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	FSlateFontInfo Font = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Medium");

	FString Message = FString::Printf(TEXT("[%s] %s"), *GetDestinationTag(InItem->Type).ToString(), *InItem->Message);

	return SNew(STableRow<TSharedPtr<FStoredChatMessage>>, OwnerTable)
		//.Style(SUWindowsStyle::Get(),"UWindows.Standard.MidGame.UserList.Row")
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(InItem->Color)
			.Font(Font)
			.Text(Message)
		];
}




void SUMidGameInfoPanel::ConsoleCommand(FString Command)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
	{
		PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
	}
}


FReply SUMidGameInfoPanel::ChatDestinationChanged()
{
	if (bIsLobbyGame)
	{
		if (ChatDestination == ChatDestinations::Global) 
		{
			ChatDestination = ChatDestinations::Match;
		}
		else if (ChatDestination == ChatDestinations::Match)
		{
			ChatDestination = ChatDestinations::Friends;
		}
		else if (ChatDestination == ChatDestinations::Friends)
		{
			ChatDestination = ChatDestinations::Global;
		}
	}
	else
	{

		if (bIsTeamGame)
		{
			ChatDestination = ChatDestination == ChatDestinations::Local ? ChatDestinations::Team : ChatDestinations::Local;
		}

/*
	
		// When friends chat is implemented, reenable this block.  For now, only switch if it's a team game
		if (ChatDestination == ChatDestinations::Local) 
		{
			ChatDestination = (bIsTeamGame) ? ChatDestinations::Team : ChatDestinations::Friends;
		}

		else if (ChatDestination == ChatDestinations::Team)
		{
			ChatDestination = ChatDestinations::Friends;
		}

		// Add code to see we are on a game server instance and allow you to chat to the lobby

		else if (ChatDestination == ChatDestinations::Friends)
		{
			ChatDestination = ChatDestinations::Local;
		}
*/		
	}

	ChatDestinationButton->SetContent( SNew(STextBlock).Text(GetChatButtonText()).TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle"));

	return FReply::Handled();
}	

void SUMidGameInfoPanel::ChatTextChanged(const FText& NewText)
{
}

void SUMidGameInfoPanel::ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		FString FianlText = NewText.ToString();
		// Figure out the type of chat...
		if (FianlText != TEXT(""))
		{
			if (ChatDestination == ChatDestinations::Global)	ConsoleCommand(FString::Printf(TEXT("GlobalSay %s"), *FianlText));
			if (ChatDestination == ChatDestinations::Friends)	ConsoleCommand(FString::Printf(TEXT("FriendSay %s"), *FianlText));
			if (ChatDestination == ChatDestinations::Lobby)		ConsoleCommand(FString::Printf(TEXT("LobbySay %s"), *FianlText));
			if (ChatDestination == ChatDestinations::Local)		ConsoleCommand(FString::Printf(TEXT("Say %s"), *FianlText));
			if (ChatDestination == ChatDestinations::Match)		ConsoleCommand(FString::Printf(TEXT("MatchSay %s"), *FianlText));
			if (ChatDestination == ChatDestinations::Team)		ConsoleCommand(FString::Printf(TEXT("TeamSay %s"), *FianlText));
		}

		ChatText->SetText(FText::GetEmpty());
	}
}

FText SUMidGameInfoPanel::GetChatButtonText()
{
	if (ChatDestination == ChatDestinations::Match)			return NSLOCTEXT("Chat", "Match", "Match Chat");
	else if (ChatDestination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "Friends", "Friends Only");
	else if (ChatDestination == ChatDestinations::Local)	return NSLOCTEXT("Chat", "Local", "Local Chat");
	else if (ChatDestination == ChatDestinations::Lobby)	return NSLOCTEXT("Chat", "Lobby", "Lobby Chat");
	else if (ChatDestination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "Team", "Team Chat");
	else													return NSLOCTEXT("Chat", "GlobalChat", "Global Chat");
}

FReply SUMidGameInfoPanel::UserListToggle()
{
	PlayerListBox->SetVisibility( PlayerListBox->GetVisibility() ==  EVisibility::Visible ? EVisibility::Collapsed :  EVisibility::Visible);
	return FReply::Handled();
}

#endif