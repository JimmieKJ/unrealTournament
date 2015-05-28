// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SClanHome.h"
#include "SClanList.h"
#include "SClanListContainer.h"
#include "ClanViewModel.h"
#include "ClanListViewModel.h"

#define LOCTEXT_NAMESPACE ""

/**
 * Declares the Clan details widget
*/
class SClanHomeImpl : public SClanHome
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(10)
			[
				SNew(SClanListContainer, InViewModel)
				.FriendStyle(&FriendStyle)
				.Visibility(EVisibility::Visible)
				.ClanListType(EClanDisplayLists::DefaultDisplay)
			]
			+ SVerticalBox::Slot()
			.Padding(10)
			[
				SNew(SClanListContainer, InViewModel)
				.FriendStyle(&FriendStyle)
				.Visibility(EVisibility::Collapsed)
				.ClanListType(EClanDisplayLists::ClanRequestsDisplay)
			]
		]);
	}

private:

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<FClanViewModel> ViewModel;
};

TSharedRef<SClanHome> SClanHome::New()
{
	return MakeShareable(new SClanHomeImpl());
}

#undef LOCTEXT_NAMESPACE
