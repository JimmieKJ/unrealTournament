// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "UTHUDWidgetMessage_DeathMessages.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage_DeathMessages : public UUTHUDWidgetMessage
{
	GENERATED_UCLASS_BODY()

	virtual bool ShouldDraw_Implementation(bool bShowScores) override;

protected:
	
	// This is the Padding Distance Between The Death Message and Damage Icon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMessage")
	float PaddingBetweenTextAndDamageIcon;

	virtual void DrawMessages(float DeltaTime);
	virtual void DrawMessage(int32 QueueIndex, float X, float Y) override;
};
