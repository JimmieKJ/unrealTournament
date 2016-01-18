// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if !UE_SERVER
#include "SlateBasics.h"
#include "SColorPicker.h"

class UNREALTOURNAMENT_API SUTColorPicker : public SColorPicker
{
public:

	bool UTSetNewTargetColorRGB(const FLinearColor& NewValue)
	{
		return SetNewTargetColorRGB(NewValue, true);
	}

};

#endif