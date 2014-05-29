// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTHUDWidgetMessage.h"


UUTHUDWidgetMessage::UUTHUDWidgetMessage(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	MessageColor = FLinearColor::White;
}

void UUTHUDWidgetMessage::Draw(float DeltaTime)
{
	if (MessageQueue[0].MessageClass == NULL) return;	// Quick out if nothing to render.

	Canvas->Reset();
	int32 QueueSize = ARRAY_COUNT(MessageQueue);

	// Pass 1 - Precache anything that's needed and age out messages.

	for (int QueueIndex = 0; QueueIndex < QueueSize; QueueIndex++)
	{
		MessageQueue[QueueIndex].bHasBeenRendered = false;
		if (MessageQueue[QueueIndex].MessageClass != NULL)
		{
			if (MessageQueue[QueueIndex].DisplayFont == NULL)
			{
				for (int j=QueueIndex; j < QueueSize - 1; j++)
				{
					MessageQueue[j] = MessageQueue[j+1];
				}
				ClearMessage(MessageQueue[QueueIndex]);
				QueueIndex--;
				continue;
			}
			else
			{
				Canvas->TextSize(MessageQueue[QueueIndex].DisplayFont, MessageQueue[QueueIndex].Text.ToString(), MessageQueue[QueueIndex].TextWidth, MessageQueue[QueueIndex].TextHeight);
			}
		}
		else if (MessageQueue[QueueIndex].MessageClass == NULL)
		{
			continue;
		}

		// Age out the message.
		if (MessageQueue[QueueIndex].EndOfLife < UTHUDOwner->GetWorld()->GetTimeSeconds())
		{
			for (int j=QueueIndex; j < QueueSize - 1; j++)
			{
				MessageQueue[j] = MessageQueue[j+1];
			}
			ClearMessage(MessageQueue[QueueIndex]);
			QueueIndex--;
			continue;
		}

	}

	// Pass 2 - Render the message

	for (int QueueIndex = 0; QueueIndex < QueueSize; QueueIndex++)
	{
		// When we hit the empty section of the array, exit out
		if (MessageQueue[QueueIndex].MessageClass == NULL)
		{
			break;
		}

		// If we were already rendered this frame, don't do it again
		if (MessageQueue[QueueIndex].bHasBeenRendered)
		{
			continue;
		}

		DrawMessage(QueueIndex);
	}
}

void UUTHUDWidgetMessage::DrawMessage(int32 QueueIndex)
{
	MessageQueue[QueueIndex].bHasBeenRendered = true;
	DrawText(MessageQueue[QueueIndex].Text, 0.0f, 0.0f, MessageQueue[QueueIndex].DisplayFont, 1.0, MessageQueue[QueueIndex].DrawColor, ETextHorzPos::Center, ETextVertPos::Top);
}

void UUTHUDWidgetMessage::ClearMessage(FLocalizedMessageData& Message)
{
	Message.MessageClass = NULL;
	Message.DisplayFont = NULL;
	Message.bHasBeenRendered = false;
}

void UUTHUDWidgetMessage::ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject)
{
	UE_LOG(UT,Log,TEXT("RecLocMsg: %s %i"), *GetNameSafe(MessageClass), MessageIndex);

	if (MessageClass == NULL || LocalMessageText.IsEmpty()) return;

	
	int32 QueueSize = ARRAY_COUNT(MessageQueue);
	int32 QueueIndex = QueueSize;
	int32 MessageCount = 0;
	if (GetDefault<UUTLocalMessage>(MessageClass)->bIsUnique)
	{
		for (QueueIndex = 0; QueueIndex < MESSAGE_QUEUE_LENGTH; QueueIndex++)
		{
			if (MessageQueue[QueueIndex].MessageClass == MessageClass)
			{
				if ( GetDefault<UUTLocalMessage>(MessageClass)->bCountInstances && MessageQueue[QueueIndex].Text.EqualTo(LocalMessageText) )
				{
					MessageCount = (MessageQueue[QueueIndex].MessageCount == 0) ? 2 : MessageQueue[QueueIndex].MessageCount + 1;
				}

				break;
			}
		}
	}

	if (GetDefault<UUTLocalMessage>(MessageClass)->bIsPartiallyUnique)
	{
		for (QueueIndex = 0; QueueIndex < MESSAGE_QUEUE_LENGTH; QueueIndex++)
		{
			if (MessageQueue[QueueIndex].MessageClass == MessageClass && 
					MessageClass->GetDefaultObject<UUTLocalMessage>()->PartiallyDuplicates(MessageIndex, MessageQueue[QueueIndex].MessageIndex, OptionalObject, MessageQueue[QueueIndex].OptionalObject) )
			{
				break;
			}
		}
	}

	if (QueueIndex == QueueSize)
	{
		for (QueueIndex = 0; QueueIndex < QueueSize; QueueIndex++)
		{
			if (MessageQueue[QueueIndex].MessageClass == NULL)
			{
				break;
			}
		}
	}

	if (QueueIndex == QueueSize)
	{
		for (QueueIndex = 0; QueueIndex < QueueSize-1; QueueIndex++)
		{
			MessageQueue[QueueIndex] = MessageQueue[QueueIndex+1];
		}
	}
	
	AddMessage(QueueIndex, MessageClass, MessageIndex, LocalMessageText, MessageCount, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}


void UUTHUDWidgetMessage::AddMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{

	MessageQueue[QueueIndex].MessageClass = MessageClass;
	MessageQueue[QueueIndex].MessageIndex = MessageIndex;
	MessageQueue[QueueIndex].Text = LocalMessageText;

	MessageQueue[QueueIndex].EndOfLife = UTHUDOwner->GetWorld()->GetTimeSeconds() + GetDefault<UUTLocalMessage>(MessageClass)->GetLifeTime(MessageIndex);
	MessageQueue[QueueIndex].DrawColor = MessageColor;
	MessageQueue[QueueIndex].DisplayFont = MessageFont == NULL ? GEngine->GetSmallFont() : MessageFont;
	MessageQueue[QueueIndex].OptionalObject = OptionalObject;

	MessageQueue[QueueIndex].MessageCount = MessageCount;

	MessageQueue[QueueIndex].bHasBeenRendered = false;
	MessageQueue[QueueIndex].TextWidth = 0;
	MessageQueue[QueueIndex].TextHeight = 0;
}
