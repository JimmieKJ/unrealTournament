// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LocateBy.h"

class IElementLocator;

class FSlateWidgetLocatorByDelegateFactory
{
public:

	static TSharedRef<IElementLocator, ESPMode::ThreadSafe> Create(
		const FLocateSlateWidgetElementDelegate& Delegate);

	static TSharedRef<IElementLocator, ESPMode::ThreadSafe> Create(
		const FLocateSlateWidgetPathElementDelegate& Delegate);
};
