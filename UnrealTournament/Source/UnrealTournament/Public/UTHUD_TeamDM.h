// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "UTHUD_DM.h"
#include "UTHUD_TeamDM.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_TeamDM : public AUTHUD_DM
{
	GENERATED_UCLASS_BODY()

public:
	virtual FLinearColor GetBaseHUDColor();

};