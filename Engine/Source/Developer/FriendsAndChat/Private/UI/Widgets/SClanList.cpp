// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SClanList.h"
#include "SClanItem.h"
#include "ClanListViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsList"

/**
 * Declares the clans List display widget
*/
class SClanListImpl : public SClanList
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanListViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		//ViewModel->OnFriendsListUpdated().AddSP(this, &SFriendsListImpl::RefreshFriendsList);
		MenuMethod = InArgs._Method;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SAssignNew(ClanContents, SVerticalBox)
		]);

		RefreshClanList();
	}

private:
	void RefreshClanList()
	{
		ClanContents->ClearChildren();

		for(const auto& ClanItem : ViewModel->GetClanList())
		{
			ClanContents->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			[
				SNew(SClanItem, ClanItem)
				.FriendStyle(&FriendStyle)
				.Method(MenuMethod)
			];
		}
	}

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FClanListViewModel> ViewModel;
	TSharedPtr<SVerticalBox> ClanContents;
	EPopupMethod MenuMethod;
};

TSharedRef<SClanList> SClanList::New()
{
	return MakeShareable(new SClanListImpl());
}

#undef LOCTEXT_NAMESPACE
