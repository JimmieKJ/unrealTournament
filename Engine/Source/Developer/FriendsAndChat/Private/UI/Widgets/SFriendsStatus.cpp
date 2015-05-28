// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsStatus.h"
#include "SFriendsAndChatCombo.h"
#include "FriendsStatusViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsStatus"

/**
 * Declares the Friends Status display widget
 */
class SFriendsStatusImpl : public SFriendsStatus
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendsStatusViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;

		FFriendsStatusViewModel* ViewModelPtr = &InViewModel.Get();

		const TArray<FFriendsStatusViewModel::FOnlineState>& StatusOptions = ViewModelPtr->GetStatusList();
		SFriendsAndChatCombo::FItemsArray ComboMenuItems;
		for (const auto& StatusOption : StatusOptions)
		{
			if (StatusOption.bIsDisplayed)
			{
				ComboMenuItems.AddItem(StatusOption.DisplayText, GetStatusBrush(StatusOption.State), FName(*StatusOption.DisplayText.ToString()));
			}
		}

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SFriendsAndChatCombo)			
			.FriendStyle(&FriendStyle)
			.ButtonText(ViewModelPtr, &FFriendsStatusViewModel::GetStatusText)
			.bShowIcon(true)
			.DropdownItems(ComboMenuItems)
			.bSetButtonTextToSelectedItem(false)
			.bAutoCloseWhenClicked(true)
			.ButtonSize(FriendStyle.StatusButtonSize)
			.OnDropdownItemClicked(this, &SFriendsStatusImpl::HandleStatusChanged)
			.IsEnabled(this, &SFriendsStatusImpl::IsStatusEnabled)
		]);
	}

private:

	bool IsStatusEnabled() const
	{
		return true;
	}

	void HandleStatusChanged(FName ItemTag)
	{
		if (ViewModel.IsValid())
		{
			const TArray<FFriendsStatusViewModel::FOnlineState>& StatusOptions = ViewModel->GetStatusList();
			
			const FFriendsStatusViewModel::FOnlineState* FoundStatePtr = StatusOptions.FindByPredicate([ItemTag](const FFriendsStatusViewModel::FOnlineState& InOnlineState) -> bool
			{
				return InOnlineState.DisplayText.ToString() == ItemTag.ToString();
			});

			EOnlinePresenceState::Type OnlineState = EOnlinePresenceState::Offline;

			if (FoundStatePtr != nullptr)
			{
				OnlineState =  FoundStatePtr->State;
			}

			ViewModel->SetOnlineStatus(OnlineState);
		}
	}

	const FSlateBrush* GetCurrentStatusBrush() const
	{
		if (ViewModel.IsValid())
		{
			return GetStatusBrush(ViewModel->GetOnlineStatus());
		}
		return nullptr;
	}

	const FSlateBrush* GetStatusBrush(EOnlinePresenceState::Type OnlineState) const
	{
		switch (OnlineState)
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
