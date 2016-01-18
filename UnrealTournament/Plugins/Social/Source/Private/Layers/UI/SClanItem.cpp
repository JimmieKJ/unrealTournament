// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SClanItem.h"
#include "ClanInfoViewModel.h"

#define LOCTEXT_NAMESPACE "SClassItem"

class SClanItemImpl : public SClanItem
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FClanInfoViewModel>& InClanInfoViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ClanInfoViewModel = InClanInfoViewModel;

// 		FFriendsAndChatComboButtonStyle ActionButtonStyle;
// 		ActionButtonStyle.ComboButtonStyle = &FriendStyle.FriendsComboStyle.ActionComboButtonStyle;
// 		ActionButtonStyle.ButtonTextStyle = &FriendStyle.FriendsComboStyle.ActionComboButtonTextStyle;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.Padding(FMargin(0))
			.BorderBackgroundColor(FLinearColor::Transparent)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10, 10)
					.AutoWidth()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							// Clan Image
							SNew(SImage)
							.Image(&FriendStyle.FriendsListStyle.FriendImageBrush)
						]
						+SOverlay::Slot()
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Right)
						[
							// Primary Clan Indicator
							SNew(SImage)
							.Visibility(this, &SClanItemImpl::PrimaryClanVisibility)
							.Image(&FriendStyle.FriendsListStyle.OnlineBrush)
						]
					]
// 					+ SHorizontalBox::Slot()
// 					.Padding(15, 0)
// 					.VAlign(VAlign_Center)
// 					.HAlign(HAlign_Right)
// 					[
// 						SAssignNew(ActionMenuButton, SFriendsAndChatCombo)
// 						.ComboStyle(&FriendStyle.FriendsComboStyle)
// 						.ButtonStyleOverride(&ActionButtonStyle)
// 						.ButtonText(FText::GetEmpty())
// 						.bShowIcon(false)
// 						.DropdownItems(this, &SClanItemImpl::GetActionItems)
// 						.bSetButtonTextToSelectedItem(false)
// 						.bAutoCloseWhenClicked(true)
// 						.ButtonSize(FriendStyle.FriendsComboStyle.ActionComboButtonSize)
// 						.Placement(MenuPlacement_ComboBoxRight)
// 						.OnDropdownItemClicked(this, &SClanItemImpl::HandleItemClicked)
// 						.OnDropdownOpened(this, &SClanItemImpl::HandleActionMenuOpened)
// 						.Visibility(this, &SClanItemImpl::ActionMenuButtonVisibility)
// 						.Cursor(EMouseCursor::Hand)
// 					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.Padding(0, 10)
					[
						SNew(SButton)
						.OnClicked(this, &SClanItemImpl::OnOpenClanDetails)
						.ButtonStyle(&FriendStyle.FriendsListStyle.GlobalChatButtonStyle)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SImage)
								.Image(&FriendStyle.FriendsListStyle.ClanMembersBrush)
							]
							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Padding(10)
							[
								SNew(STextBlock)
								.Font(FriendStyle.FriendsNormalFontStyle.FriendsFontNormalBold)
								.ColorAndOpacity(FriendStyle.FriendsNormalFontStyle.DefaultFontColor)
								.Text(this, &SClanItemImpl::GetClanMemberCountText)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SImage)
								.Image(&FriendStyle.FriendsListStyle.ClanDetailsBrush)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 0)
				[
					SNew(STextBlock)
					.Font(FriendStyle.FriendsNormalFontStyle.FriendsFontNormalBold)
					.ColorAndOpacity(FriendStyle.FriendsNormalFontStyle.DefaultFontColor)
					.Text(ClanInfoViewModel->GetClanTitle())
				]
			]
		]);
	}

private:

	FReply OnOpenClanDetails()
	{
		ClanInfoViewModel->OpenClanDetails().Broadcast(ClanInfoViewModel.ToSharedRef());
		return FReply::Handled();
	}

	FName ActionToItemTag(EClanActionType::Type Action)
	{
		return FName(*EClanActionType::ToText(Action).ToString());
	}

	EClanActionType::Type ItemTagToAction(const FName& Tag)
	{
		for (int32 ActionIdx = 0; ActionIdx < EClanActionType::MAX_None; ActionIdx++)
		{
			EClanActionType::Type ActionAsEnum = (EClanActionType::Type)ActionIdx;
			if (Tag == ActionToItemTag(ActionAsEnum))
			{
				return ActionAsEnum;
			}
		}
		return EClanActionType::MAX_None;
	}

	void HandleItemClicked(FName ItemTag)
	{
		EClanActionType::Type Action = ItemTagToAction(ItemTag);
		if (Action != EClanActionType::MAX_None)
		{
			ClanInfoViewModel->PerformAction(Action);
		}
	}

	void HandleActionMenuOpened() const
	{
	}

	EVisibility ActionMenuButtonVisibility() const
	{
		return EVisibility::Visible;
	}

	EVisibility PrimaryClanVisibility() const
	{
		return ClanInfoViewModel->IsPrimaryClan() ? EVisibility::Visible : EVisibility::Hidden;
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	FText GetClanMemberCountText() const
	{
		return FText::AsNumber(ClanInfoViewModel->GetMemberCount());
	}


	TSharedPtr<FClanInfoViewModel> ClanInfoViewModel;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	TSharedPtr<SWidget> MenuContent;

	EPopupMethod MenuMethod;
};

TSharedRef<SClanItem> SClanItem::New()
{
	return MakeShareable(new SClanItemImpl());
}

#undef LOCTEXT_NAMESPACE
