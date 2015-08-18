// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "FriendsAndChatStyle.h"
#include "SUTStyle.h"

#if !UE_SERVER
TSharedPtr<FSlateStyleSet> SUTStyle::UWindowsStyleInstance = NULL;

void SUTStyle::Initialize()
{
	if (!UWindowsStyleInstance.IsValid())
	{
		UWindowsStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle( *UWindowsStyleInstance);
	}
}

void SUTStyle::Shutdown()
{
}

FName SUTStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("SUTStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... )	FSlateImageBrush( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) 		FSlateBoxBrush( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) 	FSlateBorderBrush( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) 		FSlateFontInfo( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".ttf"), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) 		FSlateFontInfo( FPaths::GameContentDir() / "RestrictedAssets/Slate"/ RelativePath + TEXT(".otf"), __VA_ARGS__ )

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const int32 FONT_SIZE_Tiny = 12;
const int32 FONT_SIZE_Small = 14;
const int32 FONT_SIZE_Medium = 24;
const int32 FONT_SIZE_Large = 32;
const int32 FONT_SIZE_Huge = 64;

FSlateSound SUTStyle::ButtonPressSound;
FSlateSound SUTStyle::ButtonHoverSound;
FSlateColor SUTStyle::DefaultForeground;

TSharedRef<FSlateStyleSet> SUTStyle::Create()
{
	TSharedRef<FSlateStyleSet> StyleRef = MakeShareable(new FSlateStyleSet(SUTStyle::GetStyleSetName()));

	StyleRef->SetContentRoot(FPaths::GameContentDir() / TEXT("RestrictedAssets/Slate"));
	StyleRef->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	FSlateStyleSet& Style = StyleRef.Get();

	SetFonts(StyleRef);
	SetCommonStyle(StyleRef);
	SetAvatars(StyleRef);

	return StyleRef;
}

void SUTStyle::SetFonts(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UT.Font.NormalText.Tiny", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Regular", FONT_SIZE_Tiny)).SetColorAndOpacity(FLinearColor::White));
	Style.Set("UT.Font.NormalText.Tiny.Bold", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Bold", FONT_SIZE_Tiny)).SetColorAndOpacity(FLinearColor::White));

	Style.Set("UT.Font.NormalText.Small", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Regular", FONT_SIZE_Small)).SetColorAndOpacity(FLinearColor::White));
	Style.Set("UT.Font.NormalText.Small.Bold", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Bold", FONT_SIZE_Small)).SetColorAndOpacity(FLinearColor::White));

	Style.Set("UT.Font.ChatText.Text", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Regular", FONT_SIZE_Small)).SetColorAndOpacity(FLinearColor(0.7,0.7,0.7,1.0)));
	Style.Set("UT.Font.ChatText.Name", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Bold", FONT_SIZE_Small)).SetColorAndOpacity(FLinearColor::White));

	Style.Set("UT.Font.NormalText.Medium", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Regular", FONT_SIZE_Medium)).SetColorAndOpacity(FLinearColor::White));
	Style.Set("UT.Font.NormalText.Medium.Bold", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Bold", FONT_SIZE_Medium)).SetColorAndOpacity(FLinearColor::White));

	Style.Set("UT.Font.NormalText.Large", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Regular", FONT_SIZE_Large)).SetColorAndOpacity(FLinearColor::White));
	Style.Set("UT.Font.NormalText.Large.Bold", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Bold", FONT_SIZE_Large)).SetColorAndOpacity(FLinearColor::White));

	Style.Set("UT.Font.NormalText.Huge", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Regular", FONT_SIZE_Huge)).SetColorAndOpacity(FLinearColor::White));
	Style.Set("UT.Font.NormalText.Huge.Bold", FTextBlockStyle().SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Bold", FONT_SIZE_Huge)).SetColorAndOpacity(FLinearColor::White));

}

void SUTStyle::SetCommonStyle(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UT.SimpleButton", FButtonStyle()
		.SetNormal( FSlateColorBrush(FColor(25,48,180,255)) )
		.SetHovered( FSlateColorBrush(FColor(67,128,224,255)) )
		.SetPressed( FSlateColorBrush(FColor(32,32,32,255)) )
		.SetDisabled( FSlateColorBrush(FColor(1,1,1,255)) )
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
	);

	Style.Set("UT.ClearButton", FButtonStyle()
		.SetNormal( FSlateNoResource(FVector2D(128.0f, 128.0f)) )
		.SetHovered( FSlateNoResource(FVector2D(128.0f, 128.0f)) )
		.SetPressed( FSlateNoResource(FVector2D(128.0f, 128.0f)) )
		.SetDisabled( FSlateNoResource(FVector2D(128.0f, 128.0f)) )
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
	);


	Style.Set("UT.SimpleButton.Dark", FButtonStyle()
		.SetNormal( FSlateColorBrush(FColor(4,4,4,255)) )
		.SetHovered( FSlateColorBrush(FColor(10,10,10,255)) )
		.SetPressed( FSlateColorBrush(FColor(61,135,255,255)) )
		.SetDisabled( FSlateColorBrush(FColor(189,189,189,255)) )
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
	);

	Style.Set("UT.SimpleButton.Medium", FButtonStyle()
		.SetNormal( FSlateColorBrush(FColor(8,8,8,255)) )
		.SetHovered( FSlateColorBrush(FColor(13,13,13,255)) )
		.SetPressed( FSlateColorBrush(FColor(61,135,255,255)) )
		.SetDisabled( FSlateColorBrush(FColor(189,189,189,255)) )
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
	);


	Style.Set("UT.NoStyle", new FSlateNoResource(FVector2D(128.0f, 128.0f)));

	Style.Set("UT.TeamColor.Red", new FSlateColorBrush(FColor(255,12,0,255)));
	Style.Set("UT.TeamColor.Blue", new FSlateColorBrush(FColor(12,12,255,255)));
	Style.Set("UT.TeamColor.Spectator", new FSlateColorBrush(FColor(200,200,200,255)));

	Style.Set("UT.ListBackground.Even", new FSlateColorBrush(FColor(5,5,5,255)));
	Style.Set("UT.ListBackground.Odd", new FSlateColorBrush(FColor(7,7,7,255)));

	Style.Set("UT.HeaderBackground.SuperDark", new FSlateColorBrush(FColor(1,1,1,255)));
	Style.Set("UT.HeaderBackground.Dark", new FSlateColorBrush(FColor(4,4,4,255)));
	Style.Set("UT.HeaderBackground.Navy", new FSlateColorBrush(FColor(0,0,4,255)));
	Style.Set("UT.HeaderBackground.Light", new FSlateColorBrush(FColor(14,14,14,255)));
	Style.Set("UT.HeaderBackground.Medium", new FSlateColorBrush(FColor(10,10,10,255)));
	Style.Set("UT.HeaderBackground.Shaded", new FSlateColorBrush(FColor(4,4,4,200)));
	Style.Set("UT.Box", new FSlateColorBrush(FColor(13,13,13,153)));
	Style.Set("UT.Divider", new FSlateColorBrush(FColor(25,25,25,255)));


	Style.Set("UT.TabButton", FButtonStyle()
		.SetNormal( FSlateColorBrush(FColor(4,4,4,255) ) )
		.SetHovered( FSlateColorBrush(FColor(14,14,14,255) ) )
		.SetPressed( FSlateNoResource(FVector2D(256.0f, 256.0f) ) )
		.SetDisabled( FSlateColorBrush(FColor(1,1,1,255) ) )
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
		);

	Style.Set("UT.ChatEditBox", FEditableTextBoxStyle()
		.SetFont(TTF_FONT("/UTStyle/Fonts/Lato/Lato-Regular", FONT_SIZE_Small))
		.SetForegroundColor(FLinearColor(0.75f,0.75f,0.75f,1.0f))
		.SetBackgroundImageNormal( FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageHovered( FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageFocused( FSlateNoResource(FVector2D(128.0f, 128.0f)))
		.SetBackgroundImageReadOnly( FSlateNoResource(FVector2D(128.0f, 128.0f)))
		);

	Style.Set("UT.List.Background.Even", new FSlateColorBrush(FColor(27,27,27,255)));
	Style.Set("UT.List.Background.Odd", new FSlateColorBrush(FColor(30,30,30,255)));


	Style.Set("UT.CheckBox", FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
		.SetUncheckedImage(IMAGE_BRUSH("UTStyle/CommonControls/CheckBox/UT.CheckBox.UnChecked.Normal", FVector2D(32,32)))
		.SetUncheckedHoveredImage(IMAGE_BRUSH("UTStyle/CommonControls/CheckBox/UT.CheckBox.UnChecked.Hovered", FVector2D(32,32)))
		.SetUncheckedPressedImage(IMAGE_BRUSH("UTStyle/CommonControls/CheckBox/UT.CheckBox.UnChecked.Pressed", FVector2D(32,32)))
		.SetCheckedImage(IMAGE_BRUSH("UTStyle/CommonControls/CheckBox/UT.CheckBox.Checked.Normal", FVector2D(32,32)))
		.SetCheckedHoveredImage(IMAGE_BRUSH("UTStyle/CommonControls/CheckBox/UT.CheckBox.Checked.Normal", FVector2D(32,32)))
		.SetCheckedPressedImage(IMAGE_BRUSH("UTStyle/CommonControls/CheckBox/UT.CheckBox.Checked.Normal", FVector2D(32,32)))
		.SetUndeterminedImage(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetUndeterminedHoveredImage(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetUndeterminedPressedImage(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		);

	Style.Set("UT.MatchBadge.Circle", new IMAGE_BRUSH( "UTStyle/MatchBadges/UT.MatchBadge.Circle", FVector2D(78.0f, 78.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.MatchBadge.Circle.Thin", new IMAGE_BRUSH( "UTStyle/MatchBadges/UT.MatchBadge.Circle.Thin", FVector2D(78.0f, 78.0f), FLinearColor(0.3f, 0.3f, 0.3f, 1.0f) ));

	Style.Set("UT.Icon.Lock.Small", new IMAGE_BRUSH("UTStyle/Icons/UT.Icon.Lock.Small", FVector2D(18,18), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.Set("UT.Icon.Friends.Small", new IMAGE_BRUSH("UTStyle/Icons/UT.Icon.Friends.Small", FVector2D(18,18), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

	Style.Set("UT.MatchList.Row", FTableRowStyle()
		.SetEvenRowBackgroundBrush(FSlateColorBrush(FColor(4,4,4,255)))
		.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FColor(5,5,5,255)))
		.SetOddRowBackgroundBrush(FSlateColorBrush(FColor(6,6,6,255)))
		.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FColor(7,7,7,255)))
		.SetSelectorFocusedBrush(FSlateColorBrush(FColor(32,32,32,255)))
		.SetActiveBrush(FSlateColorBrush(FColor(32,32,32,255)))
		.SetActiveHoveredBrush(FSlateColorBrush(FColor(32,32,32,255)))
		.SetInactiveBrush(FSlateColorBrush(FColor(32,32,32,255)))
		.SetInactiveHoveredBrush(FSlateColorBrush(FColor(32,32,32,255)))
		.SetTextColor(FLinearColor::White)
		.SetSelectedTextColor(FLinearColor::Black)
		);

	Style.Set("UT.PlayerList.Row", FTableRowStyle()
		.SetEvenRowBackgroundBrush(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FColor(5,5,5,255)))
		.SetOddRowBackgroundBrush(FSlateNoResource(FVector2D(256.0f, 256.0f)))
		.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FColor(7,7,7,255)))
		.SetSelectorFocusedBrush(FSlateColorBrush(FColor(9,9,9,255)))
		.SetActiveBrush(FSlateColorBrush(FColor(9,9,9,255)))
		.SetActiveHoveredBrush(FSlateColorBrush(FColor(9,9,9,255)))
		.SetInactiveBrush(FSlateColorBrush(FColor(9,9,9,255)))
		.SetInactiveHoveredBrush(FSlateColorBrush(FColor(9,9,9,255)))
		.SetTextColor(FLinearColor::White)
		.SetSelectedTextColor(FLinearColor::Black)
		);


	Style.Set("UT.HomePanel.TutorialLogo", new IMAGE_BRUSH("UTStyle/MainPanel/T_TUT_ULogo_Shadow", FVector2D(2048,1024), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.FragCenterLogo", new IMAGE_BRUSH("UTStyle/MainPanel/FragCenterEmblem", FVector2D(644, 644), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.Set("UT.HomePanel.Background", new IMAGE_BRUSH( "UTStyle/MainPanel/Background", FVector2D(1920,1080), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.Flak", new IMAGE_BRUSH( "UTStyle/MainPanel/Flak", FVector2D(180,180), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.IABadge", new IMAGE_BRUSH( "UTStyle/MainPanel/IABadge", FVector2D(380,270), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.FMBadge", new IMAGE_BRUSH( "UTStyle/MainPanel/FMBadge", FVector2D(380,270), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.DMBadge", new IMAGE_BRUSH( "UTStyle/MainPanel/DMBadge", FVector2D(380,270), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.CTFBadge", new IMAGE_BRUSH( "UTStyle/MainPanel/CTFBadge", FVector2D(380,270), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.Replays", new IMAGE_BRUSH( "UTStyle/MainPanel/Replays", FVector2D(180,180), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.HomePanel.Live", new IMAGE_BRUSH( "UTStyle/MainPanel/Live", FVector2D(180,180), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));

	Style.Set("UT.HomePanel.Button", FButtonStyle()
		.SetNormal( FSlateNoResource(FVector2D(256.0f, 256.0f) ))
		.SetHovered( BOX_BRUSH("UTStyle/MainPanel/Highlight", FVector2D(256,256), FMargin(16.0f / 256.0f, 16.0f/256.0f, 16.0f / 256.0f, 16.0f/256.0f), FLinearColor(25.0/256.0, 48.0/256.0, 180.0 / 256.0, 1.0) ))
		.SetPressed( BOX_BRUSH("UTStyle/MainPanel/Highlight", FVector2D(256,256), FMargin(16.0f / 256.0f, 16.0f/256.0f, 16.0f / 256.0f, 16.0f/256.0f), FLinearColor(67.0/256.0, 128.0/256.0, 224.0 / 256.0, 1.0) ))
		.SetDisabled( FSlateNoResource(FVector2D(256.0f, 256.0f) ))
		.SetHoveredSound(ButtonHoverSound)
		.SetPressedSound(ButtonPressSound)
	);

}

void SUTStyle::SetAvatars(TSharedRef<FSlateStyleSet> StyleRef)
{
	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("UT.Avatar.0", new IMAGE_BRUSH( "UTStyle/Avatars/UT.Avatar.0", FVector2D(50.0f, 50.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.Avatar.1", new IMAGE_BRUSH( "UTStyle/Avatars/UT.Avatar.1", FVector2D(50.0f, 50.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.Avatar.2", new IMAGE_BRUSH( "UTStyle/Avatars/UT.Avatar.2", FVector2D(50.0f, 50.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.Avatar.3", new IMAGE_BRUSH( "UTStyle/Avatars/UT.Avatar.3", FVector2D(50.0f, 50.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
	Style.Set("UT.Avatar.4", new IMAGE_BRUSH( "UTStyle/Avatars/UT.Avatar.4", FVector2D(50.0f, 50.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT


void SUTStyle::Reload()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& SUTStyle::Get()
{
	return * UWindowsStyleInstance;
}
#endif