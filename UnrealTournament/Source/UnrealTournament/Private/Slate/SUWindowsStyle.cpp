// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
const FVector2D Icon36x36(36.0f, 36.0f);
const FVector2D Icon40x40(40.0f, 40.0f);
const FVector2D Icon48x48(48.0f, 48.0f);
const FVector2D Icon54x54(54.0f, 54.0f);
const FVector2D Icon64x64(64.0f, 64.0f);
const FVector2D Icon36x24(36.0f, 24.0f);
const FVector2D Icon17x22(17.0f, 22.0f);
const FVector2D Icon128x128(128.0f, 128.0f);
const FVector2D Icon256x256(256.0f, 256.0f);

FSlateSound SUWindowsStyle::ButtonPressSound;
FSlateSound SUWindowsStyle::ButtonHoverSound;
FSlateColor SUWindowsStyle::DefaultForeground;

TSharedRef<FSlateStyleSet> SUWindowsStyle::Create()
{
	TSharedRef<FSlateStyleSet> StyleRef = MakeShareable(new FSlateStyleSet(SUWindowsStyle::GetStyleSetName()));

	StyleRef->SetContentRoot(FPaths::GameContentDir() / TEXT("RestrictedAssets/Slate"));
	StyleRef->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UWindows.Standard.Font.Small", TTF_FONT("Play-Regular", 10));
	Style.Set("UWindows.Standard.Font.Medium", TTF_FONT("Play-Regular", 14));
	Style.Set("UWindows.Standard.Font.Large", TTF_FONT("Play-Regular", 22));
	

	// These are the Slate colors which reference the dynamic colors in FSlateCoreStyle; these are the colors to put into the style
	DefaultForeground = FSlateColor( MakeShareable( new FLinearColor( 0.72f, 0.72f, 0.72f, 1.f ) ) );

	ButtonHoverSound = FSlateSound::FromName_DEPRECATED(FName("SoundCue'/Game/RestrictedAssets/UI/UT99UI_LittleSelect_Cue.UT99UI_LittleSelect_Cue'"));
	ButtonPressSound = FSlateSound::FromName_DEPRECATED(FName("SoundCue'/Game/RestrictedAssets/UI/UT99UI_BigSelect_Cue.UT99UI_BigSelect_Cue'"));


	Style.Set("UWindows.Standard.Star24", new IMAGE_BRUSH( "Star24x24", FVector2D(24,24), FLinearColor(1.0f, 1.0f, 0.0f, 1.0f) ));

	Style.Set("UWindows.Standard.DarkBackground", new IMAGE_BRUSH( "UWindows.Standard.DarkBackground", FVector2D(32,32), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("UWindows.Logos.Epic_Logo200", new IMAGE_BRUSH( "UWindows.Logos.Epic_Logo200", FVector2D(176,200), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	//Style.Set("OldSchool.AnniLogo", new IMAGE_BRUSH( "OldSchool.AnniLogo", FVector2D(1024,768), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	//Style.Set("OldSchool.Background", new IMAGE_BRUSH( "OldSchool.Background", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("LoadingScreen", new IMAGE_BRUSH( "LoadingScreen", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("UT15.Logo.Overlay", new IMAGE_BRUSH("UT15Overlay", FVector2D(1920, 1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

	//Style.Set("NewSchool.AnniLogo", new IMAGE_BRUSH( "NewSchool.AnniLogo", FVector2D(1141,431), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("NewSchool.Background", new IMAGE_BRUSH( "NewSchool.Background", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	//Style.Set("BadSchool.Background", new IMAGE_BRUSH( "BadSchool.Background", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UWindows.Match.ReadyImage", new IMAGE_BRUSH( "Match/UWindows.Match.ReadyImage", FVector2D(102.0f, 128.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UWindows.Match.HostImage", new IMAGE_BRUSH( "Match/UWindows.Match.HostImage", FVector2D(102.0f, 128.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("Testing.TestPortrait", new IMAGE_BRUSH( "Testing/Testing.TestPortrait", FVector2D(102.0f, 128.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("Testing.TestMapShot", new IMAGE_BRUSH( "Testing/Testing.TestMapShot", FVector2D(400.0f, 213.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.Checkmark", new IMAGE_BRUSH( "CheckMark", FVector2D(64.0f,64.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("UWindows.Lobby.MatchBadge", new IMAGE_BRUSH( "UWindows.Lobby.MatchBadge", FVector2D(256.0f, 256.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("UT.Backgrounds.BK01", new IMAGE_BRUSH( "Backgrounds/UT.Backgrounds.BK01", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	//Style.Set("UT.Backgrounds.BK02", new IMAGE_BRUSH( "Backgrounds/UT.Backgrounds.BK02", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.Backgrounds.Overlay", new IMAGE_BRUSH( "Backgrounds/UT.Backgrounds.Overlay", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));


	Style.Set("UT.Background.Dark", new FSlateColorBrush(FLinearColor(0, 0, 0, .5f)));
	Style.Set("UT.Background.Black", new FSlateColorBrush(FLinearColor(0, 0, 0, 1.0f)));

	Style.Set("UT.TeamBrush.Red", new FSlateColorBrush(FLinearColor(1.0f, 0, 0, 1.0f)));
	Style.Set("UT.TeamBrush.Blue", new FSlateColorBrush(FLinearColor(0.0f, 0, 1.0f, 1.0f)));
	Style.Set("UT.TeamBrush.Spectator", new FSlateColorBrush(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));

	Style.Set("UT.Version.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 14))
		.SetColorAndOpacity(FLinearColor(FColor(33, 93, 220, 255)))
		);

	// Loadout
	{
		Style.Set("UT.Loadout.List.Normal", new IMAGE_BRUSH( "Loadout/UT.Loadout.List.Normal", Icon128x128, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
		Style.Set("UT.Loadout.Tab", new BOX_BRUSH("Loadout/UT.Loadout.Tab", FMargin(2.0f / 85.0f, 0.0f, 58.0f / 85.0f, 0.0f)));
		Style.Set("UT.Loadout.UpperBar", new IMAGE_BRUSH( "Loadout/UT.Loadout.UpperBar", FVector2D(16.0f, 16.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

		Style.Set("UT.Loadout.List.Row", FTableRowStyle()
			.SetEvenRowBackgroundBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("Loadout/UT.Loadout.List.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
			.SetOddRowBackgroundBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("Loadout/UT.Loadout.List.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
			.SetSelectorFocusedBrush(IMAGE_BRUSH("Loadout/UT.Loadout.List.Selected", Icon256x256, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
			.SetActiveBrush(IMAGE_BRUSH("Loadout/UT.Loadout.List.Selected", Icon256x256, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
			.SetActiveHoveredBrush(IMAGE_BRUSH("Loadout/UT.Loadout.List.Selected", Icon256x256, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
			.SetInactiveBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetInactiveHoveredBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetTextColor(FLinearColor::White)
			.SetSelectedTextColor(FLinearColor::Yellow)
			);
	}


	Style.Set("UT.Dialog.Background", new BOX_BRUSH("TopMenu/UT.Dialog.Background", FMargin(10.0f / 256.0f, 64.0f/256.0f, 10.0f / 256.0f, 25.0f/256.0f)));
	Style.Set("UT.DialogBox.Background", new BOX_BRUSH("TopMenu/UT.DialogBox.Background", FMargin(10.0f / 256.0f, 64.0f / 256.0f, 10.0f / 256.0f, 64.0f / 256.0f)));


	Style.Set("UT.Dialog.RightButtonBackground", new BOX_BRUSH("TopMenu/UT.GameMenu.BottomRightTab.Pressed", FMargin(50.0f / 85.0f, 0.0f, 2.0f / 85, 0.0f)));
	Style.Set("UT.Dialog.LeftButtonBackground", new BOX_BRUSH("TopMenu/UT.GameMenu.BottomLeftTab.Pressed", FMargin(2.0f / 85.0f, 0.0f, 50.0f / 85, 0.0f)));


	Style.Set("UT.Dialog.TitleTextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 30))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UT.Dialog.BodyTextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 24))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UT.Dialog.BodyTextTiny", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 10))
		.SetColorAndOpacity(FLinearColor::Gray)
		);

	Style.Set("UT.Hub.DescriptionText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 24))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UT.Hub.RulesTitle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 24))
		.SetColorAndOpacity(FLinearColor::White)
		);


	Style.Set("UT.Hub.RulesText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UT.Hub.RulesText_Small", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 14))
		.SetColorAndOpacity(FLinearColor::Gray)
		);


	Style.Set("UT.Hub.MapsText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 16))
		.SetColorAndOpacity(FLinearColor::White)
		);

	SetCommonStyle(StyleRef);

	Style.Set("UT.ScrollBox.Borderless", FScrollBoxStyle()
		.SetTopShadowBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBottomShadowBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetLeftShadowBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetRightShadowBrush(FSlateNoResource(FVector2D(128.0f, 128.0f)))
	);

	{ // Options Menus

	}

	{ // HUB
	
		Style.Set("UT.HUB.PlayerListText", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Bold", 18))
			.SetColorAndOpacity(FLinearColor(0.7f,0.7f,0.7f,1.0f))
		);
	
		Style.Set("UT.HUB.PlayerList", FTableRowStyle()
			.SetSelectorFocusedBrush(FSlateColorBrush(FColor(38,106,255,255)))
			.SetActiveHoveredBrush(FSlateColorBrush(FColor(34,93,221,255)))
			.SetActiveBrush(FSlateColorBrush(FColor(36,67,134,255)))
			.SetInactiveHoveredBrush(FSlateColorBrush(FColor::Black))
			.SetInactiveBrush(FSlateColorBrush(FColor::Black))
			.SetEvenRowBackgroundBrush(FSlateNoResource(FVector2D(256.0f, 256.0f)))
			.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FColor(32,32,32,255)))
			.SetOddRowBackgroundBrush(FSlateNoResource(FVector2D(256.0f, 256.0f)))
			.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FColor(32,32,32,255)))
			.SetTextColor(FLinearColor::White)
			.SetSelectedTextColor(FLinearColor::Yellow)
		);
	
	}


	{ // Badges

		Style.Set("UT.Icon.Badge0", new IMAGE_BRUSH("Icons/UT.Icon.Badge0", Icon32x32));
		Style.Set("UT.Icon.Badge1", new IMAGE_BRUSH("Icons/UT.Icon.Badge1", Icon32x32));
		Style.Set("UT.Icon.Badge2", new IMAGE_BRUSH("Icons/UT.Icon.Badge2", Icon32x32));

		Style.Set("UT.ELOBadge.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Bold", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);


		Style.Set("UT.Badge.0", new IMAGE_BRUSH("Badges/Badge.0", Icon32x32));
		Style.Set("UT.Badge.1", new IMAGE_BRUSH("Badges/Badge.1", Icon32x32));
		Style.Set("UT.Badge.2", new IMAGE_BRUSH("Badges/Badge.2", Icon32x32));

		Style.Set("UT.Badge.Numbers.1", new IMAGE_BRUSH("Badges/Badge.Numbers.1", Icon32x32));
		Style.Set("UT.Badge.Numbers.2", new IMAGE_BRUSH("Badges/Badge.Numbers.2", Icon32x32));
		Style.Set("UT.Badge.Numbers.3", new IMAGE_BRUSH("Badges/Badge.Numbers.3", Icon32x32));
		Style.Set("UT.Badge.Numbers.4", new IMAGE_BRUSH("Badges/Badge.Numbers.4", Icon32x32));
		Style.Set("UT.Badge.Numbers.5", new IMAGE_BRUSH("Badges/Badge.Numbers.5", Icon32x32));
		Style.Set("UT.Badge.Numbers.6", new IMAGE_BRUSH("Badges/Badge.Numbers.6", Icon32x32));
		Style.Set("UT.Badge.Numbers.7", new IMAGE_BRUSH("Badges/Badge.Numbers.7", Icon32x32));
		Style.Set("UT.Badge.Numbers.8", new IMAGE_BRUSH("Badges/Badge.Numbers.8", Icon32x32));
		Style.Set("UT.Badge.Numbers.9", new IMAGE_BRUSH("Badges/Badge.Numbers.9", Icon32x32));
	}

	{  // Icons
		
		Style.Set("UT.Icon.Home", new IMAGE_BRUSH("Icons/UT.Icon.Home", Icon48x48));
		Style.Set("UT.Icon.Online", new IMAGE_BRUSH("Icons/UT.Icon.Online", Icon48x48));
		Style.Set("UT.Icon.SocialBang", new IMAGE_BRUSH("Icons/UT.Icon.SocialBang", Icon12x12));
		Style.Set("UT.Icon.Settings", new IMAGE_BRUSH("Icons/UT.Icon.Settings", Icon48x48));
		Style.Set("UT.Icon.Exit", new IMAGE_BRUSH("Icons/UT.Icon.Exit", Icon48x48));
		Style.Set("UT.Icon.Stats", new IMAGE_BRUSH("Icons/UT.Icon.Stats", Icon48x48));
		Style.Set("UT.Icon.About", new IMAGE_BRUSH("Icons/UT.Icon.About", Icon48x48));
		Style.Set("UT.Icon.SignOut", new IMAGE_BRUSH("Icons/UT.Icon.SignOut", Icon48x48));
		Style.Set("UT.Icon.SignIn", new IMAGE_BRUSH("Icons/UT.Icon.SignIn", Icon48x48));
		Style.Set("UT.Icon.Chat36", new IMAGE_BRUSH("Icons/UT.Icon.Chat36", Icon36x36));
		Style.Set("UT.Icon.Browser", new IMAGE_BRUSH("Icons/UT.Icon.Browser", Icon48x48));

		Style.Set("UT.Icon.UpArrow", new IMAGE_BRUSH("Icons/UT.Icon.UpArrow", Icon48x48));
		Style.Set("UT.Icon.DownArrow", new IMAGE_BRUSH("Icons/UT.Icon.DownArrow", Icon48x48));
		
	}

	SetTopMenuStyle(StyleRef);
	

	{
		// Chat Bar

		Style.Set("UT.ChatBar.Bar", new BOX_BRUSH("ChatBar/UT.ChatBar.Bar", FMargin(4.0f / 64.0f, 2.0f/42.0f, 20.0f / 64.0f, 4.0f/42.0f)));
		Style.Set("UT.ChatBar.EntryArea", new BOX_BRUSH("ChatBar/UT.ChatBar.EntryArea", FMargin(4.0f / 64.0f, 2.0f/42.0f, 4.0f / 64.0f, 2.0f/42.0f)));
		Style.Set("UT.ChatBar.Fill", new BOX_BRUSH("ChatBar/UT.ChatBar.Fill", FMargin(4.0f / 64.0f, 2.0f/42.0f, 4.0f / 64.0f, 4.0f/42.0f)));

		Style.Set("UT.ChatBar.Button.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Regular", 26))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UT.ChatBar.Editbox.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Regular", 26))
			.SetColorAndOpacity(FLinearColor::Black)
			);


		Style.Set("UT.ChatBar.Editbox.ChatEditBox", FEditableTextBoxStyle()
			.SetFont(TTF_FONT("Play-Regular", 26))
			.SetForegroundColor(FLinearColor::Black)
			.SetBackgroundImageNormal( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageHovered( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageFocused( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			.SetBackgroundImageReadOnly( FSlateNoResource(FVector2D(128.0f, 128.0f)))
			);

			Style.Set("UT.Vertical.Playerlist.Background", new BOX_BRUSH("/ChatBar/UT.Vertical.Playerlist.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
	}

	{	// Rules and mapsets
	
		Style.Set("UT.ComplexButton", FButtonStyle()
			.SetNormal( FSlateColorBrush(FColor(72,72,72,255) ) )
			.SetHovered( FSlateColorBrush(FColor(178,191,254 ) ) )
			.SetPressed( FSlateColorBrush(FColor(254,243,178 ) ) )
			.SetDisabled( FSlateColorBrush(FLinearColor::Black) )
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

	Style.Set("UT.Background.Dark", new FSlateColorBrush(FLinearColor(0, 0, 0, .5f)));

	}


	{
		// Context Menus

		Style.Set("UT.ContextMenu.Button", FButtonStyle()
			.SetNormal ( BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered( BOX_BRUSH("UWindows.Standard.MenuList.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed( BOX_BRUSH("UWindows.Standard.MenuList.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHoveredSound(ButtonHoverSound)
			.SetPressedSound(ButtonPressSound)
			);

		Style.Set("UT.ContextMenu.Button.Empty", FButtonStyle()
			.SetNormal(BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetHovered(BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetPressed(BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			);


		Style.Set("UT.ContextMenu.Button.Spacer", FButtonStyle()
			.SetNormal(BOX_BRUSH("TopMenu/UT.MenuList.Spacer", FMargin(4.0f / 64.0f, 2.0f / 16.0f, 4.0f / 64.0f, 2.0f / 16.0f)))
			.SetHovered(BOX_BRUSH("TopMenu/UT.MenuList.Spacer", FMargin(4.0f / 64.0f, 2.0f / 16.0f, 4.0f / 64.0f, 2.0f / 16.0f)))
			.SetPressed(BOX_BRUSH("TopMenu/UT.MenuList.Spacer", FMargin(4.0f / 64.0f, 2.0f / 16.0f, 4.0f / 64.0f, 2.0f / 16.0f)))
			.SetDisabled(BOX_BRUSH("UWindows.Standard.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
			);

		Style.Set("UT.ContextMenu.Background", new IMAGE_BRUSH("UWindows.Standard.MenuList.Normal", FVector2D(1, 1)) );
		Style.Set("UT.ContextMenu.Spacer", new BOX_BRUSH("TopMenu/UT.MenuList.Spacer", FMargin(8.0f / 64.0f, 2.0f / 16.0f, 8.0f / 64.0f, 2.0f / 16.0f)));

		Style.Set("UT.ContextMenu.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Bold", 20))
			.SetColorAndOpacity(FLinearColor::White)
			);
	
	}

	{	// Toasts

		Style.Set("UT.Toast.Background", new BOX_BRUSH("Toasts/UT.Toast.Background", FMargin(16.0f / 370.0f, 16.0f / 110.0f, 16.0f / 370.0f, 16.0f / 110.0f)));
		Style.Set("UT.Toast.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Bold", 34))
			.SetColorAndOpacity(FLinearColor(FColor(33, 93, 220, 255)))
			);
	
	}

	SetLoginStyle(StyleRef);
	
	{ // Server Browser
	
		Style.Set("UT.Icon.Epic", new IMAGE_BRUSH("Icons/UT.Icon.Epic", Icon54x54));
		Style.Set("UT.Icon.Raxxy", new IMAGE_BRUSH("Icons/UT.Icon.Raxxy", Icon54x54));
		Style.Set("UT.Icon.Globe", new IMAGE_BRUSH("Icons/UT.Icon.Globe", Icon54x54));
	}

	//Replays
	{
		Style.Set("UT.Replay.Button.Play", new IMAGE_BRUSH("Icons/UT.Icon.Replay.Play", Icon48x48));
		Style.Set("UT.Replay.Button.Pause", new IMAGE_BRUSH("Icons/UT.Icon.Replay.Pause", Icon48x48));
		Style.Set("UT.Replay.Button.Record", new IMAGE_BRUSH("Icons/UT.Icon.Replay.Record", Icon24x24));
		Style.Set("UT.Replay.Button.MarkStart", new IMAGE_BRUSH("Icons/UT.Icon.Replay.MarkStart", Icon24x24));
		Style.Set("UT.Replay.Button.MarkEnd", new IMAGE_BRUSH("Icons/UT.Icon.Replay.MarkEnd", Icon24x24));

		Style.Set("UT.Replay.Tooltip.BG", new IMAGE_BRUSH("Replay/UT.Replay.Tooltip.BG", Icon64x64));
		Style.Set("UT.Replay.Tooltip.Arrow", new IMAGE_BRUSH("Replay/UT.Replay.Tooltip.Arrow", Icon16x16));
	}

	// ------------------------------------- OLD STUFF !!!!!!!



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
			.SetFont(TTF_FONT("Play-Bold", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Style.Set("UWindows.Standard.MainMenuButton.SubMenu.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Regular", 12))
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
		.SetFont(TTF_FONT("Play-Bold", 12))
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
		.SetNormal(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHovered(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetPressed(FSlateNoResource(FVector2D(256.0f, 256.0f)))
//		.SetNormal(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Normal", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
//		.SetHovered(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Hovered", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
//		.SetPressed(BOX_BRUSH("UWindows.Lobby.MatchBar.Button.Pressed", FMargin(12.0f / 64.0f, 8.0f / 32.0f, 5.0f / 64.0f, 8.0f / 32.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);



	Style.Set("UWindows.Standard.SmallButton.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 10))
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
			.SetFont(TTF_FONT("Play-Bold", 12))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

		Style.Set("UWindows.Lobby.MatchButton.Action.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Bold", 12))
			.SetColorAndOpacity(FLinearColor::Yellow)
			);

	}

	SetDialogStyle(StyleRef);
	
	// Toasts
	{
		Style.Set("UWindows.Standard.Toast.Background", new BOX_BRUSH("UWindows.Standard.Toast.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("UWindows.Standard.Toast.TextStyle", FTextBlockStyle()
			.SetFont(TTF_FONT("Play-Bold", 12))
			.SetColorAndOpacity(FLinearColor(0,0.05,0.59))
			);
	}

	Style.Set("UWindows.Standard.UpTick", new IMAGE_BRUSH("ServerBrowser/SortUpArrow", Icon8x4));
	Style.Set("UWindows.Standard.DownTick", new IMAGE_BRUSH("ServerBrowser/SortDownArrow", Icon8x4));

	Style.Set("UWindows.Standard.NormalText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.LargeText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.LargeEditableText", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("Play-Regular", 14))
		);

	Style.Set("UWindows.Standard.ExtraLargeText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 16))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.BoldText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 14))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

	Style.Set("UWindows.Standard.SmallText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 10))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.ContentText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 10))
		.SetColorAndOpacity(FLinearColor::Blue)
		);

	{
		Style.Set("UWindows.Standard.StatsViewer.Backdrop", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Backdrop", FMargin(8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f)));
	}

	SetServerBrowserStyle(StyleRef);
	
	{ // Menu Bar Background

		Style.Set("UWindows.Standard.MenuBar.Background", new BOX_BRUSH("UWindows.Standard.MenuBar.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("UWindows.Standard.Backdrop", new BOX_BRUSH("UWindows.Standard.Backdrop", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		Style.Set("UWindows.Standard.Backdrop.Highlight", new BOX_BRUSH("UWindows.Standard.Backdrop.Highlight", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));
		

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
	
		//Style.Set("UWindows.Desktop.Background", new IMAGE_BRUSH("UWindows.Desktop.Background", FVector2D(512, 512), FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Both));
		//Style.Set("UWindows.Desktop.Background.Logo", new IMAGE_BRUSH("UWindows.Desktop.Background.Logo", FVector2D(3980,1698), FLinearColor(1, 1, 1, 1), ESlateBrushTileType::NoTile));
	}

	SetMidGameMenuRedStyle(StyleRef);

	SetMidGameMenuBlueStyle(StyleRef);

	SetMOTDServerChatStyle(StyleRef);
	
	SetTextChatStyle(StyleRef);

	SetFriendsChatStyle(StyleRef);
	SetMatchBadgeStyle(StyleRef);
	SetMOTDStyle(StyleRef);

	return StyleRef;
}

void SUWindowsStyle::SetDialogStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

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
		.SetFont(TTF_FONT("Play-Bold", 10))
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
	Style.Set("UWindows.ComboBox.TickMark", new IMAGE_BRUSH("UWindows.ComboBox.TickMark", Icon8x4));

	FComboButtonStyle ComboButton = FComboButtonStyle()
		.SetButtonStyle(Style.GetWidgetStyle<FButtonStyle>("UWindows.Standard.Button"))
		.SetDownArrowImage(IMAGE_BRUSH("UWindows.ComboBox.TickMark", Icon8x4))
		.SetMenuBorderBrush(BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f)))
		.SetMenuBorderPadding(FMargin(5.0f, 0.05, 5.0f, 0.0f));
	Style.Set("UWindows.Standard.ComboButton", ComboButton);

	Style.Set("UWindows.Standard.ComboBox", FComboBoxStyle()
		.SetComboButtonStyle(ComboButton)
		);
}

void SUWindowsStyle::SetCommonStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UT.Common.NormalText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 20))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UT.Common.NormalText.Black", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 20))
		.SetColorAndOpacity(FLinearColor::Black)
		);

	Style.Set("UT.Common.ButtonText.White", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UT.Common.ButtonText.Black", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetColorAndOpacity(FLinearColor::Black)
		);

	Style.Set("", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 16))
		.SetColorAndOpacity(FLinearColor::Black)
		);

	Style.Set("UT.Common.ToolTipFont", TTF_FONT("Play-Bold", 16));



	Style.Set("UT.Common.ButtonText.Black", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetColorAndOpacity(FLinearColor::Black)
		);




	Style.Set("UT.Common.ActiveText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 20))
		.SetColorAndOpacity(FLinearColor(FColor(0, 255, 0, 255)))
		);

	Style.Set("UT.Common.Exo2.20", TTF_FONT("Play-Regular", 18));

	Style.Set("UT.Common.BoldText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 22))
		.SetColorAndOpacity(FLinearColor(FColor(93, 160, 220, 255)))
		);

	Style.Set("UT.Option.ColumnHeaders", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetColorAndOpacity(FLinearColor(FColor(180, 180, 180, 255)))
		);


	Style.Set("UT.Common.Editbox", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetForegroundColor(FLinearColor::White)
		.SetBackgroundImageNormal(BOX_BRUSH("UTCommon/UT.Editbox.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageHovered(BOX_BRUSH("UTCommon/UT.Editbox.Hovered", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageFocused(BOX_BRUSH("UTCommon/UT.Editbox.Focused", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageReadOnly(BOX_BRUSH("UTCommon/UT.Editbox.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		);


	Style.Set("UT.Common.Editbox.White", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetForegroundColor(FLinearColor::Black)
		.SetPadding(FMargin(10.0f, 12.0f, 10.0f, 10.0f))
		.SetBackgroundImageNormal(BOX_BRUSH("UTCommon/UT.Editbox.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageHovered(BOX_BRUSH("UTCommon/UT.Editbox.White.Hovered", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageFocused(BOX_BRUSH("UTCommon/UT.Editbox.White.Focused", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageReadOnly(BOX_BRUSH("UTCommon/UT.Editbox.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		);

	Style.Set("UT.Common.NumEditbox.White", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetForegroundColor(FLinearColor::Black)
		.SetPadding(FMargin(10.0f, 10.0f, 10.0f, 8.0f))
		.SetBackgroundImageNormal(BOX_BRUSH("UTCommon/UT.Editbox.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageHovered(BOX_BRUSH("UTCommon/UT.Editbox.White.Hovered", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageFocused(BOX_BRUSH("UTCommon/UT.Editbox.White.Focused", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageReadOnly(BOX_BRUSH("UTCommon/UT.Editbox.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		);


	Style.Set("UT.Common.Editbox.Dark", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetForegroundColor(FLinearColor::Black)
		.SetBackgroundImageNormal(BOX_BRUSH("UTCommon/UT.Editbox.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageHovered(BOX_BRUSH("UTCommon/UT.Editbox.White.Hovered", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageFocused(BOX_BRUSH("UTCommon/UT.Editbox.White.Focused", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetBackgroundImageReadOnly(BOX_BRUSH("UTCommon/UT.Editbox.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		);


	Style.Set("UT.Common.CheckBox", FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
		.SetUncheckedImage(IMAGE_BRUSH("UTCommon/UT.CheckBox.UnChecked.Normal", Icon32x32))
		.SetUncheckedHoveredImage(IMAGE_BRUSH("UTCommon/UT.CheckBox.UnChecked.Hovered", Icon32x32))
		.SetUncheckedPressedImage(IMAGE_BRUSH("UTCommon/UT.CheckBox.UnChecked.Pressed", Icon32x32))
		.SetCheckedImage(IMAGE_BRUSH("UTCommon/UT.CheckBox.Checked.Normal", Icon32x32))
		.SetCheckedHoveredImage(IMAGE_BRUSH("UTCommon/UT.CheckBox.Checked.Normal", Icon32x32))
		.SetCheckedPressedImage(IMAGE_BRUSH("UTCommon/UT.CheckBox.Checked.Normal", Icon32x32))
		.SetUndeterminedImage(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetUndeterminedHoveredImage(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetUndeterminedPressedImage(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		);

	Style.Set("UT.Common.Slider", FSliderStyle()
		.SetNormalBarImage(FSlateColorBrush(FColor::White))
		.SetDisabledBarImage(FSlateColorBrush(FLinearColor::Gray))
		.SetNormalThumbImage(IMAGE_BRUSH("UTCommon/UT.SliderHandle.Normal", Icon32x32))
		.SetDisabledThumbImage(IMAGE_BRUSH("UTCommon/UT.SliderHandle.Disabled", Icon32x32))
		);

	Style.Set("UT.Button.White", FButtonStyle()
		.SetNormal(BOX_BRUSH("UTCommon/UT.Button.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetHovered(BOX_BRUSH("UTCommon/UT.Button.White.Hovered", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetPressed(BOX_BRUSH("UTCommon/UT.Button.White.Pressed", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetDisabled(BOX_BRUSH("UTCommon/UT.Editbox.White.Normal", FMargin(8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f, 8.0f / 64.0f)))
		.SetNormalPadding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
		.SetPressedPadding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	FComboButtonStyle UTComboButton = FComboButtonStyle()
		.SetButtonStyle(Style.GetWidgetStyle<FButtonStyle>("UT.Button.White"))
		.SetDownArrowImage(IMAGE_BRUSH("UTCommon/UT.ComboTick", Icon16x16))
		.SetMenuBorderBrush(BOX_BRUSH("UWindows.Standard.MenuList.Normal", FMargin(8.0f / 64.0f)))
		.SetMenuBorderPadding(FMargin(5.0f, 5.0, 5.0f, 5.0f));
	Style.Set("UWindows.Standard.ComboButton", UTComboButton);

	Style.Set("UT.ComboBox", FComboBoxStyle()
		.SetComboButtonStyle(UTComboButton)
		);
}


void SUWindowsStyle::SetLoginStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UT.Login.Dialog.Background", new BOX_BRUSH("Login/UT.Login.Dialog.Background", FMargin(8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f)));
	Style.Set("UT.Login.EpicLogo", new IMAGE_BRUSH("Login/UT.Login.EpicLogo", FVector2D(110, 126), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.Set("UT.Login.Editbox.Background", new BOX_BRUSH("Login/UT.Login.Editbox.Normal", FMargin(4.0f / 338.0f, 4.0f / 62.0f, 4.0f / 338.0f, 4.0f / 62.0f)));

	Style.Set("UT.Login.Editbox", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("Rajdhani-SemiBold", 20))
		.SetForegroundColor(FLinearColor::Black)
		.SetBackgroundImageNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageHovered(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageFocused(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageReadOnly(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		);

	Style.Set("UT.Login.Error.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Rajdhani-SemiBold", 16))
		.SetColorAndOpacity(FLinearColor(FColor(126, 7, 13, 255))));



	Style.Set("UT.Login.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Rajdhani-SemiBold", 20))
		.SetColorAndOpacity(FLinearColor(FColor(78, 78, 78, 255))));

	Style.Set("UT.Login.Label.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Rajdhani-SemiBold", 20))
		.SetColorAndOpacity(FLinearColor(FColor(148, 148, 148, 255))));

	Style.Set("UT.Login.Button", FButtonStyle()
		.SetNormal(BOX_BRUSH("Login/UT.Login.Button.Normal", FMargin(4.0f / 298.0f, 4.0f / 44.0f, 4.0f / 298.0f, 4.0f / 44.0f)))
		.SetHovered(BOX_BRUSH("Login/UT.Login.Button.Hovered", FMargin(4.0f / 298.0f, 4.0f / 44.0f, 4.0f / 298.0f, 4.0f / 44.0f)))
		.SetPressed(BOX_BRUSH("Login/UT.Login.Button.Pressed", FMargin(4.0f / 298.0f, 4.0f / 44.0f, 4.0f / 298.0f, 4.0f / 44.0f)))
		.SetDisabled(BOX_BRUSH("Login/UT.Login.Button.Normal", FMargin(4.0f / 298.0f, 4.0f / 44.0f, 4.0f / 298.0f, 4.0f / 44.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UT.Login.Button.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Rajdhani-SemiBold", 20))
		.SetColorAndOpacity(FLinearColor::White));

	Style.Set("UT.Login.EmptyButton", FButtonStyle()
		.SetNormal(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHovered(BOX_BRUSH("Login/UT.Login.EmptyButton.Hovered", FMargin(4.0f / 298.0f, 4.0f / 44.0f, 4.0f / 298.0f, 4.0f / 44.0f)))
		.SetPressed(BOX_BRUSH("Login/UT.Login.EmptyButton.Hovered", FMargin(4.0f / 298.0f, 4.0f / 44.0f, 4.0f / 298.0f, 4.0f / 44.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UT.Login.EmptyButton.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Rajdhani-SemiBold", 20))
		.SetColorAndOpacity(FLinearColor(FColor(33, 93, 220, 255))));

}

void SUWindowsStyle::SetTopMenuStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	// Top Menu
	
	Style.Set("UT.TopMenu.Bar", new BOX_BRUSH("TopMenu/UT.TopMenu.Bar", FMargin(8.0f / 32.0f, 1.0f / 8.0f, 8.0f / 32.0f, 1.0f / 8.0f)));
	Style.Set("UT.TopMenu.TileBar", new BOX_BRUSH("TopMenu/UT.TopMenu.TileBar", FMargin(1.0f / 32.0f, 0.0f, 1.0f / 32.0f, 0.0f)));
	Style.Set("UT.TopMenu.LightFill", new IMAGE_BRUSH("TopMenu/UT.TopMenu.LightFill", FVector2D(256, 256), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.Set("UT.TopMenu.DarkFill", new IMAGE_BRUSH("TopMenu/UT.TopMenu.DarkFill", FVector2D(256, 256), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.Set("UT.TopMenu.Shadow", new IMAGE_BRUSH("TopMenu/UT.TopMenu.Shadow", FVector2D(64, 64), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.Set("UT.TopMenu.MidFill", new IMAGE_BRUSH("TopMenu/UT.GameMenu.MidFill", FVector2D(256, 256), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

	Style.Set("UT.TopMenu.Button", FButtonStyle()
		.SetNormal(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHovered(BOX_BRUSH("TopMenu/UT.TopMenu.Button.Hovered", FMargin(2.0f / 32.0f, 2.0f / 56.f, 2.0f / 32.0f, 4.0f / 56.f)))
		.SetPressed(BOX_BRUSH("TopMenu/UT.TopMenu.Button.Pressed", FMargin(2.0f / 32.0f, 2.0f / 56.f, 2.0f / 32.0f, 4.0f / 56.f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UT.BottomMenu.Button", FButtonStyle()
		.SetNormal(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHovered(BOX_BRUSH("TopMenu/UT.BottomMenu.Button.Hovered", FMargin(2.0f / 32.0f, 2.0f / 56.f, 2.0f / 32.0f, 4.0f / 56.f)))
		.SetPressed(BOX_BRUSH("TopMenu/UT.BottomMenu.Button.Pressed", FMargin(2.0f / 32.0f, 2.0f / 56.f, 2.0f / 32.0f, 4.0f / 56.f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);


	Style.Set("UT.TopMenu.TabButton", FButtonStyle()
		.SetNormal(IMAGE_BRUSH("TopMenu/UT.TopMenu.LightFill", FVector2D(256, 256), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetHovered(IMAGE_BRUSH("TopMenu/UT.TopMenu.LightFill", FVector2D(256, 256), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetPressed(BOX_BRUSH("TopMenu/UT.TopMenu.TabButton.Pressed", FMargin(10.0f / 85.0f, 0.0f, 55.0f / 85.0f, 0.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UT.TopMenu.SimpleTabButton", FButtonStyle()
		.SetNormal(IMAGE_BRUSH("TopMenu/UT.TopMenu.LightFill", FVector2D(256, 256), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetHovered(IMAGE_BRUSH("TopMenu/UT.TopMenu.Button.Hovered", FVector2D(256, 256), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetPressed(BOX_BRUSH("TopMenu/UT.TopMenu.Button.Pressed", FMargin(52.0f / 256.0f, 0.0f, 52.0f / 256.0f, 0.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UT.TopMenu.OptionTabButton", FButtonStyle()
		.SetNormal(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHovered(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetPressed(BOX_BRUSH("TopMenu/UT.GameMenu.TabButton.Pressed", FMargin(10.0f / 85.0f, 0.0f, 55.0f / 85.0f, 0.0f)))
		.SetDisabled(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);


	Style.Set("UT.TopMenu.Button.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 30))
		.SetColorAndOpacity(FLinearColor::White));

	Style.Set("UT.TopMenu.Button.SmallTextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 22))
		.SetColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
		);

	Style.Set("UT.TopMenu.Button.SmallTextStyle.Selected", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 22))
		.SetColorAndOpacity(FLinearColor::White)
		);
}

void SUWindowsStyle::SetServerBrowserStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	// Server Browser
	Style.Set("UWindows.Standard.ServerBrowser.LeftArrow", new IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.LeftArrow", FVector2D(12, 8), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

	Style.Set("UWindows.Standard.ServerBrowser.Backdrop", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Backdrop", FMargin(8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f, 8.0f / 256.0f)));
	Style.Set("UWindows.Standard.ServerBrowser.TopSubBar", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.TopSubBar", FMargin(28.0f / 128.0f, 32.0f / 64.0f, 28.0f / 128.0f, 32.0f / 64.0f)));
	Style.Set("UWindows.Standard.ServerBrowser.BottomSubBar", new BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BottomSubBar", FMargin(20.0f / 96.0f, 24.0f / 48.0f, 18.0f / 96.0f, 24.0f / 48.0f)));

	Style.Set("UWindows.Standard.ServerBrowser.NormalText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.ServerBrowser.BoldText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 14))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

	Style.Set("UWindows.Standard.ServerBrowser.HugeText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 48))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);


	Style.Set("UWindows.Standard.ServerBrowser.Lock", new IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Lock", Icon17x22, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.Set("UWindows.Standard.ServerBrowser.Lan", new IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Lan", Icon17x22, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

	Style.Set("UWindows.Standard.ServerBrowser.Row", FTableRowStyle()
		.SetEvenRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
		.SetOddRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
		.SetSelectorFocusedBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetInactiveBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetInactiveHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetTextColor(FLinearColor::White)
		.SetSelectedTextColor(FLinearColor::Black)
		);


	const FTableColumnHeaderStyle TableColumnHeaderStyle = FTableColumnHeaderStyle()
		.SetSortPrimaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrow", Icon8x4))
		.SetSortPrimaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrow", Icon8x4))
		.SetSortSecondaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrows", Icon16x4))
		.SetSortSecondaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrows", Icon16x4))
		.SetNormalBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f / 32.f))
		.SetHoveredBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f / 32.f))
		.SetMenuDropdownImage(IMAGE_BRUSH("ServerBrowser/ColumnHeader_Arrow", Icon8x8))
		.SetMenuDropdownNormalBorderBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f / 32.f))
		.SetMenuDropdownHoveredBorderBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f / 32.f));
	Style.Set("UWindows.Standard.ServerBrowser.Header.Column", TableColumnHeaderStyle);

	const FTableColumnHeaderStyle TableLastColumnHeaderStyle = FTableColumnHeaderStyle()
		.SetSortPrimaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrow", Icon8x4))
		.SetSortPrimaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrow", Icon8x4))
		.SetSortSecondaryAscendingImage(IMAGE_BRUSH("ServerBrowser/SortUpArrows", Icon16x4))
		.SetSortSecondaryDescendingImage(IMAGE_BRUSH("ServerBrowser/SortDownArrows", Icon16x4))
		.SetNormalBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f / 32.f))
		.SetHoveredBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f / 32.f))
		.SetMenuDropdownImage(IMAGE_BRUSH("ServerBrowser/ColumnHeader_Arrow", Icon8x8))
		.SetMenuDropdownNormalBorderBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton", 4.f / 32.f))
		.SetMenuDropdownHoveredBorderBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.MenuButton.Hovered", 4.f / 32.f));

	const FSplitterStyle TableHeaderSplitterStyle = FSplitterStyle()
		.SetHandleNormalBrush(FSlateNoResource())
		.SetHandleHighlightBrush(IMAGE_BRUSH("ServerBrowser/HeaderSplitterGrip", Icon8x8));

	Style.Set("UWindows.Standard.ServerBrowser.Header", FHeaderRowStyle()
		.SetColumnStyle(TableColumnHeaderStyle)
		.SetLastColumnStyle(TableLastColumnHeaderStyle)
		.SetColumnSplitterStyle(TableHeaderSplitterStyle)
		.SetBackgroundBrush(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.HeaderBar", 4.f / 32.f))
		.SetForegroundColor(DefaultForeground)
		);

	Style.Set("UWindows.Standard.ServerBrowser.Header.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 10))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.ServerBrowser.Button", FButtonStyle()
		.SetNormal(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetHovered(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button.Hovered", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button.Pressed", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.Button.Disabled", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UWindows.Standard.ServerBrowser.RightButton", FButtonStyle()
		.SetNormal(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetHovered(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton.Hovered", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton.Pressed", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RightButton.Disabled", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);


	Style.Set("UWindows.Standard.ServerBrowser.BlankButton", FButtonStyle()
		.SetNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetHovered(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BlankButton.Hovered", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BlankButton.Pressed", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.BlankButton.Disabled", FMargin(7.0f / 64.0f, 4.0f / 32.0f, 7.0f / 64.0f, 4.0f / 32.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);


	Style.Set("UWindows.Standard.ServerBrowser.EditBox", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("Play-Bold", 12))
		.SetForegroundColor(FLinearColor::White)
		.SetBackgroundImageNormal(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
		.SetBackgroundImageHovered(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
		.SetBackgroundImageFocused(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
		.SetBackgroundImageReadOnly(BOX_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.DarkBorder", FMargin(22.0f / 64.0f, 8.0f / 32.0f, 6.0f / 64.0f, 8.0f / 32.0f)))
		);



	Style.Set("UWindows.Standard.HUBBrowser.TitleText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 18))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.HUBBrowser.TitleText.Selected", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 18))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

	Style.Set("UWindows.Standard.HUBBrowser.NormalText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.HUBBrowser.SmallText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);


	Style.Set("UWindows.Standard.HUBBrowser.Row", FTableRowStyle()
		.SetEvenRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetEvenRowBackgroundHoveredBrush(BORDER_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", FMargin(0.5, 0.5, 0.5, 0.5)))
		.SetOddRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetOddRowBackgroundHoveredBrush(BORDER_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", FMargin(0.5, 0.5, 0.5, 0.5)))
		.SetSelectorFocusedBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetInactiveBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetInactiveHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetTextColor(FLinearColor::White)
		.SetSelectedTextColor(FLinearColor::Black)
		);

}

void SUWindowsStyle::SetMidGameMenuRedStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	// MidGame Menu Bar Background - RED

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
		.SetFont(TTF_FONT("Play-Bold", 14))
		.SetForegroundColor(FLinearColor::White)
		.SetBackgroundImageNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageHovered(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageFocused(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageReadOnly(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		);

	Style.Set("Red.UWindows.MidGameMenu.Chatbar.Background", new BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.Chatbar.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));


	Style.Set("Red.UWindows.MidGameMenu.Button.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("Red.UWindows.MidGameMenu.Button.SubMenu.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("Red.UWindows.MidGameMenu.Button.SubMenu.TextStyle.AltTeam", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 12))
		.SetColorAndOpacity(FLinearColor(0.38, 0.38, 1.0, 1.0))
		);

	Style.Set("Red.UWindows.MidGameMenu.MenuList", FButtonStyle()
		.SetNormal(BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetHovered(BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetDisabled(BOX_BRUSH("Team0/Red.UWindows.MidGameMenu.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("Red.UWindows.MidGameMenu.UserList.Background", new BOX_BRUSH("/Team0/Red.UWindows.MidGameMenu.UserList.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

	Style.Set("Red.UWindows.MidGameMenu.UserList.Row", FTableRowStyle()
		.SetEvenRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
		.SetOddRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
		.SetSelectorFocusedBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetTextColor(FLinearColor::White)
		.SetSelectedTextColor(FLinearColor::Black)
		);

	Style.Set("Red.UWindows.MidGameMenu.Status.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);
}

void SUWindowsStyle::SetMidGameMenuBlueStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	// MidGame Menu Bar Background - BLUE

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
		.SetFont(TTF_FONT("Play-Bold", 14))
		.SetForegroundColor(FLinearColor::White)
		.SetBackgroundImageNormal(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageHovered(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageFocused(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageReadOnly(FSlateNoResource(FVector2D(128.0f, 128.0f)))
		);

	Style.Set("Blue.UWindows.MidGameMenu.Chatbar.Background", new BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.Chatbar.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));


	Style.Set("Blue.UWindows.MidGameMenu.Button.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("Blue.UWindows.MidGameMenu.Button.SubMenu.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("Blue.UWindows.MidGameMenu.Button.SubMenu.TextStyle.AltTeam", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 12))
		.SetColorAndOpacity(FLinearColor(1.0, 0.38, 0.38, 1.0))
		);


	Style.Set("Blue.UWindows.MidGameMenu.MenuList", FButtonStyle()
		.SetNormal(BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Normal", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetHovered(BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Hovered", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetPressed(BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Pressed", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetDisabled(BOX_BRUSH("Team1/Blue.UWindows.MidGameMenu.MenuList.Disabled", FMargin(8.0f / 64.0f, 8.0f / 32.0f, 8.0f / 64.0f, 8.0f / 32.0f)))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);


	Style.Set("Blue.UWindows.MidGameMenu.UserList.Background", new BOX_BRUSH("/Team1/Blue.UWindows.MidGameMenu.UserList.Background", FMargin(8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f, 8.0f / 32.0f)));

	Style.Set("Blue.UWindows.MidGameMenu.UserList.Row", FTableRowStyle()
		.SetEvenRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Even", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
		.SetOddRowBackgroundBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Odd.Hovered", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), ESlateBrushTileType::Horizontal))
		.SetSelectorFocusedBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetActiveHoveredBrush(IMAGE_BRUSH("ServerBrowser/UWindows.Standard.ServerBrowser.RowBrush.Selector", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)))
		.SetTextColor(FLinearColor::White)
		.SetSelectedTextColor(FLinearColor::Black)
		);


	Style.Set("Blue.UWindows.MidGameMenu.Status.TextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 12))
		.SetColorAndOpacity(FLinearColor::White)
		);
}

void SUWindowsStyle::SetMOTDServerChatStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	// MOTD / Server / Chat

	Style.Set("UWindows.Standard.MOTD.ServerTitle", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 22))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

	Style.Set("UWindows.Standard.MOTD.GeneralText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.MOTD.RulesText", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 10))
		.SetColorAndOpacity(FLinearColor::Gray)
		);

	Style.Set("UWindows.Standard.Chat", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 10))
		.SetColorAndOpacity(FLinearColor::White)
		);
}

void SUWindowsStyle::SetTextChatStyle(TSharedRef<FSlateStyleSet> StyleRef)
{	// Text Chat styles

	FSlateStyleSet& Style = StyleRef.Get();

	const FTextBlockStyle NormalChatStyle = FTextBlockStyle().SetFont(TTF_FONT("Play-Regular", 12)).SetColorAndOpacity(FLinearColor::White);

	Style.Set("UWindows.Chat.Text.Global", FTextBlockStyle(NormalChatStyle));
	Style.Set("UWindows.Chat.Text.Lobby", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::White));
	Style.Set("UWindows.Chat.Text.Match", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Yellow));
	Style.Set("UWindows.Chat.Text.Friends", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Green));
	Style.Set("UWindows.Chat.Text.Team", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Yellow));
	Style.Set("UWindows.Chat.Text.Team.Red", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor(1.0f, 0.25f, 0.25f, 1.0f)));
	Style.Set("UWindows.Chat.Text.Team.Blue", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor(0.25, 0.25, 1.0, 1.0)));
	Style.Set("UWindows.Chat.Text.Local", FTextBlockStyle(NormalChatStyle));
	Style.Set("UWindows.Chat.Text.Admin", FTextBlockStyle(NormalChatStyle).SetColorAndOpacity(FLinearColor::Yellow));
}

void SUWindowsStyle::SetFriendsChatStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	// Friends chat styles
	//const FSlateFontInfo RobotoRegular10    = TTF_FONT("Social-WIP/Font/Lato-Regular", 10);
	//const FSlateFontInfo RobotoRegular12    = TTF_FONT("Social-WIP/Font/Lato-Regular", 12);
	//const FSlateFontInfo RobotoBold10       = TTF_FONT("Social-WIP/Font/Lato-Bold", 10 );
	//const FSlateFontInfo RobotoBold12       = TTF_FONT("Social-WIP/Font/Lato-Bold", 12);

	const FSlateFontInfo ExoRegularSmall = TTF_FONT("Play-Regular", 10);
	const FSlateFontInfo ExoRegular = TTF_FONT("Play-Regular", 14);
	const FSlateFontInfo ExoBoldSmall = TTF_FONT("Play-Bold", 10);
	const FSlateFontInfo ExoBold = TTF_FONT("Play-Bold", 14);
	const FSlateFontInfo ExoBoldLarge = TTF_FONT("Play-Bold", 18);
	const FSlateFontInfo ExoMedRegular = TTF_FONT("Play-Regular", 12);

	FLinearColor ComboItemTextColor = FLinearColor(FColor::White);
	FLinearColor DefaultChatColor = FLinearColor(FColor::White);
	FLinearColor DefaultDullFontColor =  FLinearColor(FColor(210, 210, 210));
	FLinearColor InvertedChatEntryColor =  FLinearColor(FColor(50, 50, 50));

	FLinearColor WhisplerChatColor = FLinearColor(FColor(255, 176, 59));	// orange
	FLinearColor PartyChatColor = FLinearColor(FColor(89, 234, 186));		// light green
	FLinearColor GameChatColor = FLinearColor(FColor(89, 234, 186));		// light green
	FLinearColor ChatBoxColor = FLinearColor(FColor(50, 50, 50));		// dark gray

	FLinearColor EmphasisColor = FLinearColor(FColor(234, 162, 54));		// amber
	FLinearColor CriticalColor = FLinearColor(FColor(193, 99, 67));			// red
	FLinearColor PortalPrimaryColor_Hovered = FLinearColor(0.00716, 0.35374, 0.77423);
	FLinearColor PortalPrimaryColor_Pressed = FLinearColor(0.00503, 0.25903, 0.56681);

	const FScrollBarStyle ScrollBar = FScrollBarStyle()
		.SetVerticalBackgroundImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f), FLinearColor(1.0f, 1.0f, 1.0f, 0.25f)))
		.SetHorizontalBackgroundImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f), FLinearColor(1.0f, 1.0f, 1.0f, 0.25f)))
		.SetNormalThumbImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f)))
		.SetDraggedThumbImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f)))
		.SetHoveredThumbImage(BOX_BRUSH("Social-WIP/Scrollbar", FVector2D(8, 8), FMargin(0.5f)));

	const FScrollBorderStyle ChatScrollBorderStyle = FScrollBorderStyle()
		.SetTopShadowBrush(*FStyleDefaults::GetNoBrush())
		.SetBottomShadowBrush(*FStyleDefaults::GetNoBrush());

	const FTextBlockStyle DefaultText = FTextBlockStyle()
		.SetFont(ExoRegular)
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
		.SetNormalPadding(FMargin(2))
		.SetPressedPadding(FMargin(2));
	Style.Set("FriendsListActionButton", FriendsListActionButtonStyle);

	const FButtonStyle FriendActionDropdownStyle = FButtonStyle(DefaultButton)
		.SetNormal(BOX_BRUSH("Social-WIP/SocialDropdownButton_normal", Icon24x24, FMargin(0.159292, 0.0, 0.823009, 0.0)))
		.SetPressed(BOX_BRUSH("Social-WIP/SocialDropdownButton_down", Icon24x24, FMargin(0.159292, 0.0, 0.823009, 0.0)))
		.SetHovered(BOX_BRUSH("Social-WIP/SocialDropdownButton_over", Icon24x24, FMargin(0.159292, 0.0, 0.823009, 0.0)));
	Style.Set("FriendActionDropdown", FriendActionDropdownStyle);

	FComboButtonStyle FriendListComboButtonStyle = FComboButtonStyle()
		.SetButtonStyle(FriendActionDropdownStyle)
		.SetDownArrowImage(FSlateNoResource())
		.SetMenuBorderBrush(FSlateNoResource())
		.SetMenuBorderPadding(FMargin(0.0f));

	const FButtonStyle FriendsListEmphasisButtonStyle = FButtonStyle(DefaultButton)
		.SetNormal(BOX_BRUSH("Social-WIP/SocialDefaultButton_normal", FriendButtonSize, FMargin(0.5)))
		.SetPressed(BOX_BRUSH("Social-WIP/SocialDefaultButton_down", FriendButtonSize, FMargin(0.5)))
		.SetHovered(BOX_BRUSH("Social-WIP/SocialDefaultButton_over", FriendButtonSize, FMargin(0.5)))
		.SetNormalPadding(FMargin(2))
		.SetPressedPadding(FMargin(2));
	Style.Set("FriendsListEmphasisButton", FriendsListEmphasisButtonStyle);

	const FButtonStyle FriendsListCriticalButtonStyle = FButtonStyle(DefaultButton)
		.SetNormal(BOX_BRUSH("Social-WIP/SocialDefaultButton_normal", FriendButtonSize, FMargin(0.5)))
		.SetPressed(BOX_BRUSH("Social-WIP/SocialDefaultButton_down", FriendButtonSize, FMargin(0.5)))
		.SetHovered(BOX_BRUSH("Social-WIP/SocialDefaultButton_over", FriendButtonSize, FMargin(0.5)))
		.SetNormalPadding(FMargin(2))
		.SetPressedPadding(FMargin(2));
	Style.Set("FriendsListCriticalButton", FriendsListCriticalButtonStyle);

	const FButtonStyle FriendsListAddFriendButtonStyle = FButtonStyle(DefaultButton)
		.SetNormal(BOX_BRUSH("Social-WIP/SocialDefaultButton_normal", FriendButtonSize, FMargin(0.5)))
		.SetPressed(BOX_BRUSH("Social-WIP/SocialDefaultButton_down", FriendButtonSize, FMargin(0.5)))
		.SetHovered(BOX_BRUSH("Social-WIP/SocialDefaultButton_over", FriendButtonSize, FMargin(0.5)));
	Style.Set("FriendsListActionButton", FriendsListAddFriendButtonStyle);

	Style.Set("FriendsList.AddFriendContent", new IMAGE_BRUSH("Social-WIP/FriendsAddIcon", FVector2D(22, 22)));

	const FButtonStyle FriendsListCloseAddFriendButtonStyle = FButtonStyle(DefaultButton)
		.SetNormal(IMAGE_BRUSH("Social-WIP/SocialCloseButton_normal", Icon40x40))
		.SetPressed(IMAGE_BRUSH("Social-WIP/SocialCloseButton_down", Icon40x40))
		.SetHovered(IMAGE_BRUSH("Social-WIP/SocialCloseButton_over", Icon40x40));
	Style.Set("FriendsListCloseAddFriendButton", FriendsListCloseAddFriendButtonStyle);

	Style.Set("FriendList.AddFriendEditableText.NoBackground", new FSlateNoResource(FVector2D(8, 8)));
	const FEditableTextBoxStyle AddFriendEditableTextStyle = FEditableTextBoxStyle()
		.SetBackgroundImageNormal(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
		.SetBackgroundImageHovered(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
		.SetBackgroundImageFocused(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
		.SetBackgroundImageReadOnly(*Style.GetBrush(TEXT("FriendList.AddFriendEditableText.NoBackground")))
		.SetPadding(FMargin(5.0f))
		.SetForegroundColor(FSlateColor(DefaultDullFontColor))
		.SetFont(ExoRegular);
	Style.Set("FriendList.AddFriendEditableTextStyle", AddFriendEditableTextStyle);

	Style.Set("FriendsList.AddFriendEditBorder", new BOX_BRUSH("Social-WIP/OutlinedWhiteBox_OpenTop", FVector2D(8.0f, 8.0f), FMargin(0.5), FLinearColor::White));

	const FEditableTextBoxStyle ChatEntryEditableTextStyle = FEditableTextBoxStyle()
		.SetBackgroundImageNormal(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::White))
		.SetBackgroundImageHovered(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::White))
		.SetBackgroundImageFocused(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::White))
		.SetBackgroundImageReadOnly(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::White))
		.SetPadding(FMargin(5.0f))
		.SetForegroundColor(InvertedChatEntryColor)
		.SetFont(ExoRegular);
	Style.Set("FriendList.ChatEntryEditableText", ChatEntryEditableTextStyle);

	const FButtonStyle FriendsListItemButtonStyle = FButtonStyle(DefaultButton)
		.SetNormal(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::Transparent))
		.SetHovered(IMAGE_BRUSH("UI/White", FVector2D(8, 8), PortalPrimaryColor_Hovered))
		.SetPressed(IMAGE_BRUSH("UI/White", FVector2D(8, 8), PortalPrimaryColor_Pressed));
	Style.Set("FriendsListItemButtonStyle", FriendsListItemButtonStyle);

	const FTextBlockStyle FriendsTextStyle = FTextBlockStyle(DefaultText)
		.SetFont(ExoRegularSmall)
		.SetColorAndOpacity(FLinearColor(FColor::White));
	Style.Set("FriendsTextStyle", FriendsTextStyle);

	const FTextBlockStyle ComboItemTextStyle = FTextBlockStyle(DefaultText)
		.SetFont(ExoMedRegular);
	Style.Set("ComboItemTextStyle", ComboItemTextStyle);

	Style.Set("OnlineState", new IMAGE_BRUSH("Social-WIP/FriendOnlineState", FVector2D(16, 16)));
	Style.Set("OfflineState", new IMAGE_BRUSH("Social-WIP/FriendOfflineStatus", FVector2D(16, 16)));
	Style.Set("AwayState", new IMAGE_BRUSH("Social-WIP/FriendAwayStatus", FVector2D(16, 16)));

	Style.Set("FriendImageBrush", new IMAGE_BRUSH("Social-WIP/OfflineIcon", FVector2D(40, 36)));
	Style.Set("LauncherImageBrush", new IMAGE_BRUSH("Social-WIP/LauncherIcon", FVector2D(40, 36)));
	Style.Set("UTImageBrush", new IMAGE_BRUSH("Social-WIP/UTIcon", FVector2D(40, 36)));
	Style.Set("FortniteImageBrush", new IMAGE_BRUSH("Social-WIP/FortniteIcon", FVector2D(40, 36)));

	Style.Set("GlobalChatIcon", new IMAGE_BRUSH("Social-WIP/Icon-ChatGlobal-XS", Icon16x16));
	Style.Set("WhisperChatIcon", new IMAGE_BRUSH("Social-WIP/Icon-ChatWhisper-XS", Icon16x16));
	Style.Set("PartyChatIcon", new IMAGE_BRUSH("Social-WIP/Icon-ChatParty-XS", Icon16x16));
	Style.Set("InvalidChatIcon", new IMAGE_BRUSH("Social-WIP/Icon-ChatInvalid-XS", Icon16x16));

	Style.Set("FriendComboDropdownBrush", new IMAGE_BRUSH("Social-WIP/SocialExpander_normal", Icon12x12));

	Style.Set("FriendsDefaultBackground", new IMAGE_BRUSH("Social-WIP/DefaultBackground", FVector2D(8, 8)));
	const FSlateBrush* FriendsDefaultBackgroundBrush = Style.GetBrush("FriendsDefaultBackground");

	Style.Set("FriendsChatBackground", new IMAGE_BRUSH("UI/White", FVector2D(8, 8), ChatBoxColor));
	const FSlateBrush* FriendsChatBackgroundBrush = Style.GetBrush("FriendsChatBackground");

	Style.Set("FriendsListPanelBackground", new BOX_BRUSH("Social-WIP/PanelBackground", FVector2D(38, 38), FMargin(0.4f)));
	const FSlateBrush* FriendsListPanelBackgroundBrush = Style.GetBrush("FriendsListPanelBackground");

	Style.Set("FriendsListHeader", new IMAGE_BRUSH("Social-WIP/FriendListHeader", FVector2D(32, 42)));
	const FSlateBrush* FriendsListHeaderBrush = Style.GetBrush("FriendsListHeader");
	const FSlateBrush* FriendContainerHeaderBrush = Style.GetBrush("FriendsListHeader");

	const FButtonStyle ActionComboButtonStyle = FButtonStyle(DefaultButton)
		.SetNormal(IMAGE_BRUSH ("Social-WIP/SocialExpander_normal", FVector2D(8, 8)))
		.SetPressed(IMAGE_BRUSH("Social-WIP/SocialExpanderOpen_normal", FVector2D(8, 8), PortalPrimaryColor_Pressed))
		.SetHovered(IMAGE_BRUSH("Social-WIP/SocialExpander_normal", FVector2D(8, 8), PortalPrimaryColor_Hovered));
	Style.Set("ActionComboButtonStyle", ActionComboButtonStyle);

	Style.Set("MenuBorderBrush", new IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor(FColor(24, 24, 24))));
	const FSlateBrush* MenuBorderBrush = Style.GetBrush("MenuBorderBrush");

	FComboButtonStyle ActionButtonComboButtonStyle = FComboButtonStyle()
		.SetButtonStyle(ActionComboButtonStyle)
		.SetDownArrowImage(FSlateNoResource())
		.SetMenuBorderBrush(FSlateNoResource())
		.SetMenuBorderPadding(FMargin(0));
	Style.Set("ActionButtonComboButtonStyle", ActionButtonComboButtonStyle);

	//Chat Window Style
	Style.Set("FriendsStyle", FFriendsAndChatStyle()
		.SetOnlineBrush(*Style.GetBrush("OnlineState"))
		.SetOfflineBrush(*Style.GetBrush("OfflineState"))
		.SetAwayBrush(*Style.GetBrush("AwayState"))
		.SetFriendsListClosedButtonStyle(FriendsListClosedButtonStyle)
		.SetFriendsListOpenButtonStyle(FriendsListOpenButtonStyle)
		.SetFriendGeneralButtonStyle(FriendsListActionButtonStyle)
		.SetFriendListActionButtonStyle(FriendsListActionButtonStyle)
		.SetFriendsListEmphasisButtonStyle(FriendsListEmphasisButtonStyle)
		.SetFriendsListCriticalButtonStyle(FriendsListCriticalButtonStyle)
		.SetFriendsCalloutBrush(*Style.GetBrush("FriendComboDropdownBrush"))
		.SetComboItemButtonStyle(FriendsListItemButtonStyle)
		.SetFriendsListItemButtonStyle(FriendsListItemButtonStyle)
		.SetFriendsListItemButtonSimpleStyle(FriendsListItemButtonStyle)
		.SetFriendsListComboButtonStyle(FriendListComboButtonStyle)
		.SetFriendComboBackgroundLeftBrush(*MenuBorderBrush)
		.SetFriendComboBackgroundRightBrush(*MenuBorderBrush)
		.SetFriendComboBackgroundLeftFlippedBrush(*MenuBorderBrush)
		.SetFriendComboBackgroundRightFlippedBrush(*MenuBorderBrush)
		.SetAddFriendButtonContentBrush(*Style.GetBrush("FriendsList.AddFriendContent"))
		.SetAddFriendButtonContentHoveredBrush(*Style.GetBrush("FriendsList.AddFriendContent"))
		.SetAddFriendCloseButtonStyle(FriendsListCloseAddFriendButtonStyle)
		.SetBackgroundBrush(*FriendsDefaultBackgroundBrush)
		.SetFriendContainerHeader(*FriendContainerHeaderBrush)
		.SetFriendListHeader(*FriendsListHeaderBrush)
		.SetFriendItemSelected(*FriendsDefaultBackgroundBrush)
		.SetChatContainerBackground(*FriendsChatBackgroundBrush)
		.SetFriendContainerBackground(*FriendsListPanelBackgroundBrush)
		.SetAddFriendEditBorder(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::White))
		.SetFriendImageBrush(*Style.GetBrush("FriendImageBrush"))
		.SetLauncherImageBrush(*Style.GetBrush("LauncherImageBrush"))
		.SetUTImageBrush(*Style.GetBrush("UTImageBrush"))
		.SetFortniteImageBrush(*Style.GetBrush("FortniteImageBrush"))
		.SetChatInvalidBrush(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::Transparent))
		.SetTextStyle(FriendsTextStyle)
		.SetFontStyle(ExoRegular)
		.SetFontStyleBold(ExoBold)
		.SetFontStyleUserLarge(ExoBoldLarge)
		.SetFontStyleSmall(ExoRegularSmall)
		.SetFontStyleSmallBold(ExoBoldSmall)
		.SetChatFontStyleEntry(ExoRegular)
		.SetDefaultFontColor(FLinearColor::White)
		.SetDefaultDullFontColor(DefaultDullFontColor)
		.SetDefaultChatColor(DefaultChatColor)
		.SetWhisplerChatColor(WhisplerChatColor)
//		.SetGameChatColor(GameChatColor)
		.SetPartyChatColor(PartyChatColor)
		.SetComboItemTextColorNormal(ComboItemTextColor)
		.SetComboItemTextColorHovered(ComboItemTextColor)
		.SetFriendListActionFontColor(DefaultChatColor)
		.SetFriendButtonFontColor(EmphasisColor)
		.SetChatGlobalBrush(*Style.GetBrush("GlobalChatIcon"))
//			.SetChatGameBrush(FriendsGameChatItem->Brush)
//			.SetChatPartyBrush(FriendsPartyChatItem->Brush)
		.SetChatWhisperBrush(*Style.GetBrush("WhisperChatIcon"))
		.SetAddFriendEditableTextStyle(AddFriendEditableTextStyle)
		.SetComboItemTextStyle(ComboItemTextStyle)
		.SetChatEditableTextStyle(ChatEntryEditableTextStyle)
		.SetFriendCheckboxStyle(FCoreStyle::Get().GetWidgetStyle< FCheckBoxStyle >("Checkbox"))
		.SetStatusButtonSize(FVector2D(136, 40))
		.SetActionComboButtonStyle(ActionButtonComboButtonStyle)
		.SetActionComboButtonSize(FVector2D(25, 25))
		.SetButtonInvertedForegroundColor(FLinearColor::White)
		.SetBorderPadding(FMargin(10, 20))
		.SetFriendsListWidth(500.f)
		.SetChatListPadding(130.f)
		.SetChatBackgroundBrush(*FriendsDefaultBackgroundBrush)
		.SetChatChannelsBackgroundBrush(*FriendsDefaultBackgroundBrush)
		.SetChatOptionsBackgroundBrush(*FriendsDefaultBackgroundBrush)
		.SetScrollbarStyle((ScrollBar))
		.SetScrollBorderStyle(ChatScrollBorderStyle)
		.SetComboMenuPadding(FMargin(0))
		.SetComboItemPadding(FMargin(0))
		.SetComboItemContentPadding(FMargin(10, 4, 10, 4))
		.SetChatFooterBrush(IMAGE_BRUSH("UI/White", FVector2D(8, 8), FLinearColor::Black))
		.SetFriendUserHeaderBackground(*FriendsListPanelBackgroundBrush)
		.SetHasUserHeader(true)
		);
}

void SUWindowsStyle::SetMatchBadgeStyle(TSharedRef<FSlateStyleSet> StyleRef)
{ // Match Badge Styles

	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UWindows.Standard.MatchBadge", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 18))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

	Style.Set("UWindows.Standard.MatchBadge.Bold", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 18))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.MatchBadge.Red", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 16))
		.SetColorAndOpacity(FLinearColor::Red)
		);

	Style.Set("UWindows.Standard.MatchBadge.Blue", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 16))
		.SetColorAndOpacity(FLinearColor(0.7, 0.7, 1.0, 1.0))
		);

	Style.Set("UWindows.Standard.MatchBadge.Red.Bold", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 16))
		.SetColorAndOpacity(FLinearColor::Red)
		);

	Style.Set("UWindows.Standard.MatchBadge.Blue.Bold", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 16))
		.SetColorAndOpacity(FLinearColor(0.7, 0.7, 1.0, 1.0))
		);

	Style.Set("UWindows.Standard.MatchBadge.Header", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 22))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("UWindows.Standard.MatchBadge.Small", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Bold", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);
}

void SUWindowsStyle::SetMOTDStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	// MOTD Styles

	Style.Set("MOTD.Normal", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 14))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("MOTD.Small", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 10))
		.SetColorAndOpacity(FLinearColor::White)
		);

	Style.Set("MOTD.Header", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 24))
		.SetColorAndOpacity(FLinearColor::Yellow)
		);

	Style.Set("MOTD.Header.Huge", FTextBlockStyle()
		.SetFont(TTF_FONT("Play-Regular", 32))
		.SetColorAndOpacity(FLinearColor::White)
		);
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