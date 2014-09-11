// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsStyle.h"

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


TSharedRef<FSlateStyleSet> SUWindowsStyle::Create()
{
	TSharedRef<FSlateStyleSet> StyleRef = MakeShareable(new FSlateStyleSet(SUWindowsStyle::GetStyleSetName()));

	StyleRef->SetContentRoot(FPaths::GameContentDir() / TEXT("RestrictedAssets/Slate"));
	StyleRef->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UWindows.Standard.Font.Small", TTF_FONT("Exo2-Medium", 10));

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
	const FVector2D Icon128x128(128.0f, 128.0f);

	// These are the Slate colors which reference the dynamic colors in FSlateCoreStyle; these are the colors to put into the style
	const FSlateColor DefaultForeground( MakeShareable( new FLinearColor( 0.72f, 0.72f, 0.72f, 1.f ) ) );
	const FSlateColor InvertedForeground( MakeShareable( new FLinearColor( 0, 0, 0 ) ) );
	const FSlateColor SelectorColor(  MakeShareable( new FLinearColor( 0.701f, 0.225f, 0.003f ) ) );
	const FSlateColor SelectionColor(  MakeShareable( new FLinearColor( 0.728f, 0.364f, 0.003f ) ) );
	const FSlateColor SelectionColor_Inactive( MakeShareable( new FLinearColor( 0.807f, 0.596f, 0.388f ) ) );
	const FSlateColor SelectionColor_Pressed(  MakeShareable( new FLinearColor( 0.701f, 0.225f, 0.003f ) ) );

	// Dialogs
	{
		Style.Set("UWindows.Standard.Dialog.Background", new BOX_BRUSH("UWindows.Standard.Dialog.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("UWindows.Standard.Dialog.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Roboto-Medium", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);
		Style.Set("UWindows.Standard.Dialog.Title.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 12))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Standard.Dialog.TextStyle.Legal", FTextBlockStyle()
			.SetFont(TTF_FONT("Roboto-Regular", 12))
			.SetColorAndOpacity(FLinearColor::White)
			);

	}


	// Server Browser
	{

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


	}


	{ // Menu Bar Background

		Style.Set("UWindows.Standard.MenuBar.Background", new BOX_BRUSH("UWindows.Standard.MenuBar.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("UWindows.Standard.Backdrop", new BOX_BRUSH("UWindows.Standard.Backdrop", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));


		Style.Set("UWindows.Standard.MenuButton", FButtonStyle()
			.SetNormal ( FSlateNoResource( FVector2D( 128.0f, 128.0f ) ))
			.SetHovered( BOX_BRUSH("UWindows.Standard.MenuButton.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("UWindows.Standard.MenuButton.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.MenuButton.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			);

		Style.Set("UWindows.Standard.MenuList", FButtonStyle()
			.SetNormal ( BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("UWindows.Standard.MenuList.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("UWindows.Standard.MenuList.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			);
	}

	{  // Standard Button

		Style.Set("UWindows.Standard.Button", FButtonStyle()
			.SetNormal ( BOX_BRUSH("UWindows.Standard.Button.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("UWindows.Standard.Button.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("UWindows.Standard.Button.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.Button.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			);

		Style.Set("UWindows.Standard.MainMenuButton.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Bold", 14))
			//.SetFont(TTF_FONT("Roboto-Regular", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.MainMenuButton.SubMenu.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-Medium", 12))
			//.SetFont(TTF_FONT("Roboto-Regular", 12))
			.SetColorAndOpacity(FLinearColor::Blue)
			);

	}

	{ // Background Image
	
		Style.Set("UWindows.Desktop.Background", new IMAGE_BRUSH("UWindows.Desktop.Background", FVector2D(512, 512), FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Both));
		Style.Set("UWindows.Desktop.Background.Logo", new IMAGE_BRUSH("UWindows.Desktop.Background.Logo", FVector2D(3980,1698), FLinearColor(1, 1, 1, 1), ESlateBrushTileType::NoTile));
	}


	{ // MidGame Menu Bar Background

		Style.Set("UWindows.Standard.MidGameMenuButton.TextColor", FLinearColor::White);
		Style.Set("UWindows.Standard.MidGameMenuBar", new BOX_BRUSH("UWindows.Standard.MidGameMenuBar", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

		Style.Set("UWindows.Standard.MidGameMenuButton", FButtonStyle()
			.SetNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetHovered(BOX_BRUSH("UWindows.Standard.MidGameMenuButton.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("UWindows.Standard.MidGameMenuButton.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			);


		Style.Set("UWindows.Standard.MidGameMenuButton.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Exo2-ExtraBold", 18))
			//.SetFont(TTF_FONT("Roboto-Regular", 18))
			.SetColorAndOpacity(FLinearColor::White)
			);
	}

	return StyleRef;
}

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