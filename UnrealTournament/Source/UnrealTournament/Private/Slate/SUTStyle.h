// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#if !UE_SERVER
class UNREALTOURNAMENT_API SUTStyle
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

	static void SetFonts(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetCommonStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetAvatars(TSharedRef<FSlateStyleSet> StyleRef);

	static FSlateSound ButtonPressSound;
	static FSlateSound ButtonHoverSound;
	static FSlateColor DefaultForeground;
};
#endif
