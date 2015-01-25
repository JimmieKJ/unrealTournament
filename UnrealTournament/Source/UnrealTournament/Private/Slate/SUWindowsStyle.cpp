// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "FriendsAndChatStyle.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER
TSharedPtr<FSlateStyleSet> SUWindowsStyle::UWindowsStyleInstance = NULL;

void SUWindowsStyle::Initialize()
{
	if (!UWindowsStyleInstance.IsValid())
	{
		UWindowsStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle( *UWindowsStyleInstance);
	}
}

void SUWindowsStyle::Shutdown()
{
}

FName SUWindowsStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("UWindowsStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".ttf"), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".otf"), __VA_ARGS__ )

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<FSlateStyleSet> SUWindowsStyle::Create()
{
	TSharedRef<FSlateStyleSet> StyleRef = MakeShareable(new FSlateStyleSet(SUWindowsStyle::GetStyleSetName()));

	StyleRef->SetContentRoot(FPaths::GameContentDir() / TEXT("RestrictedAssets/Slate"));
	StyleRef->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UWindows.Standard.Font.Small", TTF_FONT("Exo2-Medium", 10));
	Style.Set("UWindows.Standard.Font.Medium", TTF_FONT("Exo2-Medium", 14));
	Style.Set("UWindows.Standard.Font.Large", TTF_FONT("Exo2-Medium", 22));

	// Note, these sizes are in Slate Units.
	// Slate Units do NOT have to map to pixels.
	const FVector2D Icon5x16(5.0f, 16.0f);
	const FVector2D Icon8x4(8.0f, 4.0f);
	const FVector2D Icon16x4(16.0f, 4.0f);
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon10x10(10.0f, 10.0f);
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon12x16(12.0f, 16.0f);
	const FVector2D Icon14x14(14.0f, 14.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon22x22(22.0f, 22.0f);
	const FVector2D Icon24x24(24.0f, 24.0f);
	const FVector2D Icon25x25(25.0f, 25.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);
	const FVector2D Icon36x24(36.0f, 24.0f);
	const FVector2D Icon17x22(17.0f, 22.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);


	// These are the Slate colors which reference the dynamic colors in FSlateCoreStyle; these are the colors to put into the style
	const FSlateColor DefaultForeground( MakeShareable( new FLinearColor( 0.72f, 0.72f, 0.72f, 1.f ) ) );
	const FSlateColor InvertedForeground( MakeShareable( new FLinearColor( 0, 0, 0 ) ) );
	const FSlateColor SelectorColor(  MakeShareable( new FLinearColor( 0.701f, 0.225f, 0.003f ) ) );
	const FSlateColor SelectionColor(  MakeShareable( new FLinearColor( 0.728f, 0.364f, 0.003f ) ) );
	const FSlateColor SelectionColor_Inactive( MakeShareable( new FLinearColor( 0.807f, 0.596f, 0.388f ) ) );
	const FSlateColor SelectionColor_Pressed(  MakeShareable( new FLinearColor( 0.701f, 0.225f, 0.003f ) ) );

	const FSlateSound ButtonHoverSound = FSlateSound::FromName_DEPRECATED(FName("SoundCue'/Game/RestrictedAssets/UI/UT99UI_LittleSelect_Cue.UT99UI_LittleSelect_Cue'"));
	const FSlateSound ButtonPressSound = FSlateSound::FromName_DEPRECATED(FName("SoundCue'/Game/RestrictedAssets/UI/UT99UI_BigSelect_Cue.UT99UI_BigSelect_Cue'"));

	Style.Set("UWindows.Standard.DarkBackground", new IMAGE_BRUSH( "UWindows.Standard.DarkBackground", FVector2D(32,32), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("UWindows.Logos.Epic_Logo200", new IMAGE_BRUSH( "UWindows.Logos.Epic_Logo200", FVector2D(176,200), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("OldSchool.AnniLogo", new IMAGE_BRUSH( "OldSchool.AnniLogo", FVector2D(1024,768), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("OldSchool.Background", new IMAGE_BRUSH( "OldSchool.Background", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("LoadingScreen", new IMAGE_BRUSH( "LoadingScreen", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("UT15.Logo.Overlay", new IMAGE_BRUSH("UT15Overlay", FVector2D(1920, 1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

	Style.Set("NewSchool.AnniLogo", new IMAGE_BRUSH( "NewSchool.AnniLogo", FVector2D(1141,431), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("NewSchool.Background", new IMAGE_BRUSH( "NewSchool.Background", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("BadSchool.Background", new IMAGE_BRUSH( "BadSchool.Background", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UWindows.Match.ReadyImage", new IMAGE_BRUSH( "Match/UWindows.Match.ReadyImage", FVector2D(102.0f, 128.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("Testing.TestPortrait", new IMAGE_BRUSH( "Testing/Testing.TestPortrait", FVector2D(102.0f, 128.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("Testing.TestMapShot", new IMAGE_BRUSH( "Testing/Testing.TestMapShot", FVector2D(400.0f, 213.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	{  // Standard Button

		Style.Set("UWindows.Standard.Button", FButtonStyle()
			.SetNormal ( BOX_BRUSH("UWindows.Standard.Button.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("UWindows.Standard.Button.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("UWindows.Standard.Button.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.Button.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("UWindows.Standard.MainMenuButton.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.MainMenuButton.SubMenu.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 12))
			.SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 1.f))
			);

	}

	Style.Set("UWindows.Lobby.MatchBar.Background", new BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Normal", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

	Style.Set("UWindows.Lobby.MatchBar.Button", FButtonStyle()
		.SetNormal(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Normal", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
		.SetHovered(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Hovered", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Pressed", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);


	Style.Set("UWindows.Lobby.MatchBar.Button.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Bold", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);


	Style.Set("UWindows.Match.PlayerButton", FButtonStyle()
		.SetNormal ( BOX_BRUSH("Match/UWindows.Match.PlayerButton.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetHovered( BOX_BRUSH("Match/UWindows.Match.PlayerButton.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetPressed( BOX_BRUSH("Match/UWindows.Match.PlayerButton.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UWindows.Match.ReadyPlayerButton", FButtonStyle()
		.SetNormal ( BOX_BRUSH("Match/UWindows.Match.ReadyPlayerButton.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetHovered( BOX_BRUSH("Match/UWindows.Match.ReadyPlayerButton.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetPressed( BOX_BRUSH("Match/UWindows.Match.ReadyPlayerButton.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);


	Style.Set("UWindows.Lobby.MatchBar.Button", FButtonStyle()
		.SetNormal(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Normal", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
		.SetHovered(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Hovered", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Pressed", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);



	Style.Set("UWindows.Standard.SmallButton.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Bold", 10))
		.SetColorAndOpacity(FLinearColor::White)
		);


	// Matches
	{

		Style.Set("UWindows.BadgeBackground", new IMAGE_BRUSH( "UWindows.Lobby.MatchButton.Normal", FVector2D(256,256)));


		Style.Set("UWindows.Lobby.MatchButton", FButtonStyle()
			.SetNormal ( IMAGE_BRUSH( "UWindows.Lobby.MatchButton.Normal", FVector2D(256,256)))
			.SetHovered( IMAGE_BRUSH( "UWindows.Lobby.MatchButton.Focused", FVector2D(256,256)))
			.SetPressed( IMAGE_BRUSH( "UWindows.Lobby.MatchButton.Pressed", FVector2D(256,256)))
			.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("UWindows.Lobby.MatchButton.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 12))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Lobby.MatchButton.Action.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 12))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

	}

	// Dialogs
	{
		Style.Set("UWindows.Standard.Dialog.Background", new BOX_BRUSH("UWindows.Standard.Dialog.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

		Style.Set("UWindows.Standard.Dialog.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Roboto-Regular", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.Dialog.ErrorTextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Roboto-Regular", 10))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Standard.Dialog.Title.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 10))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Standard.Dialog.TextStyle.Legal", FTextBlockStyle()
			.SetFont(TTF_FONT("Roboto-Regular", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.Dialog.Options.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Roboto-Regular", 12))
			.SetColorAndOpacity(FLinearColor::Black)
			);


		// Legacy style; still being used by some editor widgets
		Style.Set( "UWindows.ComboBox.TickMark", new IMAGE_BRUSH("UWindows.ComboBox.TickMark", Icon8x4 ) );

		FComboButtonStyle ComboButton = FComboButtonStyle()
			.SetButtonStyle(Style.GetWidgetStyle<FButtonStyle>("UWindows.Standard.Button"))
			.SetDownArrowImage(IMAGE_BRUSH("UWindows.ComboBox.TickMark", Icon8x4))
			.SetMenuBorderBrush(BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f/64.0f)))
			.SetMenuBorderPadding(FMargin(5.0f,0.05,5.0f,0.0f));
		Style.Set( "UWindows.Standard.ComboButton", ComboButton );

		Style.Set( "UWindows.Standard.ComboBox", FComboBoxStyle()
			.SetComboButtonStyle(ComboButton)
			);
	}

	// Toasts
	{
		Style.Set("UWindows.Standard.Toast.Background", new BOX_BRUSH("UWindows.Standard.Toast.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("UWindows.Standard.Toast.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 12))
			.SetColorAndOpacity(FLinearColor(0,0.05,0.59))
			);
	}

	Style.Set("UWindows.Standard.UpTick", new IMAGE_BRUSH("ServerBrowser/SortUpArrow", Icon8x4));
	Style.Set("UWindows.Standard.DownTick", new IMAGE_BRUSH("ServerBrowser/SortDownArrow", Icon8x4));

	Style.Set("UWindows.Standard.NormalText", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.BoldText", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 14))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

	Style.Set("UWindows.Standard.SmallText", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 10))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.ContentText", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 10))
		.SetColorAndOpacity(FLinearColor::Blue)
		);

	{
		Style.Set("UWindows.Standard.StatsViewer.Backdrop", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Backdrop", FMargin(8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f)));
	}

	// Server Browser
	{
		Style.Set("UWindows.Standard.ServerBrowser.LeftArrow", new IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.LeftArrow", FVector2D(12,8), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

		Style.Set("UWindows.Standard.ServerBrowser.Backdrop", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Backdrop", FMargin(8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f)));
		Style.Set("UWindows.Standard.ServerBrowser.TopSubBar", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.TopSubBar", FMargin(14.0f / 64.0f, 16.0f / 32.0f, 14.0f / 64.0f, 16.0f / 32.0f)));
		Style.Set("UWindows.Standard.ServerBrowser.BottomSubBar", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BottomSubBar", FMargin(14.0f / 64.0f, 16.0f / 32.0f, 14.0f / 64.0f, 16.0f / 32.0f)));

		Style.Set("UWindows.Standard.ServerBrowser.NormalText", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.ServerBrowser.BoldText", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 14))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Standard.ServerBrowser.HugeText", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 48))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);


		Style.Set("UWindows.Standard.ServerBrowser.Lock", new IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.Lock", Icon17x22, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

		Style.Set( "UWindows.Standard.ServerBrowser.Row", FTableRowStyle()
			.SetEvenRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetEvenRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal) )
			.SetOddRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetOddRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal ) )
			.SetSelectorFocusedBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetTextColor( FLinearColor::White)
			.SetSelectedTextColor( FLinearColor::Black)
			);


		const FTableColumnHeaderStyle TableColumnHeaderStyle = FTableColumnHeaderStyle()
			.SetSortPrimaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrow", Icon8x4))
			.SetSortPrimaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrow", Icon8x4))
			.SetSortSecondaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrows", Icon16x4))
			.SetSortSecondaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrows", Icon16x4))
			.SetNormalBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f/32.f) )
			.SetHoveredBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f/32.f) )
			.SetMenuDropdownImage( IMAGE_BRUSH( "ServerBrowser/ColumnHeader_Arrow", Icon8x8 ) )
			.SetMenuDropdownNormalBorderBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f/32.f ) )
			.SetMenuDropdownHoveredBorderBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f/32.f ) );
		Style.Set( "UWindows.Standard.ServerBrowser.Header.Column", TableColumnHeaderStyle );

		const FTableColumnHeaderStyle TableLastColumnHeaderStyle = FTableColumnHeaderStyle()
			.SetSortPrimaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrow", Icon8x4))
			.SetSortPrimaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrow", Icon8x4))
			.SetSortSecondaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrows", Icon16x4))
			.SetSortSecondaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrows", Icon16x4))
			.SetNormalBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f/32.f) )
			.SetHoveredBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f/32.f) )
			.SetMenuDropdownImage( IMAGE_BRUSH( "ServerBrowser/ColumnHeader_Arrow", Icon8x8 ) )
			.SetMenuDropdownNormalBorderBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f/32.f ) )
			.SetMenuDropdownHoveredBorderBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f/32.f ) );

		const FSplitterStyle TableHeaderSplitterStyle = FSplitterStyle()
			.SetHandleNormalBrush( FSlateNoResource() )
			.SetHandleHighlightBrush( IMAGE_BRUSH( "ServerBrowser/HeaderSplitterGrip", Icon8x8 ) );

		Style.Set( "UWindows.Standard.ServerBrowser.Header", FHeaderRowStyle()
			.SetColumnStyle( TableColumnHeaderStyle )
			.SetLastColumnStyle( TableLastColumnHeaderStyle )
			.SetColumnSplitterStyle( TableHeaderSplitterStyle )
			.SetBackgroundBrush( BOX_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.HeaderBar", 4.f/32.f ) )
			.SetForegroundColor( DefaultForeground )
			);

		Style.Set("UWindows.Standard.ServerBrowser.Header.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 10))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.ServerBrowser.Button", FButtonStyle()
			.SetNormal ( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button",		   FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button.Hovered", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button.Pressed", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button.Disabled", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("UWindows.Standard.ServerBrowser.RightButton", FButtonStyle()
			.SetNormal ( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton",		   FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton.Hovered", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton.Pressed", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton.Disabled", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);


		Style.Set("UWindows.Standard.ServerBrowser.BlankButton", FButtonStyle()
			.SetNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHovered( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BlankButton.Hovered", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BlankButton.Pressed", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BlankButton.Disabled", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);


		Style.Set("UWindows.Standard.ServerBrowser.EditBox", FEditableTextBoxStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 12))
			.SetForegroundColor(FLinearColor::White)
			.SetBackgroundImageNormal( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
			.SetBackgroundImageHovered( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
			.SetBackgroundImageFocused( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
			.SetBackgroundImageReadOnly( BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
			);



		Style.Set("UWindows.Standard.HUBBrowser.TitleText", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 28))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.HUBBrowser.TitleText.Selected", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 28))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Standard.HUBBrowser.NormalText", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);


		Style.Set( "UWindows.Standard.HUBBrowser.Row", FTableRowStyle()
			.SetEvenRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetEvenRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal) )
			.SetOddRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetOddRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal ) )
			.SetSelectorFocusedBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetTextColor( FLinearColor::White)
			.SetSelectedTextColor( FLinearColor::Black)
			);


	}


	{ // Menu Bar Background

		Style.Set("UWindows.Standard.MenuBar.Background", new BOX_BRUSH("UWindows.Standard.MenuBar.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("UWindows.Standard.Backdrop", new BOX_BRUSH("UWindows.Standard.Backdrop", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));


		Style.Set("UWindows.Standard.MenuButton", FButtonStyle()
			.SetNormal ( FSlateNoResource( FVector2D( 128.0f, 128.0f ) ))
			.SetHovered( BOX_BRUSH("UWindows.Standard.MenuButton.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("UWindows.Standard.MenuButton.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.MenuButton.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("UWindows.Standard.MenuList", FButtonStyle()
			.SetNormal ( BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("UWindows.Standard.MenuList.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("UWindows.Standard.MenuList.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);
	}


	{ // Background Image
	
		Style.Set("UWindows.Desktop.Background", new IMAGE_BRUSH("UWindows.Desktop.Background", FVector2D(512, 512), FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Both));
		Style.Set("UWindows.Desktop.Background.Logo", new IMAGE_BRUSH("UWindows.Desktop.Background.Logo", FVector2D(3980,1698), FLinearColor(1, 1, 1, 1), ESlateBrushTileType::NoTile));
	}


	{ // MidGame Menu Bar Background - RED

		Style.Set("Red.UWindows.MidGameMenu.Button.TextColor", FLinearColor::White);
		Style.Set("Red.UWindows.MidGameMenu.Bar", new BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Bar", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("Red.UWindows.MidGameMenu.Bar.Top", new BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Bar.Top", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

		Style.Set("Red.UWindows.MidGameMenu.Button", FButtonStyle()
			.SetNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHovered(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Button.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Button.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);


		Style.Set("Red.UWindows.MidGameMenu.ChatBarButton.First", FButtonStyle()
			.SetNormal(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.FirstButton.Normal", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.FirstButton.Hovered", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.FirstButton.Pressed", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("Red.UWindows.MidGameMenu.ChatBarButton", FButtonStyle()
			.SetNormal(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.Button.Normal", FMargin(6.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.Button.Hovered", FMargin(6.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.Button.Pressed", FMargin(6.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("Red.UWindows.MidGameMenu.ChatEditBox", FEditableTextBoxStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 14))
			.SetForegroundColor(FLinearColor::White)
			.SetBackgroundImageNormal( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageHovered( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageFocused( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageReadOnly( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			);

		Style.Set("Red.UWindows.MidGameMenu.Chatbar.Background", new BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));


		Style.Set("Red.UWindows.MidGameMenu.Button.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("Red.UWindows.MidGameMenu.Button.SubMenu.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("Red.UWindows.MidGameMenu.Button.SubMenu.TextStyle.AltTeam", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 12))
			.SetColorAndOpacity(FLinearColor(0.38,0.38,1.0,1.0))
			);

		Style.Set("Red.UWindows.MidGameMenu.MenuList", FButtonStyle()
			.SetNormal ( BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("Red.UWindows.MidGameMenu.UserList.Background", new BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.UserList.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

		Style.Set( "Red.UWindows.MidGameMenu.UserList.Row", FTableRowStyle()
			.SetEvenRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetEvenRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal) )
			.SetOddRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetOddRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal ) )
			.SetSelectorFocusedBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetTextColor( FLinearColor::White)
			.SetSelectedTextColor( FLinearColor::Black)
			);

		Style.Set("Red.UWindows.MidGameMenu.Status.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);
	}


	{ // MidGame Menu Bar Background - BLUE

		Style.Set("Blue.UWindows.MidGameMenu.Button.TextColor", FLinearColor::White);
		Style.Set("Blue.UWindows.MidGameMenu.Bar", new BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Bar", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("Blue.UWindows.MidGameMenu.Bar.Top", new BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Bar.Top", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

		Style.Set("Blue.UWindows.MidGameMenu.Button", FButtonStyle()
			.SetNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHovered(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Button.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Button.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);


		Style.Set("Blue.UWindows.MidGameMenu.ChatBarButton.First", FButtonStyle()
			.SetNormal(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.FirstButton.Normal", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.FirstButton.Hovered", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.FirstButton.Pressed", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("Blue.UWindows.MidGameMenu.ChatBarButton", FButtonStyle()
			.SetNormal(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.Button.Normal", FMargin(6.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.Button.Hovered", FMargin(6.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.Button.Pressed", FMargin(6.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("Blue.UWindows.MidGameMenu.ChatEditBox", FEditableTextBoxStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 14))
			.SetForegroundColor(FLinearColor::White)
			.SetBackgroundImageNormal( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageHovered( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageFocused( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageReadOnly( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			);

		Style.Set("Blue.UWindows.MidGameMenu.Chatbar.Background", new BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));


		Style.Set("Blue.UWindows.MidGameMenu.Button.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("Blue.UWindows.MidGameMenu.Button.SubMenu.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("Blue.UWindows.MidGameMenu.Button.SubMenu.TextStyle.AltTeam", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 12))
			.SetColorAndOpacity(FLinearColor(1.0,0.38,0.38,1.0))
			);


		Style.Set("Blue.UWindows.MidGameMenu.MenuList", FButtonStyle()
			.SetNormal ( BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);


		Style.Set("Blue.UWindows.MidGameMenu.UserList.Background", new BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.UserList.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

		Style.Set( "Blue.UWindows.MidGameMenu.UserList.Row", FTableRowStyle()
			.SetEvenRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetEvenRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal) )
			.SetOddRowBackgroundBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetOddRowBackgroundHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal ) )
			.SetSelectorFocusedBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetActiveHoveredBrush( IMAGE_BRUSH( "ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) )
			.SetTextColor( FLinearColor::White)
			.SetSelectedTextColor( FLinearColor::Black)
			);


		Style.Set("Blue.UWindows.MidGameMenu.Status.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);
	}




	{	// MOTD / Server / Chat
	
		Style.Set("UWindows.Standard.MOTD.ServerTitle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 22))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Standard.MOTD.GeneralText", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.MOTD.RulesText", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 10))
			.SetColorAndOpacity(FLinearColor::Gray)
			);

		Style.Set("UWindows.Standard.Chat", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 10))
			.SetColorAndOpacity(FLinearColor::White)
			);


	}

	{	// Text Chat styles
	
		const FTextBlockStyle NormalChatStyle = FTextBlockStyle().SetFont(TTF_FONT("Exo2-Medium", 12)). SetColorAndOpacity(FLinearColor::White);

		Style.Set("UWindows.Chat.Text.Global", FTextBlockStyle(NormalChatStyle));
		Style.Set("UWindows.Chat.Text.Lobby", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::White));
		Style.Set("UWindows.Chat.Text.Match", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Yellow));
		Style.Set("UWindows.Chat.Text.Friends", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Green));
		Style.Set("UWindows.Chat.Text.Team", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Yellow));
		Style.Set("UWindows.Chat.Text.Team.Red", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor(1.0f,0.25f,0.25f,1.0f)));
		Style.Set("UWindows.Chat.Text.Team.Blue", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor(0.25,0.25,1.0,1.0)));
		Style.Set("UWindows.Chat.Text.Local", FTextBlockStyle(NormalChatStyle));
		Style.Set("UWindows.Chat.Text.Admin", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Yellow));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("EnableFriendsAndChat")))
	{
		// Friends chat styles
		const FSlateFontInfo RobotoRegular10    = TTF_FONT("Social-WIP/Font/Lato-Regular", 10);
		const FSlateFontInfo RobotoRegular12    = TTF_FONT("Social-WIP/Font/Lato-Regular", 12);
		const FSlateFontInfo RobotoBold10       = TTF_FONT("Social-WIP/Font/Lato-Bold", 10 );
		const FSlateFontInfo RobotoBold12       = TTF_FONT("Social-WIP/Font/Lato-Bold", 12);

		const FScrollBarStyle ScrollBar = FScrollBarStyle()
			.SetVerticalBackgroundImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f), FLinearColor(1.0f, 1.0f, 1.0f, 0.25f)))
			.SetHorizontalBackgroundImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f), FLinearColor(1.0f, 1.0f, 1.0f, 0.25f)))
			.SetNormalThumbImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f)))
			.SetDraggedThumbImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f)))
			.SetHoveredThumbImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f)));

		const FTextBlockStyle DefaultText = FTextBlockStyle()
			.SetFont(RobotoRegular12)
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetShadowOffset(FVector2D::ZeroVector)
			.SetShadowColorAndOpacity(FLinearColor::Black);

		const FButtonStyle DefaultButton = FButtonStyle()
			.SetNormalPadding(0)
			.SetPressedPadding(0);

		const FButtonStyle FriendsListClosedButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(IMAGE_BRUSH("Social-WIP/SocialExpander_normal", Icon16x16))
			.SetPressed(IMAGE_BRUSH("Social-WIP/SocialExpander_normal", Icon16x16))
			.SetHovered(IMAGE_BRUSH("Social-WIP/SocialExpander_over", Icon16x16));
		Style.Set("FriendsListOpen", FriendsListClosedButtonStyle);

		const FButtonStyle FriendsListOpenButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(IMAGE_BRUSH("Social-WIP/SocialExpanderOpen_normal", Icon16x16))
			.SetPressed(IMAGE_BRUSH("Social-WIP/SocialExpanderOpen_normal", Icon16x16))
			.SetHovered(IMAGE_BRUSH("Social-WIP/SocialExpanderOpen_over", Icon16x16));
		Style.Set("FriendsListClosed", FriendsListOpenButtonStyle);

		static const FVector2D FriendButtonSize(60.0f, 60.0f);

		const FButtonStyle FriendsListActionButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(BOX_BRUSH("Social-WIP/SocialDefaultButton_normal", FriendButtonSize, FMargin(0.5)))
			.SetPressed(BOX_BRUSH("Social-WIP/SocialDefaultButton_down", FriendButtonSize, FMargin(0.5)))
			.SetHovered(BOX_BRUSH("Social-WIP/SocialDefaultButton_over", FriendButtonSize, FMargin(0.5)))
			.SetNormalPadding(FMargin(5,2))
			.SetPressedPadding(FMargin(5,2));
		Style.Set("FriendsListActionButton", FriendsListActionButtonStyle);

		FComboButtonStyle FriendListComboButtonStyle = FComboButtonStyle()
			.SetButtonStyle(FriendsListActionButtonStyle)
			.SetDownArrowImage(FSlateNoResource())
			.SetMenuBorderBrush(FSlateNoResource())
			.SetMenuBorderPadding(FMargin(0.0f));
	 
		const FButtonStyle FriendActionDropdownStyle = FButtonStyle(DefaultButton)
			.SetNormal(IMAGE_BRUSH("Social-WIP/SocialDropdownButton_normal", Icon24x24))
			.SetPressed(IMAGE_BRUSH("Social-WIP/SocialDropdownButton_down", Icon24x24))
			.SetHovered(IMAGE_BRUSH("Social-WIP/SocialDropdownButton_over", Icon24x24));
		Style.Set("FriendActionDropdown", FriendActionDropdownStyle);

		const FButtonStyle FriendsListEmphasisButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(BOX_BRUSH("Social-WIP/SocialEmphasisButton_normal", FriendButtonSize, FMargin(0.5)))
			.SetPressed(BOX_BRUSH("Social-WIP/SocialEmphasisButton_down", FriendButtonSize, FMargin(0.5)))
			.SetHovered(BOX_BRUSH("Social-WIP/SocialEmphasisButton_over", FriendButtonSize, FMargin(0.5)))
			.SetNormalPadding(FMargin(5,2))
			.SetPressedPadding(FMargin(5,2));
		Style.Set("FriendsListEmphasisButton", FriendsListEmphasisButtonStyle);

		const FButtonStyle FriendsListCriticalButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(BOX_BRUSH("Social-WIP/SocialCriticalButton_normal", FriendButtonSize, FMargin(0.5)))
			.SetPressed(BOX_BRUSH("Social-WIP/SocialCriticalButton_down", FriendButtonSize, FMargin(0.5)))
			.SetHovered(BOX_BRUSH("Social-WIP/SocialCriticalButton_over", FriendButtonSize, FMargin(0.5)))
			.SetNormalPadding(FMargin(5,2))
			.SetPressedPadding(FMargin(5,2));
		Style.Set("FriendsListCriticalButton", FriendsListCriticalButtonStyle);

		const FButtonStyle FriendsListAddFriendButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(BOX_BRUSH("Social-WIP/SocialDefaultButton_normal", FriendButtonSize, FMargin(0.5)))
			.SetPressed(BOX_BRUSH("Social-WIP/SocialDefaultButton_down", FriendButtonSize, FMargin(0.5)))
			.SetHovered(BOX_BRUSH("Social-WIP/SocialDefaultButton_over", FriendButtonSize, FMargin(0.5)));
		Style.Set("FriendsListActionButton", FriendsListAddFriendButtonStyle);

		Style.Set("FriendsList.AddFriendContent", new IMAGE_BRUSH("Social-WIP/FriendsAddIcon", FVector2D(14, 14)));

		const FButtonStyle FriendsListCloseAddFriendButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(IMAGE_BRUSH("Social-WIP/SocialCloseButton_normal", Icon24x24))
			.SetPressed(IMAGE_BRUSH("Social-WIP/SocialCloseButton_down", Icon24x24))
			.SetHovered(IMAGE_BRUSH("Social-WIP/SocialCloseButton_over", Icon24x24));
		Style.Set("FriendsListCloseAddFriendButton", FriendsListCloseAddFriendButtonStyle);

		Style.Set("FriendList.AddFriendEditableText.NoBackground", new FSlateNoResource(FVector2D(8, 8)));
		const FEditableTextBoxStyle AddFriendEditableTextStyle = FEditableTextBoxStyle()
			.SetBackgroundImageNormal(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
			.SetBackgroundImageHovered(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
			.SetBackgroundImageFocused(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
			.SetBackgroundImageReadOnly(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
			.SetPadding(FMargin(5.0f))
			.SetFont(RobotoRegular10);
		Style.Set("FriendList.AddFriendEditableTextStyle", AddFriendEditableTextStyle);

		Style.Set("FriendsList.AddFriendEditBorder", new BOX_BRUSH("Social-WIP/OutlinedWhiteBox_OpenTop", FVector2D(8.0f, 8.0f), FMargin(0.5), FLinearColor::White));

		const FButtonStyle FriendsListItemButtonStyle = FButtonStyle(DefaultButton)
			.SetNormal(IMAGE_BRUSH("Social-WIP/White", FVector2D(8, 8), FLinearColor::Transparent))
			.SetHovered(IMAGE_BRUSH("Social-WIP/White", FVector2D(8, 8), FLinearColor(0.1f,0.05f,0.05f)))
			.SetPressed(IMAGE_BRUSH("Social-WIP/White", FVector2D(8, 8), FLinearColor(0.1f,0.05f,0.05f)));
		Style.Set("FriendsListItemButtonStyle", FriendsListItemButtonStyle);

		const FTextBlockStyle FriendsTextStyle = FTextBlockStyle(DefaultText)
			.SetFont(RobotoRegular10)
			.SetColorAndOpacity(FLinearColor(FColor::White));
		Style.Set("FriendsTextStyle", FriendsTextStyle);

		Style.Set("OnlineState", new IMAGE_BRUSH("Social-WIP/FriendOnlineState", FVector2D(16, 16)));
		Style.Set("OfflineState", new IMAGE_BRUSH("Social-WIP/FriendOfflineStatus", FVector2D(16, 16)));
		Style.Set("AwayState", new IMAGE_BRUSH("Social-WIP/FriendAwayStatus", FVector2D(16, 16)));

		Style.Set("GlobalChatIcon", new IMAGE_BRUSH("Social-WIP/Icon-ChatGlobal-XS", Icon16x16));
		Style.Set("WhisperChatIcon", new IMAGE_BRUSH("Social-WIP/Icon-ChatWhisper-XS", Icon16x16));
		Style.Set("PartyChatIcon", new IMAGE_BRUSH("Social-WIP/Icon-ChatParty-XS", Icon16x16));

		//Chat Window Style
		Style.Set("FriendsStyle", FFriendsAndChatStyle()
			.SetOnlineBrush(*Style.GetBrush("OnlineState"))
			.SetOfflineBrush(*Style.GetBrush("OfflineState"))
			.SetAwayBrush(*Style.GetBrush("AwayState"))
			// .SetAwayBrush(*Style.GetBrush("AwayState"))
			.SetFriendsListClosedButtonStyle(FriendsListClosedButtonStyle)
			.SetFriendsListOpenButtonStyle(FriendsListOpenButtonStyle)
			// .SetFriendsListStatusButtonStyle(FriendsListActionButtonStyle)
			// .SetFriendGeneralButtonStyle(FriendsListActionButtonStyle)
			// .SetFriendListActionButtonStyle(FriendsListActionButtonStyle)
			.SetFriendsListEmphasisButtonStyle(FriendsListEmphasisButtonStyle)
			.SetFriendsListCriticalButtonStyle(FriendsListCriticalButtonStyle)
			.SetAddFriendButtonStyle(FriendsListAddFriendButtonStyle)
			// .SetFriendActionDropdownButtonStyle(FriendActionDropdownStyle)
			// .SetFriendsListComboButtonStyle(FriendListComboButtonStyle)
			// .SetFriendComboBackgroundLeftBrush(*FriendsDefaultBackgroundBrush)
			// .SetFriendComboBackgroundRightBrush(*FriendsDefaultBackgroundBrush)
			.SetAddFriendButtonContentBrush(*Style.GetBrush("FriendsList.AddFriendContent"))
			.SetAddFriendCloseButtonStyle(FriendsListCloseAddFriendButtonStyle)
			.SetFriendsListItemButtonStyle(FriendsListItemButtonStyle)
			.SetFriendsCalloutBrush(*Style.GetBrush("FriendComboDropdownBrush"))
			// .SetBackgroundBrush(*FriendsDefaultBackgroundBrush)
			// .SetTitleBarBrush(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::White))
			// .SetFriendContainerHeader(*FriendContainerHeader)
			// .SetFriendListHeader(*FriendsListHeaderBrush)
			// .SetFriendItemSelected(*FriendsDefaultBackgroundBrush)
			// .SetChatContainerBackground(*FriendsDefaultBackgroundBrush)
			// .SetFriendContainerBackground(*FriendsDefaultBackgroundBrush)
			// .SetAddFriendEditBorder(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::White))
			// .SetFriendsComboDropdownImageBrush(*Style.GetBrush("FriendComboDropdownBrush"))
			// .SetFriendsListItemButtonSimpleStyle(FriendsListItemButtonStyle)
			.SetTextStyle(FriendsTextStyle)
			.SetFontStyle(RobotoRegular12)
			.SetFontStyleBold(RobotoBold12)
			.SetFontStyleSmall(RobotoRegular10)
			.SetFontStyleSmallBold(RobotoBold10)
			// .SetDefaultFontColor(DefaultChatColor)
			// .SetDefaultChatColor(DefaultChatColor)
			// .SetWhisplerChatColor(WhisplerChatColor)
			// .SetPartyChatColor(PartyChatColor)
			// .SetChatGlobalBrush(*Style.GetBrush("GlobalChatIcon"))
			.SetChatWhisperBrush(*Style.GetBrush("WhisperChatIcon"))
			// .SetChatPartyBrush(*Style.GetBrush("PartyChatIcon"))
			.SetAddFriendEditableTextStyle(AddFriendEditableTextStyle)
			.SetStatusButtonSize(FVector2D(108, 24))
			.SetBorderPadding(FMargin(5,5))
			.SetFriendsListWidth(600.f)
			// .SetChatListPadding(130.f)
			// .SetChatBackgroundBrush(*FriendsDefaultBackgroundBrush)
			.SetFriendCheckboxStyle(FCoreStyle::Get().GetWidgetStyle< FCheckBoxStyle >("Checkbox"))
			// .SetChatChannelsBackgroundBrush(*FriendsDefaultBackgroundBrush)
			// .SetChatOptionsBackgroundBrush(*FriendsDefaultBackgroundBrush)
			.SetScrollbarStyle(ScrollBar)
			.SetWindowStyle(Style.GetWidgetStyle<FWindowStyle>("Window"))
		);

	}

	{ // Match Badge Styles

		Style.Set("UWindows.Standard.MatchBadge", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 14))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

		Style.Set("UWindows.Standard.MatchBadge.Bold", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);

		Style.Set("UWindows.Standard.MatchBadge.Red", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 14))
		.SetColorAndOpacity(FLinearColor::Red)
		);

		Style.Set("UWindows.Standard.MatchBadge.Blue", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 14))
		.SetColorAndOpacity(FLinearColor(0.7, 0.7, 1.0, 1.0))
		);

		Style.Set("UWindows.Standard.MatchBadge.Header", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 18))
		.SetColorAndOpacity(FLinearColor::White)
		);

		Style.Set("UWindows.Standard.MatchBadge.Small", FTextBlockStyle()
		.SetFont(TTF_FONT("Exo2-Medium", 10))
		.SetColorAndOpacity(FLinearColor::White)
		);
	}

	return StyleRef;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT


void SUWindowsStyle::Reload()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& SUWindowsStyle::Get()
{
	return * UWindowsStyleInstance;
}
#endif