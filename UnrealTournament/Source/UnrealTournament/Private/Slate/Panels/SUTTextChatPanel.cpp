
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsStyle.h"
#include "../Widgets/SUTChatEditBox.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"
#include "SUTTextChatPanel.h"
#include "UTLobbyPC.h"
#include "UTLobbyPlayerState.h"
#include "SUTPlayerListPanel.h"
#include "Engine/Console.h"

#if !UE_SERVER

struct FComparePlayerByName		{FORCEINLINE bool operator()( const TSharedPtr< FTeamListTracker > A, const TSharedPtr< FTeamListTracker > B ) const {return ( A->PlayerState->PlayerName < B->PlayerState->PlayerName);}};

SUTTextChatPanel::~SUTTextChatPanel()
{
	PlayerOwner->RemoveChatArchiveChangedDelegate(RouteChatHande);
}

void SUTTextChatPanel::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	
	RouteChatHande = PlayerOwner->RegisterChatArchiveChangedDelegate( FChatArchiveChanged::FDelegate::CreateSP(this, &SUTTextChatPanel::RouteChat));
	ChatDestinationChangedDelegate = InArgs._OnChatDestinationChanged;
	
	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Medium"))
		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(1.0)
				[
					SNew(SBox)
					.HeightOverride(55)
					[
						SAssignNew(ChatDestinationBar, SHorizontalBox)
					]
				]
			]
			+SVerticalBox::Slot().FillHeight(1.0)
			.Padding(FMargin(5.0f,5.0f,5.0f,5.0f))
			[
				SAssignNew(ChatScrollBox, SScrollBox)
			]
			+SVerticalBox::Slot().AutoHeight()
			.Padding(FMargin(10.0f,5.0f,10.0f,10.0f))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox).HeightOverride(3)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(ChatSlot, SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f,5.0f,0.0f,0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT(">")))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox).HeightOverride(3)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Light"))
					]
				]
			]

			+SVerticalBox::Slot().AutoHeight()
			.Padding(FMargin(5.0f,5.0f,5.0f,5.0f))
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					SAssignNew(TeamListContainer, SBox)
					.Visibility(this, &SUTTextChatPanel::TeamListVisible)
					.HeightOverride(400.0f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SBorder)
							.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
							[
								SNew(SScrollBox)
								+SScrollBox::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().Padding(0.0f, 10.0f, 0.0f, 5.0f)
									.AutoHeight()
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot().FillWidth(0.5f).Padding(15.0f,0.0f,15.0f,0.0f)
										[
											SNew(SBorder)
											.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.DeepRed"))
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Left)
												[
													SNew(STextBlock)
													.Text(NSLOCTEXT("SUTTextChatPanel","RedTeam","RED TEAM"))
													.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
												]
											]
										]
										+SHorizontalBox::Slot().FillWidth(0.5f).Padding(15.0f,0.0f,15.0f,0.0f)
										[
											SNew(SBorder)
											.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Navy"))
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Left)
												[
													SNew(STextBlock)
													.Text(NSLOCTEXT("SUTTextChatPanel","BlueTeam","BLUE TEAM"))
													.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
												]
											]
										]
									]
									+SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot().FillWidth(0.5f).Padding(15.0f,0.0f,15.0f,0.0f)
										[
							 				SAssignNew( RedListview, SListView< TSharedPtr<FTeamListTracker> > )
							 				// List view items are this tall
							 				.ItemHeight(48)
							 				// Tell the list view where to get its source data
							 				.ListItemsSource( &RedList)
							 				// When the list view needs to generate a widget for some data item, use this method
							 				.OnGenerateRow( this, &SUTTextChatPanel::OnGenerateRedWidget)
							 				.OnMouseButtonDoubleClick(this, &SUTTextChatPanel::OnListSelect)
											.SelectionMode(ESelectionMode::Single)
										]
										+SHorizontalBox::Slot().FillWidth(0.5f).Padding(15.0f,0.0f,15.0f,0.0f)
										[
							 				SAssignNew( BlueListview, SListView< TSharedPtr<FTeamListTracker> > )
							 				// List view items are this tall
							 				.ItemHeight(48)
							 				// Tell the list view where to get its source data
							 				.ListItemsSource( &BlueList)
							 				// When the list view needs to generate a widget for some data item, use this method
							 				.OnGenerateRow( this, &SUTTextChatPanel::OnGenerateBlueWidget)
							 				.OnMouseButtonDoubleClick(this, &SUTTextChatPanel::OnListSelect)
											.SelectionMode(ESelectionMode::Single)
										]
									]

									+SVerticalBox::Slot().Padding(0.0f, 20.0f, 0.0f, 5.0f)
									.AutoHeight()
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Medium"))
										[
											SNew(SHorizontalBox)
											+SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(NSLOCTEXT("SUTTextChatPanel","SpecTeam","SPECTATORS"))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
											]
										]
									]

									+SVerticalBox::Slot()
									.AutoHeight()
									[
							 			SAssignNew( SpectatorListview, SListView< TSharedPtr<FTeamListTracker> > )
							 			// List view items are this tall
							 			.ItemHeight(48)
							 			// Tell the list view where to get its source data
							 			.ListItemsSource( &SpectatorList)
							 			// When the list view needs to generate a widget for some data item, use this method
							 			.OnGenerateRow( this, &SUTTextChatPanel::OnGenerateSpectatorWidget)
							 			//.OnMouseButtonDoubleClick(this, &SUTPlayerListPanel::OnListSelect)
										.SelectionMode(ESelectionMode::Single)
									]
								]
							]
						]
					]
				]
			]

		]
	];
}

