// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage.h"
#include "UTEngineMessage.h"
#include "UTCharacterVoice.h"

UUTHUDWidgetMessage::UUTHUDWidgetMessage(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FadeTime = 0.0f;
	bShadowedText = true;
	LargeShadowDirection = FVector2D(1.5f, 3.f);
	SmallShadowDirection = FVector2D(1.f, 2.f);
	ShadowDirection = LargeShadowDirection;
	ScaleInDirection = 0.f;
	MessageFontIndex = 2;
	SmallMessageFontIndex = 1;
	NumVisibleLines = 3;
	EmphasisOutlineColor = FLinearColor::Black;
	EmphasisScaling = 1.5f;
}

void UUTHUDWidgetMessage::InitializeWidget(AUTHUD* Hud)
{
	MessageQueue.AddZeroed(MESSAGE_QUEUE_LENGTH);
	for (int32 i=0;i<MessageQueue.Num();i++)
	{
		MessageQueue[i].MessageClass = NULL;
		MessageQueue[i].Text = FText::GetEmpty();
		MessageQueue[i].EmphasisText = FText::GetEmpty();
		MessageQueue[i].OptionalObject = NULL;
		MessageQueue[i].DisplayFont = NULL;
		MessageQueue[i].bHasBeenRendered = false;
		MessageQueue[i].DrawColor = FLinearColor::White;
		MessageQueue[i].LifeLeft = 0;
		MessageQueue[i].LifeSpan = 0;
		MessageQueue[i].MessageCount = 0;
		MessageQueue[i].MessageIndex = 0;
	}

	MessageFont = Hud->GetFontFromSizeIndex(MessageFontIndex);
	SmallMessageFont = Hud->GetFontFromSizeIndex(SmallMessageFontIndex);
	MegaFont = Hud->GetFontFromSizeIndex(3);
}

bool UUTHUDWidgetMessage::ShouldDraw_Implementation(bool bShowScores)
{
	return !bShowScores;
}

float UUTHUDWidgetMessage::GetDrawScaleOverride()
{
	return (UTHUDOwner) ? UTHUDOwner->GetHUDMessageScaleOverride() : 1.f;
}

void UUTHUDWidgetMessage::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);
	AgeMessages_Implementation(DeltaTime);
}

void UUTHUDWidgetMessage::Draw_Implementation(float DeltaTime)
{
	DrawMessages(DeltaTime);
}

void UUTHUDWidgetMessage::ClearMessages()
{
	for (int32 QueueIndex = 0; QueueIndex < MessageQueue.Num(); QueueIndex++)
	{
		if (MessageQueue[QueueIndex].MessageClass != NULL)
		{
			ClearMessage(MessageQueue[QueueIndex]);
		}
	}
}

void UUTHUDWidgetMessage::AgeMessages_Implementation(float DeltaTime)
{
	// Pass 1 - Precache anything that's needed and age out messages.

	for (int32 QueueIndex = 0; QueueIndex < MessageQueue.Num(); QueueIndex++)
	{
		MessageQueue[QueueIndex].bHasBeenRendered = false;
		if (MessageQueue[QueueIndex].MessageClass != NULL)
		{
			if (MessageQueue[QueueIndex].DisplayFont == NULL)
			{
				for (int32 j=QueueIndex; j < MessageQueue.Num() - 1; j++)
				{
					MessageQueue[j] = MessageQueue[j+1];
				}
				ClearMessage(MessageQueue[QueueIndex]);
				QueueIndex--;
				continue;
			}
		}
		else if (MessageQueue[QueueIndex].MessageClass == NULL)
		{
			continue;
		}

		// Age out the message.
		MessageQueue[QueueIndex].LifeLeft -= DeltaTime;
		if (MessageQueue[QueueIndex].LifeLeft <= 0.0)
		{
			for (int32 j=QueueIndex; j < MessageQueue.Num() - 1; j++)
			{
				MessageQueue[j] = MessageQueue[j+1];
			}
			ClearMessage(MessageQueue[MessageQueue.Num() - 1]);
			QueueIndex--;
			continue;
		}
	}
}

void UUTHUDWidgetMessage::DrawMessages(float DeltaTime)
{
	// Pass 2 - Render the message
	Canvas->Reset();

	float Y = 0.f;
	int32 DrawCnt = 0;
	for (int32 QueueIndex = 0; (QueueIndex < MessageQueue.Num()) && (DrawCnt < NumVisibleLines); QueueIndex++)
	{
		// When we hit the empty section of the array, exit out
		if (MessageQueue[QueueIndex].MessageClass == NULL)
		{
			continue;
		}

		// If we were already rendered this frame, don't do it again
		if (MessageQueue[QueueIndex].bHasBeenRendered)
		{
			continue;
		}

		DrawMessage(QueueIndex, 0.0f, Y);
		Y += MessageQueue[QueueIndex].DisplayFont->GetMaxCharHeight() * GetTextScale(QueueIndex);
		DrawCnt++;
	}
}

