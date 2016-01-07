// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SOnlinePresenceWidget.h"

#define LOCTEXT_NAMESPACE ""

#include "FriendContainerViewModel.h"
#include "FriendsUserViewModel.h"

class SOnlinePresenceWidgetImpl : public SOnlinePresenceWidget
{
public:

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendContainerViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(PresenceImage, SImage)
				.ColorAndOpacity(InArgs._ColorAndOpacity)
				.Image(this, &SOnlinePresenceWidgetImpl::GetPresenceBrush)
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Left)
			[
				SNew(SImage)
				.ColorAndOpacity(InArgs._ColorAndOpacity)
				.Image(this, &SOnlinePresenceWidgetImpl::GetStatusBrush)
			]
		]);
	}

private:

	virtual void SetColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity) override
	{
		PresenceImage->SetColorAndOpacity(InColorAndOpacity);
	}

	const FSlateBrush* GetPresenceBrush() const
	{
		return &FriendStyle.FriendsListStyle.StatusIconBrush;
	}

	const FSlateBrush* GetStatusBrush() const
	{
		switch (ViewModel->GetUserViewModel()->GetOnlineStatus())
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

	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FFriendContainerViewModel> ViewModel;
	TSharedPtr<SImage> PresenceImage;
};

TSharedRef<SOnlinePresenceWidget> SOnlinePresenceWidget::New()
{
	return MakeShareable(new SOnlinePresenceWidgetImpl());
}

#undef LOCTEXT_NAMESPACE
