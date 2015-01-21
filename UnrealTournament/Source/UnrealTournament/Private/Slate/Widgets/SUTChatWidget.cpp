// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FortniteUIPCH.h"
#include "SFortChatWidget.h"

#define CHAT_BOX_WIDTH 576.0f
#define CHAT_BOX_HEIGHT 260.0f
#define CHAT_BOX_PADDING 20.0f
#define MAX_CHAT_MESSAGES 10

void SFortChatWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	FortHUDPCTrackerBase::Init(InCtx);

	bKeepVisible = InArgs._bKeepVisible;
	LastVisibility = bKeepVisible ? EVisibility::Visible : EVisibility::Hidden;
	ChatFadeTime = 10.0;
	LastChatLineTime = -1.0;

	ViewModel = IFriendsAndChatModule::Get().GetFriendsAndChatManager()->GetChatViewModel();
	ViewModel->OnChatMessageCommitted().AddSP(this, &SFortChatWidget::OnChatTextCommitted);
	ViewModel->OnChatListUpdated().AddSP(this, &SFortChatWidget::HandleChatListUpdated);
	ViewModel->OnNetworkMessageSentEvent().AddSP(this, &SFortChatWidget::HandleFriendsNetworkChatMessage);
	
	//some constant values
	const int32 PaddingValue = 2;

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
				IFriendsAndChatModule::Get().GetFriendsAndChatManager()->GenerateChatWidget(&FFortUIStyle::Get().GetWidgetStyle< FFriendsAndChatStyle >( "FriendsStyle" ), ViewModel.ToSharedRef()).ToSharedRef()
			]
		]
	];

	if(!bKeepVisible)
	{
		const FLinearColor OverrideFontColor = FLinearColor(1,1,1,0);
		ViewModel->SetFontOverrideColor(OverrideFontColor);
		ViewModel->SetEntryBarVisibility(EVisibility::Hidden);
	}
	else
	{
		ViewModel->SetEntryBarVisibility(EVisibility::Visible);
	}

	// Zone turns off global chat, lobby or anything else turns it back on
	AFortPlayerController* FortPC = GetPlayerController();
	AFortPlayerControllerZone* ZonePC = Cast<AFortPlayerControllerZone>(FortPC);
	if (ZonePC)
	{
		ViewModel->SetInGameUI(true);
	}
	else
	{
		ViewModel->SetInGameUI(false);
	}
}

void SFortChatWidget::HandleFriendsNetworkChatMessage(const FString& NetworkMessage)
{
	const double CurrentTime = FSlateApplication::Get().GetCurrentTime();
	LastChatLineTime = CurrentTime;
	ViewModel->SetOverrideColorActive(false);
	GetPlayerController()->Say(NetworkMessage);
}

void SFortChatWidget::HandleChatListUpdated()
{
	const double CurrentTime = FSlateApplication::Get().GetCurrentTime();
	LastChatLineTime = CurrentTime;
	ViewModel->SetOverrideColorActive(false);
}

EVisibility SFortChatWidget::GetEntryVisibility() const
{
	return ViewModel->GetEntryBarVisibility();
}

void SFortChatWidget::SetEntryVisibility( TAttribute<EVisibility> InVisibility )
{
	if (ViewModel.IsValid())
	{
		ViewModel->SetEntryBarVisibility(InVisibility.Get());
	}
}

void SFortChatWidget::OnChatTextCommitted()
{
	FSlateApplication::Get().PlaySound(FFortUIStyle::Get().GetSound("Fortnite.ChatTextCommittedSound"));
}

void SFortChatWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	//Always tick the super
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (GetEntryVisibility() != LastVisibility)
	{
		LastVisibility = GetEntryVisibility();
		if (LastVisibility == EVisibility::Visible)
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

	if(!bKeepVisible)
	{
		if(ViewModel->GetOverrideColorSet() && GetEntryVisibility() ==  EVisibility::Visible)
		{
			ViewModel->SetOverrideColorActive(false);
		}
		else if(!ViewModel->GetOverrideColorSet() && GetEntryVisibility() != EVisibility::Visible)
		{
			const double CurrentTime = FSlateApplication::Get().GetCurrentTime();
			if(LastChatLineTime + ChatFadeTime < CurrentTime)
			{
				ViewModel->SetOverrideColorActive(true);
				SetEntryVisibility(EVisibility::Hidden);
			}
		}
	}
}

void SFortChatWidget::SetFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));

	if (ViewModel.IsValid())
	{
		ViewModel->SetFocus();
	}
}

FReply SFortChatWidget::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	return FReply::Handled().ReleaseMouseCapture().LockMouseToWidget( SharedThis( this ) );
}
