// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsViewModel.h"
#include "SFriendsList.h"
#include "SFriendsListContainer.h"
#include "SFriendsContainer.h"
#include "SFriendRequest.h"
#include "SFriendsStatus.h"
#include "SFriendsUserSettings.h"

#define LOCTEXT_NAMESPACE "SFriendsContainer"

/**
 * Declares the Friends List display widget
*/
class SFriendsContainerImpl : public SFriendsContainer
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsViewModel>& ViewModel)
	{
		this->ViewModel = ViewModel;
		FriendStyle = *InArgs._FriendStyle;

		// Set up titles
		const FText SearchStringLabel = LOCTEXT( "FriendList_SearchLabel", "[Enter Display Name]" );

		MenuMethod = InArgs._Method;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
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
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SFriendsStatus, ViewModel->GetStatusViewModel())
							.FriendStyle(&FriendStyle)
							.Method(MenuMethod)
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Fill)
						.Padding(4, 0, 0, 0)
						[
							SNew(SBorder)
							.Visibility(this, &SFriendsContainerImpl::AddFriendVisibility)
							.BorderImage(&FriendStyle.AddFriendEditBorder)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Fill)
							[
								SAssignNew(FriendNameTextBox, SEditableTextBox)
								.HintText(LOCTEXT("AddFriendHint", "Add friend by account name or email"))
								.Style(&FriendStyle.AddFriendEditableTextStyle)
								.OnTextCommitted(this, &SFriendsContainerImpl::HandleFriendEntered)
							]
						]
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						.Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle(&FriendStyle.AddFriendButtonStyle)
							.OnClicked(this, &SFriendsContainerImpl::HandleAddFriendButtonClicked)
							.Visibility(this, &SFriendsContainerImpl::AddFriendActionVisibility)
							.Cursor(EMouseCursor::Hand)
							[
								SNew(SBox)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.WidthOverride(FriendStyle.StatusButtonSize.Y)
								.HeightOverride(FriendStyle.StatusButtonSize.Y)
								[
									SNew(SImage)
									.Image(&FriendStyle.AddFriendButtonContentBrush)
								]
							]
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						.AutoWidth()
						.Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle(&FriendStyle.AddFriendCloseButtonStyle)
							.Visibility(this, &SFriendsContainerImpl::AddFriendVisibility)
							.Cursor(EMouseCursor::Hand)
							[
								SNew(SBox)
								.WidthOverride(FriendStyle.StatusButtonSize.Y)
								.HeightOverride(FriendStyle.StatusButtonSize.Y)
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(FriendStyle.BorderPadding)
				.BorderImage(&FriendStyle.FriendContainerBackground)
				[
					SNew(SScrollBox)
					.ScrollBarStyle(&FriendStyle.ScrollBarStyle)
					.ScrollBarThickness(FVector2D(4, 4))
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ViewModel->GetFriendListViewModel(EFriendsDisplayLists::GameInviteDisplay))
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ViewModel->GetFriendListViewModel(EFriendsDisplayLists::FriendRequestsDisplay))
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ViewModel->GetFriendListViewModel(EFriendsDisplayLists::DefaultDisplay))
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ViewModel->GetFriendListViewModel(EFriendsDisplayLists::RecentPlayersDisplay))
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ViewModel->GetFriendListViewModel(EFriendsDisplayLists::OutgoingFriendInvitesDisplay))
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
				]
			]
		]);
	}

private:


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

	void HandleFriendEntered(const FText& CommentText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter)
		{
			ViewModel->RequestFriend(CommentText);
		}
		
		if (ViewModel->IsPerformingAction())
		{
			ViewModel->PerformAction();
		}
	}

	EVisibility AddFriendVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility AddFriendActionVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Collapsed : EVisibility::Visible;
	}

private:

	// Holds the Friends List view model
	TSharedPtr<FFriendsViewModel> ViewModel;

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

};

TSharedRef<SFriendsContainer> SFriendsContainer::New()
{
	return MakeShareable(new SFriendsContainerImpl());
}

#undef LOCTEXT_NAMESPACE
