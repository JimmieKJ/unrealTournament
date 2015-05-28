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

		FFriendsAndChatComboButtonStyle ActionButtonStyle;
		ActionButtonStyle.ComboButtonStyle = &FriendStyle.ActionComboButtonStyle;
		ActionButtonStyle.ButtonTextStyle = &FriendStyle.ActionComboButtonTextStyle;

		TSharedPtr<SButton> RemoveFriendButton;
		TSharedPtr<SButton> CancelFriendButton;
		TSharedPtr<SButton> JoinGameButton;
		TSharedPtr<SButton> CancelFriendRequestButton;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(0))
			[
				SNew(SBorder)
				.OnMouseDoubleClick(this, &SFriendItemImpl::OnDoubleClick)
				.Padding(FMargin(0))
				.BorderBackgroundColor(FLinearColor::Transparent)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10, 10)
					.AutoWidth()
					.VAlign(VAlign_Top)
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
					.Padding(0, 10)
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
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Top)
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(FriendStyle.FriendsFontStyleBold)
									.ColorAndOpacity(FriendStyle.DefaultFontColor)
									.Text(FText::FromString(ViewModel->GetName()))
								]
								+ SVerticalBox::Slot()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Top)
								.AutoHeight()
								[
									SNew(STextBlock)
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.ColorAndOpacity(FriendStyle.DefaultDullFontColor)
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
								.ButtonStyleOverride(&ActionButtonStyle)
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
							 	.Cursor(EMouseCursor::Hand)
							]
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Bottom)
						.Padding(0, 40, 0, 0)
						[
							SNew(SUniformGridPanel)
							.SlotPadding(FMargin(5.0f, 0.0f))
							.MinDesiredSlotWidth(100.0f)
							.MinDesiredSlotHeight(30.0f)
							.Visibility(this, &SFriendItemImpl::PendingActionVisibility, EFriendActionType::RemoveFriend)
							+ SUniformGridPanel::Slot(0, 0)
							[
								SAssignNew(RemoveFriendButton, SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, true)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::RemoveFriend)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.Cursor(EMouseCursor::Hand)
								[
									SNew(STextBlock)
									.ColorAndOpacity(SFriendsList::GetActionButtonFontColor(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::RemoveFriend)))
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(EFriendActionType::ToText(EFriendActionType::RemoveFriend))
								]
							]
							+ SUniformGridPanel::Slot(1, 0)
							[
								SAssignNew(CancelFriendButton, SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, false)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.Cursor(EMouseCursor::Hand)
								[
									SNew(STextBlock)
									.ColorAndOpacity(SFriendsList::GetActionButtonFontColor(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(EFriendActionType::ToText(EFriendActionType::CancelFriendRequest))
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
								SAssignNew(JoinGameButton, SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, true)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::JoinGame)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.Cursor(EMouseCursor::Hand)
								[
									SNew(STextBlock)
									.ColorAndOpacity(SFriendsList::GetActionButtonFontColor(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::JoinGame)))
									.Font(FriendStyle.FriendsFontStyleSmallBold)
									.Text(EFriendActionType::ToText(EFriendActionType::JoinGame))
								]
							]
							+ SUniformGridPanel::Slot(1, 0)
							[
								SAssignNew(CancelFriendRequestButton, SButton)
								.OnClicked(this, &SFriendItemImpl::HandlePendingActionClicked, false)
								.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest)))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.Cursor(EMouseCursor::Hand)
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

		RemoveFriendButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
			TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendItemImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(RemoveFriendButton), EFriendActionType::ToActionLevel(EFriendActionType::RemoveFriend))));

		CancelFriendButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
			TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendItemImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(CancelFriendButton), EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest))));

		JoinGameButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
			TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendItemImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(JoinGameButton), EFriendActionType::ToActionLevel(EFriendActionType::JoinGame))));

		CancelFriendRequestButton->SetForegroundColor(TAttribute<FSlateColor>::Create(
			TAttribute<FSlateColor>::FGetter::CreateSP(this, &SFriendItemImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(CancelFriendRequestButton), EFriendActionType::ToActionLevel(EFriendActionType::CancelFriendRequest))));
	}

private:

	FSlateColor GetForegroundWhenHovered(TWeakPtr<SButton> WidgetInQuestionPtr, EFriendActionLevel ActionLevel) const
	{
		const TSharedPtr<SButton> WidgetInQuestion = WidgetInQuestionPtr.Pin();
		const bool IsDisabled = WidgetInQuestion.IsValid() && !WidgetInQuestion->IsEnabled();
		const bool IsHovered = WidgetInQuestion.IsValid() && WidgetInQuestion->IsHovered();

		if (IsDisabled)
		{
			return FLinearColor::Black;
		}

		if (IsHovered)
		{
			return FriendStyle.ButtonInvertedForegroundColor;
		}

		if (ActionLevel == EFriendActionLevel::Action)
		{
			return FLinearColor::White;
		}

		return FriendStyle.ButtonForegroundColor;
	}

	FReply OnDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
	{
		ViewModel->PerformAction(EFriendActionType::Chat);
		return FReply::Handled();
	}

	const FSlateBrush* GetPresenceBrush() const
	{
		if (ViewModel->IsOnline())
		{
			FString ClientId = ViewModel->GetClientId();
			if (!ClientId.IsEmpty())
			{
				//@todo samz - better way of finding known ids
				if (ClientId == FFriendItem::FortniteClientId)
				{
					return &FriendStyle.FortniteImageBrush;
				}
				else if (FFriendItem::LauncherClientIds.Contains(ClientId))
				{
					return &FriendStyle.LauncherImageBrush;
				}
				else if (ClientId == FFriendItem::UnrealTournamentClientId)
				{
					return &FriendStyle.UTImageBrush;
				}
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

			if (FriendAction == EFriendActionType::RemoveFriend || (FriendAction == EFriendActionType::JoinGame && ViewModel->IsInGameSession()))
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