FVector2D UUTHUDWidgetMessage::DrawMessage(int32 QueueIndex, float X, float Y)
{
	MessageQueue[QueueIndex].bHasBeenRendered = true;
	float CurrentTextScale = GetTextScale(QueueIndex);
	float Alpha = 1.f;

	// Fade the message in if scaling
	if ((MessageQueue[QueueIndex].ScaleInTime > 0.f) && (MessageQueue[QueueIndex].ScaleInSize != 1.f)
		&& (MessageQueue[QueueIndex].LifeLeft > MessageQueue[QueueIndex].LifeSpan - MessageQueue[QueueIndex].ScaleInTime))
	{
			Alpha = (MessageQueue[QueueIndex].LifeSpan - MessageQueue[QueueIndex].LifeLeft) / MessageQueue[QueueIndex].ScaleInTime;
			Y = Y + MessageQueue[QueueIndex].DisplayFont->GetMaxCharHeight() * (GetTextScale(QueueIndex) - 1.f) * ScaleInDirection;
	}
	else if (MessageQueue[QueueIndex].LifeLeft <= FadeTime)
	{
		Alpha = MessageQueue[QueueIndex].LifeLeft / FadeTime;
	}

	ShadowDirection = (MessageQueue[QueueIndex].DisplayFont == MessageFont) ? LargeShadowDirection : SmallShadowDirection;
	if (!MessageQueue[QueueIndex].EmphasisText.IsEmpty())
	{
		
		// draw emphasis text
		if (MessageQueue[QueueIndex].DisplayFont && !MessageQueue[QueueIndex].Text.IsEmpty())
		{
			// FIXME - precache textsize
			float XL = 0.0f;
			float YL = 0.0f;
			Canvas->StrLen(MessageQueue[QueueIndex].DisplayFont, MessageQueue[QueueIndex].Text.ToString(), XL, YL);
			FVector2D RenderPos = FVector2D(RenderPosition.X + X, RenderPosition.Y + Y);
			float TextScaling = bScaleByDesignedResolution ? RenderScale*CurrentTextScale : CurrentTextScale;

			if (bScaleByDesignedResolution)
			{
				X *= RenderScale;
				Y *= RenderScale;
			}

			// Handle Justification
			XL *= TextScaling; // cleanup FIXME
			YL *= TextScaling;
			RenderPos.X -= XL * 0.5f;
			RenderPos.Y -= YL * 0.5f;

			FLinearColor DrawColor = MessageQueue[QueueIndex].DrawColor;
			DrawColor.A = Opacity * Alpha * UTHUDOwner->WidgetOpacity;
			Canvas->DrawColor = DrawColor.ToFColor(false);
			FVector2D TextSize = FVector2D(XL, YL);

			if (!WordWrapper.IsValid())
			{
				WordWrapper = MakeShareable(new FCanvasWordWrapper());
			}
			FFontRenderInfo FontRenderInfo = FFontRenderInfo();
			FLinearColor FinalShadowColor = ShadowColor;
			FinalShadowColor.A *= Alpha * UTHUDOwner->WidgetOpacity;

			if (!MessageQueue[QueueIndex].PrefixText.IsEmpty())
			{
				FUTCanvasTextItem PrefixTextItem(RenderPos, MessageQueue[QueueIndex].PrefixText, MessageQueue[QueueIndex].DisplayFont, DrawColor, WordWrapper);
				PrefixTextItem.FontRenderInfo = FontRenderInfo;

				if (bShadowedText)
				{
					PrefixTextItem.EnableShadow(FinalShadowColor, ShadowDirection);
				}
				PrefixTextItem.Scale = FVector2D(TextScaling, TextScaling);
				Canvas->DrawItem(PrefixTextItem);
				float PreXL = 0.0f;
				float PreYL = 0.0f;
				Canvas->StrLen(MessageQueue[QueueIndex].DisplayFont, MessageQueue[QueueIndex].PrefixText.ToString(), PreXL, PreYL);
				RenderPos.X += PreXL * TextScaling;
			}

			FVector2D EmphasisRenderPos = RenderPos;
			EmphasisRenderPos.Y -= (EmphasisScaling - 1.f) * 0.5f * YL;
			FLinearColor EmphasisColor = MessageQueue[QueueIndex].EmphasisColor;
			EmphasisColor.A = Opacity * Alpha * UTHUDOwner->WidgetOpacity;
			FUTCanvasTextItem EmphasisTextItem(EmphasisRenderPos, MessageQueue[QueueIndex].EmphasisText, MessageQueue[QueueIndex].DisplayFont, EmphasisColor, WordWrapper);
			EmphasisTextItem.FontRenderInfo = FontRenderInfo;
			EmphasisTextItem.bOutlined = true;
			EmphasisTextItem.OutlineColor = EmphasisOutlineColor;
			EmphasisTextItem.OutlineColor.A *= Alpha * UTHUDOwner->WidgetOpacity;
			EmphasisTextItem.Scale = EmphasisScaling * FVector2D(TextScaling, TextScaling);
			if (bShadowedText)
			{
				EmphasisTextItem.EnableShadow(FinalShadowColor, ShadowDirection);
			}
			Canvas->DrawItem(EmphasisTextItem);
			float EmpXL = 0.0f;
			float EmpYL = 0.0f;
			Canvas->StrLen(MessageQueue[QueueIndex].DisplayFont, MessageQueue[QueueIndex].EmphasisText.ToString(), EmpXL, EmpYL);
			RenderPos.X += EmphasisScaling * EmpXL * TextScaling;
			TextSize.X += (EmphasisScaling - 1.f) * EmpXL * TextScaling;
			TextSize.Y += (EmphasisScaling - 1.f) * EmpYL * TextScaling;
			if (!MessageQueue[QueueIndex].PostfixText.IsEmpty())
			{
				FUTCanvasTextItem PostfixTextItem(RenderPos, MessageQueue[QueueIndex].PostfixText, MessageQueue[QueueIndex].DisplayFont, DrawColor, WordWrapper);
				PostfixTextItem.FontRenderInfo = FontRenderInfo;

				if (bShadowedText)
				{
					PostfixTextItem.EnableShadow(FinalShadowColor, ShadowDirection);
				}
				PostfixTextItem.Scale = FVector2D(TextScaling, TextScaling);
				Canvas->DrawItem(PostfixTextItem);
			}

			return TextSize;
		}
	}
	else
	{
		FText MessageText = MessageQueue[QueueIndex].Text;
		if (MessageQueue[QueueIndex].MessageCount > 1)
		{
			MessageText = FText::FromString(MessageText.ToString() + " (" + TTypeToString<int32>::ToString(MessageQueue[QueueIndex].MessageCount) + ")");
		}
		return DrawText(MessageText, X, Y, MessageQueue[QueueIndex].DisplayFont, bShadowedText, ShadowDirection, ShadowColor, bOutlinedText, OutlineColor, CurrentTextScale, Alpha, MessageQueue[QueueIndex].DrawColor, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), ETextHorzPos::Center, ETextVertPos::Top);
	}
	return FVector2D(0.f, 0.f);
}

