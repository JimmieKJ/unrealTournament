// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendItem.h"
#include "FriendViewModel.h"
#include "SFriendsToolTip.h"
#include "SFriendsList.h"
#include "SFriendsAndChatCombo.h"

#define LOCTEXT_NAMESPACE "SFriendItem"

class SFriendItemImpl : public SFriendItem
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		this->ViewModel = InViewModel;
		FFriendViewModel* ViewModelPtr = ViewModel.Get();
		MenuMethod = InArgs._Method;
		PendingAction = EFriendActionType::MAX_None;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SButton)
			.ButtonStyle(&FriendStyle.FriendListItemButtonSimpleStyle)
			.ContentPadding(9.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(this, &SFriendItemImpl::GetPresenceBrush)

					]
					+ SOverlay::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Right)
					[
						SNew(SImage)
						.Image(this, &SFriendItemImpl::GetStatusBrush)
					]
				]
				+ SHorizontalBox::Slot()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							[
								SNew(STextBlock)
								.Font(FriendStyle.FriendsFontStyleBold)
								.ColorAndOpacity(FriendStyle.DefaultFontColor)
								.Text(ViewModel->GetFriendName())
							]
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(FriendStyle.FriendsFontStyleSmallBold)
								.ColorAndOpacity(FriendStyle.DefaultFontColor)
								.Text(ViewModelPtr, &FFriendViewModel::GetFriendLocation)
							]
						]
						+ SHorizontalBox::Slot()
						.Padding(15, 0)
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						[
							SAssignNew(ActionMenuButton, SFriendsAndChatCombo)
							.FriendStyle(&FriendStyle)
							.ButtonText(FText::GetEmpty())
							.bShowIcon(false)
							.DropdownItems(this, &SFriendItemImpl::GetActionItems)
							.bSetButtonTextToSelectedItem(false)
							.bAutoCloseWhenClicked(true)
							.ButtonSize(FriendStyle.ActionComboButtonSize)
							.Placement(MenuPlacement_ComboBoxRight)
							.OnDropdownItemClicked(this, &SFriendItemImpl::HandleItemClicked)
							.OnDropdownOpened(this, &SFriendItemImpl::HandleActionMenuOpened)
							.Visibility(this, &SFriendItemImpl::ActionMenuButtonVisibility)
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(0, 0, 5, 0)
					[
						SNew(SUniformGridPanel)
						.Visibility(this, &SFriendItemImpl::PendingActionVisibility, EFriendActionType::RemoveFriend)
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SBox)
							.Padding(5)
							[
								SNew(SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, true)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::RemoveFriend)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.ColorAndOpacity(SFriendsList::GetActionButtonFontColor(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::RemoveFriend)))
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(EFriendActionType::ToText(EFriendActionType::RemoveFriend))
								]
							]
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SBox)
							.Padding(5)
							[
								SNew(SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, false)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.ColorAndOpacity(SFriendsList::GetActionButtonFontColor(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(EFriendActionType::ToText(EFriendActionType::CancelFriendRequest))
								]
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(0, 0, 5, 0)
					[
						SNew(SUniformGridPanel)
						.Visibility(this, &SFriendItemImpl::PendingActionVisibility, EFriendActionType::JoinGame)
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SBox)
							.Padding(5)
							[
								SNew(SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, true)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::JoinGame)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.ColorAndOpacity(SFriendsList::GetActionButtonFontColor(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::JoinGame)))
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(EFriendActionType::ToText(EFriendActionType::JoinGame))
								]
							]
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SBox)
							.Padding(5)
							[
								SNew(SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, false)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.ColorAndOpacity(SFriendsList::GetActionButtonFontColor(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(EFriendActionType::ToText(EFriendActionType::CancelFriendRequest))
								]
							]
						]
					]
				]
			]
		]);
	}

