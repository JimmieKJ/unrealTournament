// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUD_CTF.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_CTF : public AUTHUD
{
	GENERATED_UCLASS_BODY()

	virtual FLinearColor GetBaseHUDColor() override;
	virtual void NotifyMatchStateChange() override;
};