// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SInviteItem.h"
#include "SFriendsToolTip.h"
#include "SFriendsList.h"
#include "FriendViewModel.h"

#define LOCTEXT_NAMESPACE "SInviteItem"

class SInviteItemImpl : public SInviteItem
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendViewModel>& InViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		FFriendViewModel* ViewModelPtr = this->ViewModel.Get();

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			.Padding(10, 10)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(this, &SInviteItemImpl::GetPresenceBrush)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.Padding(0, 13, 0, 10)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Font(FriendStyle.FriendsFontStyleBold)
					.ColorAndOpacity(FriendStyle.DefaultFontColor)
					.Text(FText::FromString(ViewModel->GetName()))
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(0, 25, 0, 0)
				[
					SAssignNew(OptionContainer, SUniformGridPanel)
					.SlotPadding(FMargin(5.0f, 0.0f))
					.MinDesiredSlotWidth(100.0f)
					.MinDesiredSlotHeight(30.0f)
				]
			]
		]);

		CreateOptions();
	}

private:

	const FSlateBrush* GetPresenceBrush() const
	{
		FString ClientId = ViewModel->GetClientId();
		if (!ClientId.IsEmpty())
		{
			if (ClientId == FFriendItem::FortniteClientId)
			{
				return &FriendStyle.FortniteImageBrush;
			}
			else if (ClientId == FFriendItem::UnrealTournamentClientId)
			{
				return &FriendStyle.UTImageBrush;
			}
		}
		return &FriendStyle.FriendImageBrush;
	}

	void CreateOptions()
	{
		TArray<EFriendActionType::Type> Actions;
		ViewModel->EnumerateActions(Actions);

		for(const auto& FriendAction : Actions)
		{
			EFriendActionLevel ActionLevel = EFriendActionType::ToActionLevel(FriendAction);

			TSharedPtr<SButton> Button;
			OptionContainer->AddSlot(OptionContainer->GetChildren()->Num(), 0)
			[
				SAssignNew(Button, SButton)
				.ToolTip(FriendAction == EFriendActionType::JoinGame ? CreateJoingGameToolTip() : NULL)
				.IsEnabled(this, &SInviteItemImpl::IsActionEnabled, FriendAction)
				.OnClicked(this, &SInviteItemImpl::PerformAction, FriendAction)
				.ButtonStyle(SFriendsList::GetActionButtonStyle(FriendStyle, ActionLevel))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Cursor(EMouseCursor::Hand)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Font(FriendStyle.FriendsFontStyleSmallBold)
					.Text(EFriendActionType::ToText(FriendAction))
				]
			];

			Button->SetForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateSP(this, &SInviteItemImpl::GetForegroundWhenHovered, TWeakPtr<SButton>(Button), ActionLevel)));
		}
	}

	FSlateColor GetForegroundWhenHovered(TWeakPtr<SButton> WidgetInQuestionPtr, EFriendActionLevel ActionLevel) const
	{
		const TSharedPtr<SButton> WidgetInQuestion = WidgetInQuestionPtr.Pin();
		const bool IsDisabled = WidgetInQuestion.IsValid() && !WidgetInQuestion->IsEnabled();
		const bool IsHovered = WidgetInQuestion.IsValid() && WidgetInQuestion->IsHovered();

		if (IsDisabled)
		{
			return FLinearColor::Black;
		}

		if (ActionLevel == EFriendActionLevel::Action)
		{
			return FLinearColor::White;
		}

		if (IsHovered)
		{
			return FriendStyle.ButtonInvertedForegroundColor;
		}

		return FriendStyle.ButtonForegroundColor;
	}

	FReply PerformAction(EFriendActionType::Type FriendAction)
	{
		ViewModel->PerformAction(FriendAction);
		return FReply::Handled();
	}

	bool IsActionEnabled(EFriendActionType::Type FriendAction) const
	{
		return ViewModel->CanPerformAction(FriendAction);
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


private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<SUniformGridPanel> OptionContainer;
	TSharedPtr<FFriendViewModel> ViewModel;
};

TSharedRef<SInviteItem> SInviteItem::New()
{
	return MakeShareable(new SInviteItemImpl());
}

#undef LOCTEXT_NAMESPACE
