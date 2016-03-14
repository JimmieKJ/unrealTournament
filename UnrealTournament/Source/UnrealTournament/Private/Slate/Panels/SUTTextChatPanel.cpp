
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


#endif