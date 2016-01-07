// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SClanHome.h"
#include "SClanList.h"
#include "SClanListContainer.h"
#include "ClanCollectionViewModel.h"
#include "ClanListViewModel.h"

#define LOCTEXT_NAMESPACE ""

/**
 * Declares the Clan details widget
*/
class SClanHomeImpl : public SClanHome
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanCollectionViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10)
			[
				SNew(SButton)
				.Visibility(this, &SClanHomeImpl::ViewClansVisibility)
				.ButtonStyle(&FriendStyle.FriendsListStyle.GlobalChatButtonStyle)
				[
					SNew(STextBlock)
					.Font(FriendStyle.FriendsNormalFontStyle.FriendsFontNormalBold)
					.ColorAndOpacity(FriendStyle.FriendsNormalFontStyle.DefaultFontColor)
					.Text(LOCTEXT("ViewClans","View Clans"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10, 10, 30, 0))
			[
				SNew(SClanListContainer, InViewModel)
				.FriendStyle(&FriendStyle)
				.Visibility(this, &SClanHomeImpl::ClanListVisibility)
				.ClanListType(EClanDisplayLists::DefaultDisplay)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10, 10, 30, 0))
			[
				SNew(SClanListContainer, InViewModel)
				.FriendStyle(&FriendStyle)
				.Visibility(this, &SClanHomeImpl::InviteListVisibility)
				.ClanListType(EClanDisplayLists::ClanRequestsDisplay)
			]
		]);
	}

private:

	EVisibility ViewClansVisibility() const
	{
		if (ViewModel.IsValid() && ViewModel->NumberOfJoinedClans() == 0)
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	EVisibility ClanListVisibility() const
	{
		if (ViewModel.IsValid() && ViewModel->NumberOfJoinedClans() > 0)
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	EVisibility InviteListVisibility() const
	{
		if (ViewModel.IsValid() && ViewModel->NumberOfJoinedClans() > 0)
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<FClanCollectionViewModel> ViewModel;
};

TSharedRef<SClanHome> SClanHome::New()
{
	return MakeShareable(new SClanHomeImpl());
}

#undef LOCTEXT_NAMESPACE