void SUTTextChatPanel::OnShowPanel()
{
	TSharedPtr<SUTChatEditBox>ChatWidget = PlayerOwner->GetChatWidget();
	if (ChatWidget.IsValid() && ChatSlot.IsValid())
	{
		ChatSlot->AddSlot()
		.FillWidth(1.0)
		.Padding(5.0)
		[
			ChatWidget.ToSharedRef()
		];

		ChatWidget->SetCommittedDelegate(FOnTextCommitted::CreateSP(this,&SUTTextChatPanel::ChatTextCommited));
	}
}
void SUTTextChatPanel::OnHidePanel()
{
	TSharedPtr<SUTChatEditBox>ChatWidget = PlayerOwner->GetChatWidget();
	if (ChatWidget.IsValid() && ChatSlot.IsValid())
	{
		ChatSlot->RemoveSlot(ChatWidget.ToSharedRef());
	}

}


void SUTTextChatPanel::AddDestination(const FText& Caption, const FName ChatDestionation, float Weight, bool bSelect)
{
	bool bFirstButton = ChatDestinationList.Num() == 0;

	int32 InsertIndex = INDEX_NONE;

	for (int32 i = 0; i < ChatDestinationList.Num(); i++)
	{
		// Look to see if we have already found it.
		if (ChatDestinationList[i].IsValid() && ChatDestinationList[i]->ChatDestination == ChatDestionation) return;
		if (ChatDestinationList[i]->Weight > Weight)
		{
			InsertIndex = i;
			// NOTE: let the loop finish in case this tab was added with a different weight
		}
	}

	TSharedPtr<FChatDestination> Dest = FChatDestination::Make(Caption, ChatDestionation, Weight);

	if (Dest.IsValid())
	{
		if (InsertIndex == INDEX_NONE)
		{
			InsertIndex = ChatDestinationList.Num();
			ChatDestinationList.Add(Dest);
		}
		else
		{
			ChatDestinationList.Insert(Dest, InsertIndex);
		}

		TSharedPtr<SUTButton> Button;

		SAssignNew(Button, SUTButton)
			.IsToggleButton(true)
			.OnClicked(this, &SUTTextChatPanel::OnDestinationClick, Dest)
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(Dest.Get(), &FChatDestination::GetButtonCaption) ) )
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
			.CaptionHAlign(HAlign_Center)
			.ButtonStyle(SUTStyle::Get(), "UT.TabButton");

		TSharedPtr<SVerticalBox> ChatBox;
		SAssignNew(ChatBox, SVerticalBox);

		ChatDestinationList[InsertIndex]->ChatBox = ChatBox;
		ChatDestinationList[InsertIndex]->Button = Button;
	}

	BuildChatDestinationBar();

	if (bFirstButton || bSelect)
	{
		DestinationChanged(ChatDestionation);
	}

}

