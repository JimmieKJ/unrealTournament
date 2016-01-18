// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsFontStyleService.h"
#include "SChatWindow.h"
#include "ChatViewModel.h"
#include "ChatItemViewModel.h"
#include "SScrollBox.h"
#include "SFriendsList.h"
#include "SFriendActions.h"
#include "SChatEntryWidget.h"
#include "ChatTextLayoutMarshaller.h"
#include "TextDecorators.h"
#include "ChatHyperlinkDecorator.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE "SChatWindow"

class SChatWindowImpl : public SChatWindow
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FChatViewModel>& InViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService)
	{
		FontService = InFontService;
		LastMessageIndex = 0;
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;
		FChatViewModel* ViewModelPtr = ViewModel.Get();
		ViewModel->OnChatListUpdated().AddSP(this, &SChatWindowImpl::RefreshChatList);
		ViewModel->OnSettingsUpdated().AddSP(this, &SChatWindowImpl::SettingsChanged);
		ViewModel->OnScrollToEnd().AddSP(this, &SChatWindowImpl::ScrollToEnd);
		bUserHasScrolled = false;

		FFriendsAndChatModuleStyle::Initialize(FriendStyle);

		OnHyperlinkClicked = FSlateHyperlinkRun::FOnClick::CreateSP(SharedThis(this), &SChatWindowImpl::HandleNameClicked);

		FTextBlockStyle TextStyle = FriendStyle.FriendsChatStyle.TextStyle;
		TextStyle.Font = FontService->GetNormalFont();
		RichTextMarshaller = ViewModel->GetRichTextMarshaller(&FriendStyle, TextStyle);

		RichTextMarshaller->AddUserNameHyperlinkDecorator(FChatHyperlinkDecorator::Create(TEXT("UserName"), OnHyperlinkClicked));

		ExternalScrollbar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(4, 4))
		.Style(&FriendStyle.ScrollBarStyle)
		.AlwaysShowScrollbar(false);

		HorizontalScrollbar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.AlwaysShowScrollbar(false);

		ExternalScrollbar->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SChatWindowImpl::GetScrollbarVisibilty)));
		HorizontalScrollbar->SetVisibility(EVisibility::Collapsed);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			[
				SAssignNew(RichText, SMultiLineEditableTextBox)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.Style(&FriendStyle.FriendsChatStyle.ChatDisplayTextStyle)
				.WrapTextAt(this, &SChatWindowImpl::GetChatWrapWidth)
				.Marshaller(RichTextMarshaller.ToSharedRef())
				.IsReadOnly(true)
				.HScrollBar(HorizontalScrollbar)
				.VScrollBar(ExternalScrollbar)
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			[
				SAssignNew(MenuAnchor, SMenuAnchor)
				.Placement(EMenuPlacement::MenuPlacement_CenteredAboveAnchor)
			]
		]);

		RichTextMarshaller->SetOwningWidget(RichText.ToSharedRef());

		RefreshChatList();
	}

	virtual void HandleWindowActivated() override
	{
	}

	virtual void HandleWindowDeactivated() override
	{
		ViewModel->SetIsActive(false);
	}

	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		bUserHasScrolled = true;
		ViewModel->SetInteracted();
		return FReply::Handled();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SUserWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		int32 NewWindowWidth = FMath::FloorToInt(AllottedGeometry.GetLocalSize().X);
		if(NewWindowWidth != WindowWidth)
		{
			WindowWidth = NewWindowWidth;
		}

		if (!bUserHasScrolled && ExternalScrollbar.IsValid() && ExternalScrollbar->IsScrolling())
		{
			bUserHasScrolled = true;
		}
		if (bUserHasScrolled && ExternalScrollbar.IsValid() && !ExternalScrollbar->IsScrolling())
		{
			if (ExternalScrollbar->DistanceFromBottom() == 0)
			{
				bUserHasScrolled = false;
			}
		}
	}

