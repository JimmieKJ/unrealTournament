// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FMacGraphicsSwitchingStyle
{
public:
	static void Initialize();

	static void Shutdown();

	static TSharedPtr< class ISlateStyle > Get();

private:
	static TSharedPtr< class FSlateStyleSet > StyleSet;
};