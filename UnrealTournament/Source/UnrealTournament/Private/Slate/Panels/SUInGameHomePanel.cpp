
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "../Public/UTGameViewportClient.h"

#include "SlateBasics.h"
#include "../SUWScaleBox.h"
#include "Slate/SlateGameResources.h"
#include "SUInGameHomePanel.h"

#if !UE_SERVER


void SUInGameHomePanel::ConstructPanel(FVector2D CurrentViewportSize)
{
	ChatDestination = ChatDestinations::Local;

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
				// Allow children to place things over chat....
				SAssignNew(ChatArea,SVerticalBox)
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
								.Padding(30.0f,0.0f,30.0f,0.0f)
								[
									SNew(STextBlock)
									.Text(this, &SUInGameHomePanel::GetChatDestinationText)
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
									.OnTextCommitted(this, &SUInGameHomePanel::ChatTextCommited)
									.OnTextChanged(this, &SUInGameHomePanel::ChatTextChanged)
									.ClearKeyboardFocusOnCommit(false)
									.MinDesiredWidth(300.0f)
									.Text(FText::GetEmpty())
								]
							]

						]


						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.HeightOverride(42)
							.WidthOverride(80)
							[
								SNew(SOverlay)
								+SOverlay::Slot()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								[
									SNew(SImage)
									.Image(SUWindowsStyle::Get().GetBrush("UT.ChatBar.Fill"))
								]

								+SOverlay::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									BuildChatDestinationsButton()
								]
							]
						]
					]
				]
		
			]
		]
	];
}

TSharedRef<SWidget> SUInGameHomePanel::BuildChatDestinationsButton()
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

void SUInGameHomePanel::BuildChatDestinationMenu()
{
	if (ChatMenu.IsValid())
	{
		ChatMenu->ClearChildren();
		ChatMenu->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("Chat", "ChatDestination_Game", "Game"))
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Local)
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
					.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("Chat", "ChatDestination_Team", "Team"))
					.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
					.OnClicked(this, &SUInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Team)
				];
			}

			if (GS->bIsInstanceServer)
			{
				ChatMenu->AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("Chat", "ChatDestination_Lobby", "Lobby"))
					.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
					.OnClicked(this, &SUInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Lobby)
				];
			}

			if (PlayerOwner->IsLoggedIn())
			{
				ChatMenu->AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("Chat", "ChatDestination_Friends", "Friends"))
					.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
					.OnClicked(this, &SUInGameHomePanel::ChangeChatDestination, ChatDestinationsButton, ChatDestinations::Friends)
				];
			}
		}
	}
}

FReply SUInGameHomePanel::ChangeChatDestination(TSharedPtr<SComboButton> Button, FName NewDestination)
{
	if (Button.IsValid()) Button->SetIsOpen(false);
	ChatDestination = NewDestination;
	return FReply::Handled();
}

void SUInGameHomePanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = true;
		UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
		if (SB)
		{
			SB->BecomeInteractive();
		}
	}
}
void SUInGameHomePanel::OnHidePanel()
{
	SUWPanel::OnHidePanel();
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = false;
		UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
		if (SB)
		{
			SB->BecomeNonInteractive();
		}
	}
}

FText SUInGameHomePanel::GetChatDestinationText() const
{
	if (ChatDestination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "TeamTag","Team");
	if (ChatDestination == ChatDestinations::Local)		return NSLOCTEXT("Chat", "LocalTag","Game");
	if (ChatDestination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "FriendsTag","Whisper");
	if (ChatDestination == ChatDestinations::Lobby)		return NSLOCTEXT("Chat", "LobbyTag","Hub");
	if (ChatDestination == ChatDestinations::Match)		return NSLOCTEXT("Chat", "MatchTag","Match");
	
	return NSLOCTEXT("Chat", "GlobalTag","Global Chat");
}

void SUInGameHomePanel::ChatTextChanged(const FText& NewText)
{
	if (NewText.ToString().Len() > 128)
	{
		ChatText->SetText(FText::FromString(NewText.ToString().Left(128)));
	}
}

void SUInGameHomePanel::ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType)
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

FText SUInGameHomePanel::GetChatDestinationTag(FName Destination)
{
	if (Destination == ChatDestinations::Team)		return NSLOCTEXT("Chat", "TeamTag","Team");
	if (Destination == ChatDestinations::Local)		return NSLOCTEXT("Chat", "LocalTag","Local");
	if (Destination == ChatDestinations::Friends)	return NSLOCTEXT("Chat", "FriendsTag","Whisper");
	if (Destination == ChatDestinations::Match)		return NSLOCTEXT("Chat", "MatchTag","Hub");
	if (Destination == ChatDestinations::Lobby)		return NSLOCTEXT("Chat", "LobbyTag","Lobby");
	if (Destination == FName(TEXT("debug")))		return NSLOCTEXT("Chat", "DebugTag","DEBUG");
	
	return NSLOCTEXT("Chat", "GlobalTag","Global");
}

FReply SUInGameHomePanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

// @Returns true if the mouse position is inside the viewport
bool SUInGameHomePanel::GetGameMousePosition(FVector2D& MousePosition)
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

FReply SUInGameHomePanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{

	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = true;
		UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
		if (SB)
		{
			FVector2D MousePosition;
			if (GetGameMousePosition(MousePosition))
			{
				if (SB->AttemptSelection(MousePosition))
				{
					SB->SelectionClick();
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SUInGameHomePanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		PC->MyUTHUD->bForceScores = true;
		UUTScoreboard* SB = PC->MyUTHUD->GetScoreboard();
		if (SB)
		{
			FVector2D MousePosition;
			if (GetGameMousePosition(MousePosition))
			{
				SB->TrackMouseMovement(MousePosition);
			}
		}
	}

	return FReply::Unhandled();
}

FReply SUInGameHomePanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent)
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



#endif