// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "BaseTextLayoutMarshaller.h"
#include "ChatTextLayoutMarshaller.h"
#include "ChatItemViewModel.h"
#include "TimeStampRun.h"
#include "ChatLineTextRun.h"
#include "ChatHyperlinkDecorator.h"
#include "ChatDisplayService.h"

FChatItemTransparency::FChatItemTransparency(TSharedRef<SWidget> InOwningWidget, TSharedRef<class FChatDisplayService> InChatDisplayService)
	: OwningWidget(InOwningWidget)
	, ShouldFadeOut(false)
	, CachedAutoFaded(false)
	, ChatDisplayService(InChatDisplayService)
{
	CachedMinimize = ChatDisplayService->IsChatMinimized();
	FadeCurveSequence = FCurveSequence();
	FadeCurve = FadeCurveSequence.AddCurve(0,0.5f);
}

void FChatItemTransparency::FadeOut(bool AutoFade)
{
	if(CachedMinimize)
	{
		FadeCurveSequence.PlayReverse(OwningWidget);
	}
	if(!AutoFade)
	{
		ShouldFadeOut = true;
	}
}

float FChatItemTransparency::GetTransparency()
{
	bool IsMinimized = ChatDisplayService->IsChatMinimized();
	bool IsAutoFaded = ChatDisplayService->IsAutoFaded();

	if(IsAutoFaded != CachedAutoFaded)
	{
		if(IsAutoFaded && !ShouldFadeOut)
		{
			FadeOut(true);
		}
		else if(IsMinimized)
		{
			if(!ShouldFadeOut)
			{
				FadeIn();
			}
		}
		else
		{
			FadeIn();
		}
	}
	else
	{
		if(ShouldFadeOut && IsMinimized != CachedMinimize)
		{
			CachedMinimize = IsMinimized;
			IsMinimized ? FadeOut(false) : FadeIn();
		}
	}

	CachedAutoFaded = IsAutoFaded;
	CachedMinimize = IsMinimized;

	return FadeCurve.GetLerp();
}

