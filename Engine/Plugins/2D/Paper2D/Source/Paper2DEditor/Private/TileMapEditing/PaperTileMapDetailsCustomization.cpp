// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "PaperTileMapDetailsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "EdModeTileMap.h"
#include "PropertyEditing.h"

#include "STileLayerList.h"
#include "ScopedTransaction.h"
#include "IPropertyUtilities.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// FPaperTileMapDetailsCustomization

TSharedRef<IDetailCustomization> FPaperTileMapDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FPaperTileMapDetailsCustomization);
}

void FPaperTileMapDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetDetailsView().GetSelectedObjects();
	
	FNotifyHook* NotifyHook = DetailLayout.GetPropertyUtilities()->GetNotifyHook();

	bool bEditingActor = false;

	UPaperTileMap* TileMap = nullptr;
	for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
	{
		UObject* TestObject = SelectedObjects[ObjectIndex].Get();
		if (AActor* CurrentActor = Cast<AActor>(TestObject))
		{
			if (UPaperTileMapComponent* CurrentComponent = CurrentActor->FindComponentByClass<UPaperTileMapComponent>())
			{
				bEditingActor = true;
				TileMap = CurrentComponent->TileMap;
				break;
			}
		}
		else if (UPaperTileMapComponent* TestComponent = Cast<UPaperTileMapComponent>(TestObject))
		{
			TileMap = TestComponent->TileMap;
			break;
		}
		else if (UPaperTileMap* TestTileMap = Cast<UPaperTileMap>(TestObject))
		{
			TileMap = TestTileMap;
			break;
		}
	}
	TileMapPtr = TileMap;

	IDetailCategoryBuilder& TileMapCategory = DetailLayout.EditCategory("TileMap");

	if (bEditingActor)
	{
		TileMapCategory
		.AddCustomRow(LOCTEXT( "EnterTileMapEditMode", "Enter Edit Mode" ))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(3)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FPaperTileMapDetailsCustomization::EnterTileMapEditingMode)
				.Visibility(this, &FPaperTileMapDetailsCustomization::GetNonEditModeVisibility)
				.Text( LOCTEXT( "EnterTileMapEditMode", "Enter Edit Mode" ) )
			]
		];
	}

	//@TODO: Handle showing layers when multiple tile maps are selected
	if (TileMap != nullptr)
	{
		IDetailCategoryBuilder& LayersCategory = DetailLayout.EditCategory("Tile Layers");

		LayersCategory.AddCustomRow(LOCTEXT("TileLayerList", "Tile layer list"))
		[
			SNew(STileLayerList, TileMap, NotifyHook)
		];
	}

	// Make sure the setup category is near the top
	DetailLayout.EditCategory("Setup");
}

FReply FPaperTileMapDetailsCustomization::EnterTileMapEditingMode()
{
	GLevelEditorModeTools().ActivateMode(FEdModeTileMap::EM_TileMap);
	return FReply::Handled();
}

EVisibility FPaperTileMapDetailsCustomization::GetNonEditModeVisibility() const
{
	return GLevelEditorModeTools().IsModeActive(FEdModeTileMap::EM_TileMap) ? EVisibility::Collapsed : EVisibility::Visible;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE