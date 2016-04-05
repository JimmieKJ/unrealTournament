// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUD_CTF.h"
#include "UTHUD_Gauntlet.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_Gauntlet : public AUTHUD_CTF
{
	GENERATED_UCLASS_BODY()
	virtual void DrawMinimapSpectatorIcons() override;
	void DrawHUD();
};