/** Message layout marshaller for chat messages */
class FChatTextLayoutMarshallerImpl
	: public FChatTextLayoutMarshaller
	, public TSharedFromThis<FChatTextLayoutMarshallerImpl>
{
public:

	virtual ~FChatTextLayoutMarshallerImpl(){};
	
	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override
	{
		TextLayout = &TargetTextLayout;

		FadeItems.Empty();
		for(const auto& Message : Messages)
		{
			FormatMessage(Message, false);
		}
	}

	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override
	{
		SourceTextLayout.GetAsText(TargetString);
	}

	virtual void AddUserNameHyperlinkDecorator(const TSharedRef<FChatHyperlinkDecorator>& InNameHyperlinkDecorator) override
	{
		NameHyperlinkDecorator = InNameHyperlinkDecorator;
	}

	virtual bool AppendMessage(const TSharedPtr<FChatItemViewModel>& NewMessage, bool GroupText) override
	{
		Messages.Add(NewMessage);
		return FormatMessage(NewMessage, GroupText);
	}

	bool FormatMessage(const TSharedPtr<FChatItemViewModel>& NewMessage, bool GroupText)
	{
		FTextBlockStyle HighlightedTextStyle = DefaultTextStyle;
		HighlightedTextStyle.ColorAndOpacity = GetChatColor(NewMessage->GetMessageType());

		TSharedRef<FTextLayout> TextLayoutRef = TextLayout->AsShared();

		FTextRange ModelRange;
		TSharedRef<FString> ModelString = MakeShareable(new FString());
		TArray<TSharedRef<IRun>> Runs;

		TSharedRef<FChatItemTransparency> ChatFadeItem = MakeShareable( new FChatItemTransparency(OwningWidget.ToSharedRef(), ChatDisplayService));
		FadeItems.Add(ChatFadeItem);

		int32 FadeItemsNum = FadeItems.Num();
		if(FadeItemsNum > ChatDisplayService->GetMinimizedChatMessageNum())
		{
			FadeItems[FadeItemsNum - (ChatDisplayService->GetMinimizedChatMessageNum() + 1)]->FadeOut();
		}

		ChatFadeItem->FadeIn();

		if(!GroupText)
		{
			if(NewMessage->GetMessageType() == EChatMessageType::Admin)
			{
				ModelRange.BeginIndex = ModelString->Len();
				*ModelString += ": ";
				ModelRange.EndIndex = ModelString->Len();
				Runs.Add(FChatLineTextRun::Create(FRunInfo(), ModelString, HighlightedTextStyle, ModelRange, ChatFadeItem));
			}

			if(IsMultiChat && NewMessage->GetMessageType() == EChatMessageType::Whisper && NewMessage->IsFromSelf())
			{
				static const FText ToText = NSLOCTEXT("SChatWindow", "ChatTo", "to ");
				ModelRange.BeginIndex = ModelString->Len();
				*ModelString += ToText.ToString();
				ModelRange.EndIndex = ModelString->Len();
				Runs.Add(FChatLineTextRun::Create(FRunInfo(), ModelString, HighlightedTextStyle, ModelRange, ChatFadeItem));
			}

			if(NewMessage->GetMessageType() == EChatMessageType::Game || NewMessage->GetMessageType() == EChatMessageType::Admin)
			{
				// Don't add any channel information for In-Game
			}
			else if(NameHyperlinkDecorator.IsValid() && (!NewMessage->IsFromSelf() || (IsMultiChat && NewMessage->GetMessageType() == EChatMessageType::Whisper)))
			{
				FString MessageText = GetSenderName(NewMessage) + TEXT(": ");

				int32 NameLen = MessageText.Len();

				FString UIDMetaData = GetUserID(NewMessage);
				FString UsernameMetaData = GetSenderName(NewMessage);
				FString StyleMetaData = GetHyperlinkStyle(NewMessage);

				MessageText += UIDMetaData;
				MessageText += UsernameMetaData;
				MessageText += StyleMetaData;

				// Add name range
				FTextRunParseResults ParseInfo = FTextRunParseResults("a", FTextRange(0,MessageText.Len()), FTextRange(0,NameLen));

				int32 StartPos = NameLen;
				int32 EndPos = NameLen + UIDMetaData.Len();
				ParseInfo.MetaData.Add(TEXT("uid"), FTextRange(StartPos, EndPos));
				StartPos = EndPos;
				EndPos += UsernameMetaData.Len();
				ParseInfo.MetaData.Add(TEXT("UserName"), FTextRange(StartPos, EndPos));
				StartPos = EndPos;
				EndPos += StyleMetaData.Len();
				ParseInfo.MetaData.Add(TEXT("Style"), FTextRange(StartPos, EndPos));

				TSharedPtr< ISlateRun > Run = NameHyperlinkDecorator->Create(TextLayoutRef, ParseInfo, MessageText, ModelString, &FFriendsAndChatModuleStyle::Get(), ChatFadeItem);
				Runs.Add(Run.ToSharedRef());
			}
			else
			{
				ModelRange.BeginIndex = ModelString->Len();
				*ModelString += NewMessage->GetSenderName().ToString() + TEXT(": ");
				ModelRange.EndIndex = ModelString->Len();
				Runs.Add(FChatLineTextRun::Create(FRunInfo(), ModelString, HighlightedTextStyle, ModelRange, ChatFadeItem));
			}

			// Add time stamp
			if(false)
			{
				FTextBlockStyle TimeStampStyle = DecoratorStyleSet->FriendsChatStyle.TimeStampTextStyle;
				
				TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				int16 Baseline = FontMeasure->GetBaseline(TimeStampStyle.Font);
				FTimeStampRun::FTimeStampRunInfo WidgetRunInfo = FTimeStampRun::FTimeStampRunInfo(NewMessage->GetMessageTime(), Baseline, TimeStampStyle, TimestampMeasure);

				ModelRange.BeginIndex = ModelString->Len();
				TSharedPtr< ISlateRun > Run = FTimeStampRun::Create(TextLayoutRef, FRunInfo(), ModelString, WidgetRunInfo, ChatFadeItem);
				*ModelString += (" ");
				ModelRange.EndIndex = ModelString->Len();
				Runs.Add(Run.ToSharedRef());
			}
		}

		FString MessageString = NewMessage->GetMessage().ToString();
		TArray<FString> MessageLines;
		MessageString.ParseIntoArrayLines(MessageLines);

		if(MessageLines.Num())
		{
			ModelRange.BeginIndex = ModelString->Len();
			*ModelString += MessageLines[0];
			ModelRange.EndIndex = ModelString->Len();
			Runs.Add(FChatLineTextRun::Create(FRunInfo(), ModelString, HighlightedTextStyle, ModelRange, ChatFadeItem));
			TextLayout->AddLine(ModelString, Runs);
		}

		for (int32 Range = 1; Range < MessageLines.Num(); Range++)
		{
			TSharedRef<FString> LineString = MakeShareable(new FString(MessageLines[Range]));
			TArray< TSharedRef< IRun > > LineRun;
			FTextRange LineRange;
			LineRange.BeginIndex = 0;
			LineRange.EndIndex = LineString->Len();
			LineRun.Add(FChatLineTextRun::Create(FRunInfo(), LineString, HighlightedTextStyle, LineRange, ChatFadeItem));
			TextLayout->AddLine(LineString, LineRun);
		}

		return true;
	}

	virtual bool AppendLineBreak() override
	{
		return true;
	}

	virtual void SetOwningWidget(TSharedRef<SWidget> InOwningWidget) override
	{
		OwningWidget = InOwningWidget;
	}

protected:

	FChatTextLayoutMarshallerImpl(const FFriendsAndChatStyle* const InDecoratorStyleSet, const FTextBlockStyle& InTextStyle, bool InIsMultiChat, FVector2D InTimestampMeasure, const TSharedRef<FChatDisplayService>& InChatDisplayService)
		: DecoratorStyleSet(InDecoratorStyleSet)
		, DefaultTextStyle(InTextStyle)
		, TextLayout(nullptr)
		, IsMultiChat(InIsMultiChat)
		, TimestampMeasure(InTimestampMeasure)
		, ChatDisplayService(InChatDisplayService)
		, LastDisplayedMessageTime(FDateTime::Now())
		, VisibilityOverride(true)
	{
	}

	FString GetHyperlinkStyle(TSharedPtr<FChatItemViewModel> ChatItem)
	{
		FString HyperlinkStyle = TEXT("UserNameTextStyle.DefaultHyperlink");

		switch (ChatItem->GetMessageType())
		{
		case EChatMessageType::Global: 
			HyperlinkStyle = TEXT("UserNameTextStyle.GlobalHyperlink"); 
			break;
		case EChatMessageType::Whisper: 
			HyperlinkStyle = TEXT("UserNameTextStyle.Whisperlink"); 
			break;
		case EChatMessageType::Party:
			HyperlinkStyle = TEXT("UserNameTextStyle.PartyHyperlink");
			break;
		case EChatMessageType::Game: 
			HyperlinkStyle = TEXT("UserNameTextStyle.GameHyperlink"); 
			break;
		}
		return HyperlinkStyle;
	}

	const FLinearColor GetChatColor(EChatMessageType::Type MessageType)
	{
		FLinearColor TextColor;
		switch (MessageType)
		{
		case EChatMessageType::Global: 
			return DecoratorStyleSet->FriendsChatStyle.GlobalChatColor; 
		case EChatMessageType::Whisper: 
			return DecoratorStyleSet->FriendsChatStyle.WhisperChatColor; 
		case EChatMessageType::Party:
			return DecoratorStyleSet->FriendsChatStyle.PartyChatColor; 
		case EChatMessageType::Game: 
			return DecoratorStyleSet->FriendsChatStyle.GameChatColor;
		case EChatMessageType::Admin: 
			return DecoratorStyleSet->FriendsChatStyle.AdminChatColor; 
		}
		return DecoratorStyleSet->FriendsChatStyle.DefaultChatColor;
	}

	FString GetSenderName(TSharedPtr<FChatItemViewModel> ChatItem)
	{
		if(ChatItem->IsFromSelf())
		{
			return ChatItem->GetRecipientName().ToString();
		}
		else
		{
			return ChatItem->GetSenderName().ToString();
		}
	}

	FString GetUserID(TSharedPtr<FChatItemViewModel> ChatItem)
	{
		if(ChatItem->IsFromSelf())
		{
			if(ChatItem->GetRecipientID().IsValid())
			{
				return ChatItem->GetRecipientID()->ToString();
			}
		}
		else
		{
			if(ChatItem->GetSenderID().IsValid())
			{
				return ChatItem->GetSenderID()->ToString();
			}
		}
		return FString();
	}

private:

	/** The style set used for looking up styles used by decorators */
	const FFriendsAndChatStyle* DecoratorStyleSet;
	const FTextBlockStyle DefaultTextStyle;

	/** All messages to show in the text box */
	TArray< TSharedPtr<FChatItemViewModel> > Messages;

	/** Additional decorators can be appended inline. Inline decorators get precedence over decorators not specified inline. */
	TSharedPtr< FChatHyperlinkDecorator> NameHyperlinkDecorator;
	FTextLayout* TextLayout;
	bool IsMultiChat;
	FVector2D TimestampMeasure;
	TSharedRef<FChatDisplayService> ChatDisplayService;
	FDateTime LastDisplayedMessageTime;
	TArray<TSharedPtr<FChatItemTransparency> > FadeItems;
	TSharedPtr<SWidget> OwningWidget;
	bool VisibilityOverride;

	friend FChatTextLayoutMarshallerFactory;
};

TSharedRef< FChatTextLayoutMarshaller > FChatTextLayoutMarshallerFactory::Create(const FFriendsAndChatStyle* const InDecoratorStyleSet, const FTextBlockStyle& TextStyle, bool InIsMultiChat, FVector2D InTimestampMeasure, const TSharedRef<FChatDisplayService>& ChatDisplayService)
{
	TSharedRef< FChatTextLayoutMarshallerImpl > ChatMarshaller = MakeShareable(new FChatTextLayoutMarshallerImpl(InDecoratorStyleSet, TextStyle, InIsMultiChat, InTimestampMeasure, ChatDisplayService));
	return ChatMarshaller;
}
