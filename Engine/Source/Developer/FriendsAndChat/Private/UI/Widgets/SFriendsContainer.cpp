// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsViewModel.h"
#include "FriendListViewModel.h"
#include "ClanViewModel.h"
#include "SFriendUserHeader.h"
#include "SFriendsList.h"
#include "SFriendsListContainer.h"
#include "SFriendsContainer.h"
#include "SFriendRequest.h"
#include "SFriendsStatus.h"
#include "SFriendsStatusCombo.h"
#include "SFriendsUserSettings.h"
#include "SFriendsToolTip.h"
#include "SWidgetSwitcher.h"
#include "SClanContainer.h"
#include "FriendsUserViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsContainer"

/**
 * Declares the Friends List display widget
*/
class SFriendsContainerImpl : public SFriendsContainer
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsViewModel>& InViewModel)
	{
		ViewModel = InViewModel;
		FriendStyle = *InArgs._FriendStyle;

		for (int32 ListIndex = 0; ListIndex < EFriendsDisplayLists::MAX_None; ListIndex++)
		{
			ListViewModels.Add(ViewModel->GetFriendListViewModel((EFriendsDisplayLists::Type)ListIndex));
		}

		// Set up titles
		const FText SearchStringLabel = LOCTEXT( "FriendList_SearchLabel", "[Enter Display Name]" );

		MenuMethod = InArgs._Method;

		ExternalScrollbar = SNew(SScrollBar)
			.Thickness(FVector2D(4, 4))
			.Style(&FriendStyle.ScrollBarStyle)
			.AlwaysShowScrollbar(true);

		// Quick hack to disable clan system via commandline
		EVisibility ClanVisibility = FParse::Param(FCommandLine::Get(), TEXT("ShowClans")) ? EVisibility::Visible : EVisibility::Collapsed;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			[
				SNew(SBorder)
				.Padding(FriendStyle.UserHeaderPadding)
				.BorderImage(&FriendStyle.FriendUserHeaderBackground)
				.Visibility(FriendStyle.HasUserHeader ? EVisibility::Visible : EVisibility::Collapsed)
				[
					SNew(SBox)
					.WidthOverride(FriendStyle.FriendsListWidth)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SFriendsStatusCombo, ViewModel->GetStatusViewModel(), ViewModel->GetUserViewModel())
							.FriendStyle(&FriendStyle)
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				.Visibility(ClanVisibility)
				+SVerticalBox::Slot()
				.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
					.Padding(0)
				[
					SNew(SButton)
					.OnClicked(this, &SFriendsContainerImpl::HandleWidgetSwitcherClicked, 0)
						.ContentPadding(FMargin(25,10))
						.ButtonStyle(&FriendStyle.GlobalChatButtonStyle)
						.ButtonColorAndOpacity(FLinearColor::White)
						.Cursor(EMouseCursor::Hand)
						[
							SNew(STextBlock)
							.Font(FriendStyle.FriendsFontStyleSmallBold)
							.ColorAndOpacity(this, &SFriendsContainerImpl::GetFriendTabTextColor, 0)
							.Text(LOCTEXT("FriendsTab", "Friends"))
						]
				]
				+SHorizontalBox::Slot()
					.Padding(0)
				[
					SNew(SButton)
					.OnClicked(this, &SFriendsContainerImpl::HandleWidgetSwitcherClicked, 1)
						.ContentPadding(FMargin(25,10))
						.ButtonStyle(&FriendStyle.GlobalChatButtonStyle)
						.Cursor(EMouseCursor::Hand)
						[
							SNew(STextBlock)
							.Font(FriendStyle.FriendsFontStyleSmallBold)
							.ColorAndOpacity(this, &SFriendsContainerImpl::GetFriendTabTextColor, 1)
							.Text(LOCTEXT("ClansTab", "Clans"))
						]
					]
				]
				+ SVerticalBox::Slot()
				.Padding(5)
				.AutoHeight()
				.VAlign(VAlign_Center)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(Switcher, SWidgetSwitcher)
				+SWidgetSwitcher::Slot()
				[
					GetFriendsListWidget()
				]
				+SWidgetSwitcher::Slot()
				[
					GetClanWidget()
				]
			]
		]);
	}

private:

	TSharedPtr<SToolTip> CreateAddFriendToolTip()
	{
		const FText AddFriendToolTipText = LOCTEXT( "FriendContain_AddFriendToolTip", "Add friend by account name or email" );
		return SNew(SFriendsToolTip)
		.DisplayText(AddFriendToolTipText)
		.FriendStyle(&FriendStyle);
	}

	TSharedPtr<SToolTip> CreateGlobalChatToolTip()
	{
		const FText GlobalChatToolTipText = LOCTEXT("FriendContain_GlobalChatToolTip", "Fortnite Global Chat");
		return SNew(SFriendsToolTip)
			.DisplayText(GlobalChatToolTipText)
			.FriendStyle(&FriendStyle);
	}

	FText GetDisplayName() const
	{
		return FText::FromString(ViewModel->GetName());
	}

	FReply HandleAddFriendButtonClicked()
	{
		FriendNameTextBox->SetText(FText::GetEmpty());
		ViewModel->PerformAction();
		if (ViewModel->IsPerformingAction())
		{
			FSlateApplication::Get().SetKeyboardFocus(FriendNameTextBox, EFocusCause::SetDirectly);
		}
		return FReply::Handled();
	}

	FReply HandleCloseFriendButtonClicked()
	{
		if (ViewModel->IsPerformingAction())
		{
			ViewModel->PerformAction();
		}
		return FReply::Handled();
	}

	void HandleFriendEntered(const FText& CommentText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter)
		{
			ViewModel->RequestFriend(CommentText);
			if (ViewModel->IsPerformingAction())
			{
				ViewModel->PerformAction();
			}
		}		
	}

	EVisibility AddFriendBoxVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility AddFriendCloseActionVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility AddFriendActionVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	const FSlateBrush* GetAddFriendButtonImageBrush() const
	{
		return AddFriendButton.IsValid() && AddFriendButton->IsHovered() ? &FriendStyle.AddFriendButtonContentHoveredBrush : &FriendStyle.AddFriendButtonContentBrush;
	}

	EVisibility NoFriendsNoticeVisibility() const
	{
		for (auto& ListViewModel : ListViewModels)
		{
			if (ListViewModel->GetListVisibility() == EVisibility::Visible)
			{
				return EVisibility::Collapsed;
			}
		}
		return EVisibility::Visible;
	}

	FReply OnGlobalChatButtonClicked()
	{
		ViewModel->DisplayGlobalChatWindow();
		return FReply::Handled();
	}

	FReply HandleWidgetSwitcherClicked(int32 Index)
	{
		Switcher->SetActiveWidgetIndex(Index);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled();
	}

	TSharedRef<SWidget> GetClanWidget()
	{
		return SNew(SClanContainer, ViewModel->GetClanViewModel())
		.FriendStyle(&FriendStyle);
	}

	TSharedRef<SWidget> GetFriendsListWidget()
	{
		// Probably move this to its own widget Nick Davies (3/25/2015)
		return SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					[
						SNew(SBorder)
						.Padding(FriendStyle.BorderPadding)
						.BorderImage(&FriendStyle.FriendContainerHeader)
						[
							SNew(SBox)
							.WidthOverride(FriendStyle.FriendsListWidth)
							.HeightOverride(FriendStyle.StatusButtonSize.Y)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Fill)
								.Padding(4, 0, 0, 0)
								[
									SNew(SHorizontalBox)
									.Visibility(this, &SFriendsContainerImpl::AddFriendActionVisibility)
									+SHorizontalBox::Slot()
									.AutoWidth()
									[
										SAssignNew(AddFriendButton, SButton)
										.ToolTip(CreateAddFriendToolTip())
										.ButtonStyle(FCoreStyle::Get(), "NoBorder")
										.OnClicked(this, &SFriendsContainerImpl::HandleAddFriendButtonClicked)
										.ContentPadding(0)
										.Cursor(EMouseCursor::Hand)
										[
											SNew(SHorizontalBox)
											+SHorizontalBox::Slot()
											[
												SNew(SBox)
												.HAlign(HAlign_Center)
												.VAlign(VAlign_Center)
												.WidthOverride(FriendStyle.StatusButtonSize.Y)
												.HeightOverride(FriendStyle.StatusButtonSize.Y)
												[
													SNew(SImage)
													.Image(this, &SFriendsContainerImpl::GetAddFriendButtonImageBrush)
												]
											]
											+SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SNew(STextBlock)
												.Font(FriendStyle.FriendsFontStyleBold)
												.ColorAndOpacity(FLinearColor::White)
												.Text(LOCTEXT("AddAFriend", "Add a Friend"))
											]
										]
									]
									+SHorizontalBox::Slot()
									.HAlign(HAlign_Fill)
									.FillWidth(1)
									[
										SNew(SSpacer)
									]
									+SHorizontalBox::Slot()
									.Padding(15, 0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Right)
									[
										SNew(SBorder)
										.Padding(0)
										.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
										.Visibility(ViewModel->GetGlobalChatButtonVisibility())
										[
											SNew(SButton)
											.ToolTip(CreateGlobalChatToolTip())
											.ButtonStyle(&FriendStyle.GlobalChatButtonStyle)
											.OnClicked(this, &SFriendsContainerImpl::OnGlobalChatButtonClicked)
											.ContentPadding(10)
											.Cursor(EMouseCursor::Hand)
											[
												SNew(STextBlock)
												.Font(FriendStyle.FriendsFontStyleSmallBold)
												.ColorAndOpacity(FLinearColor::White)
												.Text(LOCTEXT("FortniteGlobalChatButton", "Fortnite Global Chat"))
											]
										]
									]
								]
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								.Padding(4, 0, 0, 0)
								[
									SNew(SButton)
									.ButtonStyle(&FriendStyle.AddFriendCloseButtonStyle)
									.Visibility(this, &SFriendsContainerImpl::AddFriendCloseActionVisibility)
									.OnClicked(this, &SFriendsContainerImpl::HandleAddFriendButtonClicked)
									.ContentPadding(0)
									.Cursor(EMouseCursor::Hand)
									[
										SNew(SBox)
										.WidthOverride(FriendStyle.StatusButtonSize.Y)
										.HeightOverride(FriendStyle.StatusButtonSize.Y)
									]
								]
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Fill)
								.FillWidth(1)
								.Padding(4, 0, 10, 0)
								[
									SNew(SBox)
									.Visibility(this, &SFriendsContainerImpl::AddFriendBoxVisibility)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Fill)
									.Padding(0)
									[
										SAssignNew(FriendNameTextBox, SEditableTextBox)
										.HintText(LOCTEXT("AddFriendHint", "Add friend by account name or email"))
										.Style(&FriendStyle.AddFriendEditableTextStyle)
										.OnTextCommitted(this, &SFriendsContainerImpl::HandleFriendEntered)
									]
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					[
						SNew(SBorder)
						.BorderImage(&FriendStyle.FriendsContainerBackground)
						.Padding(0)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SScrollBox)
								.ExternalScrollbar(ExternalScrollbar.ToSharedRef())
								+ SScrollBox::Slot()
								.HAlign(HAlign_Center)
								.Padding(10)
								.Padding(FMargin(10, 10, 40, 10))
								[
									SNew(STextBlock)
									.Text(LOCTEXT("NoFriendsNotice", "Press the Plus Button to add friends."))
									.Font(FriendStyle.FriendsFontStyleBold)
									.ColorAndOpacity(FLinearColor::White)
									.Visibility(this, &SFriendsContainerImpl::NoFriendsNoticeVisibility)
								]
								+SScrollBox::Slot()
								.Padding(FMargin(10, 10, 30, 0))
								[
									SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::GameInviteDisplay].ToSharedRef())
									.FriendStyle(&FriendStyle)
									.Method(MenuMethod)
									.Visibility(ListViewModels[EFriendsDisplayLists::GameInviteDisplay].Get(), &FFriendListViewModel::GetListVisibility)
								]
								+SScrollBox::Slot()
								.Padding(FMargin(10, 10, 30, 0))
								[
									SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::FriendRequestsDisplay].ToSharedRef())
									.FriendStyle(&FriendStyle)
									.Method(MenuMethod)
									.Visibility(ListViewModels[EFriendsDisplayLists::FriendRequestsDisplay].Get(), &FFriendListViewModel::GetListVisibility)
								]
								+SScrollBox::Slot()
								.Padding(FMargin(10, 10, 30, 0))
								[
									SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::DefaultDisplay].ToSharedRef())
									.FriendStyle(&FriendStyle)
									.Method(MenuMethod)
									.Visibility(ListViewModels[EFriendsDisplayLists::DefaultDisplay].Get(), &FFriendListViewModel::GetListVisibility)
								]
								+SScrollBox::Slot()
								.Padding(FMargin(10, 10, 30, 0))
								[
									SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::RecentPlayersDisplay].ToSharedRef())
									.FriendStyle(&FriendStyle)
									.Method(MenuMethod)
									.Visibility(ListViewModels[EFriendsDisplayLists::RecentPlayersDisplay].Get(), &FFriendListViewModel::GetListVisibility)
								]
								+SScrollBox::Slot()
								.Padding(FMargin(10, 10, 30, 0))
								[
									SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::OutgoingFriendInvitesDisplay].ToSharedRef())
									.FriendStyle(&FriendStyle)
									.Method(MenuMethod)
									.Visibility(ListViewModels[EFriendsDisplayLists::OutgoingFriendInvitesDisplay].Get(), &FFriendListViewModel::GetListVisibility)
								]
								+ SScrollBox::Slot()
								[
									SNew(SSpacer).Size(FVector2D(10, 10))
								]
							]
							+ SOverlay::Slot()
							.HAlign(HAlign_Right)
							.Padding(FMargin(10, 20, 10, 20))
							[
								ExternalScrollbar.ToSharedRef()
							]
						]
			];
	}


