// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTChatWidget.h"
#include "../SUWindowsStyle.h"
#include "Social.h"

#define CHAT_BOX_WIDTH 576.0f
#define CHAT_BOX_HEIGHT 320.0f
#define CHAT_BOX_HEIGHT_FADED 120.0f
#define CHAT_BOX_PADDING 20.0f
#define MAX_CHAT_MESSAGES 10

#if !UE_SERVER

void SUTChatWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	Ctx = InCtx;

	//some constant values
	const int32 PaddingValue = 2;

	TSharedPtr< class SWidget > Chat;
	TSharedRef< IFriendsAndChatManager > Manager = ISocialModule::Get().GetFriendsAndChatManager();
	Display = Manager->GenerateChatDisplayService();
	Settings = Manager->GetChatSettingsService();

	Chat = Manager->GenerateChromeWidget(InArgs._FriendStyle, Display.ToSharedRef(), Settings.ToSharedRef());

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
				Chat.ToSharedRef()
 			]
		]
	];
}

void SUTChatWidget::HandleFriendsNetworkChatMessage(const FString& NetworkMessage)
{
	//ViewModel->SetOverrideColorActive(false);
	//Ctx.GetPlayerController()->Say(NetworkMessage);
}

void SUTChatWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	//Always tick the super
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SUTChatWidget::SetFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
	
	Display->SetFocus();
}

FReply SUTChatWidget::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	return FReply::Handled().ReleaseMouseCapture().LockMouseToWidget( SharedThis( this ) );
}

#endif