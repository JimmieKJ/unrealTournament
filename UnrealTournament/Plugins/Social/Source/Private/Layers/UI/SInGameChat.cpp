// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatChromeViewModel.h"
#include "SInGameChat.h"
#include "SChatWindow.h"
#include "SChatEntryWidget.h"
#include "SWidgetSwitcher.h"
#include "ChatChromeTabViewModel.h"
#include "ChatViewModel.h"
#include "ChatEntryViewModel.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE "SInGameChat"

class SInGameChatImpl : public SInGameChat
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FChatChromeViewModel>& InChromeViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService) override
	{
		FriendStyle = *InArgs._FriendStyle;

		FontService = InFontService;

		FadeAnimation = FCurveSequence();
		FadeCurve = FadeAnimation.AddCurve(0.f, 0.5f, ECurveEaseFunction::Linear);

		ChromeViewModel = InChromeViewModel;
		ChromeViewModel->OnActiveTabChanged().AddSP(this, &SInGameChatImpl::HandleActiveTabChanged);
		ChatWindowContainer = SNew(SWidgetSwitcher);
		static const FText EntryHint = LOCTEXT("InGameChat", "Press tab to cycle channels");

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				[
					ChatWindowContainer.ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SInGameChatImpl::GetChatEntryVisibility)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0,0,5,0))
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&FriendStyle.FriendsChatStyle.TextStyle)
					.ColorAndOpacity(this, &SInGameChatImpl::GetOutgoingChannelColor)
					.Text(this, &SInGameChatImpl::GetOutgoingChannelText)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SChatEntryWidget, ChromeViewModel->GetChatEntryViewModel(), FontService.ToSharedRef())
					.FriendStyle(&FriendStyle)
					.HintText(EntryHint)
				]
			]
		]);

		RegenerateTabs();

		HandleActiveTabChanged();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SUserWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		if(!ChromeViewModel->IsChatMinimized())
		{
			if (IsHovered() || HasKeyboardFocus() || HasFocusedDescendants())
			{
				if (FadeAnimation.IsInReverse())
				{
					FadeAnimation.Reverse();
				}
				else if (FadeCurve.GetLerp() == 0.0f)
				{
					FadeAnimation.Play(this->AsShared());
				}
			}
			else
			{
				if (FadeCurve.GetLerp() == 1.0f)
				{
					FadeAnimation.PlayReverse(this->AsShared());
				}
			}
		}
	}

private:

	void RegenerateTabs()
	{
		for (const TSharedRef<FChatChromeTabViewModel>& Tab : ChromeViewModel->GetAllTabs())
		{
			if (!ChatWindows.Contains(Tab))
			{
				// Create Chat window for this tab
				TSharedRef<SChatWindow> ChatWindow = SNew(SChatWindow, Tab->GetChatViewModel().ToSharedRef(), FontService.ToSharedRef())
					.FriendStyle(&FriendStyle);

				ChatWindow->SetVisibility(EVisibility::HitTestInvisible);

				ChatWindowContainer->AddSlot()
					[
						ChatWindow
					];
				// Add Chat window to storage
				ChatWindows.Add(Tab, ChatWindow);
			}
		}
	}

	void HandleActiveTabChanged()
	{
		if (ChromeViewModel.IsValid())
		{
			TSharedPtr<FChatChromeTabViewModel> Tab = ChromeViewModel->GetActiveTab();
			if (Tab.IsValid())
			{
				TSharedPtr<SChatWindow> ChatWindow = *ChatWindows.Find(Tab);
				if (ChatWindow.IsValid())
				{
					ChatWindowContainer->SetActiveWidget(ChatWindow.ToSharedRef());
					ChatWindow->HandleWindowActivated();
				}
			}
		}
	}

	FText GetOutgoingChannelText() const
	{
		return ChromeViewModel->GetOutgoingChannelText();
	}

	FSlateColor GetOutgoingChannelColor() const
	{
		switch (ChromeViewModel->GetOutgoingChannel())
		{
			case EChatMessageType::Whisper : return FriendStyle.FriendsChatStyle.WhisperChatColor;
			case EChatMessageType::Global : return FriendStyle.FriendsChatStyle.GlobalChatColor;
			case EChatMessageType::Party : return FriendStyle.FriendsChatStyle.PartyChatColor;
			default:
				return FriendStyle.FriendsChatStyle.DefaultChatColor;
		}
	}

	EVisibility GetChatEntryVisibility() const
	{
		return ChromeViewModel->GetChatEntryVisibility();
	}
	

	FSlateColor GetTimedFadeSlateColor() const
	{
		return FLinearColor(1, 1, 1, FadeCurve.GetLerp());
	}

	// Holds the menu anchor
	TSharedPtr<SMenuAnchor> ActionMenu;

	TSharedPtr<FFriendsFontStyleService> FontService;
	
	TSharedPtr<FChatChromeViewModel> ChromeViewModel;
	TSharedPtr<SWidgetSwitcher> ChatWindowContainer;

	TMap<TSharedPtr<FChatChromeTabViewModel>, TSharedPtr<SChatWindow>> ChatWindows;
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	EChatOrientationType::Type ChatOrientation;

	// Holds the fade in animation
	FCurveSequence FadeAnimation;
	// Holds the fade in curve
	FCurveHandle FadeCurve;

};

TSharedRef<SInGameChat> SInGameChat::New()
{
	return MakeShareable(new SInGameChatImpl());
}

#undef LOCTEXT_NAMESPACE