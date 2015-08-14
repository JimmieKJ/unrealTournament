
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"
#include "SUTextChatPanel.h"
#include "UTLobbyPC.h"
#include "UTLobbyPlayerState.h"
#include "SUPlayerListPanel.h"

#if !UE_SERVER

SUTextChatPanel::~SUTextChatPanel()
{
	PlayerOwner->RemoveChatArchiveChangedDelegate(RouteChatHande);
}

void SUTextChatPanel::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	
	RouteChatHande = PlayerOwner->RegisterChatArchiveChangedDelegate( FChatArchiveChanged::FDelegate::CreateSP(this, &SUTextChatPanel::RouteChat));
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
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f,5.0f,0.0f,0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT(">")))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0)
					.Padding(5.0)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(8.0, 5.0)
							[
								SAssignNew(TypeMsg, STextBlock)
								.Text(FText::FromString(TEXT("type your message here")))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								.ColorAndOpacity(FLinearColor(1.0,1.0,1.0,0.3))
							]
						]
						+SOverlay::Slot()
						[
							SAssignNew(ChatEditBox, SEditableTextBox)
							.Style(SUTStyle::Get(), "UT.ChatEditBox")
							.OnTextChanged(this, &SUTextChatPanel::ChatTextChanged)
							.OnTextCommitted(this, &SUTextChatPanel::ChatTextCommited)
							.ClearKeyboardFocusOnCommit(false)
							.MinDesiredWidth(500.0f)
							.Text(FText::GetEmpty())
						]
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

void SUTextChatPanel::AddDestination(const FText& Caption, const FName ChatDestionation, float Weight, bool bSelect)
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
			.OnClicked(this, &SUTextChatPanel::OnDestinationClick, Dest)
			.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(Dest.Get(), &FChatDestination::GetButtonCaption) ) )
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(Dest.Get(), &FChatDestination::GetButtonColorAndOpacity) ) )
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
				]
			];

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

void SUTextChatPanel::RemoveDestination(const FName Destination)
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

void SUTextChatPanel::BuildChatDestinationBar()
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

FReply SUTextChatPanel::OnDestinationClick(TSharedPtr<FChatDestination> Destination)
{
	if (CurrentChatDestination != Destination->ChatDestination)
	{
		DestinationChanged(Destination->ChatDestination);
	}

	return FReply::Handled();
}

void SUTextChatPanel::DestinationChanged(const FName& NewDestionation)
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

void SUTextChatPanel::RouteChat(UUTLocalPlayer* LocalPlayer, TSharedPtr<FStoredChatMessage> ChatMessage)
{
	for (int32 i = 0 ; i < ChatDestinationList.Num(); i++)
	{
		if (ChatMessage->Type == ChatDestinations::MOTD || 
			ChatMessage->Type == ChatDestinations::System || 
			ChatDestinationList[i]->ChatDestination == ChatMessage->Type)
		{
			ChatDestinationList[i]->HandleChat(ChatMessage);
		}
	}
}

void SUTextChatPanel::ChatTextChanged(const FText& NewText)
{
	TypeMsg->SetVisibility( NewText.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden);
	if (NewText.ToString().Len() > 128)
	{
		ChatEditBox->SetText(FText::FromString(NewText.ToString().Left(128)));
	}
}

void SUTextChatPanel::ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		FString FinalText = NewText.ToString();
		// Figure out the type of chat...
		if (FinalText != TEXT(""))
		{
			if (CurrentChatDestination == ChatDestinations::Global)		FinalText = FString::Printf(TEXT("GlobalChat %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Friends)	FinalText = FString::Printf(TEXT("FriendSay %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Local)		FinalText = FString::Printf(TEXT("Say %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Match)		FinalText = FString::Printf(TEXT("Matchchat %s"), *FinalText);
			if (CurrentChatDestination == ChatDestinations::Team)		FinalText = FString::Printf(TEXT("TeamSay %s"), *FinalText);
		}

		if (FinalText != TEXT("") && PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
		{
			PlayerOwner->Exec(PlayerOwner->GetWorld(), *FinalText, *GLog);
		}

		ChatEditBox->SetText(FText::GetEmpty());
	}
}


int32 SUTextChatPanel::FilterPlayer(AUTPlayerState* PlayerState, FUniqueNetIdRepl PlayerIdToCheck, uint8 TeamNum)
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

void SUTextChatPanel::RouteBufferedChat()
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