// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"
#include "ChatDisplayOptionsViewModel.h"
#include "SChatItem.h"

#define LOCTEXT_NAMESPACE "SChatWindow"

class SChatItemImpl : public SChatItem
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FChatItemViewModel>& InViewModel, const TSharedRef<FChatDisplayOptionsViewModel>& InOwnerViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		MenuMethod = InArgs._Method;
		float WindowWidth = InArgs._ChatWidth;
		this->ViewModel = InViewModel;
		this->OwnerViewModel = InOwnerViewModel;

		FText DisplayNameText = ViewModel->GetFriendNameDisplayText();
		if(ViewModel->IsFromSelf())
		{ 
			if (ViewModel->GetMessageType() == EChatMessageType::Whisper)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("Username"), ViewModel->GetFriendNameDisplayText());
				DisplayNameText = FText::Format(LOCTEXT("SChatItem_To", "To {Username}"), Args);
			}
		}

		FText DisplayText;
		if(ViewModel->GetMessageType() == EChatMessageType::Party)
		{
			DisplayText = ViewModel->GetMessage();
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("DisplayName"), DisplayNameText);
			Args.Add(TEXT("Message"), ViewModel->GetMessage());
			Args.Add(TEXT("NameStyle"), FText::FromString(GetTextHyperlinkStyle()));
			DisplayText = FText::Format(LOCTEXT("SChatItem_Message", "<a id=\"UserName\" style=\"{NameStyle}\">{DisplayName}</>{Message}"), Args);
		}
		
		FTextBlockStyle TextStyle = FriendStyle.TextStyle;
		TextStyle.ColorAndOpacity = GetChatColor();

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
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(5, 1, 2, 1))
			[
				SNew( SRichTextBlock )
				.Visibility(this, &SChatItemImpl::GetTextVisibility)
				.Text(DisplayText)
				.TextStyle(&TextStyle)
				.DecoratorStyleSet(&FFriendsAndChatModuleStyle::Get())
				.WrapTextAt(WindowWidth - FriendStyle.ChatListPadding)
				+ SRichTextBlock::HyperlinkDecorator(TEXT( "UserName" ), this, &SChatItemImpl::HandleNameClicked)
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(5,1))
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(ViewModel->GetMessageTime())
				.ColorAndOpacity(this, &SChatItemImpl::GetTimeDisplayColor)
				.Font(FriendStyle.FriendsFontStyleSmall)
			]
		]);
	}
private:

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
		switch(ViewModel->GetMessageType())
		{
			case EChatMessageType::Global: return TEXT("UserNameTextStyle.GlobalHyperlink"); break;
			case EChatMessageType::Whisper: return TEXT("UserNameTextStyle.Whisperlink"); break;
			case EChatMessageType::Party: return TEXT("UserNameTextStyle.PartyHyperlink"); break;
			default:
			return FString();
		}
	}

	void HandleNameClicked( const FSlateHyperlinkRun::FMetadata& Metadata )
	{
		OwnerViewModel->SetChannelUserClicked(ViewModel.ToSharedRef());
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
};

TSharedRef<SChatItem> SChatItem::New()
{
	return MakeShareable(new SChatItemImpl());
}

#undef LOCTEXT_NAMESPACE
