// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	PaddingBetweenTextAndDamageIcon = 10.0f;
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

bool UUTHUDWidgetMessage_DeathMessages::ShouldDraw_Implementation(bool bShowScores)
{
	return !bShowScores && GetWorld()->GetGameState() &&  (GetWorld()->GetGameState()->GetMatchState() != MatchState::MatchIntermission);
}

void UUTHUDWidgetMessage_DeathMessages::DrawMessage(int32 QueueIndex, float X, float Y)
{
	if (MessageQueue[QueueIndex].MessageClass && !MessageQueue[QueueIndex].MessageClass->GetDefaultObject<UUTLocalMessage>()->bDrawAsDeathMessage)
	{
		Super::DrawMessage(QueueIndex, X, Y);
		return;
	}
	//Figure out the DamageType that we killed with
	UClass* DamageTypeClass = Cast<UClass>(MessageQueue[QueueIndex].OptionalObject);
	const UUTDamageType* DmgType = DamageTypeClass ? Cast<UUTDamageType>(DamageTypeClass->GetDefaultObject()) : nullptr;
	if (DmgType == nullptr)
	{
		//Make sure non UUTDamageType damages still get the default icon
		DmgType = Cast<UUTDamageType>(UUTDamageType::StaticClass()->GetDefaultObject());
	}

	//Draw Kill Text
	FVector2D TextSize = FVector2D(0, 0);
	if (UTHUDOwner->bDrawCenteredKillMsg)
	{
		ShadowDirection = (MessageQueue[QueueIndex].DisplayFont == MessageFont) ? LargeShadowDirection : SmallShadowDirection;
		TextSize = DrawText(MessageQueue[QueueIndex].Text, X, Y, MessageQueue[QueueIndex].DisplayFont, bShadowedText, ShadowDirection, ShadowColor, bOutlinedText, OutlineColor, GetTextScale(QueueIndex), 1.0f /*alpha*/, MessageQueue[QueueIndex].DrawColor, FLinearColor(0.0f,0.0f,0.0f,0.0f), ETextHorzPos::Center, ETextVertPos::Center);
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
		if (UTHUDOwner->bDrawCenteredKillMsg)
		{
			X += (TextSize.X / 2) + PaddingBetweenTextAndDamageIcon;
		}

		////Center message on Y
		Y -= (YL / 2);

		DrawTexture(DmgType->HUDIcon.Texture, X, Y, XL, YL, DmgType->HUDIcon.U, DmgType->HUDIcon.V, DmgType->HUDIcon.UL, DmgType->HUDIcon.VL, UTHUDOwner->HUDWidgetOpacity);
	}
}

