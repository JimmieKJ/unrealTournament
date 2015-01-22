// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "EdModeTileMap.h"
#include "TileMapEdModeToolkit.h"
#include "../TileSetEditor.h"
#include "SContentReference.h"
#include "PaperEditorCommands.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// FTileMapEdModeToolkit

FTileMapEdModeToolkit::FTileMapEdModeToolkit(class FEdModeTileMap* InOwningMode)
{
	TileMapEditor = InOwningMode;
}

void FTileMapEdModeToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
}

void FTileMapEdModeToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
}

FName FTileMapEdModeToolkit::GetToolkitFName() const
{
	return FName("TileMapToolkit");
}

FText FTileMapEdModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT( "TileMapAppLabel", "Tile Map Editor" );
}

FText FTileMapEdModeToolkit::GetToolkitName() const
{
	if ( CurrentTileSetPtr.IsValid() )
	{
		const bool bDirtyState = CurrentTileSetPtr->GetOutermost()->IsDirty();

		FFormatNamedArguments Args;
		Args.Add(TEXT("TileSetName"), FText::FromString(CurrentTileSetPtr->GetName()));
		Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty());
		return FText::Format(LOCTEXT("TileMapEditAppLabel", "{TileSetName}{DirtyState}"), Args);
	}
	return GetBaseToolkitName();
}

FEdMode* FTileMapEdModeToolkit::GetEditorMode() const
{
	return TileMapEditor;
}

TSharedPtr<SWidget> FTileMapEdModeToolkit::GetInlineContent() const
{
	return MyWidget;
}

void FTileMapEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	const float ContentRefWidth = 140.0f;

	BindCommands();

	// Try to determine a good default tile set based on the current selection set
	if (UPaperTileMapComponent* TargetComponent = TileMapEditor->FindSelectedComponent())
	{
		if (TargetComponent->TileMap != nullptr)
		{
			CurrentTileSetPtr = TargetComponent->TileMap->SelectedTileSet.Get();
		}
	}

	TileSetPalette = SNew(STileSetSelectorViewport, CurrentTileSetPtr.Get(), TileMapEditor);

	// Create the contents of the editor mode toolkit
	MyWidget = 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ))
		.Content()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(4.0f)
			[
				BuildToolBar()
			]

			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				.Visibility(this, &FTileMapEdModeToolkit::GetTileSetSelectorVisibility)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CurrentTileSetAssetToPaintWith", "Active Set"))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Right)
					[
						SNew(SContentReference)
						.WidthOverride(ContentRefWidth)
						.AssetReference(this, &FTileMapEdModeToolkit::GetCurrentTileSet)
						.OnSetReference(this, &FTileMapEdModeToolkit::OnChangeTileSet)
						.AllowedClass(UPaperTileSet::StaticClass())
						.AllowSelectingNewAsset(true)
						.AllowClearingReference(false)
					]
				]

				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.VAlign(VAlign_Fill)
				.Padding(4.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						TileSetPalette.ToSharedRef()
					]
				]
			]
		];

	FModeToolkit::Init(InitToolkitHost);
}

void FTileMapEdModeToolkit::OnChangeTileSet(UObject* NewAsset)
{
	if (UPaperTileSet* NewTileSet = Cast<UPaperTileSet>(NewAsset))
	{
		if (CurrentTileSetPtr.Get() != NewTileSet)
		{
			CurrentTileSetPtr = NewTileSet;
			if (TileSetPalette.IsValid())
			{
				TileSetPalette->ChangeTileSet(NewTileSet);
			}

			// Save the newly selected tile set in the asset so it can be restored next time we edit it
			if (UPaperTileMapComponent* TargetComponent = TileMapEditor->FindSelectedComponent())
			{
				if (TargetComponent->TileMap != nullptr)
				{
					TargetComponent->TileMap->SelectedTileSet = NewTileSet;
				}
			}
		}
	}
}

UObject* FTileMapEdModeToolkit::GetCurrentTileSet() const
{
	return CurrentTileSetPtr.Get();
}

