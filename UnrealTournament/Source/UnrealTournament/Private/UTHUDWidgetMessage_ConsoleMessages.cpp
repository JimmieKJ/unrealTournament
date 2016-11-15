// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage.h"
#include "UTDeathMessage.h"
#include "UTHUDWidgetMessage_ConsoleMessages.h"

UUTHUDWidgetMessage_ConsoleMessages::UUTHUDWidgetMessage_ConsoleMessages(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ManagedMessageArea = FName(TEXT("ConsoleMessage"));
	Position = FVector2D(0.0f, 0.0f);			
	ScreenPosition = FVector2D(0.01f, 0.8f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.0f, 0.0f);				
	NumVisibleLines=4;
	LargeShadowDirection = FVector2D(1.f, 1.f);
	SmallShadowDirection = FVector2D(1.f, 1.f);
}

// @TODO FIXMESTEVE temp - need smaller font
float UUTHUDWidgetMessage_ConsoleMessages::GetDrawScaleOverride()
{
	return UTHUDOwner->GetHUDWidgetScaleOverride();
}

void UUTHUDWidgetMessage_ConsoleMessages::DrawMessages(float DeltaTime)
{
	Canvas->Reset();

	float Y = 0;
	int32 MessageIndex = NumVisibleLines < MessageQueue.Num() ? NumVisibleLines : MessageQueue.Num();
	while (MessageIndex >= 0)
	{
		if (MessageQueue[MessageIndex].MessageClass != NULL)	
		{
			DrawMessage(MessageIndex,0,Y);
			Canvas->TextSize(MessageQueue[MessageIndex].DisplayFont, MessageQueue[MessageIndex].Text.ToString(), MessageQueue[MessageIndex].TextWidth, MessageQueue[MessageIndex].TextHeight);
			Y -= MessageQueue[MessageIndex].TextHeight;
		}
		MessageIndex--;
	}
}

FVector2D UUTHUDWidgetMessage_ConsoleMessages::DrawMessage(int32 QueueIndex, float X, float Y)
{
	MessageQueue[QueueIndex].bHasBeenRendered = true;

	// If this is an UMG widget, then don't try to draw it.  
	if (MessageQueue[QueueIndex].UMGWidget.IsValid())
	{
		return FVector2D(X,Y);
	}

	FText DestinationTag = FText::GetEmpty();
	switch (MessageQueue[QueueIndex].MessageClass->GetDefaultObject<UUTLocalMessage>()->GetDestinationIndex(MessageQueue[QueueIndex].MessageIndex))
	{
	case 1: DestinationTag = FText::FromString(TEXT("[Global]")); break;
	case 2:	DestinationTag = FText::FromString(TEXT("[System]")); break;
	case 3: DestinationTag = FText::FromString(TEXT("[Lobby]")); break;
	case 4: DestinationTag = FText::FromString(TEXT("[Chat]")); break;
	case 5: DestinationTag = FText::FromString(TEXT("[Match]")); break;
	case 6: DestinationTag = FText::FromString(TEXT("[Team]")); break;
	case 7: DestinationTag = FText::FromString(TEXT("[MOTD]")); break;
	case 8: DestinationTag = FText::FromString(TEXT("[Whisper]")); break;
	case 9: DestinationTag = FText::FromString(TEXT("[Instance]")); break;
	}

	if (MessageQueue[QueueIndex].DisplayFont && !MessageQueue[QueueIndex].Text.IsEmpty())
	{
		if (bScaleByDesignedResolution)
		{
			X *= RenderScale;
			Y *= RenderScale;
		}
		FVector2D RenderPos = FVector2D(RenderPosition.X + X, RenderPosition.Y + Y);
		float CurrentTextScale = GetTextScale(QueueIndex);
		float TextScaling = bScaleByDesignedResolution ? RenderScale*CurrentTextScale : CurrentTextScale;

		FLinearColor DrawColor = FLinearColor::White;
		Canvas->DrawColor = DrawColor.ToFColor(false);

		if (!WordWrapper.IsValid())
		{
			WordWrapper = MakeShareable(new FCanvasWordWrapper());
		}
		FFontRenderInfo FontRenderInfo = FFontRenderInfo();

		if (!DestinationTag.IsEmpty())
		{
			FUTCanvasTextItem PrefixTextItem(RenderPos, DestinationTag, MessageQueue[QueueIndex].DisplayFont, DrawColor, WordWrapper);
			PrefixTextItem.FontRenderInfo = FontRenderInfo;
			PrefixTextItem.Scale = FVector2D(TextScaling, TextScaling);
			PrefixTextItem.EnableShadow(ShadowColor, MessageQueue[QueueIndex].ShadowDirection);
			Canvas->DrawItem(PrefixTextItem);
			float PreXL, PreYL;
			Canvas->StrLen(MessageQueue[QueueIndex].DisplayFont, DestinationTag.ToString(), PreXL, PreYL);
			RenderPos.X += PreXL * TextScaling;
		}
		if (MessageQueue[QueueIndex].RelatedPlayerState_1 && (MessageQueue[QueueIndex].MessageClass != UUTDeathMessage::StaticClass()))
		{
			FText PlayerName = FText::FromString(MessageQueue[QueueIndex].RelatedPlayerState_1->PlayerName);
			FUTCanvasTextItem TextItem(RenderPos, PlayerName, MessageQueue[QueueIndex].DisplayFont, MessageQueue[QueueIndex].DrawColor, WordWrapper);
			TextItem.FontRenderInfo = FontRenderInfo;
			TextItem.Scale = FVector2D(TextScaling, TextScaling);
			TextItem.EnableShadow(ShadowColor, MessageQueue[QueueIndex].ShadowDirection);
			Canvas->DrawItem(TextItem);
			float PreXL, PreYL;
			Canvas->StrLen(MessageQueue[QueueIndex].DisplayFont, MessageQueue[QueueIndex].RelatedPlayerState_1->PlayerName, PreXL, PreYL);
			RenderPos.X += PreXL * TextScaling;
		}
		FUTCanvasTextItem TextItem(RenderPos, MessageQueue[QueueIndex].Text, MessageQueue[QueueIndex].DisplayFont, DrawColor, WordWrapper);
		TextItem.FontRenderInfo = FontRenderInfo;
		TextItem.Scale = FVector2D(TextScaling, TextScaling);
		TextItem.EnableShadow(ShadowColor, MessageQueue[QueueIndex].ShadowDirection);
		Canvas->DrawItem(TextItem);
	}
	return FVector2D(0.f,0.f);
}

void UUTHUDWidgetMessage_ConsoleMessages::LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	Super::LayoutMessage(QueueIndex, MessageClass, MessageIndex, LocalMessageText, MessageCount, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);

	AUTPlayerState* InstigatorPS = Cast<AUTPlayerState>(RelatedPlayerState_1);
	if (InstigatorPS != NULL && InstigatorPS->Team != NULL)
	{
		MessageQueue[QueueIndex].DrawColor = FMath::LerpStable(InstigatorPS->Team->TeamColor, FLinearColor::White, 0.1f);
	}
}