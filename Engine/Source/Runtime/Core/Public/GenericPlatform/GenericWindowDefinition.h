// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "GenericApplicationMessageHandler.h"

struct CORE_API FGenericWindowDefinition
{
	float XDesiredPositionOnScreen;
	float YDesiredPositionOnScreen;

	float WidthDesiredOnScreen;
	float HeightDesiredOnScreen;

	bool HasOSWindowBorder;
	bool SupportsTransparency;
	bool AppearsInTaskbar;
	bool IsTopmostWindow;
	bool AcceptsInput;
	bool ActivateWhenFirstShown;

	bool SupportsMinimize;
	bool SupportsMaximize;

    bool IsModalWindow;
	bool IsRegularWindow;
	bool HasSizingFrame;
	bool SizeWillChangeOften;
	int32 ExpectedMaxWidth;
	int32 ExpectedMaxHeight;

	FString Title;
	float Opacity;
	int32 CornerRadius;

	FWindowSizeLimits SizeLimits;
};