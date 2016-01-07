// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendUserHeader.h"
#include "IUserInfo.h"

#define LOCTEXT_NAMESPACE "SFriendUserHeader"

class SFriendUserHeaderImpl : public SFriendUserHeader
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<const IUserInfo>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		FriendsFontStyle = *InArgs._FontStyle;
		this->ViewModel = InViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.Visibility(InArgs._ShowPresenceBrush ? EVisibility::Visible : EVisibility::Collapsed)
					.WidthOverride(FriendStyle.FriendsListStyle.UserPresenceImageSize.X)
					.HeightOverride(FriendStyle.FriendsListStyle.UserPresenceImageSize.Y)
					[
						SNew(SImage)
						.Image(this, &SFriendUserHeaderImpl::GetPresenceBrush)
					]
				]
				+ SOverlay::Slot()
				.VAlign(InArgs._ShowPresenceBrush ? VAlign_Top : VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SImage)
					.Image(this, &SFriendUserHeaderImpl::GetStatusBrush)
				]
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(10, 0, 10, 2)
			[
				SNew(STextBlock)
				.Visibility(InArgs._ShowUserName ? EVisibility::Visible : EVisibility::Collapsed)
				.Font(FriendStyle.FriendsLargeFontStyle.FriendsFontNormal)
				.ColorAndOpacity(FriendsFontStyle.DefaultFontColor)
				.Text(this, &SFriendUserHeaderImpl::GetDisplayName)
			]
		]);
	}

private:

	const FSlateBrush* GetPresenceBrush() const
	{
		return ViewModel->GetPresenceBrush();
	}

	const FSlateBrush* GetStatusBrush() const
	{
		switch (ViewModel->GetOnlineStatus())
		{
			case EOnlinePresenceState::Away:
			case EOnlinePresenceState::ExtendedAway:
				return &FriendStyle.FriendsListStyle.AwayBrush;
			case EOnlinePresenceState::Chat:
			case EOnlinePresenceState::DoNotDisturb:
			case EOnlinePresenceState::Online:
				return &FriendStyle.FriendsListStyle.OnlineBrush;
			case EOnlinePresenceState::Offline:
			default:
				return &FriendStyle.FriendsListStyle.OfflineBrush;
		};
	}

	FText GetDisplayName() const
	{
		return FText::FromString(ViewModel->GetName());
	}

private:

	TSharedPtr<const IUserInfo> ViewModel;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	FFriendsFontStyle FriendsFontStyle;
};

TSharedRef<SFriendUserHeader> SFriendUserHeader::New()
{
	return MakeShareable(new SFriendUserHeaderImpl());
}

#undef LOCTEXT_NAMESPACE
