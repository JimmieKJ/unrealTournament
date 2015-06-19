// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Perception/AISenseEvent.h"
#include "AISenseEvent_Hearing.generated.h"

UCLASS()
class AIMODULE_API UAISenseEvent_Hearing : public UAISenseEvent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FAINoiseEvent Event;

public:
	UAISenseEvent_Hearing(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual FAISenseID GetSenseID() const override;
	
	FORCEINLINE FAINoiseEvent GetNoiseEvent()
	{
		Event.Compile();
		return Event;
	}
};