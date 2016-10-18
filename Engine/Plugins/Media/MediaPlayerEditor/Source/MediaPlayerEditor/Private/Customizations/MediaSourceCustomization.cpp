// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "MediaSourceCustomization.h"


#define LOCTEXT_NAMESPACE "FMediaSourceCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FMediaSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// customize 'Platforms' category
	IDetailCategoryBuilder& PlatformsCategory = DetailBuilder.EditCategory("Platforms");
	{
		// DefaultPlayers
		DefaultPlayersProperty = DetailBuilder.GetProperty("DefaultPlayers");
		{
			IDetailPropertyRow& DefaultPlayersRow = PlatformsCategory.AddProperty(DefaultPlayersProperty);

			DefaultPlayersRow.CustomWidget()
				.NameContent()
					[
						DefaultPlayersProperty->CreatePropertyNameWidget()
					]
				.ValueContent()
					[
						MakeDefaultPlayersValueWidget()
					];
		}
	}
}


/* FMediaSourceCustomization implementation
 *****************************************************************************/

TSharedRef<SWidget> FMediaSourceCustomization::MakeDefaultPlayersValueWidget() const
{
	TArray<const PlatformInfo::FPlatformInfo*> AvailablePlatforms;

	// get available platforms
	for (const PlatformInfo::FPlatformInfo& PlatformInfo : PlatformInfo::EnumeratePlatformInfoArray())
	{
		if (PlatformInfo.IsVanilla() && (PlatformInfo.PlatformType == PlatformInfo::EPlatformType::Game))
		{
			// @todo AllDesktop: Re-enable here
			if (PlatformInfo.PlatformInfoName != TEXT("AllDesktop"))
			{
				AvailablePlatforms.Add(&PlatformInfo);
			}
		}
	}

	// sort available platforms alphabetically
	AvailablePlatforms.Sort([](const PlatformInfo::FPlatformInfo& One, const PlatformInfo::FPlatformInfo& Two) -> bool
	{
		return One.DisplayName.CompareTo(Two.DisplayName) < 0;
	});

	// build value widget
	TSharedRef<SGridPanel> PlatformPanel = SNew(SGridPanel)
		.FillColumn(2, 1.0f);

	for (int32 PlatformIndex = 0; PlatformIndex < AvailablePlatforms.Num(); ++PlatformIndex)
	{
		const PlatformInfo::FPlatformInfo* Platform = AvailablePlatforms[PlatformIndex];

		// platform icon
		PlatformPanel->AddSlot(0, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
					.Image(FEditorStyle::GetBrush(Platform->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)))
			];

		// platform name
		PlatformPanel->AddSlot(1, PlatformIndex)
			.Padding(4.0f, 0.0f, 16.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(Platform->DisplayName)
			];

		// player combo box
		PlatformPanel->AddSlot(2, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				//Platform->PlatformInfoName
			];
	}

	return PlatformPanel;
}


#undef LOCTEXT_NAMESPACE