void SUTTextChatPanel::RemoveDestination(const FName Destination)
{
	for (int32 i = 0; i < ChatDestinationList.Num(); i++)
	{
		if (ChatDestinationList[i].IsValid() && ChatDestinationList[i]->ChatDestination == Destination)
		{
			bool bPickNew = (ChatDestinationList[i]->Button->IsPressed());
			ChatDestinationList.RemoveAt(i);

			BuildChatDestinationBar();

			if (bPickNew && ChatDestinationList.Num() > 0)
			{
				DestinationChanged(ChatDestinationList[0]->ChatDestination);
			}
		}
	}
}

void SUTTextChatPanel::BuildChatDestinationBar()
{
	if (ChatDestinationBar.IsValid())
	{
		ChatDestinationBar->ClearChildren();
		for (int32 i = 0; i < ChatDestinationList.Num(); i++)
		{
			if (ChatDestinationList[i]->Button.IsValid())
			{
				ChatDestinationBar->AddSlot()
				.Padding(4.0f,4.0f,4.0f,4.0f)
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0)
					[
						SNew(SBox).WidthOverride(100)
						[
							ChatDestinationList[i]->Button.ToSharedRef()
						]
					]
				];

			}
		}

		// Fill in remainding space.
		ChatDestinationBar->AddSlot()
			.Padding(4.0f,4.0f,4.0f,4.0f)
			.FillWidth(1.0)
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
			];
	}

}

FReply SUTTextChatPanel::OnDestinationClick(TSharedPtr<FChatDestination> Destination)
{
	if (CurrentChatDestination != Destination->ChatDestination)
	{
		DestinationChanged(Destination->ChatDestination);
	}

	return FReply::Handled();
}

void SUTTextChatPanel::DestinationChanged(const FName& NewDestionation)
{
	CurrentChatDestination = NewDestionation;
	int32 CurrentIndex = INDEX_NONE;
	for (int32 i = 0; i < ChatDestinationList.Num(); i++)
	{
		if (ChatDestinationList[i]->ChatDestination == CurrentChatDestination)
		{
			ChatDestinationList[i]->Button->BePressed();
			CurrentIndex = i;
		}
		else
		{
			ChatDestinationList[i]->Button->UnPressed();
		}
	}

	// Hook up the scroll box.
	if (ChatScrollBox.IsValid() && CurrentIndex != INDEX_NONE && ChatDestinationList[CurrentIndex]->ChatBox.IsValid())
	{
		ChatScrollBox->ClearChildren();
		ChatScrollBox->AddSlot()
			[
				ChatDestinationList[CurrentIndex]->ChatBox.ToSharedRef()
			];

		ChatScrollBox->SetScrollOffset(290999);
	}

	if (ChatDestinationChangedDelegate.IsBound())
	{
		ChatDestinationChangedDelegate.Execute(CurrentChatDestination);
	}

}

void SUTTextChatPanel::RouteChat(UUTLocalPlayer* LocalPlayer, TSharedPtr<FStoredChatMessage> ChatMessage)
{
	for (int32 i = 0 ; i < ChatDestinationList.Num(); i++)
	{
		if (ChatMessage->Type == ChatDestinations::MOTD || 
			ChatMessage->Type == ChatDestinations::System || 
			ChatMessage->Type == ChatDestinations::Whisper ||
			(ChatMessage->Type == ChatDestinations::Lobby && i==0) ||
			ChatDestinationList[i]->ChatDestination == ChatMessage->Type)
		{

			if (ChatMessage->Type == ChatDestinations::Team)
			{
				// Look to see if we should reject this based on the player owner's team.

				int32 MyTeamNum = 255;
				AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
				if (UTPlayerState)
				{
					MyTeamNum = UTPlayerState->GetTeamNum();
				}

				if (ChatMessage->TeamNum != MyTeamNum)
				{
					return;
				}
			}

			ChatDestinationList[i]->HandleChat(ChatMessage);
		}
	}
}

