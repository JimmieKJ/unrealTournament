// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatTipViewModel.h"
#include "FriendsChatMarkupService.h"
#include "SChatMarkupTips.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE ""

/**
 * Declares the chat markup tips widget
*/
class SChatMarkupTipsImpl : public SChatMarkupTips
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FChatTipViewModel>& InViewModel, const TSharedRef<FFriendsFontStyleService> InFontService)
	{
		ViewModel = InViewModel;
		ViewModel->OnChatTipAvailable().AddSP(this, &SChatMarkupTipsImpl::HandleTipAvailable);
		ViewModel->OnChatTipSelected().AddSP(this, &SChatMarkupTipsImpl::OnChatTipSelected);
		MarkupStyle = *InArgs._MarkupStyle;
		FontService = InFontService;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&MarkupStyle.MarkupBackground)
			.Padding(MarkupStyle.MarkupPadding)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.MaxHeight(500)
				.AutoHeight()
				[
					SAssignNew(ChatTipList, SListView<TSharedRef<IChatTip > >)
					.SelectionMode(ESelectionMode::Single)
					.ListItemsSource(&ChatTips)
					.OnGenerateRow(this, &SChatMarkupTipsImpl::OnGenerateChatTip)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&MarkupStyle.SeperatorBrush)
					.Thickness(MarkupStyle.SeperatorThickness)
				]
				+SVerticalBox::Slot()
				.Padding(MarkupStyle.ButtonPadding)
				[
					SNew(STextBlock)
					.Font(InFontService, &FFriendsFontStyleService::GetSmallFont)
					.ColorAndOpacity(MarkupStyle.TipColor)
					.Text(FText::FromString("Press Enter to confirm or Esc to cancel"))
				]
			]
		]);
	}

private:

	void HandleTipAvailable()
	{
		ChatTips = ViewModel->GetChatTips();
		ChatTipList->RequestListRefresh();
	}

	TSharedRef<ITableRow> OnGenerateChatTip(TSharedRef<IChatTip > ChatTip, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<IChatTip> >, OwnerTable)
			[
				SNew(SButton)
				.ButtonStyle(&MarkupStyle.MarkupButtonStyle)
				.ButtonColorAndOpacity(this, &SChatMarkupTipsImpl::GetActionBackgroundColor, TWeakPtr<IChatTip>(ChatTip))
				.OnClicked(this, &SChatMarkupTipsImpl::HandleActionClicked, ChatTip)
				.ContentPadding(MarkupStyle.ButtonPadding)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(ChatTip->GetTipText())
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
						.ColorAndOpacity(FLinearColor::White)
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SSpacer)
						.Size(FVector2D(10, 0))
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(ChatTip->GetTipTextParam())
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetSmallFont)
						.ColorAndOpacity(FLinearColor::Gray)
					]
				]
			];
	}

	FReply HandleActionClicked(TSharedRef<IChatTip> ChatTip)
	{
		return ChatTip->ExecuteTip();
	}

	FSlateColor GetActionBackgroundColor(TWeakPtr<IChatTip> TabPtr) const
	{
		TSharedPtr<IChatTip> Tab = TabPtr.Pin();
		if (Tab.IsValid() && Tab == ViewModel->GetActiveTip())
		{
			return MarkupStyle.ButtonHoverColor;

		}
		return MarkupStyle.ButtonColor;
	}

	void OnChatTipSelected(TSharedRef<IChatTip> NewChatTip)
	{
		ChatTipList->RequestScrollIntoView(NewChatTip);
		ChatTipList->RequestListRefresh();
	}

private:

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	TSharedPtr<FChatTipViewModel> ViewModel;
	TSharedPtr<SVerticalBox> ChatTipBox;
	TArray<TSharedRef<IChatTip> > ChatTips;
	TSharedPtr< SListView<TSharedRef<IChatTip > > > ChatTipList;

	/** Holds the style to use when making the widget. */
	FFriendsMarkupStyle MarkupStyle;
};

TSharedRef<SChatMarkupTips> SChatMarkupTips::New()
{
	return MakeShareable(new SChatMarkupTipsImpl());
}

#undef LOCTEXT_NAMESPACE
