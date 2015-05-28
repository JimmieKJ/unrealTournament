// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatViewModel.h"
#include "ChatItemViewModel.h"
#include "ChatDisplayOptionsViewModel.h"
#include "SChatWindow.h"
#include "SChatItem.h"
#include "SFriendsList.h"

#define LOCTEXT_NAMESPACE "SChatWindow"

class SChatWindowImpl : public SChatWindow
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FChatDisplayOptionsViewModel>& InDisplayViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		MenuMethod = InArgs._Method;
		MaxChatLength = InArgs._MaxChatLength;
		DisplayViewModel = InDisplayViewModel;
		DisplayViewModel->OnChatListSetFocus().AddSP(this, &SChatWindowImpl::SetFocus);
		SharedChatViewModel = DisplayViewModel->GetChatViewModel();
		FChatViewModel* ViewModelPtr = SharedChatViewModel.Get();
		ViewModelPtr->OnChatListUpdated().AddSP(this, &SChatWindowImpl::RefreshChatList);
		TimeTransparency = 0.0f;

		FadeAnimation = FCurveSequence();
		FadeCurve = FadeAnimation.AddCurve(0.f, 0.5f, ECurveEaseFunction::Linear);
		if (DisplayViewModel.IsValid())
		{
			DisplayViewModel->SetCurve(FadeCurve);
		}
		
		ExternalScrollbar = SNew(SScrollBar)
		.Thickness(FVector2D(4, 4))
		.Style(&FriendStyle.ScrollBarStyle)
		.AlwaysShowScrollbar(true);

		TSharedRef<STableViewBase> TableBase = ConstructChatList();
		TSharedPtr<SButton> SendFriendRequestButton;
		TSharedPtr<SButton> InviteToGameButton;
		TSharedPtr<SButton> OpenWhisperButton;
		TSharedPtr<SButton> IgnoreFriendRequestButton;
		TSharedPtr<SButton> AcceptFriendRequestButton;
		TSharedPtr<SButton> CancelFriendRequestButton;
		TSharedPtr<SUniformGridPanel> UniformPanel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.ChatBackgroundBrush)
			.BorderBackgroundColor(this, &SChatWindowImpl::GetTimedFadeSlateColor)
			.Padding(0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SScrollBorder, TableBase)
						[
							SAssignNew(ChatListBox, SBox)
							[
								TableBase
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.Padding(FMargin(10, 20, 10, 20))
					[
						SNew( SBorder )
						.BorderBackgroundColor(FLinearColor::Transparent)
						.ColorAndOpacity(this, &SChatWindowImpl::GetTimedFadeColor)
						.Padding(FMargin(0))
						[
							ExternalScrollbar.ToSharedRef()
						]
					]
				]
				+SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.AutoHeight()
				[
					SNew(SBox)
					.MinDesiredHeight(74)
					[
						SNew(SBorder)
						.BorderImage(&FriendStyle.ChatFooterBrush)
						.BorderBackgroundColor(this, &SChatWindowImpl::GetTimedFadeSlateColor)
						.Padding(FMargin(10, 10, 10, 10))
						[
							SNew(SHorizontalBox)
							.Visibility(this, &SChatWindowImpl::GetActionVisibility)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Fill)
							[
								SAssignNew(ActionMenu, SMenuAnchor)
								.Placement(MenuMethod != EPopupMethod::UseCurrentWindow ? EMenuPlacement::MenuPlacement_AboveAnchor : EMenuPlacement::MenuPlacement_BelowAnchor)  	// MenuPlacement_BelowAnchor is a workaround until a workarea bug is fixed in SlateApplication menu placement code
								.IsEnabled(this, &SChatWindowImpl::HasMenuOptions)
								.Method(EPopupMethod::UseCurrentWindow)
								.OnGetMenuContent(this, &SChatWindowImpl::GetMenuContent)
								.Visibility(this, &SChatWindowImpl::GetChatChannelVisibility)
								[
									SNew(SButton)
									.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
									.ContentPadding(FMargin(2, 2.5))
									.OnClicked(this, &SChatWindowImpl::HandleActionDropDownClicked)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Center)
									.Cursor(EMouseCursor::Hand)
									[
										SNew(SImage)
										.Image(this, &SChatWindowImpl::GetChatChannelIcon)
									]
								]
							]
							+SHorizontalBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								[
									SNew(SHorizontalBox)
									.Visibility(this, &SChatWindowImpl::GetChatEntryVisibility)
									+SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Fill)
									.AutoWidth()
									[
										SNew(SBorder)
										.BorderImage(&FriendStyle.ChatContainerBackground)
										.BorderBackgroundColor(FLinearColor(FColor(255, 255, 255, 128)))
										[
											SNew(SHorizontalBox)
											.Visibility(this, &SChatWindowImpl::GetFriendNameVisibility)
											+SHorizontalBox::Slot()
											.Padding(5,0)
											.AutoWidth()
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
												.Font(FriendStyle.FriendsFontStyleSmallBold)
												.Text(ViewModelPtr, &FChatViewModel::GetChatGroupText)
											]
											+SHorizontalBox::Slot()
											.Padding(0, 0, 5, 0)
											.AutoWidth()
											.VAlign(VAlign_Center)
											[
												SNew(SButton)
												.Visibility(this, &SChatWindowImpl::GetFriendActionVisibility)
												.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
												.ContentPadding(FMargin(-2,3))
												.OnClicked(this, &SChatWindowImpl::HandleFriendActionDropDownClicked)
												[
													SAssignNew(ChatItemActionMenu, SMenuAnchor)
													.Placement(EMenuPlacement::MenuPlacement_BelowAnchor)	// MenuPlacement_BelowAnchor is a workaround until a workarea bug is fixed in SlateApplication menu placement code
													.Method(EPopupMethod::UseCurrentWindow)
													.OnGetMenuContent(this, &SChatWindowImpl::GetFriendActionMenu)
													[
														SNew(SImage)
														.Image(&FriendStyle.FriendsCalloutBrush)
													]
												]
											]
										]
									]
									+SHorizontalBox::Slot()
									[
										SAssignNew(ChatTextBox, SMultiLineEditableTextBox)
										.Style(&FriendStyle.ChatEditableTextStyle)
										.TextStyle(&FriendStyle.TextStyle)
										.ClearKeyboardFocusOnCommit(false)
										.OnTextCommitted(this, &SChatWindowImpl::HandleChatEntered)
										.HintText(InArgs._ActivationHintText)
										.OnTextChanged(this, &SChatWindowImpl::OnChatTextChanged)
										.IsEnabled(this, &SChatWindowImpl::IsChatEntryEnabled)
										.ModiferKeyForNewLine(EModifierKey::Shift)
										.WrapTextAt(this, &SChatWindowImpl::GetMessageBoxWrapWidth)
									]
								]
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SBorder)
									.Padding(FMargin(10, 10))
									.VAlign(VAlign_Center)									
									.Visibility(this, &SChatWindowImpl::GetLostConnectionVisibility)
									.BorderImage(&FriendStyle.ChatContainerBackground)
									.BorderBackgroundColor(FLinearColor(FColor(255, 255, 255, 128)))
									[
										SNew(STextBlock)
										.ColorAndOpacity(FLinearColor::Red)
										.AutoWrapText(true)
										.Font(FriendStyle.FriendsFontStyleSmallBold)
										.Text(ViewModelPtr, &FChatViewModel::GetChatDisconnectText)
									]
								]
								+SVerticalBox::Slot()
								[
									SNew(SBorder)
									.Visibility(this, &SChatWindowImpl::GetConfirmationVisibility)
									.BorderImage(&FriendStyle.ChatContainerBackground)
									.BorderBackgroundColor(FLinearColor(FColor(255, 255, 255, 128)))
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot()
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot()
											.Padding(10, 10, 10, 0)
											.VAlign(VAlign_Top)
											.HAlign(HAlign_Left)
											.AutoHeight()
											[
												SNew(STextBlock)
												.Font(FriendStyle.FriendsFontStyleBold)
												.ColorAndOpacity(FriendStyle.DefaultFontColor)
												.Text(ViewModelPtr, &FChatViewModel::GetChatGroupText)
											]
											+ SVerticalBox::Slot()
											.VAlign(VAlign_Top)
											.HAlign(HAlign_Left)
											.Padding(0, 10)
											.AutoHeight()
											[
												SAssignNew(UniformPanel, SUniformGridPanel)
												.SlotPadding(FMargin(10.0f, 0.0f))
												.MinDesiredSlotWidth(150.0f)
												.MinDesiredSlotHeight(30.0f)
												+ SUniformGridPanel::Slot(0,0)
												[
													SAssignNew(SendFriendRequestButton, SButton)
													.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::SendFriendRequest)))
													.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::SendFriendRequest)
													.Visibility(ViewModelPtr, &FChatViewModel::GetFriendRequestVisibility)
													.HAlign(HAlign_Center)
													.VAlign(VAlign_Center)
													.Cursor(EMouseCursor::Hand)
													[
														SNew(STextBlock)
														.Font(FriendStyle.FriendsFontStyleSmallBold)
														.Text(EFriendActionType::ToText(EFriendActionType::SendFriendRequest))
														.ColorAndOpacity(FSlateColor::UseForeground())
													]
												]
												+ SUniformGridPanel::Slot(0, 0)
												[
													SAssignNew(InviteToGameButton, SButton)
													.Visibility(ViewModelPtr, &FChatViewModel::GetInviteToGameVisibility)
													.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::InviteToGame)))
													.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::InviteToGame)
													.ContentPadding(5)
													.VAlign(VAlign_Center)
													.HAlign(HAlign_Center)
													.Cursor(EMouseCursor::Hand)
													[
														SNew(STextBlock)
														.Font(FriendStyle.FriendsFontStyleSmallBold)
														.Text(EFriendActionType::ToText(EFriendActionType::InviteToGame))
														.ColorAndOpacity(FSlateColor::UseForeground())
													]
												]
												+ SUniformGridPanel::Slot(0, 0)
												[
													SAssignNew(OpenWhisperButton, SButton)
													.Visibility(ViewModelPtr, &FChatViewModel::GetOpenWhisperVisibility)
													.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::Whisper)))
													.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::Whisper)
													.ContentPadding(5)
													.VAlign(VAlign_Center)
													.HAlign(HAlign_Center)
													.Cursor(EMouseCursor::Hand)
													[
														SNew(STextBlock)
														.Font(FriendStyle.FriendsFontStyleSmallBold)
														.Text(EFriendActionType::ToText(EFriendActionType::Whisper))
														.ColorAndOpacity(FSlateColor::UseForeground())
													]
												]
												+ SUniformGridPanel::Slot(0, 0)
												[
													SAssignNew(CancelFriendRequestButton, SButton)
													.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
													.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::CancelFriendRequest)
													.Visibility(ViewModelPtr, &FChatViewModel::GetCancelFriendRequestVisibility)
													.HAlign(HAlign_Center)
													.VAlign(VAlign_Center)
													.Cursor(EMouseCursor::Hand)
													[
														SNew(STextBlock)
														.Font(FriendStyle.FriendsFontStyleSmallBold)
														.Text(EFriendActionType::ToText(EFriendActionType::CancelFriendRequest))
														.ColorAndOpacity(FSlateColor::UseForeground())
													]
												]
												+ SUniformGridPanel::Slot(0, 0)
												[
													SAssignNew(AcceptFriendRequestButton, SButton)
													.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::AcceptFriendRequest)))
													.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::AcceptFriendRequest)
													.Visibility(ViewModelPtr, &FChatViewModel::GetAcceptFriendRequestVisibility)
													.HAlign(HAlign_Center)
													.VAlign(VAlign_Center)
													.Cursor(EMouseCursor::Hand)
													[
														SNew(STextBlock)
														.Font(FriendStyle.FriendsFontStyleSmallBold)
														.Text(EFriendActionType::ToText(EFriendActionType::AcceptFriendRequest))
														.ColorAndOpacity(FSlateColor::UseForeground())
													]
												]
												+ SUniformGridPanel::Slot(1, 0)
												[
													SAssignNew(IgnoreFriendRequestButton, SButton)
													.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::IgnoreFriendRequest)))
													.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::IgnoreFriendRequest)
													.Visibility(ViewModelPtr, &FChatViewModel::GetIgnoreFriendRequestVisibility)
													.HAlign(HAlign_Center)
													.VAlign(VAlign_Center)
													.Cursor(EMouseCursor::Hand)
													[
														SNew(STextBlock)
														.Font(FriendStyle.FriendsFontStyleSmallBold)
														.Text(EFriendActionType::ToText(EFriendActionType::IgnoreFriendRequest))
														.ColorAndOpacity(FSlateColor::UseForeground())
													]
												]
											]
										]
										+ SHorizontalBox::Slot()
										.HAlign(HAlign_Left)
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(SButton)
											.ButtonStyle(&FriendStyle.AddFriendCloseButtonStyle)
											.OnClicked(this, &SChatWindowImpl::CancelActionClicked)
											.Cursor(EMouseCursor::Hand)
										]
									]
								]
							]
						]
					]
				]
			]
		]);

		if (SendFriendRequestButton.IsValid())
		{
			SendFriendRequestButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatWindowImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(SendFriendRequestButton), EFriendActionType::ToActionLevel(EFriendActionType::SendFriendRequest))));
		}

		if (InviteToGameButton.IsValid())
		{
			InviteToGameButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatWindowImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(InviteToGameButton), EFriendActionType::ToActionLevel(EFriendActionType::InviteToGame))));
		}

		if (OpenWhisperButton.IsValid())
		{
			OpenWhisperButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatWindowImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(OpenWhisperButton), EFriendActionType::ToActionLevel(EFriendActionType::Whisper))));
		}

		if (AcceptFriendRequestButton.IsValid())
		{
			AcceptFriendRequestButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatWindowImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(AcceptFriendRequestButton), EFriendActionType::ToActionLevel(EFriendActionType::AcceptFriendRequest))));
		}

		if (CancelFriendRequestButton.IsValid())
		{
			CancelFriendRequestButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatWindowImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(CancelFriendRequestButton), EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest))));
		}

		if (IgnoreFriendRequestButton.IsValid())
		{
			IgnoreFriendRequestButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateSP(this, &SChatWindowImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(IgnoreFriendRequestButton), EFriendActionType::ToActionLevel(EFriendActionType::IgnoreFriendRequest))));
		}

		RegenerateChatList();
	}

	virtual void HandleWindowActivated() override
	{
		FSlateApplication::Get().SetKeyboardFocus(ChatTextBox, EFocusCause::WindowActivate);
	}

	FSlateColor GetForegroundWhenHovered(TWeakPtr<SButton> WidgetInQuestionPtr, const EFriendActionLevel ActionLevel) const
	{
		const TSharedPtr<SButton> WidgetInQuestion = WidgetInQuestionPtr.Pin();
		const bool IsDisabled = WidgetInQuestion.IsValid() && !WidgetInQuestion->IsEnabled();
		const bool IsHovered = WidgetInQuestion.IsValid() && WidgetInQuestion->IsHovered();

		if (IsDisabled)
		{
			return FLinearColor::Black;
		}

		if (ActionLevel == EFriendActionLevel::Action)
		{
			return FLinearColor::White;
		}

		if (IsHovered)
		{
			return FriendStyle.ButtonInvertedForegroundColor;
		}

		return FriendStyle.ButtonForegroundColor;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SUserWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		int32 NewWindowWidth = FMath::FloorToInt(AllottedGeometry.GetLocalSize().X);
		if(NewWindowWidth != WindowWidth)
		{
			WindowWidth = NewWindowWidth;
			RegenerateChatList();
		}

		if(DisplayViewModel.IsValid())
		{
			if (IsHovered() || ChatTextBox->HasKeyboardFocus())
			{
				if (FadeCurve.GetLerp() == 0.0f)
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

				if (FadeAnimation.IsPlaying())
				{
					TArray<TSharedRef<FChatItemViewModel>>& FilteredChatList = SharedChatViewModel->GetMessages();
					if (FilteredChatList.Num() > 0)
					{
						ChatList->RequestScrollIntoView(FilteredChatList.Last());
					}
				}
			}
		}

		if(!bUserHasScrolled && ChatList.IsValid() && ChatList->IsUserScrolling())
		{
			bUserHasScrolled = true;
		}
	}

	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		bUserHasScrolled = true;
		return FReply::Unhandled();
	}

