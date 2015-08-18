// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTKillIconMessage.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTKillIconMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UUTKillIconMessage(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		MessageArea = FName(TEXT("KillMessage"));
		bIsSpecial = false;
		Lifetime = 4.f;
	}

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
	{
		return FText::GetEmpty();
	}
	virtual bool UseLargeFont(int32 MessageIndex) const override
	{
		return false;
	}
	virtual float GetScaleInSize_Implementation(int32 MessageIndex) const override
	{
		return 3.f;
	}
	virtual float GetScaleInTime_Implementation(int32 MessageIndex) const override
	{
		return 0.3f;
	}
};



