
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTGameViewportClient.h"

#include "SlateBasics.h"
#include "../Widgets/SUTScaleBox.h"
#include "Slate/SlateGameResources.h"
#include "SUTInGameHomePanel.h"
#include "../Widgets/SUTBorder.h"
#include "../Widgets/SUTButton.h"
#include "SUTMatchSummaryPanel.h"

#if !UE_SERVER


void SUTInGameHomePanel::ConstructPanel(FVector2D CurrentViewportSize)
{
	bFocusSummaryInv = false;
	ChatDestination = ChatDestinations::Local;
	bShowingContextMenu = false;
	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(SubMenuOverlay, SOverlay)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SAssignNew(MenuArea, SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.HeightOverride(42)
							.WidthOverride(60)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								[
									SNew(SImage)
									.Image(SUWindowsStyle::Get().GetBrush("UT.ChatBar.Fill"))
								]
								+ SOverlay::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									BuildChatDestinationsButton()
								]
							]
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SBox)
								.HeightOverride(42)
								[
									SNew(SImage)
									.Image(SUWindowsStyle::Get().GetBrush("UT.ChatBar.Bar"))
								]
							]
							+SOverlay::Slot()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f,0.0f,30.0f,0.0f)
								[
									SNew(STextBlock)
									.Text(this, &SUTInGameHomePanel::GetChatDestinationText)
									.TextStyle(SUWindowsStyle::Get(), "UT.ChatBar.Button.TextStyle")
								]
							]
						]
			
						+SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[

							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SBox)
								.HeightOverride(42)
								[
									SNew(SImage)
									.Image(SUWindowsStyle::Get().GetBrush("UT.ChatBar.EntryArea"))
								]
							]
							+SOverlay::Slot()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(30.0f, 4.0f, 10.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT(" >")))
									.TextStyle(SUWindowsStyle::Get(), "UT.ChatBar.Editbox.TextStyle")
								]
								+SHorizontalBox::Slot()
								.Padding(0.0f, 4.0f, 10.0f, 0.0f)
								[
									SAssignNew(ChatText, SEditableTextBox)
									.Style(SUWindowsStyle::Get(), "UT.ChatBar.Editbox.ChatEditBox")
									.OnTextCommitted(this, &SUTInGameHomePanel::ChatTextCommited)
									.OnTextChanged(this, &SUTInGameHomePanel::ChatTextChanged)
									.ClearKeyboardFocusOnCommit(false)
									.MinDesiredWidth(300.0f)
									.Text(FText::GetEmpty())
								]
							]

						]
					]
				]
		
			]
		]
	];

	if (SubMenuOverlay.IsValid())
	{
		SubMenuOverlay->AddSlot(0)
		[
			// Allow children to place things over chat....
			SAssignNew(ChatArea,SVerticalBox)
		];
		SubMenuOverlay->AddSlot(1)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.Visibility(this, &SUTInGameHomePanel::GetSummaryVisibility)
			[
				SAssignNew(SummaryOverlay, SOverlay)
			]
		];
	}
}

EVisibility SUTInGameHomePanel::GetSummaryVisibility() const
{
	if (PlayerOwner.IsValid() && SummaryPanel.IsValid())
	{
		AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GameState && (GameState->GetMatchState() == MatchState::PlayerIntro || GameState->GetMatchState() == MatchState::WaitingPostMatch))
		{
			return bFocusSummaryInv ? EVisibility::Hidden : EVisibility::Visible;
		}
	}
	return EVisibility::Hidden;
}




TSharedRef<SWidget> SUTInGameHomePanel::BuildChatDestinationsButton()
{

	SAssignNew(ChatDestinationsButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
		.ButtonContent()
		[
			SNew(SImage)
			.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Chat36"))
		];

	SAssignNew(ChatMenu, SVerticalBox);
	if (ChatMenu.IsValid())
	{
		BuildChatDestinationMenu();
	}

	ChatDestinationsButton->SetMenuContent(ChatMenu.ToSharedRef());
	return ChatDestinationsButton.ToSharedRef();

}

void SUTInGameHomePanel::BuildChatDestinationMenu()
{
	if (ChatMenu.IsValid())
	{
		ChatMenu->ClearChildren();
		ChatMenu->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("Chat", "ChatDestination_Game", "Game"))
			.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
			.OnClicked(this, &SUTInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Local)
		];
	
		AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GS)
		{
			if (GS->bTeamGame)
			{
				ChatMenu->AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("Chat", "ChatDestination_Team", "Team"))
					.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
					.OnClicked(this, &SUTInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Team)
				];
			}

			if (GS->bIsInstanceServer)
			{
				ChatMenu->AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("Chat", "ChatDestination_Lobby", "Lobby"))
					.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
					.OnClicked(this, &SUTInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Lobby)
				];
			}

			if (PlayerOwner->IsLoggedIn())
			{
				ChatMenu->AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("Chat", "ChatDestination_Friends", "Friends"))
					.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
					.OnClicked(this, &SUTInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Friends)
				];
			}
		}
	}
}

