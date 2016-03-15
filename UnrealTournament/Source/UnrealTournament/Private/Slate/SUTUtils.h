// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Base/SUTDialogBase.h"

class UNREALTOURNAMENT_API SUTUtils
{
public:
	static TSharedRef<SToolTip> CreateTooltip(const TAttribute<FText>& Text);
};