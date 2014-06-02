// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_DeathMessages.h"
#include "UTVictimMessage.h"

UUTHUDWidgetMessage_DeathMessages::UUTHUDWidgetMessage_DeathMessages(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	ManagedMessageArea = FName(TEXT("DeathMessage"));
	Position = FVector2D(0.0f, 0.0f);			
	ScreenPosition = FVector2D(0.5f, 0.35f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.5f, 0.0f);				

	MessageColor = FLinearColor::Red;

	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("Font'/Game/RestrictedAssets/Proto/UI/Fonts/fntRobotoBlack36.fntRobotoBlack36'"));
	MessageFont = Font.Object;
}

void UUTHUDWidgetMessage_DeathMessages::DrawMessages(float DeltaTime)
{
	Canvas->Reset();
	int32 QueueSize = ARRAY_COUNT(MessageQueue);

	float Y = 0;
	int DrawCnt=0;
	for (int MessageIndex = QueueSize - 1; MessageIndex >= 0 && DrawCnt < 2; MessageIndex--)
	{
		if (MessageQueue[MessageIndex].MessageClass != NULL)	
		{
			DrawMessage(MessageIndex, 0, Y);
			Y += MessageQueue[MessageIndex].TextHeight;
			DrawCnt++;
		}
	}
}

void UUTHUDWidgetMessage_DeathMessages::DrawMessage(int32 QueueIndex, float X, float Y)
{
	MessageQueue[QueueIndex].bHasBeenRendered = true;

	// Fade the Message...

	float Alpha = 1.0;
	float FadeTime = MessageQueue[QueueIndex].LifeSpan > 2.0f ? MessageQueue[QueueIndex].LifeSpan - 2.0f : MessageQueue[QueueIndex].LifeSpan;
	if (MessageQueue[QueueIndex].LifeLeft < FadeTime)
	{
		Alpha = MessageQueue[QueueIndex].LifeLeft / FadeTime;
	}

	DrawText(MessageQueue[QueueIndex].Text, X, Y, MessageQueue[QueueIndex].DisplayFont, FLinearColor::Black, 1.0f, Alpha, MessageQueue[QueueIndex].DrawColor, ETextHorzPos::Center, ETextVertPos::Top);
}

void UUTHUDWidgetMessage_DeathMessages::LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	MessageQueue[QueueIndex].DisplayFont = MessageFont == NULL ? GEngine->GetSmallFont() : MessageFont;
	MessageQueue[QueueIndex].OptionalObject = OptionalObject;


	if (MessageClass == UUTVictimMessage::StaticClass() && MessageIndex < 1)
	{
		MessageQueue[QueueIndex].DrawColor = FLinearColor::White;
	}
	else
	{
		MessageQueue[QueueIndex].DrawColor = FLinearColor::Red;
	}
}
