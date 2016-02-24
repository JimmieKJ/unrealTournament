
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
#include "UTConsole.h"
#include "SUTChatEditBox.h"

#if !UE_SERVER


void SUTInGameHomePanel::ConstructPanel(FVector2D CurrentViewportSize)
{
	Tag = FName(TEXT("InGameHomePanel"));
	bCloseOnSubmit = false;

	bFocusSummaryInv = false;
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
			[
				SNew(SBox).HeightOverride(42)
				[
					SAssignNew(ChatBar, SUTChatBar, PlayerOwner)
					.InitialChatDestination(ChatDestinations::Local)
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



void SUTInGameHomePanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
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

	HideMatchSummary();

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
					else
					{
						PlayerOwner->HideMenu();
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
		PlayerOwner->GetSlateOperations() = FReply::Handled().ReleaseMouseCapture().SetUserFocus(SummaryPanel.ToSharedRef(), EFocusCause::SetDirectly);
	}

}

void SUTInGameHomePanel::HideMatchSummary()
{
	bFocusSummaryInv = true;
	if (SummaryOverlay.IsValid() && SummaryPanel.IsValid())
	{
		SummaryOverlay->RemoveSlot(SummaryPanel.ToSharedRef());
	}
	SummaryPanel.Reset();
}

TSharedPtr<SUTMatchSummaryPanel> SUTInGameHomePanel::GetSummaryPanel()
{
	return SummaryPanel;
}

TSharedPtr<SWidget> SUTInGameHomePanel::GetInitialFocus()
{
	if (SummaryPanel.IsValid())
	{
		return GetSummaryPanel();
	}
	
	return PlayerOwner->GetChatWidget();
}

#endif