void SUTTextChatPanel::ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		FString FinalText = NewText.ToString();
		// Figure out the type of chat...

		if (FinalText.Left(1) == TEXT("\\") || FinalText.Left(1) == TEXT("/"))
		{
			FinalText = FinalText.Right(FinalText.Len() - 1);
			PlayerOwner->ConsoleCommand(FinalText);
			PlayerOwner->GetChatWidget()->SetText(FText::GetEmpty());
			return;
		}

		if (FinalText != TEXT(""))
		{
			if (CurrentChatDestination == ChatDestinations::Global)		FinalText = FString::Printf(TEXT("GlobalChat %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Friends)	FinalText = FString::Printf(TEXT("FriendSay %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Local)		FinalText = FString::Printf(TEXT("Say %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Match)		FinalText = FString::Printf(TEXT("Matchchat %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Team)		FinalText = FString::Printf(TEXT("TeamSay %s"), *FinalText);
		}
		else
		{
			int32 CurDestIndex = INDEX_NONE;
			for (int32 i=0; i < ChatDestinationList.Num(); i++)
			{
				if (ChatDestinationList[i]->ChatDestination == CurrentChatDestination)
				{
					CurDestIndex = i;
					break;
				}
			}

			CurDestIndex++;
			if (CurDestIndex >= ChatDestinationList.Num()) CurDestIndex = 0;
			if (CurDestIndex < ChatDestinationList.Num() && ChatDestinationList[CurDestIndex].IsValid() && ChatDestinationList[CurDestIndex]->ChatDestination != CurrentChatDestination)
			{
				DestinationChanged(ChatDestinationList[CurDestIndex]->ChatDestination);
			}
		}

		if (FinalText != TEXT("") && PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
		{
			PlayerOwner->Exec(PlayerOwner->GetWorld(), *FinalText, *GLog);
		}

		PlayerOwner->GetChatWidget()->SetText(FText::GetEmpty());
	}
}


int32 SUTTextChatPanel::FilterPlayer(AUTPlayerState* PlayerState, FUniqueNetIdRepl PlayerIdToCheck, uint8 TeamNum)
{
	// Depending on the channel, determine where this player resides.

	if (CurrentChatDestination == ChatDestinations::Friends)
	{
		return INDEX_NONE;
	}

	if (CurrentChatDestination == ChatDestinations::Local || CurrentChatDestination == ChatDestinations::Global)
	{
		return true;
	}
	else if (CurrentChatDestination == ChatDestinations::Match)
	{
		AUTLobbyPlayerState* MyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (MyPlayerState && MyPlayerState->CurrentMatch)
		{
			for (int32 i = 0; i < MyPlayerState->CurrentMatch->Players.Num(); i++)
			{
				if (MyPlayerState->CurrentMatch->Players[i]->UniqueId == PlayerIdToCheck) return true;
			}

			return false;
		}
	}
	else if (CurrentChatDestination == ChatDestinations::Team)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState)
		{
			AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(UTPlayerState);

			// If we are a lobby game then compare DesiredTeamNums
			if (LobbyPlayerState != NULL)
			{
				AUTLobbyPlayerState* TheirLobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerState);
				return (LobbyPlayerState != NULL && TheirLobbyPlayerState != NULL && LobbyPlayerState->DesiredTeamNum == TheirLobbyPlayerState->DesiredTeamNum);
			}
			else
			{
				return (UTPlayerState && PlayerState && UTPlayerState->GetTeamNum() == PlayerState->GetTeamNum());
			}

		}
	}

	return true;
}

void SUTTextChatPanel::RouteBufferedChat()
{
	if (PlayerOwner->ChatArchive.Num() > 0)
	{
		for (int32 i = 0; i < PlayerOwner->ChatArchive.Num(); i++)
		{
			RouteChat(PlayerOwner.Get(), PlayerOwner->ChatArchive[i]);
		}
	}

}


TSharedPtr<SVerticalBox> SUTTextChatPanel::ChangeTeamListBoxVisibility(bool bIsVisible)
{
	TeamListContainer->SetVisibility(bIsVisible ? EVisibility::Visible : EVisibility::Collapsed);
	return TeamListBox;
}

EVisibility SUTTextChatPanel::TeamListVisible() const
{
	if (PlayerOwner != nullptr && PlayerOwner->PlayerController != nullptr && PlayerOwner->PlayerController->PlayerState != nullptr)
	{
		AUTLobbyPlayerState* UTPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState != nullptr && UTPlayerState->CurrentMatch != nullptr && UTPlayerState->CurrentMatch->CurrentRuleset.IsValid())
		{
			if (UTPlayerState->CurrentMatch->CurrentRuleset->bTeamGame)
			{
				return EVisibility::Visible;
			}
		}
	}
	return EVisibility::Collapsed;
}

