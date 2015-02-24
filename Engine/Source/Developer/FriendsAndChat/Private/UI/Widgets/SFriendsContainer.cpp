// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsViewModel.h"
#include "FriendListViewModel.h"
#include "SFriendUserHeader.h"
#include "SFriendsList.h"
#include "SFriendsListContainer.h"
#include "SFriendsContainer.h"
#include "SFriendRequest.h"
#include "SFriendsStatus.h"
#include "SFriendsUserSettings.h"
#include "SFriendsToolTip.h"
#include "FriendListViewModel.h"

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

		for (int32 ListIndex = 0; ListIndex < EFriendsDisplayLists::MAX_None; ListIndex++)
		{
			ListViewModels.Add(ViewModel->GetFriendListViewModel((EFriendsDisplayLists::Type)ListIndex));
		}

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
				.Padding(FriendStyle.UserHeaderPadding)
				.BorderImage(&FriendStyle.Background)
				.Visibility(FriendStyle.HasUserHeader ? EVisibility::Visible : EVisibility::Collapsed)
				[
					SNew(SBox)
					.WidthOverride(FriendStyle.FriendsListWidth)
					[
						SNew(SFriendUserHeader, ViewModel->GetUserViewModel())
						.FriendStyle(&FriendStyle)
					]
				]
			]
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
					.HeightOverride(FriendStyle.StatusButtonSize.Y)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SFriendsStatus, ViewModel->GetStatusViewModel())
							.FriendStyle(&FriendStyle)
							.Method(MenuMethod)
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.FillWidth(1)
						.Padding(4, 0, 0, 0)
						[
							SNew(SBorder)
							.Visibility(this, &SFriendsContainerImpl::AddFriendBoxVisibility)
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
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.ToolTip(CreateAddFriendToolTip())
							.ButtonStyle(&FriendStyle.AddFriendButtonStyle)
							.OnClicked(this, &SFriendsContainerImpl::HandleAddFriendButtonClicked)
							.Visibility(this, &SFriendsContainerImpl::AddFriendActionVisibility)
							.ContentPadding(0)
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
						.AutoWidth()
						.Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle(&FriendStyle.AddFriendCloseButtonStyle)
							.Visibility(this, &SFriendsContainerImpl::AddFriendCloseActionVisibility)
							.ContentPadding(0)
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
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FriendStyle.BorderPadding)
				.BorderImage(&FriendStyle.FriendsContainerBackground)
				.Visibility(FFriendsAndChatManager::Get()->HasPermission(TEXT("fortnite:play")) ? EVisibility::Visible : EVisibility::Collapsed)
				[
					SNew(SButton)
					.ButtonStyle(&FriendStyle.GlobalChatButtonStyle)
					.OnClicked(this, &SFriendsContainerImpl::OnGlobalChatButtonClicked)
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(FriendStyle.BorderPadding)
				.BorderImage(&FriendStyle.FriendsContainerBackground)
				[
					SNew(SScrollBox)
					.ScrollBarStyle(&FriendStyle.ScrollBarStyle)
					.ScrollBarThickness(FVector2D(4, 4))
					+ SScrollBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(10)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoFriendsNotice", "Press the Plus Button to add friends."))
						.Font(FriendStyle.FriendsFontStyleBold)
						.ColorAndOpacity(FLinearColor::White)
						.Visibility(this, &SFriendsContainerImpl::NoFriendsNoticeVisibility)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::GameInviteDisplay].ToSharedRef())
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::FriendRequestsDisplay].ToSharedRef())
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::DefaultDisplay].ToSharedRef())
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::RecentPlayersDisplay].ToSharedRef())
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
					+SScrollBox::Slot()
					[
						SNew(SFriendsListContainer, ListViewModels[EFriendsDisplayLists::OutgoingFriendInvitesDisplay].ToSharedRef())
						.FriendStyle(&FriendStyle)
						.Method(MenuMethod)
					]
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

	EVisibility AddFriendBoxVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Visible : EVisibility::Hidden;
	}

	EVisibility AddFriendCloseActionVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility AddFriendActionVisibility() const
	{
		return ViewModel->IsPerformingAction() ? EVisibility::Collapsed : EVisibility::Visible;
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
		FFriendsAndChatManager::Get()->JoinPublicChatRoom(TEXT("Fortnite"));
		FFriendsAndChatManager::Get()->OpenGlobalChat();
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled();
	}

private:

	// Holds the Friends List view model
	TSharedPtr<FFriendsViewModel> ViewModel;

	// Holds the Friends Sub-List view models
	TArray<TSharedPtr<FFriendListViewModel>> ListViewModels;

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