FReply SUTInGameHomePanel::ChangeChatDestination(TSharedPtr<SComboButton> Button, FName NewDestination)
{
	if (Button.IsValid()) Button->SetIsOpen(false);
	ChatDestination = NewDestination;
	return FReply::Handled();
}

void SUTInGameHomePanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = true;
		UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
		if (SB)
		{
			SB->BecomeInteractive();
		}
		UUTHUDWidget_ReplayTimeSlider* ReplayTimeSlider = PC->MyUTHUD->GetReplayTimeSlider();
		if (ReplayTimeSlider)
		{
			ReplayTimeSlider->BecomeInteractive();
		}
	}
}
void SUTInGameHomePanel::OnHidePanel()
{
	SUTPanelBase::OnHidePanel();
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = false;
		UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
		if (SB)
		{
			SB->BecomeNonInteractive();
		}
		UUTHUDWidget_ReplayTimeSlider* ReplayTimeSlider = PC->MyUTHUD->GetReplayTimeSlider();
		if (ReplayTimeSlider)
		{
			ReplayTimeSlider->BecomeNonInteractive();
		}
	}
}

FText SUTInGameHomePanel::GetChatDestinationText() const
{
	if (ChatDestination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "TeamTag","Team");
	if (ChatDestination == ChatDestinations::Local)		return NSLOCTEXT("Chat", "LocalTag","Game");
	if (ChatDestination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "FriendsTag","Whisper");
	if (ChatDestination == ChatDestinations::Lobby)		return NSLOCTEXT("Chat", "LobbyTag","Hub");
	if (ChatDestination == ChatDestinations::Match)		return NSLOCTEXT("Chat", "MatchTag","Match");
	
	return NSLOCTEXT("Chat", "GlobalTag","Global Chat");
}

void SUTInGameHomePanel::ChatTextChanged(const FText& NewText)
{
	if (NewText.ToString().Len() > 128)
	{
		ChatText->SetText(FText::FromString(NewText.ToString().Left(128)));
	}
}

void SUTInGameHomePanel::ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
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
			ChatText->SetText(FText::GetEmpty());
		}
		else
		{
			// Add code to change chat mode here.
		}
	}
}

FText SUTInGameHomePanel::GetChatDestinationTag(FName Destination)
{
	if (Destination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "TeamTag","Team");
	if (Destination == ChatDestinations::Local)		return NSLOCTEXT("Chat", "LocalTag","Local");
	if (Destination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "FriendsTag","Whisper");
	if (Destination == ChatDestinations::Match)		return NSLOCTEXT("Chat", "MatchTag","Hub");
	if (Destination == ChatDestinations::Lobby)		return NSLOCTEXT("Chat", "LobbyTag","Lobby");
	if (Destination == FName(TEXT("debug")))		return NSLOCTEXT("Chat", "DebugTag","DEBUG");
	
	return NSLOCTEXT("Chat", "GlobalTag","Global");
}

FReply SUTInGameHomePanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

// @Returns true if the mouse position is inside the viewport
bool SUTInGameHomePanel::GetGameMousePosition(FVector2D& MousePosition)
{
	// We need to get the mouse input but the mouse event only has the mouse in screen space.  We need it in viewport space and there
	// isn't a good way to get there.  So we punt and just get it from the game viewport.

	UUTGameViewportClient* GVC = Cast<UUTGameViewportClient>(PlayerOwner->ViewportClient);
	if (GVC)
	{
		return GVC->GetMousePosition(MousePosition);
	}
	return false;
}

