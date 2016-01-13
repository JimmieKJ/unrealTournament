// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "UTHUD_TeamDM.h"
#include "UTHUD_Duel.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_Duel : public AUTHUD_TeamDM
{
	GENERATED_UCLASS_BODY()

public:
	virtual FLinearColor GetBaseHUDColor();

};