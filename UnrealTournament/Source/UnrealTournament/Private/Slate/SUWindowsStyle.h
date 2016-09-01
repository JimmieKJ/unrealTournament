// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#if !UE_SERVER
class UNREALTOURNAMENT_API SUWindowsStyle
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


	static void SetCommonStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetDialogStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetTopMenuStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetServerBrowserStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetMidGameMenuRedStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetMidGameMenuBlueStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetMOTDServerChatStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetTextChatStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetMatchBadgeStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetMOTDStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetMatchSummaryStyle(TSharedRef<FSlateStyleSet> StyleRef);

	static FSlateSound ButtonPressSound;
	static FSlateSound ButtonHoverSound;
	static FSlateColor DefaultForeground;
};
#endif