void SUTInGameHomePanel::ShowContextMenu(UUTScoreboard* Scoreboard, FVector2D ContextMenuLocation, FVector2D ViewportBounds)
{
	if (bShowingContextMenu)
	{
		HideContextMenu();
	}

	if (Scoreboard == nullptr) return;
	
	TWeakObjectPtr<AUTPlayerState> SelectedPlayer = Scoreboard->GetSelectedPlayer();
	
	if (!SelectedPlayer.IsValid()) return;

	TSharedPtr<SVerticalBox> MenuBox;

	bShowingContextMenu = true;
	SubMenuOverlay->AddSlot(1)
	.Padding(FMargin(ContextMenuLocation.X, ContextMenuLocation.Y, 0, 0))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SUTBorder)
				.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
				[
					SAssignNew(MenuBox, SVerticalBox)
				]
			]
		]
	];


	if (MenuBox.IsValid())
	{
		// Add the show player card
		MenuBox->AddSlot()
		.AutoHeight()
		[
			SNew(SUTButton)
			.OnClicked(this, &SUTInGameHomePanel::ContextCommand, 0, SelectedPlayer)
			.ButtonStyle(SUTStyle::Get(),"UT.ContextMenu.Item")
			.Text(NSLOCTEXT("SUTInGameHomePanel","ShowPlayerCard","Show Player Card"))
			.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
		];

		// If we are in a netgame, show online options.
		if ( PlayerOwner->GetWorld()->GetNetMode() == ENetMode::NM_Client)
		{
			MenuBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().Padding(FMargin(10.0,0.0,10.0,0.0))
				[
					SNew(SBox).HeightOverride(3)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
					]
				]
			];

			if (!PlayerOwner->IsAFriend(SelectedPlayer->UniqueId))
			{
				MenuBox->AddSlot()
				.AutoHeight()
				[
					SNew(SUTButton)
					.OnClicked(this, &SUTInGameHomePanel::ContextCommand, 1, SelectedPlayer)
					.ButtonStyle(SUTStyle::Get(),"UT.ContextMenu.Item")
					.Text(NSLOCTEXT("SUTInGameHomePanel","SendFriendRequest","Send Friend Request"))
					.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
				];
			}


			MenuBox->AddSlot()
			.AutoHeight()
			[
				SNew(SUTButton)
				.OnClicked(this, &SUTInGameHomePanel::ContextCommand, 2, SelectedPlayer)
				.ButtonStyle(SUTStyle::Get(),"UT.ContextMenu.Item")
				.Text(NSLOCTEXT("SUTInGameHomePanel","VoteToKick","Vote to Kick"))
				.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
			];

			if (PlayerOwner->PlayerController)
			{
				AUTPlayerState* OwnerPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
				if (OwnerPlayerState && OwnerPlayerState->bIsRconAdmin && SelectedPlayer != OwnerPlayerState)
				{
					MenuBox->AddSlot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().Padding(FMargin(10.0,0.0,10.0,0.0))
						[
							SNew(SBox).HeightOverride(3)
							[
								SNew(SImage)
								.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
							]
						]
					];

					MenuBox->AddSlot()
					.AutoHeight()
					[
						SNew(SUTButton)
						.OnClicked(this, &SUTInGameHomePanel::ContextCommand, 3, SelectedPlayer)
						.ButtonStyle(SUTStyle::Get(),"UT.ContextMenu.Item")
						.Text(NSLOCTEXT("SUTInGameHomePanel","AdminKick","Admin Kick"))
						.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
					];
					MenuBox->AddSlot()
					.AutoHeight()
					[
						SNew(SUTButton)
						.OnClicked(this, &SUTInGameHomePanel::ContextCommand, 4, SelectedPlayer)
						.ButtonStyle(SUTStyle::Get(),"UT.ContextMenu.Item")
						.Text(NSLOCTEXT("SUTInGameHomePanel","AdminBan","Admin Ban"))
						.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
					];
				}
			}
		}
	
	
	}

}

void SUTInGameHomePanel::HideContextMenu()
{
	if (bShowingContextMenu)
	{
		SubMenuOverlay->RemoveSlot(1);
		bShowingContextMenu = false;
	}
}

