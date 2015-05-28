// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsListContainer.h"
#include "SFriendsList.h"
#include "SInvitesList.h"
#include "FriendListViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsList"

/**
 * Declares the Friends List display widget
*/
class SFriendsListContainerImpl : public SFriendsListContainer
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendListViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		FriendsVisibility = EVisibility::Visible;
		ViewModel = InViewModel;

		FFriendListViewModel* ViewModelPtr = ViewModel.Get();
		MenuMethod = InArgs._Method;

		EVisibility FilterVisibility = ViewModel->GetListType() == EFriendsDisplayLists::ClanMemberDisplay ? EVisibility::Visible : EVisibility::Collapsed;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			.Visibility(ViewModelPtr, &FFriendListViewModel::GetListVisibility)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FriendStyle.BorderPadding)
				.BorderImage(&FriendStyle.FriendListHeader)
				[
					SNew(SButton)
					.OnClicked(this, &SFriendsListContainerImpl::HandleShowFriendsClicked)
					.ButtonStyle(FCoreStyle::Get(), "NoBorder")
					.ContentPadding(FMargin(7.0f, 2.0f, 4.0f, 2.0f))
					.Cursor(EMouseCursor::Hand)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.OnClicked(this, &SFriendsListContainerImpl::HandleShowFriendsClicked)
							.ButtonStyle(&FriendStyle.FriendListOpenButtonStyle)
							.Visibility(this, &SFriendsListContainerImpl::GetFriendsListOpenVisbility, true)
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.OnClicked(this, &SFriendsListContainerImpl::HandleShowFriendsClicked)
							.ButtonStyle(&FriendStyle.FriendListCloseButtonStyle)
							.Visibility(this, &SFriendsListContainerImpl::GetFriendsListOpenVisbility, false)
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(FMargin(5, 0))
						[
							SNew(STextBlock)
							.ColorAndOpacity(FLinearColor::White)
							.Font(FriendStyle.FriendsFontStyleUserLarge)
							.Text(ViewModel->GetListName())
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.Padding(FMargin(5, 0))
						[
							SNew(SEditableTextBox)
							.Style(&FriendStyle.AddFriendEditableTextStyle)
							.HintText(LOCTEXT("FilterListHint", "Filter"))
							.OnTextChanged(this, &SFriendsListContainerImpl::HandleFilterChanged, ETextCommit::Default)
							.Visibility(FilterVisibility)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Visibility(ViewModel->GetOnlineCountVisibility())
							.ColorAndOpacity(FLinearColor::White)
							.Font(FriendStyle.FriendsFontStyleBold)
							.Text(ViewModelPtr, &FFriendListViewModel::GetOnlineCountText)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FLinearColor::White)
							.Font(FriendStyle.FriendsFontStyleBold)
							.Text(ViewModelPtr, &FFriendListViewModel::GetListCountText)
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 2.0f, 0.0f, 8.0f))
				.Visibility(this, &SFriendsListContainerImpl::GetFriendsVisibility)
				[
					CreateList()
				]
			]
		]);
	}

private:
	EVisibility GetFriendsListOpenVisbility(bool bOpenButton) const
	{
		if (bOpenButton)
		{
			return FriendsVisibility == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible;
		}
		return FriendsVisibility;
	}

	EVisibility GetFriendsVisibility() const
	{
		return FriendsVisibility;
	}

	FReply HandleShowFriendsClicked()
	{
		FriendsVisibility = FriendsVisibility == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible;
		return FReply::Handled();
	}

	void HandleFilterChanged(const FText& CommentText, ETextCommit::Type CommitInfo)
	{
		ViewModel->SetListFilter(CommentText);
	}

private:
	TSharedRef<SWidget> CreateList()
	{
		switch(ViewModel->GetListType())
		{
			case EFriendsDisplayLists::FriendRequestsDisplay :
			case EFriendsDisplayLists::OutgoingFriendInvitesDisplay :
			case EFriendsDisplayLists::GameInviteDisplay :
			case EFriendsDisplayLists::RecentPlayersDisplay :
			{
				return SNew(SInvitesList, ViewModel.ToSharedRef())
				.FriendStyle(&FriendStyle);
			}
			break;
			default:
			{
				return SNew(SFriendsList, ViewModel.ToSharedRef())
					.FriendStyle(&FriendStyle)
					.Method(MenuMethod);
			}
		}
	}

private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	EVisibility  FriendsVisibility;

	TSharedPtr<FFriendListViewModel> ViewModel;

	EPopupMethod MenuMethod;
};

TSharedRef<SFriendsListContainer> SFriendsListContainer::New()
{
	return MakeShareable(new SFriendsListContainerImpl());
}

#undef LOCTEXT_NAMESPACE
