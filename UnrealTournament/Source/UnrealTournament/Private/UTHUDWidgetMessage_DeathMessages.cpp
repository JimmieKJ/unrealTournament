// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_DeathMessages.h"
#include "UTVictimMessage.h"

UUTHUDWidgetMessage_DeathMessages::UUTHUDWidgetMessage_DeathMessages(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ManagedMessageArea = FName(TEXT("DeathMessage"));
	Position = FVector2D(0.0f, 0.0f);			
	ScreenPosition = FVector2D(0.5f, 0.37f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.5f, 0.0f);				
	FadeTime = 1.0;
	ScaleInDirection = -1.f;
}

void UUTHUDWidgetMessage_DeathMessages::DrawMessages(float DeltaTime)
{
	Canvas->Reset();
	float Y = 0.f;
	int32 DrawCnt=0;
	for (int32 MessageIndex = MessageQueue.Num() - 1; (MessageIndex >= 0) && (DrawCnt < NumVisibleLines); MessageIndex--)
	{
		if (MessageQueue[MessageIndex].MessageClass != NULL)	
		{
			Y -= MessageQueue[MessageIndex].DisplayFont->GetMaxCharHeight();
			DrawMessage(MessageIndex, 0.f, Y);
			DrawCnt++;
		}
	}
}
