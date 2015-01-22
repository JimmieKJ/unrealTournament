// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * BSP mode slate style
 */
class BSPMODE_API FBspModeStyle
{
public:

	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();

	static const FName& GetStyleSetName();

private:

	/** Singleton instances of this style. */
	static TSharedPtr< class FSlateStyleSet > StyleSet;
};
