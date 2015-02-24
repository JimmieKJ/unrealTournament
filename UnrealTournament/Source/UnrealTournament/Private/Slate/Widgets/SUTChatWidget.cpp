// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUTChatWidget.h"
#include "FriendsAndChat.h"

#define CHAT_BOX_WIDTH 576.0f
#define CHAT_BOX_HEIGHT 260.0f
#define CHAT_BOX_PADDING 20.0f
#define MAX_CHAT_MESSAGES 10

#if !UE_SERVER

void SUTChatWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	LastVisibility = EVisibility::Visible;
	Ctx = InCtx;

	ViewModel = IFriendsAndChatModule::Get().GetFriendsAndChatManager()->GetChatViewModel();
	ViewModel->OnChatMessageCommitted().AddSP(this, &SUTChatWidget::OnChatTextCommitted);
	ViewModel->OnChatListUpdated().AddSP(this, &SUTChatWidget::HandleChatListUpdated);
	ViewModel->OnNetworkMessageSentEvent().AddSP(this, &SUTChatWidget::HandleFriendsNetworkChatMessage);
	ViewModel->EnableGlobalChat(false);

	//some constant values
	const int32 PaddingValue = 2;

	auto& FriendsAndChat = *IFriendsAndChatModule::Get().GetFriendsAndChatManager();

	// Initialize Menu
	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Bottom)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(CHAT_BOX_HEIGHT)
			.WidthOverride(CHAT_BOX_WIDTH)
			[
				FriendsAndChat.GenerateChatWidget(
					&SUWindowsStyle::Get().GetWidgetStyle< FFriendsAndChatStyle >( "FriendsStyle" ),
					ViewModel.ToSharedRef(),
					TAttribute<FText>()
				).ToSharedRef()
			]
		]
	];

	ViewModel->SetEntryBarVisibility(EVisibility::Visible);
}

void SUTChatWidget::HandleFriendsNetworkChatMessage(const FString& NetworkMessage)
{
	ViewModel->SetOverrideColorActive(false);
//	Ctx.GetPlayerController()->Say(NetworkMessage);
}

void SUTChatWidget::HandleChatListUpdated()
{
	ViewModel->SetOverrideColorActive(false);
}

void SUTChatWidget::OnChatTextCommitted()
{
//	FSlateApplication::Get().PlaySound(FFortUIStyle::Get().GetSound("Fortnite.ChatTextCommittedSound"));
}

void SUTChatWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	//Always tick the super
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	auto EntryBarVisibility = ViewModel->GetEntryBarVisibility();
	if (EntryBarVisibility != LastVisibility)
	{
		LastVisibility = EntryBarVisibility;
		if (EntryBarVisibility == EVisibility::Visible)
		{
			// Enter UI mode
			SetFocus();
		}
		else
		{
			// Exit UI mode
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
}

void SUTChatWidget::SetFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));

	if (ViewModel.IsValid())
	{
		ViewModel->SetFocus();
	}
}

FReply SUTChatWidget::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	return FReply::Handled().ReleaseMouseCapture().LockMouseToWidget( SharedThis( this ) );
}

#endif