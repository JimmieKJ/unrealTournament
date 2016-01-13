// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUD.h"

#include "UTHUD_CastingGuide.generated.h"

/** HUD used for the caster's multiview; omits most game details and displays what buttons can be pressed in the main view to change cameras to the ones in the various splitscreens */
UCLASS()
class UNREALTOURNAMENT_API AUTHUD_CastingGuide : public AUTHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};