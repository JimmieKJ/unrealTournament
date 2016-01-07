// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsFontStyleService.h"
#include "SChatEntryWidget.h"
#include "RichTextLayoutMarshaller.h"
#include "ChatViewModel.h"
#include "ChatEntryViewModel.h"
#include "ChatEntryCommands.h"
#include "SFriendActions.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE ""

class SChatEntryWidgetImpl : public SChatEntryWidget
{
public:

	/**
	 * Construct the chat entry widget.
	 * @param InArgs		Widget args
	 * @param InViewModel	The chat view model - used for accessing chat markup etc.
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<FChatEntryViewModel>& InViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService)
	{
		FontService = InFontService;
		ViewModel = InViewModel;
		ViewModel->OnTextValidated().AddSP(this, &SChatEntryWidgetImpl::HandleValidatedText);
		ViewModel->OnChatListSetFocus().AddSP(this, &SChatEntryWidgetImpl::SetFocus);
		FChatEntryViewModel* ViewModelPtr = ViewModel.Get();
		FriendStyle = *InArgs._FriendStyle;
		MaxChatLength = InArgs._MaxChatLength;
		HintText = InArgs._HintText;

		FFriendsAndChatModuleStyle::Initialize(FriendStyle);

		Marshaller = FRichTextLayoutMarshaller::Create(
			TArray<TSharedRef<ITextDecorator>>(), 
			&FFriendsAndChatModuleStyle::Get()
			);

		OnHyperlinkClicked = FSlateHyperlinkRun::FOnClick::CreateSP(SharedThis(this), &SChatEntryWidgetImpl::HandleNameClicked);

		Marshaller->AppendInlineDecorator(FHyperlinkDecorator::Create(TEXT("UserName"), OnHyperlinkClicked));

		static const FText EntryHint = LOCTEXT("ChatEntryHint", "Enter to chat or type \"/\" for more options");
		SetHintText = HintText.IsSet() ? HintText.Get() : EntryHint;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(FriendStyle.FriendsChatStyle.ChatEntryHeight)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ChatTextBox, SMultiLineEditableTextBox)
					.ForegroundColor(FriendStyle.FriendsChatStyle.DefaultChatColor)
					.Style(&FriendStyle.FriendsChatStyle.ChatEntryTextStyle)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextCommitted(this, &SChatEntryWidgetImpl::HandleChatEntered)
					.OnKeyDownHandler(this, &SChatEntryWidgetImpl::HandleChatKeydown)
					.HintText(this, &SChatEntryWidgetImpl::GetChatHintTest)
					.OnTextChanged(this, &SChatEntryWidgetImpl::HandleChatTextChanged)
					.ModiferKeyForNewLine(EModifierKey::Shift)
					.Marshaller(Marshaller)
					.WrapTextAt(this, &SChatEntryWidgetImpl::GetChatWrapWidth)
				]
			]
		]);
	}

	// Begin SUserWidget

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/**
	 * Set the focus to the chat entry bar when this widget is given focus.
	 */
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override
	{
		if(ViewModel->ShouldCaptureFocus())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
	
		return FReply::Handled();
	}

	// End SUserWidget

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SUserWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		int32 NewWindowWidth = FMath::FloorToInt(AllottedGeometry.GetLocalSize().X);
		if(NewWindowWidth != WindowWidth)
		{
			WindowWidth = NewWindowWidth;
		}
	}

	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		return FReply::Handled();
	}

