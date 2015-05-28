// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginCreatorPrivatePCH.h"

#include "PluginCreatorStyle.h"
#include "SlateGameResources.h"

TSharedPtr< FSlateStyleSet > FPluginCreatorStyle::PCStyleInstance = NULL;

void FPluginCreatorStyle::Initialize()
{
	if (!PCStyleInstance.IsValid())
	{
		PCStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*PCStyleInstance);
	}
}

void FPluginCreatorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*PCStyleInstance);
	ensure(PCStyleInstance.IsUnique());
	PCStyleInstance.Reset();
}

FName FPluginCreatorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("PCStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

// Note, these sizes are in Slate Units.
// Slate Units do NOT have to map to pixels.
const FVector2D Icon5x16(5.0f, 16.0f);
const FVector2D Icon8x4(8.0f, 4.0f);
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

TSharedRef< FSlateStyleSet > FPluginCreatorStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("PCStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	const FTextBlockStyle TabButtonText = FTextBlockStyle()
		.SetFont(TTF_FONT("Fonts/Roboto-Bold", 10))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
		.SetShadowOffset(FVector2D(-1, 1))
		.SetHighlightShape(BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f / 8.f)));

	Style->Set("TabButton.Text", TabButtonText);

	Style->Set("PluginNameFont", TTF_FONT("Fonts/Roboto-Bold", 18));


	Style->SetContentRoot(GetPluginCreatorRootPath() / TEXT("Resources"));

	Style->Set("DefaultPluginIcon", new IMAGE_BRUSH(TEXT("DefaultIcon128"), Icon128x128));

	Style->Set("BlankPluginSource", new IMAGE_BRUSH(TEXT("blankpluginSource"), FVector2D(301, 200)));
	Style->Set("BasicPluginSource", new IMAGE_BRUSH(TEXT("basicpluginSource"), FVector2D(288, 290)));

	const FButtonStyle NoBorder = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateNoResource())
		.SetPressed(FSlateNoResource())
		.SetNormalPadding(FMargin(0, 0, 0, 1))
		.SetPressedPadding(FMargin(0, 1, 0, 0));

	Style->Set("NoBorderButton", NoBorder);
	Style->Set("NoBorder", new FSlateNoResource());

	Style->Set("DarkGrayBackground", new BOX_BRUSH("DarkGrayBackground", FMargin(4 / 16.0f)));

	

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FPluginCreatorStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FPluginCreatorStyle::Get()
{
	return *PCStyleInstance;
}