void SUTTextChatPanel::UpdateTeamList()
{
	AUTLobbyGameState* LobbyGameState = PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>();

	if (LobbyGameState && PlayerOwner != nullptr && PlayerOwner->PlayerController != nullptr && PlayerOwner->PlayerController->PlayerState != nullptr)
	{
		AUTLobbyPlayerState* UTPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState != nullptr && UTPlayerState->CurrentMatch != nullptr && UTPlayerState->CurrentMatch->CurrentRuleset.IsValid())
		{
			AUTLobbyMatchInfo* CurrentMatch = UTPlayerState->CurrentMatch;
			if (UTPlayerState->CurrentMatch->CurrentRuleset->bTeamGame)
			{
				bool bNeedsRefresh = false;
				AUTLobbyMatchInfo* CurrentMatch = UTPlayerState->CurrentMatch;

				// Remove any dead weight
				if (TeamPSList.Num() > 0)
				{
					// Check to see if any of them have expired
					for (int32 i = TeamPSList.Num()-1; i >= 0; i--)
					{
						if (!TeamPSList[i].IsValid() || !TeamPSList[i]->PlayerState.IsValid() || TeamPSList[i]->PlayerState->IsPendingKillPending() || TeamPSList[i]->PlayerState->CurrentMatch != CurrentMatch)
						{
							TeamPSList[i]->PlayerState.Reset();
							TeamPSList.RemoveAt(i);
							bNeedsRefresh = true;
						}
					}
				}

				// Add anyone missing
				for (int32 i=0; i < LobbyGameState->PlayerArray.Num();i++)
				{
					AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(LobbyGameState->PlayerArray[i]);
					if (PlayerState != nullptr)
					{
						bool bFound = false;
						for (int32 j = 0; j < TeamPSList.Num(); j++)
						{
							if (TeamPSList[i]->PlayerState.Get() == PlayerState)
							{
								if (TeamPSList[i]->TeamNum != PlayerState->DesiredTeamNum)
								{
									TeamPSList[i]->TeamNum = PlayerState->DesiredTeamNum;
									bNeedsRefresh = true;
								}
								bFound = true;
								break;
							}
						}

						if (!bFound)
						{
							bNeedsRefresh = true;
							TeamPSList.Add( FTeamListTracker::Make(PlayerState, PlayerState->DesiredTeamNum));
						}
					}
				}

				if (bNeedsRefresh)
				{
		
					TeamPSList.StableSort(FComparePlayerByName());
					int32 RedTeamCount = 0;
					int32 BlueTeamCount = 0;
					int32 SpecCount = 0;

					RedList.Empty();
					BlueList.Empty();
					SpectatorList.Empty();

					for (int32 i=0; i < TeamPSList.Num(); i++)
					{
						if (TeamPSList[i]->TeamNum == 0) RedList.Add(TeamPSList[i]);
						else if (TeamPSList[i]->TeamNum == 1) BlueList.Add(TeamPSList[i]);
						else 
						{
							SpectatorList.Add(TeamPSList[i]);
						}
					}

					RedListview->RequestListRefresh();
					BlueListview->RequestListRefresh();
					SpectatorListview->RequestListRefresh();
				}


			
			}
		}

	}
	else if (TeamPSList.Num() > 0)
	{
		TeamPSList.Empty();
	}
}

TSharedRef<SBox> SUTTextChatPanel::GenerateBadge(TSharedPtr<FTeamListTracker> InItem)
{
	return SNew(SBox).WidthOverride(32).HeightOverride(32)
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SImage)
			.Image(InItem.Get(), &FTeamListTracker::GetBadge)
		]
		+SOverlay::Slot()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0,0.0,0.0,0.0))
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTeamListTracker::GetRank)))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
			.ShadowOffset(FVector2D(0.0f,1.0f))
		]
		+SOverlay::Slot()
		[
			SNew(SImage)
			.Image(InItem.Get(), &FTeamListTracker::GetXPStarImage)
		]
	];
}

