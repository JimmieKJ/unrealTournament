// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "StaticMeshActorDetails.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/LevelEditorActions.h"

#define LOCTEXT_NAMESPACE "StaticMeshActorDetails"

TSharedRef<IDetailCustomization> FStaticMeshActorDetails::MakeInstance()
{
	return MakeShareable( new FStaticMeshActorDetails );
}

void FStaticMeshActorDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );

	const FLevelEditorCommands& Commands = LevelEditor.GetLevelEditorCommands();
	TSharedRef<const FUICommandList> CommandBindings = LevelEditor.GetGlobalLevelEditorActions();

	FMenuBuilder BlockingVolumeBuilder( true, CommandBindings );
	{
		BlockingVolumeBuilder.BeginSection("StaticMeshActorDetailsBlockingVolume");
		{
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateBoundingBoxVolume, NAME_None, LOCTEXT("CreateBlockingVolume", "Blocking Volume")  );
		}
		BlockingVolumeBuilder.EndSection();

		BlockingVolumeBuilder.BeginSection("StaticMeshActorDetailsBlockingVolume2");
		{
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateHeavyConvexVolume, NAME_None, LOCTEXT("CreateHeavyConvexVolume", "Heavy Convex Volume") );
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateNormalConvexVolume, NAME_None,LOCTEXT("CreateNormalConvexVolume", "Normal Convex Volume") );
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateLightConvexVolume, NAME_None, LOCTEXT("CreateLightConvexVolume", "Light Convex Volume") );
			BlockingVolumeBuilder.AddMenuEntry( Commands.CreateRoughConvexVolume, NAME_None, LOCTEXT("CreateRoughConvexVolume", "Rough Convex Volume") );
		}
		BlockingVolumeBuilder.EndSection();
	}

	IDetailCategoryBuilder& StaticMeshCategory = DetailBuilder.EditCategory( "StaticMesh" );

	// The blocking volume menu is advanced
	const bool bForAdvanced = true;

	const FText CreateBlockingVolumeString = LOCTEXT("BlockingVolumeMenu", "Create Blocking Volume");

	StaticMeshCategory.AddCustomRow( CreateBlockingVolumeString, bForAdvanced )
	.NameContent()
	[
		SNullWidget::NullWidget
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	.MaxDesiredWidth(250)
	[
		SNew(SComboButton)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT("CreateBlockingVolumeTooltip", "Creates a blocking volume from the static mesh"))
		.ButtonContent()
		[
			SNew( STextBlock )
			.Text( CreateBlockingVolumeString ) 
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
		.MenuContent()
		[
			BlockingVolumeBuilder.MakeWidget()
		]
	];
}

#undef LOCTEXT_NAMESPACE
