// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SInvitesList.h"
#include "SInviteItem.h"
#include "FriendsViewModel.h"
#include "FriendListViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsList"

/**
 * Declares the Friends List display widget
*/
class SInvitesListImpl : public SInvitesList
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendListViewModel>& ViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		this->ViewModel = ViewModel;
		
		ViewModel->OnFriendsListUpdated().AddSP(this, &SInvitesListImpl::RefreshFriendsList);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SAssignNew(Contents, SVerticalBox)
		]);

		RefreshFriendsList();
	}

private:

	void RefreshFriendsList()
	{
		Contents->ClearChildren();

		for(const auto& OnlineFriend : ViewModel->GetFriendsList())
		{
			Contents->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			[
				SNew(SInviteItem, OnlineFriend.ToSharedRef())
				.FriendStyle(&FriendStyle)
			];
		}
	}

private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SVerticalBox> Contents;
	TSharedPtr<FFriendListViewModel> ViewModel;
};

TSharedRef<SInvitesList> SInvitesList::New()
{
	return MakeShareable(new SInvitesListImpl());
}

#undef LOCTEXT_NAMESPACE
