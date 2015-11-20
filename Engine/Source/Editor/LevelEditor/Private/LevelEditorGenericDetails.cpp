// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "LevelEditorGenericDetails.h"
#include "LevelEditorActions.h"
#include "AssetSelection.h"
#include "SSurfaceProperties.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "SurfaceIterators.h"
#include "ScopedTransaction.h"

#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "FLevelEditorGenericDetails"

/** Makes a new instance of this detail layout class for a specific detail view requesting it */
TSharedRef<IDetailCustomization> FLevelEditorGenericDetails::MakeInstance()
{
	return MakeShareable( new FLevelEditorGenericDetails );
}

void FLevelEditorGenericDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	FSelectedActorInfo SelectionInfo = AssetSelectionUtils::GetSelectedActorInfo();	
	if( AssetSelectionUtils::IsAnySurfaceSelected( SelectionInfo.SharedWorld ) )
	{
		AddSurfaceDetails( DetailLayout );
	}
}

void FLevelEditorGenericDetails::GetSelectedSurfaceMaterials(IMaterialListBuilder& MaterialList) const
{
	FSelectedActorInfo SelectionInfo = AssetSelectionUtils::GetSelectedActorInfo();	

	for (TSelectedSurfaceIterator<> SurfaceIt(SelectionInfo.SharedWorld); SurfaceIt; ++SurfaceIt)
	{
        FBspSurf* SelectedSurface = *SurfaceIt;

		const bool bCanBeReplaced = true;
		MaterialList.AddMaterial( 0, SelectedSurface->Material, bCanBeReplaced );
	}
}

void FLevelEditorGenericDetails::OnMaterialChanged( UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll )
{
	bool bModelDirtied = false;
	{
		FSelectedActorInfo SelectionInfo = AssetSelectionUtils::GetSelectedActorInfo();	

		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "PolySetMaterial", "Set Material") );
		GEditor->FlagModifyAllSelectedSurfacesInLevels( SelectionInfo.SharedWorld );

		for ( TSelectedSurfaceIterator<> It(SelectionInfo.SharedWorld) ; It ; ++It )
		{
			UModel* Model = It.GetModel();
			const int32 SurfaceIndex = It.GetSurfaceIndex();

			Model->Surfs[SurfaceIndex].Material = NewMaterial;
			const bool bUpdateTexCoords = false;
			const bool bOnlyUpdateSurfaceMaterials = true;
			GEditor->polyUpdateMaster(Model, SurfaceIndex, bUpdateTexCoords, bOnlyUpdateSurfaceMaterials);
			Model->MarkPackageDirty();

			bModelDirtied = true;
		}
	}
	GEditor->RedrawLevelEditingViewports();
	if ( bModelDirtied )
	{
		ULevel::LevelDirtiedEvent.Broadcast();
	}
}