FSlateColor GetFriendTabTextColor(int32 WidgetIndex) const
{
	return Switcher->GetActiveWidgetIndex() == WidgetIndex ?  FriendStyle.DefaultFontColor : FriendStyle.FriendListActionFontColor ;
}

private:

	// Holds the Friends List view model
	TSharedPtr<FFriendsViewModel> ViewModel;

	// Holds the Friends Sub-List view models
	TArray<TSharedPtr<FFriendListViewModel>> ListViewModels;

	// Holds the list container switcher
	TSharedPtr<SWidgetSwitcher> Switcher;

	// Holds the menu method - Full screen requires use owning window or crashes.
	EPopupMethod MenuMethod;

	/** Holds the list of friends. */
	TArray< TSharedPtr< IFriendItem > > FriendsList;

	/** Holds the list of outgoing invites. */
	TArray< TSharedPtr< IFriendItem > > OutgoingFriendsList;

	/** Holds the tree view of the Friends list. */
	TSharedPtr< SListView< TSharedPtr< IFriendItem > > > FriendsListView;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	/** Holds the recent players check box. */
	TSharedPtr< SCheckBox > RecentPlayersButton;

	/** Holds the recent players check box. */
	TSharedPtr< SCheckBox > FriendRequestsButton;

	/** Holds the default friends check box. */
	TSharedPtr< SCheckBox > DefaultPlayersButton;

	// Holds the Friends add text box
	TSharedPtr< SEditableTextBox > FriendNameTextBox;

	TSharedPtr<SScrollBar> ExternalScrollbar;

	TSharedPtr<SButton> AddFriendButton;
};

TSharedRef<SFriendsContainer> SFriendsContainer::New()
{
	return MakeShareable(new SFriendsContainerImpl());
}

#undef LOCTEXT_NAMESPACE
