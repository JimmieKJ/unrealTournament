// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
		Lifetime = 4.f;
		FontSizeIndex = 0;
		ScaleInSize = 2.5f;
		ScaleInTime = 0.3f;
	}

	virtual FText GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
	{
		return FText::GetEmpty();
	}

};



