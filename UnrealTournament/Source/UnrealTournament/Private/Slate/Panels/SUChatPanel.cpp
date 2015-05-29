

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "SUMidGameInfoPanel.h"
#include "SUChatPanel.h"


#if !UE_SERVER

AUTPlayerState* SUChatPanel::GetOwnerPlayerState()
{
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
	if (PC) 
	{
		return Cast<AUTPlayerState>(PC->PlayerState);
	}
	return NULL;
}


struct FCompareUsersChat
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


void SUChatPanel::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("ChatPanel"));

	float Perc = 150 / ViewportSize.X;

	LastChatCount=0;
	AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	bIsTeamGame = (GS != NULL && GS->bTeamGame);

	bIsLobbyGame =  PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>() != NULL;
	bUserListVisible = true;

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()		
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SAssignNew( Splitter, SSplitter )
				.Orientation(Orient_Horizontal)

				// Left side of the spliter is the chat and non-chat areas

				+SSplitter::Slot()
				.Value(1.0f - Perc)
				[
					SNew(SVerticalBox)

					// First slot is the non-chat panel.  It will be filled out later.
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(NonChatPanel, SOverlay)
					]

					// Next slot is the actual chat.,  
					+SVerticalBox::Slot()		
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.Padding(20,36,8,16)
					[
						SAssignNew(ChatScroller, SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(ChatDisplay, SRichTextBlock)
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Chat.Text.Global")
							.Justification(ETextJustify::Left)
							.DecoratorStyleSet( &SUWindowsStyle::Get() )
							.AutoWrapText( true )
						]
					]
				]

				// Right side of the splitter is the player list

				+SSplitter::Slot()
				.Value(Perc)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush(PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.UserList.Background")))
					]
					+SOverlay::Slot()
					.Padding(10.0f)
					[
						SAssignNew(PlayerListBox,SBox)
						.WidthOverride(150)
						[
								SAssignNew( UserListView, SListView< TSharedPtr<FSimpleListData> > )
								// List view items are this tall
								.ItemHeight(24)
								// Tell the list view where to get its source data
								.ListItemsSource( &UserList)
								// When the list view needs to generate a widget for some data item, use this method
								.OnGenerateRow( this, &SUChatPanel::OnGenerateWidgetForList )
								.SelectionMode(ESelectionMode::Single)
						]
					]
				]
			]

			// Chat Entry area
			+ SVerticalBox::Slot()		
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

						// Background
						+SOverlay::Slot()
						[
							SNew(SBox)
							.HeightOverride(31)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush(PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Chatbar.Background")))
							]
						]


						// Chat text enter box and label
						+SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(115.0,0.0,0.0,0.0)
							.AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
								.Text(FText::FromString(TEXT(" >")))
							]
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Fill)
							[
								SAssignNew(ChatText, SEditableTextBox)
								.Style(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.ChatEditBox"))
								.OnTextChanged(this, &SUMidGameInfoPanel::ChatTextChanged)
								.OnTextCommitted(this, &SUMidGameInfoPanel::ChatTextCommited)
								.ClearKeyboardFocusOnCommit(false)
								.MinDesiredWidth(300.0f)
								.Text(FText::GetEmpty())
							]
						]
					]
					
					// Chat destination button
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.AutoWidth()
					[
						SAssignNew(ChatDestinationButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.ChatBarButton.First"))
						.ContentPadding(FMargin(20.0f, 5.0f, 20.0f, 5.0))
						.Text(this, &SUChatPanel::GetChatButtonText)
						.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
						.OnClicked(this, &SUMidGameInfoPanel::ChatDestinationChanged)
					]

					// The User List toggle
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.AutoWidth()
					[
						SAssignNew(UserButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.ChatBarButton"))
						.ContentPadding(FMargin(10.0f, 5.0f))
						.OnClicked(this, &SUMidGameInfoPanel::UserListToggle)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Fill)
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Generic", "Game", "Users"))
								.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
							]

							+SHorizontalBox::Slot()
							.Padding(14,0,14,0)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Right)
							.AutoWidth()
							[
								SNew(SBox)
								.WidthOverride(8)
								.HeightOverride(4)
								[
									SAssignNew(UserListTic, SImage)
									.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DownTick"))
								]
							]
						]
					]
				]
			]
		];


	BuildNonChatPanel();		
	PlayerListBox->SetVisibility( EVisibility::Visible );
}

