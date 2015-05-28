// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"

class FChatItemViewModelImpl
	: public FChatItemViewModel
{
public:

	virtual FText GetMessage() const override
	{
		return ComposedMessage;
	}

	virtual const EChatMessageType::Type GetMessageType() const override
	{
		return ChatMessages[0]->MessageType;
	}

	virtual FText GetMessageTimeText() const override
	{
		return MessageTimeAsText;
	}

	virtual FDateTime GetMessageTime() const override
	{
		return ChatMessages.Last()->MessageTime;
	}

	virtual void AddMessage(const TSharedRef<FFriendChatMessage>& ChatMessage) override
	{
		ChatMessages.Add(ChatMessage);

		//Display the time of the latest message only
		FTicker::GetCoreTicker().RemoveTicker(TickerHandle);

		FTextBuilder MessageBuilder;
		for (int32 Index = 0; Index < ChatMessages.Num(); Index++)
		{
			if (Index > 0)
			{
				static FString MessageBreak(TEXT("<MessageBreak></>"));
				MessageBuilder.AppendLine(MessageBreak);
			}

			MessageBuilder.AppendLine(ChatMessages[Index]->Message);
		}

		ComposedMessage = MessageBuilder.ToText();

		if (UpdateTimeStamp(60)) //this will broadcast a changed event
		{
			TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FChatItemViewModelImpl::UpdateTimeStamp), 60);
		}
	}

	virtual FText GetSenderName() const override
	{
		return ChatMessages[0]->FromName;
	}

	virtual FText GetRecipientName() const override
	{
		return ChatMessages[0]->ToName;
	}

	virtual const TSharedPtr<FUniqueNetId> GetSenderID() const override
	{
		return ChatMessages[0]->SenderId;
	}

	virtual const TSharedPtr<FUniqueNetId> GetRecipientID() const override
	{
		return ChatMessages[0]->RecipientId;
	}

	const bool IsFromSelf() const override
	{
		return ChatMessages[0]->bIsFromSelf;
	}

	DECLARE_DERIVED_EVENT(FChatItemViewModelImpl, FChatItemViewModel::FChangedEvent, FChangedEvent)
	virtual FChatItemViewModel::FChangedEvent& OnChanged() override { return ChangedEvent; }

	virtual ~FChatItemViewModelImpl()
	{
		FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}

private:

	FChatItemViewModelImpl()
	{}

	bool UpdateTimeStamp(float Delay)
	{
		bool ContinueTicking = true;

		TSharedRef<FFriendChatMessage> Message = ChatMessages.Last();
		FDateTime Now = FDateTime::UtcNow();

		if ((Now - Message->MessageTime).GetDuration() <= FTimespan::FromMinutes(1.0))
		{
			static FText NowTimeStamp = NSLOCTEXT("SChatWindow", "Now_TimeStamp", "Now");
			MessageTimeAsText = NowTimeStamp;
		}
		else if ((Now - Message->MessageTime).GetDuration() <= FTimespan::FromMinutes(2.0))
		{
			static FText OneMinuteTimeStamp = NSLOCTEXT("SChatWindow", "1_Minute_TimeStamp", "1 min");
			MessageTimeAsText = OneMinuteTimeStamp;
		}
		else if ((Now - Message->MessageTime).GetDuration() <= FTimespan::FromMinutes(3.0))
		{
			static FText TwoMinuteTimeStamp = NSLOCTEXT("SChatWindow", "2_Minute_TimeStamp", "2 min");
			MessageTimeAsText = TwoMinuteTimeStamp;
		}
		else if ((Now - Message->MessageTime).GetDuration() <= FTimespan::FromMinutes(4.0))
		{
			static FText ThreeMinuteTimeStamp = NSLOCTEXT("SChatWindow", "3_Minute_TimeStamp", "3 min");
			MessageTimeAsText = ThreeMinuteTimeStamp;
		}
		else if ((Now - Message->MessageTime).GetDuration() <= FTimespan::FromMinutes(5.0))
		{
			static FText FourMinuteTimeStamp = NSLOCTEXT("SChatWindow", "4_Minute_TimeStamp", "4 min");
			MessageTimeAsText = FourMinuteTimeStamp;
		}
		else if ((Now - Message->MessageTime).GetDuration() <= FTimespan::FromMinutes(6.0))
		{
			static FText FiveMinuteTimeStamp = NSLOCTEXT("SChatWindow", "5_Minute_TimeStamp", "5 min");
			MessageTimeAsText = FiveMinuteTimeStamp;
		}
		else
		{
			MessageTimeAsText = FText::AsTime(ChatMessages.Last()->MessageTime, EDateTimeStyle::Short);
			ContinueTicking = false;
		}

		OnChanged().Broadcast(SharedThis(this));
		return ContinueTicking;
	}

private:

	TArray<TSharedRef<FFriendChatMessage>> ChatMessages;

	FText ComposedMessage;
	FText MessageTimeAsText;
	FDelegateHandle TickerHandle;

	FChangedEvent ChangedEvent;

	friend FChatItemViewModelFactory;
};

TSharedRef< FChatItemViewModel > FChatItemViewModelFactory::Create(
	const TSharedRef<FFriendChatMessage>& ChatMessage)
{
	TSharedRef< FChatItemViewModelImpl > ViewModel = MakeShareable(new FChatItemViewModelImpl());

	ViewModel->AddMessage(ChatMessage);
	return ViewModel;
}
