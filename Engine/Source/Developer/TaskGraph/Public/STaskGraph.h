// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VisualizerEvents.h"

class IProfileVisualizerModule
	: public IModuleInterface
{
public:
	virtual void DisplayProfileVisualizer( TSharedPtr< FVisualizerEvent > InProfileData, const TCHAR* InProfilerType, const FText& HeaderMessageText = FText::GetEmpty(), const FLinearColor& HeaderMessageTextColor = FLinearColor::White ) = 0;
};
