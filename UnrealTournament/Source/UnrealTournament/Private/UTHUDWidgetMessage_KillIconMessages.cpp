// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage_KillIconMessages.h"
#include "UTKillIconMessage.h"

UUTHUDWidgetMessage_KillIconMessages::UUTHUDWidgetMessage_KillIconMessages(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ManagedMessageArea = FName(TEXT("KillMessage"));
	Position = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.0f, 0.0f);
	Size = FVector2D(0.0f, 0.0f);
	Origin = FVector2D(0.01f, 0.0f);
	NumVisibleLines = 5;
	MessageFontIndex = 0;
	SmallMessageFontIndex = 0;
	LargeShadowDirection = FVector2D(1.f, 1.f);
	SmallShadowDirection = FVector2D(1.f, 1.f);

	MessageHeight = 30.0f;
	MessagePadding = 10.0f;
	IconHeight = 20.0f;
	ColumnPadding = 10.0f;
	FadeTime = 1.0f;

	CurrentIndex = 0;
}


float UUTHUDWidgetMessage_KillIconMessages::GetDrawScaleOverride()
{
	return 0.75f * UTHUDOwner->HUDWidgetScaleOverride;
}


void UUTHUDWidgetMessage_KillIconMessages::DrawMessages(float DeltaTime)
{
	Canvas->Reset();
	
	//Find the height of the bottom message
	float Y = ((MessageHeight + MessagePadding) * (GetNumberOfMessages() - 1)) + (MessageHeight * 0.5f);

	//Draw in reverse order
	int32 MessageIndex = FMath::Min(CurrentIndex, MessageQueue.Num() - 1);
	while (MessageIndex >= 0)
	{
		if (MessageQueue[MessageIndex].MessageClass != nullptr)
		{
			DrawMessage(MessageIndex, 0, Y);
			Y -= MessageHeight + MessagePadding;
		}
		MessageIndex--;
	}
}

int32 UUTHUDWidgetMessage_KillIconMessages::GetNumberOfMessages()
{
	return (NumVisibleLines < MessageQueue.Num() - 1 ? NumVisibleLines : MessageQueue.Num());
}

