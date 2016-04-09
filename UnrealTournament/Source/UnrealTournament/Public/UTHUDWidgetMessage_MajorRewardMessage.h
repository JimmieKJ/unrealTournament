// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "UTHUDWidgetMessage_MajorRewardMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage_MajorRewardMessage : public UUTHUDWidgetMessage
{
	GENERATED_UCLASS_BODY()

		virtual bool ShouldDraw_Implementation(bool bShowScores) override;

protected:

	virtual void DrawMessages(float DeltaTime);
};
#pragma once