TSharedRef<ITableRow> SUTTextChatPanel::OnGenerateRedWidget( TSharedPtr<FTeamListTracker> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return GenerateWidget(InItem, OwnerTable, FName(TEXT("UT.PlayerList.Row.Red")));
}

TSharedRef<ITableRow> SUTTextChatPanel::OnGenerateBlueWidget( TSharedPtr<FTeamListTracker> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return GenerateWidget(InItem, OwnerTable, FName(TEXT("UT.PlayerList.Row.Blue")));
}

TSharedRef<ITableRow> SUTTextChatPanel::OnGenerateSpectatorWidget( TSharedPtr<FTeamListTracker> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return GenerateWidget(InItem, OwnerTable, FName(TEXT("UT.PlayerList.Row")));
}


TSharedRef<ITableRow> SUTTextChatPanel::GenerateWidget( TSharedPtr<FTeamListTracker> InItem, const TSharedRef<STableViewBase>& OwnerTable, FName StyleName)
{
	TSharedPtr<SVerticalBox> ReturnWidget;
	if (InItem->TeamNum == 1)
	{
		SAssignNew(ReturnWidget,SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(15.0,0.0,15.0,0.0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,5.0f,0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(*InItem->PlayerState->PlayerName))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.ColorAndOpacity(FLinearColor(0.8f,0.8f,1.0f,1.0f))
			]
			+SHorizontalBox::Slot().AutoWidth()
			[
				GenerateBadge(InItem)
			]
			
		];
	}
	else
	{
		SAssignNew(ReturnWidget,SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().VAlign(VAlign_Center).HAlign(HAlign_Left).Padding(15.0,0.0,15.0,0.0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,5.0f,0.0f)
			[
				GenerateBadge(InItem)
			]
			+SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(*InItem->PlayerState->PlayerName))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.ColorAndOpacity(FLinearColor(1.0f,0.8f,0.8f,1.0f))
			]
			
		];
	}

	return SNew(STableRow<TSharedPtr<FTrackedPlayer>>, OwnerTable).ShowSelection(false)
		.Style(SUTStyle::Get(), StyleName) // "UT.PlayerList.Row")
		[
			SNew(SUTMenuAnchor)
			.MenuButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Dark")
			.MenuButtonTextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
			.OnGetSubMenuOptions(this, &SUTTextChatPanel::GetMenuContent)
			.SearchTag(InItem->PlayerState->UniqueId.ToString())
			.OnSubMenuSelect(FUTSubMenuSelect::CreateSP(this, &SUTTextChatPanel::OnSubMenuSelect, InItem))
			.MenuPlacement(MenuPlacement_MenuRight)
			.ButtonContent()
			[			
				ReturnWidget.ToSharedRef()
			]
		];
}


void SUTTextChatPanel::OnListSelect(TSharedPtr<FTeamListTracker> Selected)
{
	if (PlayerOwner != nullptr && PlayerOwner->PlayerController != nullptr && PlayerOwner->PlayerController->PlayerState != nullptr)
	{
		AUTLobbyPlayerState* UTPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState != nullptr && UTPlayerState->CurrentMatch != nullptr)
		{
			UTPlayerState->CurrentMatch->ServerManageUser(0,Selected->PlayerState.Get());
		}
	}

}

void SUTTextChatPanel::GetMenuContent(FString SearchTag, TArray<FMenuOptionData>& MenuOptions)
{
	MenuOptions.Empty();
	MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","ChangeTeam","Change Team"), EPlayerListContentCommand::ChangeTeam));
	MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Spectate","Spectate"), EPlayerListContentCommand::Spectate));
}

void SUTTextChatPanel::OnSubMenuSelect(FName Tag, TSharedPtr<FTeamListTracker> InItem)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
	{
		AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		AUTLobbyPlayerState* TargetPlayerState = Cast<AUTLobbyPlayerState>(InItem->PlayerState.Get());

		if (Tag == EPlayerListContentCommand::ChangeTeam)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerManageUser(0,TargetPlayerState);
			}
		}
		else if (Tag == EPlayerListContentCommand::Spectate)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerManageUser(1,TargetPlayerState);
			}
		}
	}
}


#endif