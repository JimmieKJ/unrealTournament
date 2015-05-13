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
	ScreenPosition = FVector2D(0.5f, 0.25f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.5f, 0.0f);				
	FadeTime = 1.0;
	ScaleInDirection = -1.f;
}

void UUTHUDWidgetMessage_DeathMessages::DrawMessages(float DeltaTime)
{
	Canvas->Reset();
	float Y = 0;
	int32 DrawCnt=0;
	for (int32 MessageIndex = MessageQueue.Num() - 1; MessageIndex >= 0 && DrawCnt < 2; MessageIndex--)
	{
		if (MessageQueue[MessageIndex].MessageClass != NULL)	
		{
			DrawMessage(MessageIndex, 0, Y);
			Y += MessageQueue[MessageIndex].TextHeight;
			DrawCnt++;
		}
	}
}