private:

	const FSlateBrush* GetPresenceBrush() const
	{
		if (ViewModel->IsOnline())
		{
			FString ClientId = ViewModel->GetClientId();
			//@todo samz - better way of finding known ids
			if (ClientId == TEXT("300d79839c914445948e3c1100f211db"))
			{
				return &FriendStyle.FortniteImageBrush;
			}
			else if (ClientId == TEXT("f3e80378aed4462498774a7951cd263f"))
			{
				return &FriendStyle.LauncherImageBrush;
			}
		}
		return &FriendStyle.FriendImageBrush;
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

	SFriendsAndChatCombo::FItemsArray GetActionItems() const
	{
		TArray<EFriendActionType::Type> Actions;
		ViewModel->EnumerateActions(Actions);

		SFriendsAndChatCombo::FItemsArray ActionItems;
		for (const auto& FriendAction : Actions)
		{
			ActionItems.AddItem(EFriendActionType::ToText(FriendAction), nullptr, FName(*EFriendActionType::ToText(FriendAction).ToString()), IsActionEnabled(FriendAction));
		}

		return ActionItems;
	}

	FName ActionToItemTag(EFriendActionType::Type Action)
	{
		return FName(*EFriendActionType::ToText(Action).ToString());
	}

	EFriendActionType::Type ItemTagToAction(const FName& Tag)
	{
		for (int32 ActionIdx = 0; ActionIdx < EFriendActionType::MAX_None; ActionIdx++)
		{
			EFriendActionType::Type ActionAsEnum = (EFriendActionType::Type)ActionIdx;
			if (Tag == ActionToItemTag(ActionAsEnum))
			{
				return ActionAsEnum;
			}
		}
		return EFriendActionType::MAX_None;
	}

	EVisibility PendingActionVisibility(EFriendActionType::Type ActionType) const
	{
		return PendingAction == ActionType ? EVisibility::Visible : EVisibility::Collapsed;
	}

	bool IsActionEnabled(const EFriendActionType::Type FriendAction) const
	{
		return ViewModel->CanPerformAction(FriendAction);
	}

	FReply HandlePendingActionClicked(bool bConfirm)
	{
		if (bConfirm)
		{
			ViewModel->PerformAction(PendingAction);
		}
		PendingAction = EFriendActionType::MAX_None;
		return FReply::Handled();
	}

	void HandleItemClicked(FName ItemTag)
	{
		if (ViewModel.IsValid())
		{
			EFriendActionType::Type FriendAction = ItemTagToAction(ItemTag);

			if (FriendAction == EFriendActionType::RemoveFriend || FriendAction == EFriendActionType::JoinGame)
			{
				PendingAction = FriendAction;
				FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
			}
			else
			{
				ViewModel->PerformAction(FriendAction);
			}
		}
	}

	void HandleActionMenuOpened() const
	{
		LastActionMenuOpened = ActionMenuButton;
	}

	EVisibility ActionMenuButtonVisibility() const
	{
		return (bIsHovered && PendingAction == EFriendActionType::MAX_None && !IsAnyActionMenuOpen()) || ActionMenuButton->IsOpen() ? EVisibility::Visible : EVisibility::Hidden;
	}

	TSharedPtr<SToolTip> CreateJoingGameToolTip()
	{
		if(!ViewModel->CanPerformAction(EFriendActionType::JoinGame))
		{
			return SNew(SFriendsToolTip)
			.DisplayText(ViewModel->GetJoinGameDisallowReason())
			.FriendStyle(&FriendStyle);
		}
		return nullptr;
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath) override
	{
		if (!NewWidgetPath.ContainsWidget(SharedThis(this)))
		{
			PendingAction = EFriendActionType::MAX_None;
		}
	}

	static bool IsAnyActionMenuOpen()
	{
		return LastActionMenuOpened.IsValid() && LastActionMenuOpened.Pin()->IsOpen();
	}

private:

	TSharedPtr<FFriendViewModel> ViewModel;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<SFriendsAndChatCombo> ActionMenuButton;

	TSharedPtr<SWidget> MenuContent;

	EPopupMethod MenuMethod;

	float OpenTime;

	EFriendActionType::Type PendingAction;

	/**
	 * Static ref to the last action menu combo that was opened
	 * Used to allow a single item to open/show its menu at once
	 * @todo: static isn't ideal but fine for now
	 */
	static TWeakPtr<SFriendsAndChatCombo> LastActionMenuOpened;
};

TWeakPtr<SFriendsAndChatCombo> SFriendItemImpl::LastActionMenuOpened;


TSharedRef<SFriendItem> SFriendItem::New()
{
	return MakeShareable(new SFriendItemImpl());
}

#undef LOCTEXT_NAMESPACE
