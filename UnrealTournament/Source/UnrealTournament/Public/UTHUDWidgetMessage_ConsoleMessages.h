// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "UTHUDWidgetMessage_ConsoleMessages.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage_ConsoleMessages : public UUTHUDWidgetMessage
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool ShouldDraw_Implementation(bool bShowScores) override
	{
		return (!UTHUDOwner->UTPlayerOwner || !UTHUDOwner->UTPlayerOwner->UTPlayerState 
			|| !UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator || !UTHUDOwner->UTPlayerOwner->bShowCameraBinds);
	}

protected:
	virtual void DrawMessages(float DeltaTime);
	virtual FVector2D DrawMessage(int32 QueueIndex, float X, float Y);
	virtual void LayoutMessage(int32 QueueIndex, TSubclassOf<class UUTLocalMessage> MessageClass, uint32 MessageIndex, FText LocalMessageText, int32 MessageCount, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) override;
	virtual float GetDrawScaleOverride() override;
};
