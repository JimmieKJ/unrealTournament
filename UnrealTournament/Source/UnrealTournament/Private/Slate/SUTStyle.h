// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

	static FSlateSound MessageSound;
	static FSlateSound PauseSound;

	static const FSlateColor GetSlateColor( const FName PropertyName, const ANSICHAR* Specifier = nullptr );

private:
	static TSharedRef<class FSlateStyleSet> Create();
	static TSharedPtr<class FSlateStyleSet> UWindowsStyleInstance;

	static void SetFonts(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetIcons(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetCommonStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetAvatars(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetRankBadges(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetChallengeBadges(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetContextMenus(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetServerBrowser(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetTextChatStyle(TSharedRef<FSlateStyleSet> StyleRef);
	static void SetLoginStyle(TSharedRef<FSlateStyleSet> StyleRef);

	static FSlateSound ButtonPressSound;
	static FSlateSound ButtonHoverSound;
	static FSlateColor DefaultForeground;
};
#endif
