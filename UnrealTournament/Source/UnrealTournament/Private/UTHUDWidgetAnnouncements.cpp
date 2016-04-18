// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetAnnouncements.h"

UUTHUDWidgetAnnouncements::UUTHUDWidgetAnnouncements(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	EmphasisOutlineColor = FLinearColor::Black;
	EmphasisScaling = 1.25f;
	ManagedMessageArea = FName(TEXT("Announcements"));
	Slots.Add(FAnnouncementSlot(FName(TEXT("DeathMessage")), 0.36f));
	Slots.Add(FAnnouncementSlot(FName(TEXT("VictimMessage")), 0.41f));
	Slots.Add(FAnnouncementSlot(FName(TEXT("MultiKill")), 0.25f));
	Slots.Add(FAnnouncementSlot(FName(TEXT("Spree")), 0.31f));
	Slots.Add(FAnnouncementSlot(FName(TEXT("GameMessages")), 0.7f));
	Slots.Add(FAnnouncementSlot(FName(TEXT("CountDownMessages")), 0.77f));
	Slots.Add(FAnnouncementSlot(FName(TEXT("MajorRewardMessage")), 0.46f));
	Slots.Add(FAnnouncementSlot(FName(TEXT("PickupMessage")), 0.51f));
	Position = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.5f, 0.f);
	Size = FVector2D(0.0f, 0.0f);
	Origin = FVector2D(0.0f, 0.0f);
	FadeTime = 0.5f;
	PaddingBetweenTextAndDamageIcon = 10.f;
}

// draw in slots
void UUTHUDWidgetAnnouncements::DrawMessages(float DeltaTime)
{
	// Pass 2 - Render the message
	Canvas->Reset();

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); SlotIndex++)
	{
		Slots[SlotIndex].bHasBeenRendered = false;
	}
	bool bIsAtIntermission = UTGameState && UTGameState->IsMatchIntermission();
	for (int32 QueueIndex = 0; QueueIndex < MessageQueue.Num(); QueueIndex++)
	{
		// When we hit the empty section of the array, exit out
		if ((MessageQueue[QueueIndex].MessageClass == NULL) || MessageQueue[QueueIndex].bHasBeenRendered || (bIsAtIntermission && !GetDefault<UUTLocalMessage>(MessageQueue[QueueIndex].MessageClass)->bDrawAtIntermission))
		{
			continue;
		}

		int32 RequestedSlot = MessageQueue[QueueIndex].RequestedSlot;
		if ((RequestedSlot >= 0) && (RequestedSlot < Slots.Num()) && !Slots[RequestedSlot].bHasBeenRendered)
		{
			DrawMessage(QueueIndex, 0.0f, Slots[RequestedSlot].SlotYPosition * Canvas->ClipY);
			Slots[RequestedSlot].bHasBeenRendered = true;
		}
/*		else
		{
			if ((RequestedSlot >= 0) && (RequestedSlot < Slots.Num()))
			{
				UE_LOG(UT, Warning, TEXT("COULD NOT RENDER %s"), *MessageQueue[QueueIndex].Text.ToString());
			}
		}*/
	}
}

void UUTHUDWidgetAnnouncements::AddMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	Super::AddMessage(QueueIndex, MessageClass, MessageIndex, LocalMessageText, MessageCount, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);

	// find which slot this message wants
	MessageQueue[QueueIndex].RequestedSlot = -1;
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); SlotIndex++)
	{
		if (Slots[SlotIndex].MessageArea == GetDefault<UUTLocalMessage>(MessageClass)->MessageSlot)
		{
			MessageQueue[QueueIndex].RequestedSlot = SlotIndex;
			break;
		}
	}

	// FIXMESTEVE TEMP
	if (MessageQueue[QueueIndex].RequestedSlot == -1)
	{
		UE_LOG(UT, Warning, TEXT("No slot found for %s"), *MessageClass->GetName());
	}
}

FVector2D UUTHUDWidgetAnnouncements::DrawMessage(int32 QueueIndex, float X, float Y)
{
	if (MessageQueue[QueueIndex].MessageClass && MessageQueue[QueueIndex].MessageClass->GetDefaultObject<UUTLocalMessage>()->bDrawAsDeathMessage)
	{
		return DrawDeathMessage(QueueIndex, X, Y);
	}
		
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
			EmphasisRenderPos.Y -= (EmphasisScaling - 1.f) * YL * 0.7f; // FIXMESTEVE 0.7f is guess - what is ideal? maybe less emphasis scaling?
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
			TextSize.X += 2.f * (EmphasisScaling - 1.f) * EmpXL * TextScaling;
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

FVector2D UUTHUDWidgetAnnouncements::DrawDeathMessage(int32 QueueIndex, float X, float Y)
{
	FVector2D TextSize = FVector2D(0.f, 0.f);
	if (UTHUDOwner->GetDrawCenteredKillMsg())
	{
		TextSize = Super::DrawMessage(QueueIndex, X, Y);

		//Figure out the DamageType that we killed with
		UClass* DamageTypeClass = Cast<UClass>(MessageQueue[QueueIndex].OptionalObject);
		const UUTDamageType* DmgType = DamageTypeClass ? Cast<UUTDamageType>(DamageTypeClass->GetDefaultObject()) : nullptr;
		if (DmgType == nullptr)
		{
			//Make sure non UUTDamageType damages still get the default icon
			DmgType = Cast<UUTDamageType>(UUTDamageType::StaticClass()->GetDefaultObject());
		}

		//Gather all the info needed to display the message
		APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(GetWorld());
		AUTPlayerState* LocalPS = (LocalPC != nullptr) ? Cast<AUTPlayerState>(LocalPC->PlayerState) : nullptr;
		AUTPlayerState* VictimPS = Cast<AUTPlayerState>(MessageQueue[QueueIndex].RelatedPlayerState_2);

		if ((DmgType != nullptr) && (DmgType->HUDIcon.Texture != nullptr) && (VictimPS == LocalPS))
		{
			float XL = FMath::Abs(DmgType->HUDIcon.UL) * GetTextScale(QueueIndex);
			float YL = FMath::Abs(DmgType->HUDIcon.VL) * GetTextScale(QueueIndex);

			//Move X so we are rendering after text
			if (UTHUDOwner->GetDrawCenteredKillMsg())
			{
				X += (TextSize.X / 2) + PaddingBetweenTextAndDamageIcon;
			}

			////Center message on Y
			Y -= (YL / 2);

			DrawTexture(DmgType->HUDIcon.Texture, X, Y, XL, YL, DmgType->HUDIcon.U, DmgType->HUDIcon.V, DmgType->HUDIcon.UL, DmgType->HUDIcon.VL, UTHUDOwner->GetHUDWidgetOpacity());
		}
	}
	return TextSize;
}
