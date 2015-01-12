// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_GameMessages.h"
#include "UTGameMessage.h"

UUTHUDWidgetMessage_GameMessages::UUTHUDWidgetMessage_GameMessages(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ManagedMessageArea = FName(TEXT("GameMessages"));
	Position = FVector2D(0.0f, 0.0f);			
	ScreenPosition = FVector2D(0.5f, 0.7f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.0f, 0.0f);				
	MessageColor = FLinearColor::Yellow;
	bShadowedText=true;
	ShadowDirection=FVector2D(0,1);

	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("Font'/Game/RestrictedAssets/Fonts/fntAmbex72'"));
	MessageFont = Font.Object;
	FadeTime = 0.5f;
}

float UUTHUDWidgetMessage_GameMessages::GetTextScale(int32 QueueIndex)
{
	return 0.75;
}

void UUTHUDWidgetMessage_GameMessages::LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	Super::LayoutMessage(QueueIndex, MessageClass, MessageIndex, LocalMessageText, MessageCount, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);

	// @TODO FIXMESTEVE temp hack until get proper per message coloring support implemented
	if (MessageClass == UUTGameMessage::StaticClass())
	{
		if (MessageIndex == 9)
		{
			MessageQueue[QueueIndex].DrawColor = FLinearColor::Red;
		}
		else if (MessageIndex == 10)
		{
			MessageQueue[QueueIndex].DrawColor = FLinearColor::Blue;
		}
	}
}