void SUChatPanel::BuildNonChatPanel()
{
}

void SUChatPanel::TickNonChatPanel(float DeltaTime)
{
}


void SUChatPanel::SortUserList()
{
	UserList.Sort(FCompareUsersChat());
}

void SUChatPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
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

		SortUserList();
		UserListView->RequestListRefresh();
	}

	if (PlayerOwner->ChatArchive.Num() != LastChatCount)
	{
		LastChatCount = PlayerOwner->ChatArchive.Num();

		FString RichText = TEXT("");
		for (int32 i=0;i<PlayerOwner->ChatArchive.Num(); i++)
		{
			TSharedPtr<FStoredChatMessage> Msg = PlayerOwner->ChatArchive[i];
			FString Style;

			if (i>0) RichText += TEXT("\n");

			if (Msg->Type == ChatDestinations::Friends)
			{
				Style = TEXT("UWindows.Chat.Text.Friends");
			}
			else if (Msg->Type == ChatDestinations::System)
			{
				Style = TEXT("UWindows.Chat.Text.Admin");
			}
			else if (Msg->Type == ChatDestinations::Lobby)
			{
				Style = TEXT("UWindows.Chat.Text.Lobby");
			}
			else if (Msg->Type == ChatDestinations::Match)
			{
				Style = TEXT("UWindows.Chat.Text.Match");
			}
			else if (Msg->Type == ChatDestinations::Local)
			{
				Style = TEXT("UWindows.Chat.Text.Local");
			}
			else if (Msg->Type == ChatDestinations::Team)
			{
				if (Msg->Color.R > Msg->Color.B)
				{
					Style = TEXT("UWindows.Chat.Text.Team.Red"); ;
				}
				else
				{
					Style = TEXT("UWindows.Chat.Text.Team.Blue"); ;
				}
			}
			else
			{
				Style = TEXT("UWindows.Chat.Text.Global"); 
			}

			RichText += FString::Printf(TEXT("<%s>[%s]%s</>"), *Style, *GetChatDestinationTag(Msg->Type).ToString(), *Msg->Message);
		}

		ChatDisplay->SetText(FText::FromString(RichText));
		ChatScroller->SetScrollOffset(290999);
	}


	FVector2D ViewportSize;
	PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
	float SlotSize;
	SlotSize = Splitter->SlotAt(1).SizeValue.Get();
	if (SlotSize < (75 / ViewportSize.X))
	{
		if (!bUserListVisible)
		{
			UserListTic->SetImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.UpTick"));
		}
		bUserListVisible = true;
	}
	else
	{
		if (bUserListVisible)
		{
			UserListTic->SetImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DownTick"));
		}
		bUserListVisible = false;
	}

	TickNonChatPanel(InDeltaTime);

	if (GetOwnerPlayerState() != nullptr)
	{
		FName CurrentChatDestination = GetOwnerPlayerState()->ChatDestination;
		if (LastChatDestination != CurrentChatDestination)
		{
			LastChatDestination = CurrentChatDestination;
			ChatDestinationButton->SetContent( SNew(STextBlock).Text(GetChatButtonText()).TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle")));
		}
	}
}

TSharedRef<ITableRow> SUChatPanel::OnGenerateWidgetForList( TSharedPtr<FSimpleListData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	FSlateFontInfo Font = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Chat");
	return SNew(STableRow<TSharedPtr<FSimpleListData>>, OwnerTable)
		.Style(SUWindowsStyle::Get(),PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.UserList.Row"))
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(InItem->DisplayColor)
			.Font(Font)
			.Text(FText::FromString(InItem->DisplayText))

		];
}

