// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class IElementLocator;

class FSlateWidgetLocatorByPathFactory
{
public:

	static TSharedRef<IElementLocator, ESPMode::ThreadSafe> Create(
		const FString& Path);
};
