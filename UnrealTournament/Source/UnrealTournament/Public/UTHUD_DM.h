// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUD_DM.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_DM : public AUTHUD
{
	GENERATED_UCLASS_BODY()

	// Short circult toggle coms in DM.
	virtual void ToggleComsMenu(bool bShow) {}

};