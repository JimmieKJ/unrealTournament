// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FPackageContentStyle
{
public:
	static void Initialize();

	static void Shutdown();

	static TSharedPtr< class ISlateStyle > Get();

	static const FName& GetStyleSetName()
	{
		return StyleSet->GetStyleSetName();
	}

private:
	static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);

private:
	static TSharedPtr< class FSlateStyleSet > StyleSet;
};