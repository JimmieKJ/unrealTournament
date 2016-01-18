// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SClanContainer.h"
#include "SClanDetails.h"
#include "SClanHome.h"
#include "SWidgetSwitcher.h"
#include "ClanCollectionViewModel.h"

#define LOCTEXT_NAMESPACE ""

/**
 * Declares the Clan display widget
*/
class SClanContainerImpl : public SClanContainer
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanCollectionViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		MenuMethod = InArgs._Method;

		ViewModel->OpenClanDetails().AddSP(this, &SClanContainerImpl::OnOpenClanDetails);

		ExternalScrollbar = SNew(SScrollBar)
			.Thickness(FVector2D(4, 4))
			.Style(&FriendStyle.ScrollBarStyle)
			.AlwaysShowScrollbar(true);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground)
			.Padding(0)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SScrollBox)
					.ExternalScrollbar(ExternalScrollbar.ToSharedRef())
					+ SScrollBox::Slot()
					.HAlign(HAlign_Fill)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Top)
						.AutoHeight()
						[
							SAssignNew(Switcher, SWidgetSwitcher)
							+ SWidgetSwitcher::Slot()
							[
								SNew(SClanHome, ViewModel.ToSharedRef())
								.FriendStyle(&FriendStyle)
							]
							+ SWidgetSwitcher::Slot()
							[
								SAssignNew(ClanDetailsBox, SBox)
							]
						]
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.Padding(FMargin(10, 20, 10, 20))
				[
					ExternalScrollbar.ToSharedRef()
				]
			]
		]);
	}

private:

	void OnOpenClanDetails(const TSharedRef < FClanInfoViewModel > &ClanInfoViewModel)
	{
		TSharedRef<SClanDetails> ClanDetails = SNew(SClanDetails, ClanInfoViewModel)
			.FriendStyle(&FriendStyle)
			.OnHomeClicked(this, &SClanContainerImpl::HandleWidgetSwitcherClicked, 0);
		ClanDetailsBox->SetContent(ClanDetails);
		Switcher->SetActiveWidgetIndex(1);
	}

	FReply HandleWidgetSwitcherClicked(int32 Index)
	{
		Switcher->SetActiveWidgetIndex(Index);
		return FReply::Handled();
	}
private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<SBox> ClanDetailsBox;
	TSharedPtr<FClanCollectionViewModel> ViewModel;

	// Holds the list container switcher
	TSharedPtr<SWidgetSwitcher> Switcher;

	TSharedPtr<SScrollBar> ExternalScrollbar;

	EPopupMethod MenuMethod;
};

TSharedRef<SClanContainer> SClanContainer::New()
{
	return MakeShareable(new SClanContainerImpl());
}

#undef LOCTEXT_NAMESPACE
