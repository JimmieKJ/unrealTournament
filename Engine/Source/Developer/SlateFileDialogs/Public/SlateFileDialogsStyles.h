// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

class FSlateFileDialogsStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static const FSlateStyleSet *Get();
	static FName GetStyleSetName();

	static const FSlateBrush * GetBrush(FName PropertyName, const ANSICHAR* Specifier = NULL)
	{
		return StyleInstance->GetBrush(PropertyName, Specifier);
	}

	static FSlateFontInfo GetFontStyle(FName PropertyName, const ANSICHAR* Specifier = NULL)
	{
		return StyleInstance->GetFontStyle(PropertyName, Specifier);
	}
	
private:
	static TSharedPtr<class FSlateStyleSet> Create();
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
