// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"

#include "GeneralProjectSettingsDetails.h"
#include "SExternalImageReference.h"

#define LOCTEXT_NAMESPACE "FGeneralProjectSettingsDetails"

/////////////////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FGeneralProjectSettingsDetails::MakeInstance()
{
	return MakeShareable(new FGeneralProjectSettingsDetails);
}

void FGeneralProjectSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ImageCategory = DetailBuilder.EditCategory(TEXT("About"));

	const FText ProjectThumbnailDesc(LOCTEXT("ProjectThumbnailLabel", "Project Thumbnail"));

	const FString ProjectThumbnail_TargetImagePath = FPaths::GetPath(FPaths::GetProjectFilePath()) / FString::Printf(TEXT("%s.png"), FApp::GetGameName());
	FString ProjectThumbnail_AutomaticImagePath = FPaths::GameSavedDir() / TEXT("AutoScreenshot.png");
	if (!FPaths::FileExists(ProjectThumbnail_AutomaticImagePath))
	{
		ProjectThumbnail_AutomaticImagePath = FPaths::EngineContentDir() / TEXT("Editor") / TEXT("Slate") / TEXT("GameProjectDialog") / TEXT("default_game_thumbnail_192x.png");
	}

	ImageCategory.AddCustomRow(ProjectThumbnailDesc)
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(ProjectThumbnailDesc)
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, ProjectThumbnail_AutomaticImagePath, ProjectThumbnail_TargetImagePath)
			.FileDescription(ProjectThumbnailDesc)
			.RequiredSize(FIntPoint(192, 192))
		]
	];

}

#undef LOCTEXT_NAMESPACE
