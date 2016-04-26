// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
/*
	HUD Windows are controlless windows managed by the hud.
*/
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTWindowBase.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTHUDWindow : public SUTWindowBase
{
public:
	// Returns true if this window can close.
	virtual bool CanWindowClose();

	// Returns the input mode needed if this window is active
	virtual EInputMode::Type GetInputMode()
	{
		return EInputMode::EIM_GameOnly;
	}

	// Allows this window to be identified as a given type.
	virtual FName IdentifyWindow()
	{
		return FName(TEXT("BaseHudWindow"));
	}

protected:
	virtual void BuildWindow();

	TSharedPtr<SOverlay> Overlay;
	TSharedPtr<SCanvas> Canvas;


};

#endif