private:

	bool HasMenuOptions() const
	{
		return true;
	}

	void SettingsChanged()
	{
		RefreshChatList();
	}

	void HandleChatMessageCommitted()
	{
		if (RichText.IsValid())
		{
			ScrollToEnd();
		}
	}

	void HandleChatListGotFocus()
	{
	}

	void RefreshChatList()
	{
		// ToDo - Move the following logic into the view model
		TArray<TSharedRef<FChatItemViewModel>> Messages = ViewModel->GetMessages();
		for (;LastMessageIndex < Messages.Num(); ++LastMessageIndex)
		{
			TSharedRef<FChatItemViewModel> ChatItem = Messages[LastMessageIndex];

			if (LastMessage.IsValid() &&
				LastMessage->GetMessageType() == ChatItem->GetMessageType() &&
				LastMessage->IsFromSelf() == ChatItem->IsFromSelf() &&
				LastMessage->GetSenderID().IsValid() && ChatItem->GetSenderID().IsValid() &&
				*LastMessage->GetSenderID() == *ChatItem->GetSenderID()  &&
				LastMessage->GetRecipientID().IsValid() && ChatItem->GetRecipientID().IsValid() &&
				*LastMessage->GetRecipientID() == *ChatItem->GetRecipientID() &&
				(ChatItem->GetMessageTime() - LastMessage->GetMessageTime()).GetDuration() <= FTimespan::FromMinutes(1.0))
			{
				RichTextMarshaller->AppendMessage(ChatItem, true);
			}
			else
			{
				RichTextMarshaller->AppendLineBreak();
				RichTextMarshaller->AppendMessage(ChatItem, false);
			}
			LastMessage = ChatItem;
		}

		if (RichText.IsValid())
		{
			if(!bUserHasScrolled || ExternalScrollbar->DistanceFromBottom() == 0)
			{
				ScrollToEnd();
			}
		}
	}

	void HandleNameClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		const FString* UsernameString = Metadata.Find(TEXT("Username"));
		const FString* UniqueIDString = Metadata.Find(TEXT("uid"));

		if (UsernameString && UniqueIDString)
		{
			FText Username = FText::FromString(*UsernameString);
			const TSharedRef<FFriendViewModel> FriendViewModel = ViewModel->GetFriendViewModel(*UniqueIDString, Username).ToSharedRef();

			bool DisplayChatOption = ViewModel->DisplayChatOption(FriendViewModel);

			TSharedRef<SBorder> ActionBorder = SNew(SBorder)
			.BorderImage(&FriendStyle.FriendsChatChromeStyle.ChatMenuBackgroundBrush)
			.BorderBackgroundColor(FLinearColor::Black)
			.Padding(0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FriendStyle.FriendsChatStyle.FriendActionHeaderPadding)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalBoldFont)
						.Text(FText::FromString(*UsernameString))
					]
					+ SHorizontalBox::Slot()
					.Padding(FriendStyle.FriendsChatStyle.FriendActionStatusMargin)
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SImage)
						.Image(this, &SChatWindowImpl::GetStatusBrush, FriendViewModel->GetOnlineStatus())
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
				]
				+SVerticalBox::Slot()
				[
					SNew(SFriendActions, FriendViewModel, FontService.ToSharedRef())
					.FriendStyle(&FriendStyle).
					FromChat(true).
					DisplayChatOption(DisplayChatOption)
				]
			];

			MenuAnchor->SetMenuContent(ActionBorder);
			MenuAnchor->SetIsOpen(true);
		}
	}

	const FSlateBrush* GetStatusBrush(EOnlinePresenceState::Type PresenceType) const
	{
		switch (PresenceType)
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

	EVisibility GetScrollbarVisibilty() const
	{
		return ViewModel->IsChatMinimized() ? EVisibility::Hidden : EVisibility::Visible;
	}

	EVisibility GetChatListVisibility() const
	{
		return ViewModel->IsChatMinimized() ? EVisibility::HitTestInvisible : EVisibility::Visible;
	}

	float GetChatWrapWidth() const
	{
		return FMath::Max(WindowWidth - 40, 0.0f);
	}
	
	FSlateColor GetOutgoingChannelTextColor() const 
	{
 		EChatMessageType::Type Channel = ViewModel->GetOutgoingChatChannel();
		switch (Channel)
		{
		case EChatMessageType::Global:
			return FriendStyle.FriendsChatStyle.GlobalChatColor;
		case EChatMessageType::Party:
			return FriendStyle.FriendsChatStyle.PartyChatColor;
		case EChatMessageType::Whisper:
			return FriendStyle.FriendsChatStyle.WhisperChatColor;
		default:
			return FriendStyle.FriendsChatStyle.GameChatColor;
		}
	}

	void ScrollToEnd()
	{
		RichText->GoTo(ETextLocation::EndOfDocument);
		RichText->GoTo(ETextLocation::BeginningOfLine);
		bUserHasScrolled = false;
	}

private:

	TSharedPtr<FChatTextLayoutMarshaller> RichTextMarshaller;

	TSharedPtr<SMultiLineEditableTextBox> RichText;

	int32 LastMessageIndex;

	TSharedPtr<FChatItemViewModel> LastMessage;

	// Holds the scroll bar
	TSharedPtr<SScrollBar> ExternalScrollbar;
	TSharedPtr<SScrollBar> HorizontalScrollbar;

	// Holds the Friends List view model
	TSharedPtr<FChatViewModel> ViewModel;

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	// Has the user scrolled
	bool bUserHasScrolled;

	// Holds the window width
	float WindowWidth;

	FSlateHyperlinkRun::FOnClick OnHyperlinkClicked;
	FWidgetDecorator::FCreateWidget OnTimeStampCreated;

	TSharedPtr<SMenuAnchor> MenuAnchor;
	TSharedPtr<FFriendsFontStyleService> FontService;
};

TSharedRef<SChatWindow> SChatWindow::New()
{
	return MakeShareable(new SChatWindowImpl());
}

#undef LOCTEXT_NAMESPACE
