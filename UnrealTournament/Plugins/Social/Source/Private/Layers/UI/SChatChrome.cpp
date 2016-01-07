// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatChromeViewModel.h"
#include "SChatChrome.h"
#include "SChatWindow.h"
#include "SChatEntryWidget.h"
#include "SChatChromeTabQuickSettings.h"
#include "SWidgetSwitcher.h"
#include "ChatChromeTabViewModel.h"
#include "ChatViewModel.h"
#include "ChatEntryViewModel.h"
#include "SChatMarkupTips.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE "SChatChrome"

class SChatChromeImpl : public SChatChrome
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FChatChromeViewModel>& InChromeViewModel, const TSharedRef<FFriendsFontStyleService> InFontService) override
	{
		FriendStyle = *InArgs._FriendStyle;
		FontService = InFontService;
		ChatOrientation = InArgs._ChatOrientation;

		FadeAnimation = FCurveSequence();
		FadeCurve = FadeAnimation.AddCurve(0.f, 0.5f, ECurveEaseFunction::Linear);

		NotificationAnimation = FCurveSequence();
		NotificationCurve = NotificationAnimation.AddCurve(0.f, 0.5f, ECurveEaseFunction::Linear);

		ChromeViewModel = InChromeViewModel;
		ChromeViewModel->OnActiveTabChanged().AddSP(this, &SChatChromeImpl::HandleActiveTabChanged);
		ChromeViewModel->OnVisibleTabsChanged().AddSP(this, &SChatChromeImpl::HandleVisibleTabsChanged);
		ChromeViewModel->OnPlayNotification().AddSP(this, &SChatChromeImpl::HandlePlayNotification);
		ChromeViewModel->OnRegenerateChat().AddSP(this, &SChatChromeImpl::HandleRegenerateChat);
		FChatChromeViewModel* ViewModelPtr = ChromeViewModel.Get();

		TSharedRef<SWidget> HeaderWidget = GenerateHeaderWidget();
		ChatWindowContainer = SNew(SWidgetSwitcher);

		bHasFocus = false;
		bRegenerateTabs = false;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+SVerticalBox::Slot()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.Visibility(this, &SChatChromeImpl::GetChatBackgroundVisibility)
					.BorderImage(&FriendStyle.FriendsChatChromeStyle.ChatBackgroundBrush)
					.BorderBackgroundColor(this, &SChatChromeImpl::GetTimedFadeSlateColor)
					.Padding(0)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.AutoHeight()
					[
						SNew(SBorder)
						.Visibility(this, &SChatChromeImpl::GetHeaderVisibility)
						.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
						.Padding(0)
						[
							HeaderWidget
						]
					]
					+SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SBorder)
						.Visibility(this, &SChatChromeImpl::GetChatWindowVisibility)
						.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
						.Padding(FriendStyle.FriendsChatChromeStyle.ChatWindowPadding)
						[
							ChatWindowContainer.ToSharedRef()
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.Padding(FriendStyle.FriendsChatChromeStyle.ChatWindowToEntryMargin)
			.AutoHeight()
			[
				// Chat Entry
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(&FriendStyle.FriendsChatChromeStyle.ChannelBackgroundBrush)
					.VAlign(VAlign_Center)
					.Padding(FriendStyle.FriendsChatChromeStyle.ChatChannelPadding)
					[
						SNew(STextBlock)
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
						.ColorAndOpacity(this, &SChatChromeImpl::GetOutgoingChannelColor)
						.Text(this, &SChatChromeImpl::GetOutgoingChannelText)
					]
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(2,0))
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SAssignNew(MarkupTipMenuAnchor, SMenuAnchor)
						.Placement(EMenuPlacement::MenuPlacement_AboveAnchor)
						.MenuContent
						(
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
							.Padding(0)
							[
								SNew(SChatMarkupTips, ChromeViewModel->GetChatTipViewModel(), InFontService)
								.MarkupStyle(&FriendStyle.FriendsMarkupStyle)
							]
						)
					]
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(&FriendStyle.FriendsChatChromeStyle.ChatEntryBackgroundBrush)
						.BorderBackgroundColor(this, &SChatChromeImpl::GetChatEntryBackgroundColor)
						.VAlign(VAlign_Center)
						.Padding(FriendStyle.FriendsChatChromeStyle.ChatEntryPadding)
						[
							SNew(SChatEntryWidget, ChromeViewModel->GetChatEntryViewModel(), InFontService)
							.FriendStyle(&FriendStyle)
						]
					]
				]
			]
		]);

		RegenerateTabs(false);

		HandleActiveTabChanged();

		ViewModelPtr->OnChatTipAvailable().AddSP(this, &SChatChromeImpl::HandleChatTipAvailable);
	}

	void HandleChatTipAvailable(bool IsAvailable)
	{
		MarkupTipMenuAnchor->SetIsOpen(IsAvailable, false);
	}

	virtual void HandleWindowActivated() override
	{
	}

	TSharedRef<SWidget> GenerateHeaderWidget()
	{
		return SNew(SHorizontalBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+SHorizontalBox::Slot()
		[
			SAssignNew(TabContainer, SHorizontalBox)
		];
		
	}

	void HandleVisibleTabsChanged()
	{
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SUserWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		if (bRegenerateTabs)
		{
			bRegenerateTabs = false;
			RegenerateTabs(true);
		}

		if(bHasFocus)
		{
			// Minimize if lost focus
			if(!IsHovered() && !HasFocusedDescendants())
			{
				SetMinimized(true);
			}
		}
		else if(HasFocusedDescendants())
		{
			// Restore if has focus
			SetMinimized(false);
		}

		if(NotificationAnimation.IsAtEnd())
		{
			NotificationAnimation.PlayReverse(this->AsShared());
		}
	}

private:

	void HandlePlayNotification(EChatMessageType::Type MessageType)
	{
		if(!NotificationAnimation.IsPlaying())
		{
			switch (MessageType)
			{
			case EChatMessageType::Global:
				NotificationTint = FriendStyle.FriendsChatStyle.GlobalChatColor;
				break;
			case EChatMessageType::Party:
				NotificationTint = FriendStyle.FriendsChatStyle.PartyChatColor;
				break;
			case EChatMessageType::Whisper:
				NotificationTint = FriendStyle.FriendsChatStyle.WhisperChatColor;
				break;
			default:
				NotificationTint = FriendStyle.FriendsChatStyle.GameChatColor;
			}

			NotificationAnimation.Play(this->AsShared());
		}
	}

	void HandleRegenerateChat()
	{
		bRegenerateTabs = true;
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

	FReply OnTabClicked(TWeakPtr<FChatChromeTabViewModel> TabPtr)
	{
		TSharedPtr<FChatChromeTabViewModel> Tab = TabPtr.Pin();
		if (Tab.IsValid())
		{
			ChromeViewModel->ActivateTab(Tab.ToSharedRef(), true);
		}
		return FReply::Handled();
	}

	FSlateColor GetTabBackgroundColor(TWeakPtr<FChatChromeTabViewModel> TabPtr) const
	{
		if(!FadeAnimation.IsPlaying() && ChromeViewModel->IsChatMinimized())
		{
			return FLinearColor::Transparent;
		}

		FLinearColor TabColor = FriendStyle.FriendsChatChromeStyle.NoneActiveTabColor;
		TSharedPtr<FChatChromeTabViewModel> Tab = TabPtr.Pin();
		if (Tab.IsValid())
		{
			if (Tab == ChromeViewModel->GetActiveTab() || Tab->HasPendingMessages())
			{
				EChatMessageType::Type Channel = Tab->GetTabID();
				switch (Channel)
				{
				case EChatMessageType::Global:
					TabColor = FriendStyle.FriendsChatStyle.GlobalChatColor;
					break;
				case EChatMessageType::Party:
					TabColor = FriendStyle.FriendsChatStyle.PartyChatColor;
					break;
				case EChatMessageType::Whisper:
					TabColor = FriendStyle.FriendsChatStyle.WhisperChatColor;
					break;
				default:
					TabColor = FriendStyle.FriendsChatStyle.GameChatColor;
				}
			}
			
			if(Tab->HasPendingMessages())
			{
				if(NotificationAnimation.IsPlaying())
				{
					float Tint = 0.5f * NotificationAnimation.GetLerp();
					TabColor *= FLinearColor(Tint,Tint,Tint,1);
				}
				else
				{
					TabColor *= FLinearColor(0.2f,0.2f,0.2f,1);
				}
			}
		}
		if(FadeAnimation.IsPlaying())
		{
			TabColor = TabColor.CopyWithNewOpacity(FadeAnimation.GetLerp() * TabColor.A);
		}
		return TabColor;
	}

	FSlateColor GetTabFontColor(TWeakPtr<FChatChromeTabViewModel> TabPtr) const
	{
		if(!FadeAnimation.IsPlaying() && ChromeViewModel->IsChatMinimized())
		{
			return FLinearColor::Transparent;
		}

		FLinearColor FontColor = FriendStyle.FriendsChatChromeStyle.TabFontColorInverted;
		TSharedPtr<FChatChromeTabViewModel> Tab = TabPtr.Pin();
		if (Tab.IsValid() && Tab == ChromeViewModel->GetActiveTab())
		{
			FontColor = FriendStyle.FriendsChatChromeStyle.TabFontColor;
		}

		if(FadeAnimation.IsPlaying())
		{
			FontColor = FontColor.CopyWithNewOpacity(FadeAnimation.GetLerp() * FontColor.A);
		}

		return FontColor;
	}

	EVisibility GetTabVisibility(TWeakPtr<FChatChromeTabViewModel> TabPtr) const
	{
		TSharedPtr<FChatChromeTabViewModel> Tab = TabPtr.Pin();
		if (Tab.IsValid() && Tab->IsTabVisible())
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}
	
	void RegenerateTabs(bool bForceCreateChatWindows)
	{
		if (bForceCreateChatWindows)
		{
			ChatWindows.Empty();
		}

		TabContainer->ClearChildren();

		// Clear Tabs
		for (const TSharedRef<FChatChromeTabViewModel>& Tab : ChromeViewModel->GetAllTabs())
		{
			// Create Button for Tab
			TSharedRef<SButton> TabButton = SNew(SButton)
			.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
			.OnClicked(this, &SChatChromeImpl::OnTabClicked, TWeakPtr<FChatChromeTabViewModel>(Tab))
			.VAlign(VAlign_Center)
			.ContentPadding(0)
			[
				SNew(SBox)
				.WidthOverride(FriendStyle.FriendsChatChromeStyle.TabWidth)
				.Visibility(this, &SChatChromeImpl::GetTabVisibility, TWeakPtr<FChatChromeTabViewModel>(Tab))
				[
					SNew(SBorder)
					.HAlign(HAlign_Center)
					.BorderImage(&FriendStyle.FriendsChatChromeStyle.TabBackgroundBrush)
					.BorderBackgroundColor(this, &SChatChromeImpl::GetTabBackgroundColor, TWeakPtr<FChatChromeTabViewModel>(Tab))
					.Padding(FriendStyle.FriendsChatChromeStyle.TabPadding)
					[
						SNew(STextBlock)
						.ColorAndOpacity(this, &SChatChromeImpl::GetTabFontColor, TWeakPtr<FChatChromeTabViewModel>(Tab))
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
						.Text(Tab->GetTabText())
					]
				]
			];

			// Create container for a quick settings options
			TSharedPtr<SBox> QuickSettingsContainer = SNew(SBox);

			// Add Tab
			TabContainer->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.Padding(0)
			[
				TabButton
			];

			// Add a quick settings if needed
			//QuickSettingsContainer->SetContent(GetQuickSettingsWidget(Tab).ToSharedRef());

			if (!ChatWindows.Contains(Tab))
			{
				// Create Chat window for this tab
				TSharedRef<SChatWindow> ChatWindow = SNew(SChatWindow, Tab->GetChatViewModel().ToSharedRef(), FontService.ToSharedRef())
					.FriendStyle(&FriendStyle);

				ChatWindow->SetVisibility(EVisibility::SelfHitTestInvisible);

				ChatWindowContainer->AddSlot()
					[
						ChatWindow
					];
				// Add Chat window to storage
				ChatWindows.Add(Tab, ChatWindow);
			}
		}
	}

	TSharedPtr<SWidget> GetQuickSettingsWidget(const TSharedRef<FChatChromeTabViewModel>& Tab) const
	{
		return SNew(SComboButton)
			.ForegroundColor(FLinearColor::White)
			.ButtonColorAndOpacity(FLinearColor::Transparent)
			.ContentPadding(FMargin(2, 2.5))
			.MenuContent()
			[
				SNew(SChatChromeTabQuickSettings, Tab)
				.ChatType(Tab->GetChatViewModel()->GetDefaultChannelType())
				.FriendStyle(&FriendStyle)
			];
	}

	EVisibility GetChatBackgroundVisibility() const
	{
		return FadeAnimation.IsPlaying() || !ChromeViewModel->IsChatMinimized() ? EVisibility::Visible :EVisibility::Collapsed;
	}

	EVisibility GetHeaderVisibility() const
	{
		return FadeAnimation.IsPlaying() || !ChromeViewModel->IsChatMinimized() ? EVisibility::Visible :EVisibility::Hidden;
	}

	EVisibility GetChatWindowVisibility() const
	{
		return FadeAnimation.IsPlaying() || !ChromeViewModel->IsChatMinimized() ? EVisibility::Visible :EVisibility::HitTestInvisible;
	}

	void SetMinimized(bool bIsMinimized)
	{
		bHasFocus = !bIsMinimized;

		if (bHasFocus)
		{
			FadeAnimation.Play(this->AsShared());
		}
		else
		{
			FadeAnimation.PlayReverse(this->AsShared());
		}

		ChromeViewModel->SetChatMinimized(bIsMinimized);
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

	FSlateColor GetChatEntryBackgroundColor() const
	{
		FLinearColor BackgroundColor = FriendStyle.FriendsChatChromeStyle.ChatEntryBackgroundColor;

		if(ChromeViewModel->IsChatMinimized() && ChromeViewModel->HasPendingMessages())
		{
			if(NotificationAnimation.IsPlaying())
			{
				float Tint = 0.5f * NotificationAnimation.GetLerp();
				BackgroundColor = NotificationTint * FLinearColor(Tint,Tint,Tint,1);
			}
			else
			{
				BackgroundColor = NotificationTint * FLinearColor(0.2f,0.2,0.2f,1);
			}
		}
		return BackgroundColor;
	}

	EVisibility GetMinimizeVisibility() const
	{
		return (!ChromeViewModel->IsMinimizeEnabled() || ChromeViewModel->IsChatMinimized()) ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FSlateColor GetTimedFadeSlateColor() const
	{
		FLinearColor BackgroundColor = FriendStyle.FriendsChatChromeStyle.ChatBackgroundColor;

		if(FadeAnimation.IsPlaying())
		{
			BackgroundColor = BackgroundColor.CopyWithNewOpacity(FadeAnimation.GetLerp() * BackgroundColor.A);
		}

		return BackgroundColor;
	}

private:

	// Holds the menu anchor
	TSharedPtr<SMenuAnchor> ActionMenu;

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;
	
	TSharedPtr<FChatChromeViewModel> ChromeViewModel;
	TSharedPtr<SHorizontalBox> TabContainer;
	TSharedPtr<SVerticalBox> VerticalTabContainer;
	TSharedPtr<SMenuAnchor> MarkupTipMenuAnchor;

	TSharedPtr<SWidgetSwitcher> ChatWindowContainer;
	SHorizontalBox::FSlot* VerticalTabBox;

	TMap<TSharedPtr<FChatChromeTabViewModel>, TSharedPtr<SChatWindow>> ChatWindows;
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	EChatOrientationType::Type ChatOrientation;

	// Holds the fade in animation
	FCurveSequence FadeAnimation;
	FCurveSequence NotificationAnimation;
	// Holds the fade in curve
	FCurveHandle FadeCurve;
	FCurveHandle NotificationCurve;
	FLinearColor NotificationTint;
	bool bHasFocus;
	bool bRegenerateTabs;
};

TSharedRef<SChatChrome> SChatChrome::New()
{
	return MakeShareable(new SChatChromeImpl());
}

#undef LOCTEXT_NAMESPACE