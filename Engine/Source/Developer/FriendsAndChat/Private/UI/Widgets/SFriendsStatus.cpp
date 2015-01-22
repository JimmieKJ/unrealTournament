// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsStatus.h"
#include "FriendsStatusViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsStatus"

/**
 * Declares the Friends Status display widget
*/
class SFriendsStatusImpl : public SFriendsStatus
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsStatusViewModel>& ViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		this->ViewModel = ViewModel;

		FFriendsStatusViewModel* ViewModelPtr = &ViewModel.Get();

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SAssignNew(ActionMenu, SMenuAnchor)
				.Placement(EMenuPlacement::MenuPlacement_ComboBox)
				.Method(InArgs._Method)
				.MenuContent(GetMenuContent())
				[
					SNew(SButton)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.OnClicked(this, &SFriendsStatusImpl::HandleVersionDropDownClicked)
					.ButtonStyle(&FriendStyle.FriendListStatusButtonStyle)
					.Cursor(EMouseCursor::Hand)
					[
						SNew(SBox)
						.WidthOverride(FriendStyle.StatusButtonSize.X)
						.HeightOverride(FriendStyle.StatusButtonSize.Y)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								SNew(SImage)
								.Image(this, &SFriendsStatusImpl::GetStatusBrush)
							]
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoWidth()
							.Padding(FMargin(5,2))
							[
								SNew(STextBlock)
								.Text(ViewModelPtr, &FFriendsStatusViewModel::GetStatusText)
								.Font(FriendStyle.FriendsFontStyleBold)
								.ColorAndOpacity(FLinearColor::White)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.ColorAndOpacity(FLinearColor::White)
								.Image(&FriendStyle.FriendsComboDropdownImageBrush)
							]
						]
					]
				]
			]
		]);
	}

private:
	FReply HandleVersionDropDownClicked() const
	{
		ActionMenu->SetIsOpen(true);
		return FReply::Handled();
	}

	/**
	* Generate the action menu.
	* @return the action menu widget
	*/
	TSharedRef<SWidget> GetMenuContent()
	{
		return SNew(SBorder)
			.BorderImage(&FriendStyle.Background)
			.Padding(FMargin(1, 5))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &SFriendsStatusImpl::HandleStatusChanged, EOnlinePresenceState::Online)
					.ButtonStyle(&FriendStyle.FriendListItemButtonStyle)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::White)
						.Font(FriendStyle.FriendsFontStyle)
						.Text(FText::FromString("Online"))
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &SFriendsStatusImpl::HandleStatusChanged, EOnlinePresenceState::Away)
					.ButtonStyle(&FriendStyle.FriendListItemButtonStyle)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::White)
						.Font(FriendStyle.FriendsFontStyle)
						.Text(FText::FromString("Away"))
					]
				]
			];
	}

	FReply HandleStatusChanged(EOnlinePresenceState::Type OnlineState)
	{
		ActionMenu->SetIsOpen(false);
		ViewModel->SetOnlineStatus(OnlineState);
		return FReply::Handled();
	}


	const FSlateBrush* GetStatusBrush() const
	{
		switch (ViewModel->GetOnlineStatus())
		{	
		case EOnlinePresenceState::Away:
		case EOnlinePresenceState::ExtendedAway:
			return &FriendStyle.AwayBrush;
		case EOnlinePresenceState::Chat:
		case EOnlinePresenceState::DoNotDisturb:
		case EOnlinePresenceState::Online:
			return &FriendStyle.OnlineBrush;
		case EOnlinePresenceState::Offline:
		default:
			return &FriendStyle.OfflineBrush;
		};
	}

private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SMenuAnchor> ActionMenu;
	TSharedPtr<FFriendsStatusViewModel> ViewModel;
};

TSharedRef<SFriendsStatus> SFriendsStatus::New()
{
	return MakeShareable(new SFriendsStatusImpl());
}

#undef LOCTEXT_NAMESPACE
