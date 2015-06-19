// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"
#include "ChatDisplayOptionsViewModel.h"
#include "SChatItem.h"

#include "RichTextLayoutMarshaller.h"

#define LOCTEXT_NAMESPACE "SChatWindow"

class SChatItemImpl : public SChatItem
{
public:

	virtual ~SChatItemImpl()
	{
		ViewModel->OnChanged().RemoveAll(this);
	}

	void Construct(const FArguments& InArgs, const TSharedRef<FChatItemViewModel>& InViewModel, const TSharedRef<FChatDisplayOptionsViewModel>& InOwnerViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		MenuMethod = InArgs._Method;
		this->ViewModel = InViewModel;
		this->OwnerViewModel = InOwnerViewModel;

		WrapWidth = InArgs._ChatWidth;

		ViewModel->OnChanged().AddSP(this, &SChatItemImpl::HandleChange);

		TextStyle = FriendStyle.TextStyle;
		if (!OwnerViewModel->GetChatViewModel()->IsChatChannelLocked())
		{
			TextStyle.ColorAndOpacity = GetChatColor();
		}

		if (FriendStyle.ChatChannelStyle == EChatChannelStyle::Conversation)
		{
			ConstructConversationChatItem(InArgs);
		}
		else
		{
			ConstructListChatItem(InArgs);
		}
	}

private:

	void HandleChange(const TSharedRef<FChatItemViewModel>& Item)
	{
		this->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SChatItemImpl::OneTimeTickUpdate));
	}

	EActiveTimerReturnType OneTimeTickUpdate(double InCurrentTime, float InDeltaTime)
	{
		return EActiveTimerReturnType::Stop;
	}

	void ConstructConversationChatItem(const FArguments& InArgs)
	{
		EHorizontalAlignment MessageHAlignment = !ViewModel->IsFromSelf() ? HAlign_Left : HAlign_Right;
		FMargin BoxPadding = !ViewModel->IsFromSelf() ? FMargin(0, 5, 40, 5) : FMargin(40, 5, 0, 5);

		EVisibility LeftCalloutVisibility = !ViewModel->IsFromSelf() ? EVisibility::Visible : EVisibility::Collapsed;
		EVisibility RightCalloutVisibility = ViewModel->IsFromSelf() ? EVisibility::Visible : EVisibility::Collapsed;

		FText SenderName;
		if (ViewModel->IsFromSelf())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("SenderName"), ViewModel->GetSenderName());
			SenderName = FText::Format(NSLOCTEXT("FChatItemViewModel", "SelfDisplayName", "<UserNameTextStyle.Default>{SenderName}</>"), Args);
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("SenderName"), ViewModel->GetSenderName());
			Args.Add(TEXT("NameStyle"), FText::FromString(GetTextHyperlinkStyle()));
			SenderName = FText::Format(NSLOCTEXT("FChatItemViewModel", "DisplayName", "<a id=\"UserName\" style=\"{NameStyle}\">{SenderName}</>"), Args);
		}

		TSharedPtr<SWidget> MessageFooter;
		if (!ViewModel->GetSenderName().IsEmptyOrWhitespace())
		{
			MessageFooter = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SRichTextBlock)
				.Visibility(this, &SChatItemImpl::GetTextVisibility)
				.Text(SenderName)
				.TextStyle(&TextStyle)
				.DecoratorStyleSet(&FFriendsAndChatModuleStyle::Get())
				+ SRichTextBlock::HyperlinkDecorator(TEXT("UserName"), this, &SChatItemImpl::HandleNameClicked)
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			.Padding(3, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("-")))
				.TextStyle(&TextStyle)
				.Font(FriendStyle.FriendsFontStyleSmall)
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(ViewModel.Get(), &FChatItemViewModel::GetMessageTimeText)
				.TextStyle(&TextStyle)
				.Font(FriendStyle.FriendsFontStyleSmall)
			];
		}
		else
		{
			MessageFooter = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(ViewModel.Get(), &FChatItemViewModel::GetMessageTimeText)
				.TextStyle(&TextStyle)
				.Font(FriendStyle.FriendsFontStyleSmall)
			];
		}

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			.Padding(10, 20, 0, 0)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(&FriendStyle.ChatBubbleLeftCallout)
				.Visibility(LeftCalloutVisibility)
			]
			+SHorizontalBox::Slot()
			.Padding(BoxPadding)
			.HAlign(MessageHAlignment)
			[
				SNew(SBorder)
				.Padding(FMargin(5, 10, 10, 10))
				.BorderImage(&FriendStyle.ChatBubbleBackgroundBrush)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					.Padding(FMargin(5, 0, 0, 0))
					[
						SNew(SRichTextBlock)
						.Visibility(this, &SChatItemImpl::GetTextVisibility)
						.Text(ViewModel.Get(), &FChatItemViewModel::GetMessage)
						.TextStyle(&TextStyle)
						.DecoratorStyleSet(&FFriendsAndChatModuleStyle::Get())
						.WrapTextAt(this, &SChatItemImpl::GetWrapWidth)
						+ SRichTextBlock::HyperlinkDecorator(TEXT("UserName"), this, &SChatItemImpl::HandleNameClicked)
					]
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Left)
					.Padding(FMargin(0, 10, 0, 0))
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Top)
						.AutoWidth()
						.Padding(FMargin(5, 0))
						[
							SNew(SBox)
							.WidthOverride(12)
							.HeightOverride(12)
							.Visibility(ViewModel->GetMessageType() == EChatMessageType::Whisper || OwnerViewModel->GetChatViewModel()->IsChatChannelLocked() ? EVisibility::Collapsed : EVisibility::Visible)
							[
								SNew(SImage)
								.ColorAndOpacity(this, &SChatItemImpl::GetChannelColor)
								.Image(this, &SChatItemImpl::GetChatIcon)
							]
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.AutoWidth()
						.Padding(FMargin(5, 0))
						[
							MessageFooter.ToSharedRef()
						]
					]
				]
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			.Padding(0, 20, 25, 0)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(&FriendStyle.ChatBubbleRightCallout)
				.Visibility(RightCalloutVisibility)
			]
		]);
	}

	void ConstructListChatItem(const FArguments& InArgs)
	{
		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(FMargin(5,1))
			[
				SNew(SImage)
				.ColorAndOpacity(this, &SChatItemImpl::GetChannelColor)
				.Image(this, &SChatItemImpl::GetChatIcon)
				.Visibility(ViewModel->GetMessageType() == EChatMessageType::Whisper && OwnerViewModel->GetChatViewModel()->IsChatChannelLocked() ? EVisibility::Collapsed : EVisibility::Visible)
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			.Padding(FMargin(5, 1, 2, 1))
			[
				SNew( SRichTextBlock )
				.Visibility(this, &SChatItemImpl::GetTextVisibility)
				.Text(this, &SChatItemImpl::GetListMessageText)
				.TextStyle(&TextStyle)
				.DecoratorStyleSet(&FFriendsAndChatModuleStyle::Get())
				.WrapTextAt(this, &SChatItemImpl::GetWrapWidth)
				+ SRichTextBlock::HyperlinkDecorator(TEXT( "UserName" ), this, &SChatItemImpl::HandleNameClicked)
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(5,1))
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(ViewModel.Get(), &FChatItemViewModel::GetMessageTimeText)
				.ColorAndOpacity(this, &SChatItemImpl::GetTimeDisplayColor)
				.Font(FriendStyle.FriendsFontStyleSmall)
			]
		]);
	}

	FSlateColor GetTimeDisplayColor() const
	{
		if(OwnerViewModel->GetOverrideColorSet())
		{
			return OwnerViewModel->GetFontOverrideColor();
		}
		else
		{
			switch(ViewModel->GetMessageType())
			{
				case EChatMessageType::Global: return FriendStyle.DefaultChatColor.CopyWithNewOpacity(OwnerViewModel->GetTimeTransparency()); break;
				case EChatMessageType::Whisper: return FriendStyle.WhisplerChatColor.CopyWithNewOpacity(OwnerViewModel->GetTimeTransparency()); break;
				case EChatMessageType::Party: return FriendStyle.PartyChatColor.CopyWithNewOpacity(OwnerViewModel->GetTimeTransparency()); break;
				default: return FLinearColor::Gray;
			}
		}
	}

	EVisibility GetTextVisibility() const
	{
		return OwnerViewModel->GetOverrideColorSet() ? EVisibility::Hidden : EVisibility::Visible;
	}

	FSlateColor GetChannelColor () const
	{
		return OwnerViewModel->GetOverrideColorSet() ? OwnerViewModel->GetFontOverrideColor() : GetChatColor();
	}

	FSlateColor GetChatColor () const
	{
		FSlateColor DisplayColor;
		switch(ViewModel->GetMessageType())
		{
			case EChatMessageType::Global: DisplayColor = FriendStyle.DefaultChatColor; break;
			case EChatMessageType::Whisper: DisplayColor =  FriendStyle.WhisplerChatColor; break;
			case EChatMessageType::Party: DisplayColor =  FriendStyle.PartyChatColor; break;
			default: DisplayColor = FLinearColor::Gray;
		}
		return DisplayColor;
	}
	
	const FSlateBrush* GetChatIcon() const
	{
		if(OwnerViewModel->GetOverrideColorSet())
		{
			return nullptr;
		}
		else
		{
			switch(ViewModel->GetMessageType())
			{
				case EChatMessageType::Global: return &FriendStyle.ChatGlobalBrush; break;
				case EChatMessageType::Whisper: return &FriendStyle.ChatWhisperBrush; break;
				case EChatMessageType::Party: return &FriendStyle.ChatPartyBrush; break;
				default:
				return nullptr;
			}
		}
	}

	const FString GetTextHyperlinkStyle() const
	{
		if (OwnerViewModel->GetChatViewModel()->IsChatChannelLocked())
		{
			return TEXT("UserNameTextStyle.DefaultHyperlink");
		}
		else if (OwnerViewModel->GetOverrideColorSet())
		{
			return TEXT("UserNameTextStyle.DefaultHyperlink");
		}
		else
		{
			switch (ViewModel->GetMessageType())
			{
				case EChatMessageType::Global: return TEXT("UserNameTextStyle.GlobalHyperlink"); break;
				case EChatMessageType::Whisper: return TEXT("UserNameTextStyle.Whisperlink"); break;
				case EChatMessageType::Party: return TEXT("UserNameTextStyle.PartyHyperlink"); break;
				default:
					return TEXT("UserNameTextStyle.DefaultHyperlink");
			}
		}
	}

	void HandleNameClicked( const FSlateHyperlinkRun::FMetadata& Metadata )
	{
		OwnerViewModel->SetChannelUserClicked(ViewModel.ToSharedRef());
	}

	float GetWrapWidth() const
	{
		return WrapWidth.Get(-1) - 110;
	}

	FText GetListMessageText() const
	{
		bool bRequiresTextUpdate = false;
		const FText& TextToSet = ViewModel->GetMessage();
		if (!TextLastUpdate.IdenticalTo(TextToSet))
		{
			// The pointer used by the bound text has changed, however the text may still be the same - check that now
			if (!TextLastUpdate.IsDisplayStringEqualTo(TextToSet))
			{
				// The source text has changed, so update the internal text
				bRequiresTextUpdate = true;
			}

			// Update this even if the text is lexically identical, as it will update the pointer compared by IdenticalTo for the next Tick
			TextLastUpdate = FTextSnapshot(TextToSet);
		}

		if (bRequiresTextUpdate)
		{
			if(ViewModel->GetMessageType() == EChatMessageType::Party)
			{
				CurrentListMessageText = TextToSet;
			}
			else
			{
				FFormatNamedArguments Args;
				if(ViewModel->IsFromSelf() && ViewModel->GetMessageType() != EChatMessageType::Global)
				{
					Args.Add(TEXT("SenderName"), ViewModel->GetRecipientName());
				}
				else
				{
					Args.Add(TEXT("SenderName"), ViewModel->GetSenderName());
				}

				Args.Add(TEXT("NameStyle"), FText::FromString(GetTextHyperlinkStyle()));
				Args.Add(TEXT("Message"), TextToSet);
				CurrentListMessageText = FText::Format(NSLOCTEXT("FChatItemViewModel", "DisplayNameAndMessage", "<a id=\"UserName\" style=\"{NameStyle}\">{SenderName}</>: {Message}"), Args);
			}
		}

		return CurrentListMessageText;
	}

private:

	// Holds the Friends List view model
	TSharedPtr<FChatItemViewModel> ViewModel;

	// Holds the owner view model
	TSharedPtr<FChatDisplayOptionsViewModel> OwnerViewModel;

	TSharedPtr<SBorder> FriendItemBorder;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	// Holds the menu method - Full screen requires use owning window or crashes.
	EPopupMethod MenuMethod;

	TAttribute<float> WrapWidth;

	FTextBlockStyle TextStyle;

	/** The state of the text the last time it was updated (used to allow updates when the text is changed) */
	mutable FTextSnapshot TextLastUpdate;

	mutable FText CurrentListMessageText;
};

TSharedRef<SChatItem> SChatItem::New()
{
	return MakeShareable(new SChatItemImpl());
}

#undef LOCTEXT_NAMESPACE
