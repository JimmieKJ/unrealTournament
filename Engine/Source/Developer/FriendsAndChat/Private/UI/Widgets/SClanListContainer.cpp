// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SClanListContainer.h"
#include "SClanList.h"
#include "ClanListViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsList"

/**
 * Declares the Clan List display widget
*/
class SClanListContainerImpl : public SClanListContainer
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ClanListType = InArgs._ClanListType;
		ClansVisibility = EVisibility::Visible;
		ViewModel = InViewModel;

		MenuMethod = InArgs._Method;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FriendStyle.BorderPadding)
				.BorderImage(&FriendStyle.FriendListHeader)
				[
					SNew(SButton)
					.OnClicked(this, &SClanListContainerImpl::HandleShowClansClicked)
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
							.OnClicked(this, &SClanListContainerImpl::HandleShowClansClicked)
							.ButtonStyle(&FriendStyle.FriendListOpenButtonStyle)
							.Visibility(this, &SClanListContainerImpl::GetClansListOpenVisbility, true)
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.OnClicked(this, &SClanListContainerImpl::HandleShowClansClicked)
							.ButtonStyle(&FriendStyle.FriendListCloseButtonStyle)
							.Visibility(this, &SClanListContainerImpl::GetClansListOpenVisbility, false)
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
							.Text(GetListName())
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 2.0f, 0.0f, 8.0f))
				.Visibility(this, &SClanListContainerImpl::GetClansListOpenVisbility, false)
				[
					SNew(SClanList, FClanListViewModelFactory::Create(ViewModel.ToSharedRef(), ClanListType)).FriendStyle(&FriendStyle)
				]
			]
		]);
	}

private:

	FText GetListName() const
	{
		return EClanDisplayLists::ToFText(ClanListType);
	}

	EVisibility GetClansListOpenVisbility(bool bOpenButton) const
	{
		if (bOpenButton)
		{
			return ClansVisibility == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible;
		}
		return ClansVisibility;
	}

	FReply HandleShowClansClicked()
	{
		ClansVisibility = ClansVisibility == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible;
		return FReply::Handled();
	}

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	EVisibility  ClansVisibility;

	TSharedPtr<FClanViewModel> ViewModel;

	EPopupMethod MenuMethod;

	EClanDisplayLists::Type ClanListType;

	bool bListOpen;
};

TSharedRef<SClanListContainer> SClanListContainer::New()
{
	return MakeShareable(new SClanListContainerImpl());
}

#undef LOCTEXT_NAMESPACE
