// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "EditorStyle.h"

/**
* Declares the Collision Analyzer window's visual style.
*/
class FCollisionAnalyzerStyle
{
public:

	class FStyle : public FSlateStyleSet
	{
	public:
		FStyle();

		void Initialize();
	};

	static void Initialize();

	static void Shutdown();

	static TSharedPtr<class ISlateStyle> Get();

	static TSharedPtr< FCollisionAnalyzerStyle::FStyle > StyleInstance;
};