void UUTHUDWidgetMessage_KillIconMessages::DrawMessage(int32 QueueIndex, float X, float Y)
{
	MessageQueue[QueueIndex].bHasBeenRendered = true;

	//Gather all the info needed to display the message
	APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(GetWorld());
	AUTPlayerState* LocalPS = (LocalPC != nullptr) ? Cast<AUTPlayerState>(LocalPC->PlayerState) : nullptr;

	AUTPlayerState* KillerPS = Cast<AUTPlayerState>(MessageQueue[QueueIndex].RelatedPlayerState_1);
	AUTPlayerState* VictimPS = Cast<AUTPlayerState>(MessageQueue[QueueIndex].RelatedPlayerState_2);

	UClass* DamageTypeClass = Cast<UClass>(MessageQueue[QueueIndex].OptionalObject);
	const UUTDamageType* DmgType = DamageTypeClass ? Cast<UUTDamageType>(DamageTypeClass->GetDefaultObject()) : nullptr;
	if (DmgType == nullptr)
	{
		//Make sure non UUTDamageType damages still get the default icon
		DmgType = Cast<UUTDamageType>(UUTDamageType::StaticClass()->GetDefaultObject());
	}

	//Only scale in the messages that involve the local character
	float CurrentScale = 1.0f;
	if (LocalPS == KillerPS || LocalPS == VictimPS)
	{
		CurrentScale = GetTextScale(QueueIndex);
	}

	// Fade the message
	float Alpha = 1.f;
	if ((MessageQueue[QueueIndex].ScaleInTime > 0.f) && (MessageQueue[QueueIndex].ScaleInSize != 1.f)
		&& (MessageQueue[QueueIndex].LifeLeft > MessageQueue[QueueIndex].LifeSpan - MessageQueue[QueueIndex].ScaleInTime))
	{
		Alpha = (MessageQueue[QueueIndex].LifeSpan - MessageQueue[QueueIndex].LifeLeft) / MessageQueue[QueueIndex].ScaleInTime;
	}
	else if (MessageQueue[QueueIndex].LifeLeft <= FadeTime)
	{
		Alpha = MessageQueue[QueueIndex].LifeLeft / FadeTime;
	}
	ShadowDirection = (MessageQueue[QueueIndex].DisplayFont == MessageFont) ? LargeShadowDirection : SmallShadowDirection;


	//figure out the sizing of all the elements
	float CurrentMessageHeight = MessageHeight * CurrentScale;
	float XL = 0, YL = 0;
	FVector4 KillerSize;
	FVector4 VictimSize;
	FVector4 DamageSize;
	FVector4 BGSize(0, Y - (CurrentMessageHeight * 0.5f), 0.0f, CurrentMessageHeight);

	X += ColumnPadding * CurrentScale;
	float TextYOffset = 4.0f;
	if (KillerPS != nullptr)
	{
		Canvas->TextSize(MessageQueue[QueueIndex].DisplayFont, KillerPS->PlayerName, XL, YL, CurrentScale);
		KillerSize = FVector4(X, Y - TextYOffset, XL, YL);
		X += XL + (ColumnPadding * CurrentScale);
	}
	if (DmgType != nullptr && DmgType->HUDIcon.Texture != nullptr)
	{
		XL = DmgType->HUDIcon.UL;
		YL = DmgType->HUDIcon.VL;

		//Scale the icon to match the height of the message
		float IconScale = IconHeight * CurrentScale / FMath::Abs(DmgType->HUDIcon.VL);
		XL = FMath::Abs(DmgType->HUDIcon.UL) * IconScale;
		YL = FMath::Abs(DmgType->HUDIcon.VL) * IconScale;

		//center the icon
		float IconY = Y - (YL * 0.5f);
		DamageSize = FVector4(X, IconY, XL, YL);

		X += XL + (ColumnPadding * CurrentScale);
	}
	if (VictimPS != nullptr)
	{
		Canvas->TextSize(MessageQueue[QueueIndex].DisplayFont, VictimPS->PlayerName, XL, YL, CurrentScale);
		VictimSize = FVector4(X, Y - TextYOffset, XL, YL);
		X += XL + (ColumnPadding * CurrentScale);
	}


	//Draw the background
	BGSize.Z = X;
	FHUDRenderObject_Texture BG = Background;
	if (LocalPS == KillerPS)
	{
		BG = LocalKillerBG;
	}
	else if (LocalPS == VictimPS)
	{
		BG = LocalVictimBG;
	}
	DrawTexture(BG.Atlas, BGSize.X, BGSize.Y, BGSize.Z, BGSize.W, BG.UVs.U, BG.UVs.V, BG.UVs.UL, BG.UVs.VL, BG.RenderOpacity * Alpha, BG.RenderColor, BG.RenderOffset, BG.Rotation, BG.RotPivot);

	//Draw the killer name
	if (KillerPS != nullptr)
	{
		DrawText(FText::FromString(KillerPS->PlayerName), KillerSize.X, KillerSize.Y, MessageQueue[QueueIndex].DisplayFont, bShadowedText, ShadowDirection, ShadowColor, bOutlinedText, OutlineColor, CurrentScale, Alpha, GetPlayerColor(KillerPS, true), ETextHorzPos::Left, ETextVertPos::Center);
	}

	//Draw the Damage Icon
	if (DmgType != nullptr && DmgType->HUDIcon.Texture != nullptr)
	{
		DrawTexture(DmgType->HUDIcon.Texture, DamageSize.X, DamageSize.Y, DamageSize.Z, DamageSize.W, DmgType->HUDIcon.U, DmgType->HUDIcon.V, DmgType->HUDIcon.UL, DmgType->HUDIcon.VL, Alpha);
	}

	//Draw the victim name
	if (VictimPS != nullptr)
	{
		DrawText(FText::FromString(VictimPS->PlayerName), VictimSize.X, VictimSize.Y, MessageQueue[QueueIndex].DisplayFont, bShadowedText, ShadowDirection, ShadowColor, bOutlinedText, OutlineColor, CurrentScale, Alpha, GetPlayerColor(VictimPS, false), ETextHorzPos::Left, ETextVertPos::Center);
	}
}

void UUTHUDWidgetMessage_KillIconMessages::LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	Super::LayoutMessage(QueueIndex, MessageClass, MessageIndex, LocalMessageText, MessageCount, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);

	CurrentIndex = QueueIndex;
	MessageQueue[QueueIndex].RelatedPlayerState_1 = RelatedPlayerState_1;
	MessageQueue[QueueIndex].RelatedPlayerState_2 = RelatedPlayerState_2;
}

FLinearColor UUTHUDWidgetMessage_KillIconMessages::GetPlayerColor(AUTPlayerState* PS, bool bKiller)
{
	if (PS != nullptr)
	{
		if (PS->Team != nullptr)
		{
			return FMath::LerpStable(PS->Team->TeamColor, FLinearColor::White, 0.3f);
		}
		else
		{
			return FMath::LerpStable((bKiller) ? FLinearColor::Green : FLinearColor::Red, FLinearColor::White, 0.5f);
		}
	}
	return FLinearColor::White;
}