private:

	/**
	 * Handle chat commited - send the message to be broadcast from the message service
	 * @param CommentText	The committed text
	 * @param CommitInfo	The commit type.
	 */
	void HandleChatEntered(const FText& CommittedText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter)
		{
			SendChatMessage();
		}
	}

	/**
	 * Handle key event from the rich text box
	 * @param Geometry	The geometry
	 * @param KeyEvent	The key event.
	 */
	FReply HandleChatKeydown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
	{
		return ViewModel->HandleChatKeyEntry(KeyEvent);
	}

	FText GetChatHintTest() const
	{
		return ViewModel->IsChatConnected() ? SetHintText : ViewModel->GetChatDisconnectText();
	}

	/**
	 * Handle chat entered - Check for markup after each character is added
	 * @param CurrentText	The current text in the chat box
	 */
	void HandleChatTextChanged(const FText& CurrentText)
	{
		FText PlainText = ChatTextBox->GetPlainText();
		FString PlainTextString = PlainText.ToString();

		if(ViewModel->IsChatConnected() || PlainTextString.StartsWith(TEXT("/")))
		{
			if (PlainTextString.Len() > MaxChatLength)
			{
				int32 CharactersOver = PlainTextString.Len() - MaxChatLength;
				FString CurrentTextString = CurrentText.ToString();
				SetText(FText::FromString(CurrentTextString.LeftChop(CharactersOver-1)));
				ChatTextBox->SetError(LOCTEXT("OverCharacterLimitMsg", "Message Over Character Limit"));
			}
			else
			{
				ChatTextBox->SetError(FText());
				ViewModel->ValidateChatInput(CurrentText, PlainText);
			}
		}
		else
		{
			SetText(FText::GetEmpty());
		}
	}

	/**
	 * Handle event for text being validated my the markup system.
	 * Set focus to the entry box, and set the validated text
	 */
	void HandleValidatedText()
	{
		SetFocus();
		FText InputText = ViewModel->GetValidatedInput();
		SetText(InputText);
		ChatTextBox->GoTo(ETextLocation::EndOfDocument);
	}

	/**
	 * Send the entered message out through the message system.
	 */
	void SendChatMessage()
	{
		const FText& CurrentText = ChatTextBox->GetPlainText();
		if (CheckLimit(CurrentText))
		{
			ViewModel->SendMessage(ChatTextBox->GetText(), CurrentText);
			SetText(FText::GetEmpty());
		}
	}

	/**
	 * Set focus to the chat entry box.
	 */
	void SetFocus()
	{
		if (ChatTextBox.IsValid() && !ChatTextBox->HasKeyboardFocus())
		{
			FWidgetPath WidgetToFocusPath;

			bool bFoundPath = FSlateApplication::Get().FindPathToWidget(ChatTextBox.ToSharedRef(), WidgetToFocusPath);
			if (bFoundPath && WidgetToFocusPath.IsValid())
			{
				FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EKeyboardFocusCause::SetDirectly);
			}
		}
	}

	/**
	 * Check the length of chat entry, and warn if too long
	 * @param CurrentText	The current text in the chat box
	 */
	bool CheckLimit(const FText& CurrentText)
	{
		int32 OverLimit = CurrentText.ToString().Len() - MaxChatLength;

		if (OverLimit == 1)
		{
			ChatTextBox->SetError(LOCTEXT("OneCharacterOverLimitFmt", "1 Character Over Limit"));
			return false;
		}
		else if (OverLimit > 1)
		{
			ChatTextBox->SetError(FText::Format(LOCTEXT("TooManyCharactersFmt", "{0} Characters Over Limit"), FText::AsNumber(OverLimit)));
			return false;
		}

		// no error
		ChatTextBox->SetError(FText());
		return true;
	}

	void HandleNameClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		const FString* UsernameString = Metadata.Find(TEXT("Username"));
		const FString* UniqueIDString = Metadata.Find(TEXT("uid"));

		if (UsernameString && UniqueIDString)
		{
			FText Username = FText::FromString(*UsernameString);
			const TSharedRef<FFriendViewModel> FriendViewModel = ViewModel->GetFriendViewModel(*UniqueIDString, Username).ToSharedRef();
			TSharedRef<SWidget> Widget = SNew(SFriendActions, FriendViewModel, FontService.ToSharedRef()).FriendStyle(&FriendStyle);

			FSlateApplication::Get().PushMenu(
				SharedThis(this),
				FWidgetPath(),
				Widget,
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
				);
		}
	}

	EVisibility GetLostConnectionVisibility() const
	{
		return ViewModel->IsChatConnected() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	float GetChatWrapWidth() const
	{
		return FMath::Max(WindowWidth - 40.f, 0.0f);
	}

	void SetText(const FText& InText)
	{
		ChatTextBox->SetText(InText);
	}

private:
	// Chat hint text
	FText SetHintText;
	/* Holds the max chat length */
	int32 MaxChatLength;
	/* Holds the current window width (Hack until Rich Text can figure this out) */
	mutable float WindowWidth;
	/* Holds the Chat entry box */
	TSharedPtr<SMultiLineEditableTextBox> ChatTextBox;
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	/* Holds the Chat view model */
	TSharedPtr<FChatEntryViewModel> ViewModel;
	/* Holds the hint text for the chat box*/
	TAttribute< FText > HintText;
	/* Holds the marshaller for the chat box*/
	TSharedPtr<FRichTextLayoutMarshaller> Marshaller;
	FSlateHyperlinkRun::FOnClick OnHyperlinkClicked;

	TSharedPtr< FFriendsFontStyleService> FontService;
};

TSharedRef<SChatEntryWidget> SChatEntryWidget::New()
{
	return MakeShareable(new SChatEntryWidgetImpl());
}

#undef LOCTEXT_NAMESPACE
