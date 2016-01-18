// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendsList.h"
#include "FriendContainerViewModel.h"
#include "FriendViewModel.h"
#include "FriendListViewModel.h"
#include "SFriendItem.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE "SFriendsList"

/**
 * Declares the Friends List display widget
*/
class SFriendsListImpl : public SFriendsList
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService>& InFontService, const TSharedRef<FFriendListViewModel>& InViewModel, const TSharedRef<FFriendContainerViewModel>& InContainerViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		FontService = InFontService;
		ContainerViewModel = InContainerViewModel;
		ViewModel = InViewModel;
		ViewModel->OnFriendsListUpdated().AddSP(this, &SFriendsListImpl::RefreshFriendsList);

		ExternalScrollbar = SNew(SScrollBar)
			.Thickness(FVector2D(4, 4))
			.Style(&FriendStyle.ScrollBarStyle)
			.AlwaysShowScrollbar(true);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(&FriendStyle.FriendsListStyle.FriendsListBackground)
				[
					SAssignNew(ListScrollBox, SScrollBox)
					.ExternalScrollbar(ExternalScrollbar.ToSharedRef())
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.Padding(FMargin(10, 20, 10, 20))
			[
				ExternalScrollbar.ToSharedRef()
			]
		]);

		RefreshFriendsList();
	}

private:

	void RefreshFriendsList()
	{
		ListScrollBox->ClearChildren();
		AddListSpecificOptions();

		for (const auto& OnlineFriend : ViewModel->GetFriendsList())
		{
			ListScrollBox->AddSlot()
			[
				SNew(SFriendItem, FontService.ToSharedRef(), OnlineFriend.ToSharedRef(), ContainerViewModel.ToSharedRef())
				.FriendStyle(&FriendStyle)
			];
		}
	}

	void AddListSpecificOptions()
	{
		if (ViewModel->GetListType() == EFriendsDisplayLists::SuggestedFriendsDisplay)
		{
			ListScrollBox->AddSlot()
			[
				SNew(SButton)
				.ButtonStyle(&FriendStyle.FriendsListStyle.FriendItemButtonStyle)
				.ContentPadding(20)
				.OnClicked(FOnClicked::CreateSP(this, &SFriendsListImpl::OnImportAll))
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalBoldFont)
					.ColorAndOpacity(FriendStyle.FriendsNormalFontStyle.DefaultFontColor)
					.Text(LOCTEXT("ImportAll", "Import All"))
				]
			];
		}
	}

	FReply OnImportAll()
	{
		for (const auto& OnlineFriend : ViewModel->GetFriendsList())
		{
			if (OnlineFriend->CanPerformAction(EFriendActionType::Import))
			{
				OnlineFriend->PerformAction(EFriendActionType::Import);
			}
		}
		return FReply::Handled();
	}


	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FFriendListViewModel> ViewModel;
	TSharedPtr<FFriendContainerViewModel> ContainerViewModel;
	TSharedPtr<SScrollBar> ExternalScrollbar;
	TSharedPtr<SScrollBox> ListScrollBox;
	TSharedPtr<SVerticalBox> List;
};

TSharedRef<SFriendsList> SFriendsList::New()
{
	return MakeShareable(new SFriendsListImpl());
}

#undef LOCTEXT_NAMESPACE
