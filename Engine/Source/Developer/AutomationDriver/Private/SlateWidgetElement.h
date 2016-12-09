// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class IApplicationElement;

class FSlateWidgetElementFactory
{
public:

	static TSharedRef<IApplicationElement> Create(
		const FWidgetPath& WidgetPath);
};