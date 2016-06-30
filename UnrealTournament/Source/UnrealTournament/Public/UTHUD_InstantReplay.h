// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUD_InstantReplay.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_InstantReplay : public AUTHUD
{
	GENERATED_UCLASS_BODY()

	// Short circult toggle coms
	virtual void ToggleComsMenu(bool bShow) {}

	virtual EInputMode::Type GetInputMode_Implementation() const override;
};