private:

	int32 MaxChatLength;

	void OnChatTextChanged(const FText& CurrentText)
	{
		CheckLimit(CurrentText);
	}

	bool CheckLimit(const FText& CurrentText)
	{
		int32 OverLimit = CurrentText.ToString().Len() - MaxChatLength;
		if (OverLimit > 0)
		{
			ChatTextBox->SetError(FText::Format(LOCTEXT("TooManyCharactersFmt", "{0} Characters Over Limit"), FText::AsNumber(OverLimit)));
			return false;
		}

		// no error
		ChatTextBox->SetError(FText());
		return true;
	}

	TSharedRef<ITableRow> MakeChatWidget(TSharedRef<FChatItemViewModel> ChatMessage, const TSharedRef<STableViewBase>& OwnerTable)
	{
		if(bUserHasScrolled)
		{
			bAutoScroll = ChatMessage == SharedChatViewModel->GetMessages().Last() ? true : false;
		}

		TSharedRef<SWidget> Content = SNew(SChatItem, ChatMessage, DisplayViewModel.ToSharedRef())
			.FriendStyle(&FriendStyle)
			.Method(MenuMethod)
			.ChatWidth(this, &SChatWindowImpl::GetChatWrapWidth);

		if (!SharedChatViewModel->IsChatChannelLocked())
		{
			return SNew(STableRow< TSharedPtr<SWidget> >, OwnerTable)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.OnClicked(SharedChatViewModel.Get(), &FChatViewModel::HandleSelectionChanged, ChatMessage)
				.VAlign(VAlign_Center)
				[
						Content
				]
			];
		}

		return SNew(STableRow< TSharedPtr<SWidget> >, OwnerTable)
		[
			Content
		];
	}

	FReply HandleActionDropDownClicked() const
	{
		ActionMenu->IsOpen() ? ActionMenu->SetIsOpen(false) : ActionMenu->SetIsOpen(true);
		return FReply::Handled();
	}

	FReply HandleFriendActionDropDownClicked() const
	{
		ChatItemActionMenu->IsOpen() ? ChatItemActionMenu->SetIsOpen(false) : ChatItemActionMenu->SetIsOpen(true);
		return FReply::Handled();
	}

	/**
	 * Generate the action menu.
	 * @return the action menu widget
	 */
	TSharedRef<SWidget> GetMenuContent()
	{
		TSharedPtr<SVerticalBox> ChannelSelection = SNew(SVerticalBox);
		
		for( const auto& RecentFriend : SharedChatViewModel->GetRecentOptions())
		{
			ChannelSelection->AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(&FriendStyle.FriendListItemButtonStyle)
				.OnClicked(this, &SChatWindowImpl::HandleChannelWhisperChanged, RecentFriend)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(RecentFriend->DisplayName)
					.Font(FriendStyle.FriendsFontStyleSmallBold)
					.ColorAndOpacity(FriendStyle.DefaultFontColor)
				]
			];
		}

		TArray<EChatMessageType::Type> ChatMessageOptions;
		DisplayViewModel->EnumerateChatChannelOptionsList(ChatMessageOptions);
		for( const auto& Option : ChatMessageOptions)
		{
			FSlateBrush* ChatImage = nullptr;

			switch(Option)
			{
				case EChatMessageType::Global: ChatImage =  &FriendStyle.ChatGlobalBrush; break;
				case EChatMessageType::Whisper: ChatImage = &FriendStyle.ChatWhisperBrush; break;
				case EChatMessageType::Party: ChatImage = &FriendStyle.ChatPartyBrush; break;
			}

			FLinearColor ChannelColor = FLinearColor::Gray;
			switch(Option)
			{
				case EChatMessageType::Global: ChannelColor = FriendStyle.DefaultChatColor; break;
				case EChatMessageType::Whisper: ChannelColor = FriendStyle.WhisplerChatColor; break;
				case EChatMessageType::Party: ChannelColor = FriendStyle.PartyChatColor; break;
			}

			ChannelSelection->AddSlot()
			[
				SNew(SButton)
				.ButtonStyle(&FriendStyle.FriendListItemButtonStyle)
				.OnClicked(this, &SChatWindowImpl::HandleChannelChanged, Option)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(5)
					[
						SNew(SImage)
						.ColorAndOpacity(ChannelColor)
						.Image(ChatImage)
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(5)
					[
						SNew(STextBlock)
						.Text(EChatMessageType::ToText(Option))
						.ColorAndOpacity(FColor::White)
						.Font(FriendStyle.FriendsFontStyleSmallBold)
					]
				]
			];
		};

		TSharedRef<SWidget> Contents =
		SNew(SUniformGridPanel)
		+SUniformGridPanel::Slot(0,0)
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.ChatChannelsBackgroundBrush)
			.Visibility(ChannelSelection->GetChildren()->Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed)
			[
				ChannelSelection.ToSharedRef()
			]
		]
		+SUniformGridPanel::Slot(1,0)
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.ChatOptionsBackgroundBrush)
			.Padding(8.0f)
			.Visibility(SharedChatViewModel->IsGlobalChatEnabled() ? EVisibility::Visible : EVisibility::Collapsed)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			[
				SNew(SCheckBox)
				.Style(&FriendStyle.FriendCheckboxStyle)
				.OnCheckStateChanged(this, &SChatWindowImpl::OnGlobalOptionChanged)
				.IsChecked(this, &SChatWindowImpl::GetGlobalOptionState)
				[
					SNew(SBox)
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GlobalChatterCheckBox", "Global Chatter"))
						.Font(FriendStyle.FriendsFontStyleSmallBold)
						.ColorAndOpacity(FriendStyle.DefaultFontColor)
					]
				]
			]
		];

		return Contents;
	}

	bool HasMenuOptions() const
	{
		if (SharedChatViewModel->IsGlobalChatEnabled())
		{
			return true;
		}

		if (SharedChatViewModel->GetRecentOptions().Num() > 0)
		{
			return true;
		}

		TArray<EChatMessageType::Type> ChatMessageOptions;
		DisplayViewModel->EnumerateChatChannelOptionsList(ChatMessageOptions);
		if (ChatMessageOptions.Num() > 0)
		{
			return true;
		}

		return false;
	}

	TSharedRef<SWidget> GetFriendActionMenu()
	{
		TSharedPtr<SVerticalBox> FriendActionBox;

		TSharedRef<SWidget> Contents =
			SNew(SBorder)
			.BorderImage(&FriendStyle.ChatContainerBackground)
			.BorderBackgroundColor(FLinearColor(FColor(255, 255, 255, 128)))
			.Padding(8.0f)
			[
				SAssignNew(FriendActionBox, SVerticalBox)
			];
	
		TArray<EFriendActionType::Type> ActionList;
		SharedChatViewModel->EnumerateFriendOptions(ActionList);

		for (const auto& FriendAction : ActionList)
		{
			FriendActionBox->AddSlot()
				[
					SNew(SButton)
					.ButtonStyle(&FriendStyle.FriendListItemButtonStyle)
					.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, FriendAction)
					[
						SNew(STextBlock)
						.Font(FriendStyle.FriendsFontStyleSmallBold)
						.ColorAndOpacity(FriendStyle.DefaultFontColor)
						.Text(EFriendActionType::ToText(FriendAction))
					]
				];
		}

		return Contents;
	}

	FReply HandleChannelChanged(const EChatMessageType::Type NewOption)
	{
		SharedChatViewModel->SetChatChannel(NewOption);
		ActionMenu->SetIsOpen(false);
		SetFocus();
		return FReply::Handled();
	}

	FReply HandleChannelWhisperChanged(const TSharedPtr<FSelectedFriend> Friend)
	{
		SharedChatViewModel->SetWhisperChannel(Friend);
		ActionMenu->SetIsOpen(false);
		SetFocus();
		return FReply::Handled();
	}

	FReply HandleFriendActionClicked(EFriendActionType::Type ActionType)
	{
		SharedChatViewModel->PerformFriendAction(ActionType);
		ChatItemActionMenu->SetIsOpen(false);
		if (ActionType != EFriendActionType::Whisper)
		{
			SetFocus();
		}
		return FReply::Handled();
	}

	FReply CancelActionClicked()
	{
		SharedChatViewModel->CancelAction();
		return FReply::Handled();
	}

	void CreateChatList()
	{
		if (SharedChatViewModel->GetMessages().Num())
		{
			ChatList->RequestListRefresh();
			if(bAutoScroll)
			{
				ChatList->ScrollToBottom();
			}
		}
	}

	void HandleChatEntered(const FText& CommentText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter)
		{
			SendChatMessage();
		}
	}

	bool IsChatEntryEnabled() const
	{
		return SharedChatViewModel->IsChatChannelValid() && SharedChatViewModel->IsChatConnected();
	}

	FReply HandleSendClicked()
	{
		SendChatMessage();
		return FReply::Handled();
	}

	void SendChatMessage()
	{
		const FText& CurrentText = ChatTextBox->GetText();
		if (CheckLimit(CurrentText))
		{
			if (DisplayViewModel->SendMessage(CurrentText))
			{
				ChatTextBox->SetText(FText::GetEmpty());
			}
			else if(!CurrentText.IsEmpty())
			{
				ChatTextBox->SetError(LOCTEXT("CouldNotSend", "Unable to send chat message"));
			}
		}
	}

	void RefreshChatList()
	{
		CreateChatList();
	}

	void SetFocus()
	{
		FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
		if (ChatTextBox.IsValid())
		{
			FWidgetPath WidgetToFocusPath;

			bool bFoundPath = FSlateApplication::Get().FindPathToWidget(ChatTextBox.ToSharedRef(), WidgetToFocusPath);
			if (bFoundPath && WidgetToFocusPath.IsValid())
			{
				FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EKeyboardFocusCause::SetDirectly);
			}
		}
	}

	EVisibility GetFriendNameVisibility() const
	{
		return SharedChatViewModel->HasValidSelectedFriend() && !SharedChatViewModel->IsChatChannelLocked() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetFriendActionVisibility() const
	{
		return SharedChatViewModel->HasFriendChatAction() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetActionVisibility() const
	{
		return DisplayViewModel->GetEntryBarVisibility();
	}

	EVisibility GetChatChannelVisibility() const
	{
		return !SharedChatViewModel->IsChatChannelLocked() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetChatEntryVisibility() const
	{
		return DisplayViewModel->GetTextEntryVisibility();
	}

	EVisibility GetConfirmationVisibility() const
	{
		return DisplayViewModel->GetConfirmationVisibility();
	}

	EVisibility GetLostConnectionVisibility() const
	{
		return SharedChatViewModel->IsChatConnected() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	const FSlateBrush* GetChatChannelIcon() const
	{
		if (!SharedChatViewModel->IsChatChannelValid())
		{
			return &FriendStyle.ChatInvalidBrush;
		}

		switch(SharedChatViewModel->GetChatChannelType())
		{
			case EChatMessageType::Global: return &FriendStyle.ChatGlobalBrush; break;
			case EChatMessageType::Whisper: return &FriendStyle.ChatWhisperBrush; break;
			case EChatMessageType::Party: return &FriendStyle.ChatPartyBrush; break;
			default:
			return nullptr;
		}
	}

	void OnGlobalOptionChanged(ECheckBoxState NewState)
	{
		const bool bDisabled = NewState == ECheckBoxState::Unchecked;
		SharedChatViewModel->SetDisplayGlobalChat(!bDisabled);
	}

	ECheckBoxState GetGlobalOptionState() const
	{
		return SharedChatViewModel->IsDisplayingGlobalChat() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override
	{
		if(DisplayViewModel->ShouldCaptureFocus())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		
		return FReply::Handled();
	}

	FSlateColor GetTimedFadeSlateColor() const
	{
		return GetTimedFadeColor();
	}

	FLinearColor GetTimedFadeColor() const
	{
		return FLinearColor(1, 1, 1, FadeCurve.GetLerp());
	}

	void RegenerateChatList()
	{
		if (SharedChatViewModel->GetMessages().Num())
		{
			ChatList->RequestScrollIntoView(SharedChatViewModel->GetMessages().Last());
		}
		
		bUserHasScrolled = false;
		bAutoScroll = true;
	}

	TSharedRef<STableViewBase> ConstructChatList()
	{
		return SAssignNew(ChatList, SListView<TSharedRef<FChatItemViewModel>>)
			.ListItemsSource(&SharedChatViewModel->GetMessages())
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SChatWindowImpl::MakeChatWidget)
			.ExternalScrollbar(ExternalScrollbar.ToSharedRef())
			.ConsumeMouseWheel( EConsumeMouseWheel::Always );
	}

	float GetChatWrapWidth() const
		{
		return FMath::Max(WindowWidth, 0.0f);
		}
		
	float GetMessageBoxWrapWidth() const
	{
		return FMath::Max(WindowWidth - 30, 0.0f);
	}

private:

	// Holds the chat list
	TSharedPtr<SListView<TSharedRef<FChatItemViewModel> > > ChatList;

	// Holds the chat list
	TSharedPtr<SBox> ChatListBox;

	// Holds the scroll bar
	TSharedPtr<SScrollBar> ExternalScrollbar;

	// Holds the chat list display
	TSharedPtr<SMultiLineEditableTextBox> ChatTextBox;

	// Holds the menu anchor
	TSharedPtr<SMenuAnchor> ActionMenu;

	// Holds the Friend action button
	TSharedPtr<SMenuAnchor> ChatItemActionMenu;

	// Holds the shared chat view model - shared between all chat widgets
	TSharedPtr<FChatViewModel> SharedChatViewModel;

	// Holds the Friends List view model
	TSharedPtr<FChatDisplayOptionsViewModel> DisplayViewModel;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	// Holds the menu method - Full screen requires use owning window or crashes.
	EPopupMethod MenuMethod;

	// Should AutoScroll
	bool bAutoScroll;

	// Has the user scrolled
	bool bUserHasScrolled;

	// Holds the time transparency.
	float TimeTransparency;

	// Holds the window width
	float WindowWidth;

	// Holds the fade in animation
	FCurveSequence FadeAnimation;
	// Holds the fade in curve
	FCurveHandle FadeCurve;

	static const float CHAT_HINT_UPDATE_THROTTLE;
};

const float SChatWindowImpl::CHAT_HINT_UPDATE_THROTTLE = 1.0f;

TSharedRef<SChatWindow> SChatWindow::New()
{
	return MakeShareable(new SChatWindowImpl());
}

#undef LOCTEXT_NAMESPACE