void UUTHUDWidgetMessage::FadeMessage(FText FadeMessageText)
{
	for (int32 QueueIndex = 0; QueueIndex < MessageQueue.Num(); QueueIndex++)
	{
		if (MessageQueue[QueueIndex].Text.EqualTo(FadeMessageText))
		{
			MessageQueue[QueueIndex].LifeLeft = 0.f; 
			break;
		}
	}
}

void UUTHUDWidgetMessage::ClearMessage(FLocalizedMessageData& Message)
{
	Message.MessageClass = NULL;
	Message.DisplayFont = NULL;
	Message.bHasBeenRendered = false;
}

void UUTHUDWidgetMessage::ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject)
{
	const UUTLocalMessage* DefaultMessageObject = GetDefault<UUTLocalMessage>(MessageClass);
	if (MessageClass && (MessageClass->IsChildOf(UUTEngineMessage::StaticClass()) || MessageClass->IsChildOf(UUTCharacterVoice::StaticClass())))
	{
		LocalMessageText = DefaultMessageObject->ResolveMessage(MessageIndex, true, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}
	if (MessageClass == NULL || LocalMessageText.IsEmpty())
	{
		return;
	}
	int32 QueueIndex = MessageQueue.Num();
	int32 MessageCount = 0;
	if (DefaultMessageObject->bIsUnique)
	{
		for (QueueIndex = 0; QueueIndex < MessageQueue.Num(); QueueIndex++)
		{
			if (MessageQueue[QueueIndex].MessageClass == MessageClass)
			{
				if (DefaultMessageObject->ShouldCountInstances(MessageIndex) && MessageQueue[QueueIndex].Text.EqualTo(LocalMessageText))
				{
					MessageCount = (MessageQueue[QueueIndex].MessageCount == 0) ? 2 : MessageQueue[QueueIndex].MessageCount + 1;
				}
				break;
			}
		}
	}

	if (GetDefault<UUTLocalMessage>(MessageClass)->bIsPartiallyUnique)
	{
		for (QueueIndex = 0; QueueIndex < MessageQueue.Num(); QueueIndex++)
		{
			if (MessageQueue[QueueIndex].MessageClass == MessageClass && 
				DefaultMessageObject->PartiallyDuplicates(MessageIndex, MessageQueue[QueueIndex].MessageIndex, OptionalObject, MessageQueue[QueueIndex].OptionalObject))
			{
				break;
			}
		}
	}

	if (QueueIndex == MessageQueue.Num())
	{
		for (QueueIndex = 0; QueueIndex < MessageQueue.Num(); QueueIndex++)
		{
			if (MessageQueue[QueueIndex].MessageClass == NULL)
			{
				break;
			}
		}
	}

	if (QueueIndex == MessageQueue.Num())
	{
		for (QueueIndex = 0; QueueIndex < MessageQueue.Num() - 1; QueueIndex++)
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
	GetDefault<UUTLocalMessage>(MessageClass)->GetEmphasisText(MessageQueue[QueueIndex].PrefixText, MessageQueue[QueueIndex].EmphasisText, MessageQueue[QueueIndex].PostfixText, MessageQueue[QueueIndex].EmphasisColor, MessageIndex, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	MessageQueue[QueueIndex].LifeSpan = GetDefault<UUTLocalMessage>(MessageClass)->GetLifeTime(MessageIndex);
	MessageQueue[QueueIndex].LifeLeft = MessageQueue[QueueIndex].LifeSpan;
	MessageQueue[QueueIndex].ScaleInTime = GetDefault<UUTLocalMessage>(MessageClass)->GetScaleInTime(MessageIndex);
	MessageQueue[QueueIndex].ScaleInSize = GetDefault<UUTLocalMessage>(MessageClass)->GetScaleInSize(MessageIndex);

	// Layout the widget
	LayoutMessage(QueueIndex, MessageClass, MessageIndex, LocalMessageText, MessageCount, RelatedPlayerState_1,RelatedPlayerState_2,OptionalObject);

	MessageQueue[QueueIndex].MessageCount = MessageCount;
	MessageQueue[QueueIndex].bHasBeenRendered = false;
	MessageQueue[QueueIndex].TextWidth = 0;
	MessageQueue[QueueIndex].TextHeight = 0;

	MessageQueue[QueueIndex].RelatedPlayerState_1 = RelatedPlayerState_1;
	MessageQueue[QueueIndex].RelatedPlayerState_2 = RelatedPlayerState_2;
}

void UUTHUDWidgetMessage::LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	MessageQueue[QueueIndex].DrawColor = GetDefault<UUTLocalMessage>(MessageClass)->GetMessageColor(MessageIndex);
	if (GetDefault<UUTLocalMessage>(MessageClass)->UseMegaFont(MessageIndex))
	{
		MessageQueue[QueueIndex].DisplayFont = MegaFont;
	}
	else
	{
		MessageQueue[QueueIndex].DisplayFont = GetDefault<UUTLocalMessage>(MessageClass)->UseLargeFont(MessageIndex) ? MessageFont : SmallMessageFont;
	}
	MessageQueue[QueueIndex].OptionalObject = OptionalObject;
}

float UUTHUDWidgetMessage::GetTextScale(int32 QueueIndex)
{
	if ((MessageQueue[QueueIndex].ScaleInTime > 0.f) && (MessageQueue[QueueIndex].ScaleInSize != 1.f)
		&& (MessageQueue[QueueIndex].LifeLeft > MessageQueue[QueueIndex].LifeSpan - MessageQueue[QueueIndex].ScaleInTime))
	{
		float Pct = (MessageQueue[QueueIndex].LifeSpan - MessageQueue[QueueIndex].LifeLeft) / MessageQueue[QueueIndex].ScaleInTime;
		return Pct + MessageQueue[QueueIndex].ScaleInSize * (1.f - Pct);
	}
	return 1.0f;
}

void UUTHUDWidgetMessage::DumpMessages()
{
	for (int32 i=0;i<MessageQueue.Num();i++)
	{
		if (MessageQueue[i].MessageClass == NULL)
		{
			UE_LOG(UT,Log,TEXT("Message %i is NULL"),i);
		}
		else
		{
			UE_LOG(UT,Log,TEXT("Message %i = %s %s %i %f %f"), i, *GetNameSafe(MessageQueue[i].MessageClass), *MessageQueue[i].Text.ToString(), MessageQueue[i].MessageIndex, MessageQueue[i].LifeSpan, MessageQueue[i].LifeLeft);
		}
	}
}