void FLevelEditorGenericDetails::AddSurfaceDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TSharedPtr< const FUICommandList > CommandBindings = LevelEditorModule.GetGlobalLevelEditorActions();

	/** Level editor commands for use with the selection detail view */
	const FLevelEditorCommands& Commands = FLevelEditorCommands::Get();

	FMenuBuilder SelectionBuilder( true, CommandBindings );
	{
		SelectionBuilder.BeginSection("LevelEditorGenericDetailsSurface");
		{
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectAllMatchingBrush );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectAllMatchingTexture );
		}
		SelectionBuilder.EndSection();

		SelectionBuilder.BeginSection("LevelEditorGenericDetailsSurface2");
		{
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectAllAdjacents );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectAllAdjacentCoplanars );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectAllAdjacentWalls );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectAllAdjacentSlants );
		}
		SelectionBuilder.EndSection();

		SelectionBuilder.BeginSection("LevelEditorGenericDetailsSurface3");
		{
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectReverse );
		}
		SelectionBuilder.EndSection();

		SelectionBuilder.BeginSection("LevelEditorGenericDetailsSurface4");
		{
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectMemorize );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectRecall );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectOr );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectAnd );
			SelectionBuilder.AddMenuEntry( Commands.SurfSelectXor );
		}
		SelectionBuilder.EndSection();
	}

	FMenuBuilder AlignmentBuilder( true, CommandBindings );
	{
		AlignmentBuilder.AddMenuEntry( Commands.SurfUnalign );
		AlignmentBuilder.AddMenuEntry( Commands.SurfAlignPlanarAuto );
		AlignmentBuilder.AddMenuEntry( Commands.SurfAlignPlanarWall );
		AlignmentBuilder.AddMenuEntry( Commands.SurfAlignPlanarFloor );
		AlignmentBuilder.AddMenuEntry( Commands.SurfAlignBox );
		AlignmentBuilder.AddMenuEntry( Commands.SurfAlignFit );
	}


	struct Local
	{
		static FReply ExecuteExecCommand(FString InCommand)
		{
			FSelectedActorInfo SelectionInfo = AssetSelectionUtils::GetSelectedActorInfo();	

			GUnrealEd->Exec( SelectionInfo.SharedWorld, *InCommand );
			return FReply::Handled();
		}
	};

	const FSlateFontInfo& FontInfo = IDetailLayoutBuilder::GetDetailFont();

	IDetailCategoryBuilder& MaterialsCategory = DetailBuilder.EditCategory( "Surface Materials", LOCTEXT("BSPSurfaceMaterials", "Surface Materials") );
	{
		FMaterialListDelegates MaterialListDelegates;
		MaterialListDelegates.OnGetMaterials.BindSP(this, &FLevelEditorGenericDetails::GetSelectedSurfaceMaterials);
		MaterialListDelegates.OnMaterialChanged.BindSP(this, &FLevelEditorGenericDetails::OnMaterialChanged);

		TSharedRef<FMaterialList> MaterialList = MakeShareable(new FMaterialList(DetailBuilder, MaterialListDelegates));

		MaterialsCategory.AddCustomBuilder( MaterialList );
	}

	// Add a new section for static meshes
	IDetailCategoryBuilder& BSPCategory = DetailBuilder.EditCategory( "Geometry", LOCTEXT("BSPSurfacesTitle", "Geometry") );
	BSPCategory.AddCustomRow( FText::GetEmpty() )
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 3.0f, 1.0f )
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f )
			[
				SNew( SComboButton )
				.ButtonContent()
				[
					SNew( STextBlock )
					.Text( LOCTEXT("SelectSurfacesMenu", "Select") ) 
					.Font( FontInfo )
				]
				.MenuContent()
				[
					SelectionBuilder.MakeWidget()
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f )
			[
				SNew( SComboButton )
				.ButtonContent()
				[
					SNew( STextBlock )
					.Text( LOCTEXT("AlignSurfaceTexMenu", "Alignment") ) 
					.Font( FontInfo )
				]
				.MenuContent()
				[
					AlignmentBuilder.MakeWidget()
				]
			]
		]
	];

	BSPCategory.AddCustomRow( LOCTEXT("CleanBSPMaterials", "Clean Geometry Materials"), true )
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 2.0f )
		[
			SNew( SButton )
			.ToolTipText( LOCTEXT("CleanBSPMaterials_Tooltip", "Cleans Geometry Materials") )
			.OnClicked( FOnClicked::CreateStatic( &Local::ExecuteExecCommand, FString( TEXT("CLEANBSPMATERIALS") ) ) )
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("CleanBSPMaterials", "Clean Geometry Materials") )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		]
	];

	IDetailCategoryBuilder& SurfaceCategory = DetailBuilder.EditCategory( "Surface Properties", LOCTEXT("BSPSurfaceProperties", "Surface Properties") );
	SurfaceCategory.AddCustomRow( LOCTEXT("BSPSurfaceProperties", "Surface Properties") )
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 3.0f, 1.0f )
		[
			SNew(SSurfaceProperties)
		]
	];
}

#undef LOCTEXT_NAMESPACE
