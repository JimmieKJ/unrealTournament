// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class SUWindowsStyle 
{
public:
	static void Initialize();
	static void Shutdown();
	static void Reload();
	
	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	static TSharedRef<class FSlateStyleSet> Create();
	static TSharedPtr<class FSlateStyleSet> UWindowsStyleInstance;
};

