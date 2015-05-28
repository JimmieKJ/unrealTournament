// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "GenericApplicationMessageHandler.h"


#ifndef ALPHA_BLENDED_WINDOWS
	#define ALPHA_BLENDED_WINDOWS IS_PROGRAM || WITH_EDITOR
#endif


/** Enumeration to specify different transparency options for SWindows */
enum class EWindowTransparency
{
	/** Value indicating that a window does not support transparency */
	None,

	/** Value indicating that a window supports transparency at the window level (one opacity applies to the entire window) */
	PerWindow,

#if ALPHA_BLENDED_WINDOWS
	/** Value indicating that a window supports per-pixel alpha blended transparency */
	PerPixel,
#endif
};


struct CORE_API FGenericWindowDefinition
{
	float XDesiredPositionOnScreen;
	float YDesiredPositionOnScreen;

	float WidthDesiredOnScreen;
	float HeightDesiredOnScreen;

	EWindowTransparency TransparencySupport;

	bool HasOSWindowBorder;
	bool AppearsInTaskbar;
	bool IsTopmostWindow;
	bool AcceptsInput;
	bool ActivateWhenFirstShown;

	bool HasCloseButton;
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
