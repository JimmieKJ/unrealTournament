// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatViewModel.h"
#include "ChatItemViewModel.h"
#include "ChatDisplayOptionsViewModel.h"
#include "SChatWindow.h"
#include "SChatItem.h"

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

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.ChatBackgroundBrush)
			.BorderBackgroundColor(this, &SChatWindowImpl::GetTimedFadeSlateColor)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(FMargin(0,5))
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
			 		.AutoWidth()
					.VAlign(VAlign_Fill)
					[
						SNew( SBorder )
						.BorderBackgroundColor(FLinearColor::Transparent)
						.ColorAndOpacity(this, &SChatWindowImpl::GetTimedFadeColor)
						[
							ExternalScrollbar.ToSharedRef()
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
			 		[
						SAssignNew(ChatListContainer, SBorder)
						.BorderBackgroundColor(FLinearColor::Transparent)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Fill)
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
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SAssignNew(ChatTextBox, SEditableTextBox)
								.Style(&FriendStyle.ChatEditableTextStyle)
								.ClearKeyboardFocusOnCommit(false)
								.OnTextCommitted(this, &SChatWindowImpl::HandleChatEntered)
								.HintText(InArgs._ActivationHintText)
								.OnTextChanged(this, &SChatWindowImpl::OnChatTextChanged)
								.IsEnabled(this, &SChatWindowImpl::IsChatEntryEnabled)
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
								.Padding(5, 0)
								.AutoWidth()
								.VAlign(VAlign_Center)
								.MaxWidth(200)
								[
									SNew(STextBlock)
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(ViewModelPtr, &FChatViewModel::GetChatGroupText)
								]
								+SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 5, 0)
								[
									SNew(SButton)
									.Visibility(ViewModelPtr, &FChatViewModel::GetFriendRequestVisibility)
									.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
									.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::SendFriendRequest)
									.ContentPadding(5)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Font(FriendStyle.FriendsFontStyleSmallBold)
										.ColorAndOpacity(FriendStyle.DefaultFontColor)
										.Text(FText::FromString("Add Friend"))
									]
								]
								+SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 5, 0)
								[
									SNew(SButton)
									.Visibility(ViewModelPtr, &FChatViewModel::GetInviteToGameVisibility)
									.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
									.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::InviteToGame)
									.ContentPadding(5)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Font(FriendStyle.FriendsFontStyleSmallBold)
										.ColorAndOpacity(FriendStyle.DefaultFontColor)
										.Text(FText::FromString("Invite To Game"))
									]
								]
								+SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 5, 0)
								[
									SNew(SButton)
									.Visibility(ViewModelPtr, &FChatViewModel::GetOpenWhisperVisibility)
									.ButtonStyle(&FriendStyle.FriendGeneralButtonStyle)
									.OnClicked(this, &SChatWindowImpl::HandleFriendActionClicked, EFriendActionType::Whisper)
									.ContentPadding(5)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Font(FriendStyle.FriendsFontStyleSmallBold)
										.ColorAndOpacity(FriendStyle.DefaultFontColor)
										.Text(FText::FromString("Whisper"))
									]
								]
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								.Padding(0, 0, 5, 0)
								.AutoWidth()
								[
									SNew(SButton)
									.ButtonStyle(&FriendStyle.AddFriendCloseButtonStyle)
									.OnClicked(this, &SChatWindowImpl::CancelActionClicked)
								]
							]
						]
					]
				]
			]
		]);

		RegenerateChatList();
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
					FadeAnimation.Play();
				}
			}
			else
			{
				if (FadeCurve.GetLerp() == 1.0f)
				{
					FadeAnimation.PlayReverse();
				}

				if (FadeAnimation.IsPlaying())
				{
				    TArray<TSharedRef<FChatItemViewModel>>& FilteredChatList = SharedChatViewModel->GetFilteredChatList();
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
			bAutoScroll = ChatMessage == SharedChatViewModel->GetFilteredChatList().Last() ? true : false;
		}

		return SNew(STableRow< TSharedPtr<SWidget> >, OwnerTable)
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.OnClicked(SharedChatViewModel.Get(), &FChatViewModel::HandleSelectionChanged, ChatMessage)
			.VAlign(VAlign_Center)
			[
				SNew(SChatItem, ChatMessage, DisplayViewModel.ToSharedRef())
				.FriendStyle(&FriendStyle)
				.Method(MenuMethod)
				.ChatWidth(WindowWidth)
			]
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
					.Text(RecentFriend->FriendName)
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
		if(SharedChatViewModel->GetFilteredChatList().Num())
		{
			ChatList->RequestListRefresh();
			if(bAutoScroll)
			{
				ChatList->RequestScrollIntoView(SharedChatViewModel->GetFilteredChatList().Last());
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
		return SharedChatViewModel->IsChatChannelValid();
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
		return SharedChatViewModel->HasValidSelectedFriend() ? EVisibility::Visible : EVisibility::Collapsed;
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

		FFriendsAndChatManager::Get()->GetAnalytics().RecordToggleChat(TEXT("Global"), !bDisabled, TEXT("Social.Chat.Toggle"));
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
		ChatListContainer->ClearContent();
		ChatListContainer->SetContent(
			SAssignNew(ChatList, SListView<TSharedRef<FChatItemViewModel>>)
			.ListItemsSource(&SharedChatViewModel->GetFilteredChatList())
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SChatWindowImpl::MakeChatWidget)
			.ExternalScrollbar(ExternalScrollbar.ToSharedRef())
		);

		if(SharedChatViewModel->GetFilteredChatList().Num())
		{
			ChatList->RequestScrollIntoView(SharedChatViewModel->GetFilteredChatList().Last());
		}
		
		bUserHasScrolled = false;
		bAutoScroll = true;
	}

private:

	// Holds the chat list
	TSharedPtr<SListView<TSharedRef<FChatItemViewModel> > > ChatList;

	// Holds the chat list
	TSharedPtr<SBorder> ChatListContainer;

	// Holds the scroll bar
	TSharedPtr<SScrollBar> ExternalScrollbar;

	// Holds the chat list display
	TSharedPtr<SEditableTextBox> ChatTextBox;

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

	static const float CHAT_HINT_UPDATE_THROTTLE;
};

const float SChatWindowImpl::CHAT_HINT_UPDATE_THROTTLE = 1.0f;

TSharedRef<SChatWindow> SChatWindow::New()
{
	return MakeShareable(new SChatWindowImpl());
}

#undef LOCTEXT_NAMESPACE
