// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

#if !CRASH_REPORT_UNATTENDED_ONLY

#include "CrashReportClientStyle.h"

TSharedPtr< FSlateStyleSet > FCrashReportClientStyle::StyleSet = nullptr;

void FCrashReportClientStyle::Initialize()
{
	if(!StyleSet.IsValid())
	{
		StyleSet = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	}
}

void FCrashReportClientStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	ensure(StyleSet.IsUnique());
	StyleSet.Reset();
}

#define TTF_FONT(RelativePath, ...) FSlateFontInfo(ContentFromEngine(TEXT(RelativePath), TEXT(".ttf")), __VA_ARGS__)
#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush(ContentFromEngine(RelativePath, TEXT(".png")), __VA_ARGS__)

namespace
{
	FString ContentFromEngine(const FString& RelativePath, const TCHAR* Extension)
	{
		static const FString ContentDir = FPaths::EngineDir() / TEXT("Content/Slate");
		return ContentDir / RelativePath + Extension;
	}
}

TSharedRef< FSlateStyleSet > FCrashReportClientStyle::Create()
{
	TSharedRef<FSlateStyleSet> StyleRef = MakeShareable(new FSlateStyleSet("CrashReportClientStyle"));
	FSlateStyleSet& Style = StyleRef.Get();

	const FTextBlockStyle DefaultText = FTextBlockStyle()
		.SetFont(TTF_FONT("Fonts/Roboto-Bold", 10))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D::ZeroVector)
		.SetShadowColorAndOpacity(FLinearColor::Black);

	// Set the client app styles
	Style.Set(TEXT("Code"), FTextBlockStyle(DefaultText)
		.SetFont(TTF_FONT("Fonts/Roboto-Regular", 8))
		.SetColorAndOpacity(FSlateColor(FLinearColor::White * 0.8f))
	);

	Style.Set(TEXT("Title"), FTextBlockStyle(DefaultText)
		.SetFont(TTF_FONT("Fonts/Roboto-Bold", 12))
	);

	Style.Set(TEXT("Status"), FTextBlockStyle(DefaultText)
		.SetColorAndOpacity(FSlateColor::UseSubduedForeground())
	);

	const FVector2D Icon16x16( 16.0f, 16.0f );
	FSlateBrush* GenericWhiteBox = new IMAGE_BRUSH( "Old/White", Icon16x16 );

	// SEditableTextBox defaults...
	const FScrollBarStyle& ScrollBarStyle = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>( "ScrollBar" );
	const FEditableTextBoxStyle NormalEditableTextBoxStyle = FEditableTextBoxStyle()
		.SetBackgroundImageNormal( *GenericWhiteBox )
		.SetBackgroundImageHovered( *GenericWhiteBox )
		.SetBackgroundImageFocused( *GenericWhiteBox )
		.SetBackgroundImageReadOnly( *GenericWhiteBox )
		.SetScrollBarStyle( ScrollBarStyle );
	{
		Style.Set( "NormalEditableTextBox", NormalEditableTextBoxStyle );
	}

	return StyleRef;
}

#undef TTF_FONT

const ISlateStyle& FCrashReportClientStyle::Get()
{
	return *StyleSet;
}

#endif // !CRASH_REPORT_UNATTENDED_ONLY