FReply SUTInGameHomePanel::ContextCommand(int32 CommandId, TWeakObjectPtr<AUTPlayerState> TargetPlayerState)
{
	HideContextMenu();
	if (TargetPlayerState.IsValid())
	{
		AUTPlayerState* MyPlayerState =  Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);

		if (MyPlayerState && PC)
		{
			switch (CommandId)
			{
				case 0: PlayerOwner->ShowPlayerInfo(TargetPlayerState); break;
				case 1: PlayerOwner->RequestFriendship(TargetPlayerState->UniqueId.GetUniqueNetId()); break;
				case 2: if (TargetPlayerState != MyPlayerState)
						{
							PC->ServerRegisterBanVote(TargetPlayerState.Get());
						}
						break;
				case 3: PC->RconKick(TargetPlayerState->UniqueId.ToString(), false); break;
				case 4: PC->RconKick(TargetPlayerState->UniqueId.ToString(), true);	break;
			}
		}
	}

	return FReply::Handled();
}


FReply SUTInGameHomePanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = true;
		FVector2D MousePosition;
		if (GetGameMousePosition(MousePosition))
		{
			UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
			if (SB)
			{
				if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
				{
					if (SB->AttemptSelection(MousePosition))
					{
						// We are over a item.. pop up the context menu
						FVector2D LocalPosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
						ShowContextMenu(SB, LocalPosition, MyGeometry.GetLocalSize());
					}
					else
					{
						HideContextMenu();
					}
				}
				else
				{
					HideContextMenu();
					if (SB->AttemptSelection(MousePosition))
					{
						SB->SelectionClick();
						return FReply::Handled();
					}
				}
			}

			UUTHUDWidget_ReplayTimeSlider* ReplayTimeSlider = PC->MyUTHUD->GetReplayTimeSlider();
			if (ReplayTimeSlider)
			{
				if (ReplayTimeSlider->SelectionClick(MousePosition))
				{
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SUTInGameHomePanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = true;
		FVector2D MousePosition;
		if (GetGameMousePosition(MousePosition))
		{
			UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
			if (SB)
			{
				SB->TrackMouseMovement(MousePosition);
			}
			UUTHUDWidget_ReplayTimeSlider* ReplayTimeSlider = PC->MyUTHUD->GetReplayTimeSlider();
			if (ReplayTimeSlider)
			{
				ReplayTimeSlider->BecomeInteractive();
				ReplayTimeSlider->TrackMouseMovement(MousePosition);
			}
		}
	}

	return FReply::Unhandled();
}

FReply SUTInGameHomePanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = true;
		UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
		if (SB)
		{
			if (InKeyboardEvent.GetKey() == EKeys::Up || InKeyboardEvent.GetKey() == EKeys::Gamepad_DPad_Up)
			{
				SB->SelectionUp();
				return FReply::Handled();
			}
			else if (InKeyboardEvent.GetKey() == EKeys::Down || InKeyboardEvent.GetKey() == EKeys::Gamepad_DPad_Down)
			{
				SB->SelectionDown();
				return FReply::Handled();
			}
			else if (InKeyboardEvent.GetKey() == EKeys::Left || InKeyboardEvent.GetKey() == EKeys::Gamepad_DPad_Left)
			{
				SB->SelectionLeft();
				return FReply::Handled();
			}
			else if (InKeyboardEvent.GetKey() == EKeys::Right || InKeyboardEvent.GetKey() == EKeys::Gamepad_DPad_Right)
			{
				SB->SelectionRight();
				return FReply::Handled();
			}
			else if (InKeyboardEvent.GetKey() == EKeys::Enter)
			{
				SB->SelectionClick();
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();

}

void SUTInGameHomePanel::FocusChat()
{
	FSlateApplication::Get().SetKeyboardFocus(ChatText, EKeyboardFocusCause::SetDirectly);
}

void SUTInGameHomePanel::ShowMatchSummary(bool bInitial)
{
	if (!SummaryPanel.IsValid())
	{
		
		if (SummaryOverlay.IsValid())
		{
			SummaryOverlay->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			[
				SAssignNew(SummaryPanel, SUTMatchSummaryPanel, PlayerOwner)
				.GameState(PlayerOwner->GetWorld()->GetGameState<AUTGameState>())
			];

			if ( SummaryPanel.IsValid() )
			{
				SummaryPanel->ParentPanel = SharedThis(this);
			}
		}
		else
		{
			return;
		}
	}

	bFocusSummaryInv = false;
	if (bInitial && SummaryPanel.IsValid())
	{
		SummaryPanel->SetInitialCams();
	}
}

void SUTInGameHomePanel::HideMatchSummary()
{
	bFocusSummaryInv = true;
}


#endif