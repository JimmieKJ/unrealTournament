// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "UTHUDWidgetMessage_ConsoleMessages.generated.h"

UCLASS()
class UUTHUDWidgetMessage_ConsoleMessages : public UUTHUDWidgetMessage 
{
	GENERATED_UCLASS_BODY()

public:
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
		int32 NumVisibleLines;

protected:
	virtual void DrawMessages(float DeltaTime);
	virtual void DrawMessage(int32 QueueIndex, float X, float Y);
private:

};