void FTileMapEdModeToolkit::BindCommands()
{
	UICommandList = MakeShareable(new FUICommandList());

	const FPaperEditorCommands& Commands = FPaperEditorCommands::Get();

	UICommandList->MapAction(
		Commands.SelectPaintTool,
		FExecuteAction::CreateSP(this, &FTileMapEdModeToolkit::OnSelectTool, ETileMapEditorTool::Paintbrush),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FTileMapEdModeToolkit::IsToolSelected, ETileMapEditorTool::Paintbrush) );
	UICommandList->MapAction(
		Commands.SelectEraserTool,
		FExecuteAction::CreateSP(this, &FTileMapEdModeToolkit::OnSelectTool, ETileMapEditorTool::Eraser),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FTileMapEdModeToolkit::IsToolSelected, ETileMapEditorTool::Eraser) );
	UICommandList->MapAction(
		Commands.SelectFillTool,
		FExecuteAction::CreateSP(this, &FTileMapEdModeToolkit::OnSelectTool, ETileMapEditorTool::PaintBucket),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FTileMapEdModeToolkit::IsToolSelected, ETileMapEditorTool::PaintBucket) );

	UICommandList->MapAction(
		Commands.SelectVisualLayersPaintingMode,
		FExecuteAction::CreateSP(this, &FTileMapEdModeToolkit::OnSelectLayerPaintingMode, ETileMapLayerPaintingMode::VisualLayers),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FTileMapEdModeToolkit::IsLayerPaintingModeSelected, ETileMapLayerPaintingMode::VisualLayers) );
	UICommandList->MapAction(
		Commands.SelectCollisionLayersPaintingMode,
		FExecuteAction::CreateSP(this, &FTileMapEdModeToolkit::OnSelectLayerPaintingMode, ETileMapLayerPaintingMode::CollisionLayers),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FTileMapEdModeToolkit::IsLayerPaintingModeSelected, ETileMapLayerPaintingMode::CollisionLayers) );
}

void FTileMapEdModeToolkit::OnSelectTool(ETileMapEditorTool::Type NewTool)
{
	TileMapEditor->SetActiveTool(NewTool);
}

bool FTileMapEdModeToolkit::IsToolSelected(ETileMapEditorTool::Type QueryTool) const
{
	return (TileMapEditor->GetActiveTool() == QueryTool);
}

void FTileMapEdModeToolkit::OnSelectLayerPaintingMode(ETileMapLayerPaintingMode::Type NewMode)
{
	TileMapEditor->SetActiveLayerPaintingMode(NewMode);
}

bool FTileMapEdModeToolkit::IsLayerPaintingModeSelected(ETileMapLayerPaintingMode::Type PaintingMode) const
{
	return (TileMapEditor->GetActiveLayerPaintingMode() == PaintingMode);
}

EVisibility FTileMapEdModeToolkit::GetTileSetSelectorVisibility() const
{
	bool bShouldShowSelector = (TileMapEditor->GetActiveLayerPaintingMode() == ETileMapLayerPaintingMode::VisualLayers);
	
	return bShouldShowSelector ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FTileMapEdModeToolkit::BuildToolBar() const
{
	const FPaperEditorCommands& Commands = FPaperEditorCommands::Get();

	FToolBarBuilder ToolsToolbar(UICommandList, FMultiBoxCustomization::None);
	{
		ToolsToolbar.AddToolBarButton(Commands.SelectPaintTool);
		ToolsToolbar.AddToolBarButton(Commands.SelectEraserTool);
		ToolsToolbar.AddToolBarButton(Commands.SelectFillTool);
	}

	FToolBarBuilder PaintingModeToolbar(UICommandList, FMultiBoxCustomization::None);
	{
		PaintingModeToolbar.AddToolBarButton(Commands.SelectVisualLayersPaintingMode);
		PaintingModeToolbar.AddToolBarButton(Commands.SelectCollisionLayersPaintingMode);
	}

	return
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Left)
		.Padding(4,0)
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
			[
				PaintingModeToolbar.MakeWidget()
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4,0)
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
			[
				ToolsToolbar.MakeWidget()
			]
		];
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
