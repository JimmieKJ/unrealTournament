#include "SubstanceCorePrivatePCH.h"
#include "SubstanceStyle.h"
#include "SlateStyle.h"
#include "IPluginManager.h"

#if WITH_EDITOR
#include "ClassIconFinder.h"
#endif

#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FSubstanceStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )

TSharedPtr< FSlateStyleSet > FSubstanceStyle::StyleSet = nullptr;

void FSubstanceStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	//Create style set
	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	StyleSet->Set("ClassThumbnail.SubstanceGraphInstance", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_Substance_White_40x"), Icon40x40));
	StyleSet->Set("ClassThumbnail.SubstanceInstanceFactory", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_SubstanceFactory_White_40x"), Icon40x40));
	
#if WITH_EDITOR
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
#endif
}

void FSubstanceStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
#if WITH_EDITOR
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
#endif
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

FName FSubstanceStyle::GetStyleSetName()
{
	static FName SubstanceStyleName(TEXT("SubstanceStyle"));
	return SubstanceStyleName;
}

FString FSubstanceStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("Substance"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

#undef IMAGE_PLUGIN_BRUSH