FText SUChatPanel::GetChatDestinationTag(FName Destination)
{
	if (Destination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "TeamTag","Team");
	if (Destination == ChatDestinations::Local)		return NSLOCTEXT("Chat", "LocalTag","Local");
	if (Destination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "FriendsTag","Friends");
	if (Destination == ChatDestinations::Match)		return NSLOCTEXT("Chat", "MatchTag","Match");
	if (Destination == ChatDestinations::Lobby)		return NSLOCTEXT("Chat", "LobbyTag","Lobby");
	
	return NSLOCTEXT("Chat", "GlobalTag","Global");
}


FReply SUChatPanel::ChatDestinationChanged()
{
	GetOwnerPlayerState()->ServerNextChatDestination();
	return FReply::Handled();
}	

void SUChatPanel::ChatTextChanged(const FText& NewText)
{
	if (NewText.ToString().Len() > 128)
	{
		ChatText->SetText(FText::FromString(NewText.ToString().Left(128)));
	}
}

void SUChatPanel::ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	FName ChatDestination = GetOwnerPlayerState() ? GetOwnerPlayerState()->ChatDestination : ChatDestinations::Global;

	if (CommitType == ETextCommit::OnEnter)
	{
		FString FinalText = NewText.ToString();
		// Figure out the type of chat...
		if (FinalText != TEXT(""))
		{
			if (ChatDestination == ChatDestinations::Global)	ConsoleCommand(FString::Printf(TEXT("GlobalChat %s"), *FinalText));
			if (ChatDestination == ChatDestinations::Friends)	ConsoleCommand(FString::Printf(TEXT("FriendSay %s"), *FinalText));
			if (ChatDestination == ChatDestinations::Lobby)		ConsoleCommand(FString::Printf(TEXT("LobbySay %s"), *FinalText));
			if (ChatDestination == ChatDestinations::Local)		ConsoleCommand(FString::Printf(TEXT("Say %s"), *FinalText));
			if (ChatDestination == ChatDestinations::Match)		ConsoleCommand(FString::Printf(TEXT("MatchChat %s"), *FinalText));
			if (ChatDestination == ChatDestinations::Team)		ConsoleCommand(FString::Printf(TEXT("TeamSay %s"), *FinalText));
		}

		ChatText->SetText(FText::GetEmpty());
	}
}

FText SUChatPanel::GetChatButtonText() const
{
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
	if (PC) 
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PC->PlayerState);
		if (PS)
		{
			FName ChatDestination = PS->ChatDestination;

			if		(ChatDestination == ChatDestinations::Match)	return NSLOCTEXT("Chat", "Match", "Match Chat");
			else if (ChatDestination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "Friends", "Friends Only");
			else if (ChatDestination == ChatDestinations::Local)	return NSLOCTEXT("Chat", "Local", "Local Chat");
			else if (ChatDestination == ChatDestinations::Lobby)	return NSLOCTEXT("Chat", "Lobby", "Lobby Chat");
			else if (ChatDestination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "Team", "Team Chat");
		}
	}
	return NSLOCTEXT("Chat", "GlobalChat", "Global Chat");
}

FReply SUChatPanel::UserListToggle()
{
	FVector2D ViewportSize;
	PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
	float SlotSize;
	SlotSize = Splitter->SlotAt(1).SizeValue.Get();
	if (SlotSize < (75 / ViewportSize.X))
	{
		Splitter->SlotAt(1).Value(150 / ViewportSize.X);
	}
	else
	{
		Splitter->SlotAt(1).Value(20 / ViewportSize.X);
	}

	return FReply::Handled();
}

void SUChatPanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);
	FSlateApplication::Get().SetKeyboardFocus(ChatText, EKeyboardFocusCause::Keyboard);
}


#endif