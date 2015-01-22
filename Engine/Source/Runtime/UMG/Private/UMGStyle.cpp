// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "UMGStyle.h"
#include "SlateGameResources.h"

TSharedPtr< FSlateStyleSet > FUMGStyle::UMGStyleInstance = NULL;

void FUMGStyle::Initialize()
{
	if ( !UMGStyleInstance.IsValid() )
	{
		UMGStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle( *UMGStyleInstance );
	}
}

void FUMGStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle( *UMGStyleInstance );
	ensure( UMGStyleInstance.IsUnique() );
	UMGStyleInstance.Reset();
}

FName FUMGStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("UMGStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FUMGStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("UMGStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/UMG"));
	
	Style->Set("MarchingAnts", new BORDER_BRUSH( TEXT("NonMarchingAnts"), FMargin(0.25f), FLinearColor(1,1,1,0.5) ));

	Style->Set("Widget", new IMAGE_BRUSH(TEXT("Widget"), Icon16x16));
	Style->Set("Widget.CheckBox", new IMAGE_BRUSH(TEXT("CheckBox"), Icon16x16));
	Style->Set("Widget.Button", new IMAGE_BRUSH(TEXT("Button"), Icon16x16));
	Style->Set("Widget.EditableTextBox", new IMAGE_BRUSH(TEXT("EditableTextBox"), Icon16x16));
	Style->Set("Widget.EditableText", new IMAGE_BRUSH(TEXT("EditableText"), Icon16x16));
	Style->Set("Widget.HorizontalBox", new IMAGE_BRUSH(TEXT("HorizontalBox"), Icon16x16));
	Style->Set("Widget.VerticalBox", new IMAGE_BRUSH(TEXT("VerticalBox"), Icon16x16));
	Style->Set("Widget.Image", new IMAGE_BRUSH(TEXT("Image"), Icon16x16));
	Style->Set("Widget.Canvas", new IMAGE_BRUSH(TEXT("Canvas"), Icon16x16));
	Style->Set("Widget.TextBlock", new IMAGE_BRUSH(TEXT("TextBlock"), Icon16x16));
	Style->Set("Widget.Border", new IMAGE_BRUSH(TEXT("Border"), Icon16x16));
	Style->Set("Widget.Slider", new IMAGE_BRUSH(TEXT("Slider"), Icon16x16));
	Style->Set("Widget.Spacer", new IMAGE_BRUSH(TEXT("Spacer"), Icon16x16));
	Style->Set("Widget.ScrollBox", new IMAGE_BRUSH(TEXT("ScrollBox"), Icon16x16));
	Style->Set("Widget.ProgressBar", new IMAGE_BRUSH(TEXT("ProgressBar"), Icon16x16));
	Style->Set("Widget.MenuAnchor", new IMAGE_BRUSH(TEXT("MenuAnchor"), Icon16x16));
	Style->Set("Widget.ScrollBar", new IMAGE_BRUSH(TEXT("ScrollBar"), Icon16x16));
	Style->Set("Widget.UniformGrid", new IMAGE_BRUSH(TEXT("UniformGrid"), Icon16x16));
	Style->Set("Widget.WidgetSwitcher", new IMAGE_BRUSH(TEXT("WidgetSwitcher"), Icon16x16));
	Style->Set("Widget.MultiLineEditableText", new IMAGE_BRUSH(TEXT("MultiLineEditableText"), Icon16x16));
	Style->Set("Widget.MultiLineEditableTextBox", new IMAGE_BRUSH(TEXT("MultiLineEditableTextBox"), Icon16x16));
	Style->Set("Widget.Viewport", new IMAGE_BRUSH(TEXT("Viewport"), Icon16x16));
	Style->Set("Widget.ComboBox", new IMAGE_BRUSH(TEXT("icon_umg_ComboBox_16x"), Icon16x16));
	Style->Set("Widget.ListView", new IMAGE_BRUSH(TEXT("icon_umg_ListView_16x"), Icon16x16));
	Style->Set("Widget.TileView", new IMAGE_BRUSH(TEXT("icon_umg_TileView_16x"), Icon16x16));
	Style->Set("Widget.Overlay", new IMAGE_BRUSH(TEXT("icon_umg_Overlay_16x"), Icon16x16));
	Style->Set("Widget.Throbber", new IMAGE_BRUSH(TEXT("icon_umg_ThrobberHorizontal_16x"), Icon16x16));
	Style->Set("Widget.CircularThrobber", new IMAGE_BRUSH(TEXT("icon_umg_ThrobberA_16x"), Icon16x16));
	Style->Set("Widget.NativeWidgetHost", new IMAGE_BRUSH(TEXT("NativeWidgetHost"), Icon16x16));
	Style->Set("Widget.ScaleBox", new IMAGE_BRUSH(TEXT("ScaleBox"), Icon16x16));
	Style->Set("Widget.SizeBox", new IMAGE_BRUSH(TEXT("SizeBox"), Icon16x16));
	Style->Set("Widget.SpinBox", new IMAGE_BRUSH(TEXT("SpinBox"), Icon16x16));
	Style->Set("Widget.Grid", new IMAGE_BRUSH(TEXT("Grid"), Icon16x16));
	Style->Set("Widget.WrapBox", new IMAGE_BRUSH(TEXT("WrapBox"), Icon16x16));
	Style->Set("Widget.NamedSlot", new IMAGE_BRUSH(TEXT("NamedSlot"), Icon16x16));

	Style->Set("Widget.UserWidget", new IMAGE_BRUSH(TEXT("UserWidget"), Icon16x16));

	Style->Set("Animations.TabIcon", new IMAGE_BRUSH(TEXT("Animations_16x"), Icon16x16));
	Style->Set("Designer.TabIcon", new IMAGE_BRUSH(TEXT("Designer_16x"), Icon16x16));
	Style->Set("Palette.TabIcon", new IMAGE_BRUSH(TEXT("Palette_16x"), Icon16x16));
	Style->Set("Sequencer.TabIcon", new IMAGE_BRUSH(TEXT("Timeline_16x"), Icon16x16));

	Style->Set("Animations.Icon", new IMAGE_BRUSH(TEXT("Animations_40x"), Icon40x40));
	Style->Set("Animations.Icon.Small", new IMAGE_BRUSH(TEXT("Animations_40x"), Icon20x20));

	Style->Set("Designer.Icon", new IMAGE_BRUSH(TEXT("Designer_40x"), Icon40x40));
	Style->Set("Designer.Icon.Small", new IMAGE_BRUSH(TEXT("Designer_40x"), Icon20x20));

	Style->Set("Palette.Icon", new IMAGE_BRUSH(TEXT("Palette_40x"), Icon40x40));
	Style->Set("Palette.Icon.Small", new IMAGE_BRUSH(TEXT("Palette_40x"), Icon20x20));

	Style->Set("Timeline.Icon", new IMAGE_BRUSH(TEXT("Timeline_40x"), Icon40x40));
	Style->Set("Timeline.Icon.Small", new IMAGE_BRUSH(TEXT("Timeline_40x"), Icon20x20));


	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FUMGStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FUMGStyle::Get()
{
	return *UMGStyleInstance;
}
