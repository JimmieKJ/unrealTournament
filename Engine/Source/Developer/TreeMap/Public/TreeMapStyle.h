// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Style data for STreeMap
 */
class TREEMAP_API FTreeMapStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create();

private:

	static TSharedPtr< class FSlateStyleSet > StyleInstance;
};
