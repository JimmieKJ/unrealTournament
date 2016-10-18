// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFbxAutomationBuilderStyle
{
public:

	static void Initialize();
	static void Shutdown();

	static TSharedPtr< class ISlateStyle > Get();

	static FName GetStyleSetName();

private:

	static TSharedPtr< class FSlateStyleSet > StyleSet;
};