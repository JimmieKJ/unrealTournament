// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_MajorRewardMessage.h"
#include "UTVictimMessage.h"

UUTHUDWidgetMessage_MajorRewardMessage::UUTHUDWidgetMessage_MajorRewardMessage(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ManagedMessageArea = FName(TEXT("MajorRewardMessage"));
	Position = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.5f, 0.45f);
	Size = FVector2D(0.0f, 0.0f);
	Origin = FVector2D(0.5f, 0.0f);
	FadeTime = 1.f;
	ScaleInDirection = -1.f;
}

void UUTHUDWidgetMessage_MajorRewardMessage::DrawMessages(float DeltaTime)
{
	Canvas->Reset();
	float Y = 0.f;
	int32 DrawCnt = 0;
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

bool UUTHUDWidgetMessage_MajorRewardMessage::ShouldDraw_Implementation(bool bShowScores)
{
	return !bShowScores;
}
