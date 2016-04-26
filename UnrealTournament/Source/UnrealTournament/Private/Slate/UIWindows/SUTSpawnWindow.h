// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../FLoadoutData.h"
#include "../Base/SUTWindowBase.h"
#include "SUTHUDWindow.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTSpawnWindow: public SUTHUDWindow
{
public:
	// Allows this window to be identified as a given type.
	virtual FName IdentifyWindow()
	{
		return FName(TEXT("SpawnWindow"));
	}

protected:
	virtual void BuildWindow();
	void BuildSpawnBar(float YPosition);
	FText GetSpawnBarText() const;

};

#endif
