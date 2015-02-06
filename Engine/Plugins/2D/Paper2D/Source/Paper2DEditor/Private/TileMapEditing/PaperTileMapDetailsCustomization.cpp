// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "PaperTileMapDetailsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "EdModeTileMap.h"
#include "PropertyEditing.h"
#include "AssetToolsModule.h"

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
	MyDetailLayout = &DetailLayout;
	
	FNotifyHook* NotifyHook = DetailLayout.GetPropertyUtilities()->GetNotifyHook();

	bool bEditingActor = false;

	UPaperTileMap* TileMap = nullptr;
	UPaperTileMapComponent* TileComponent = nullptr;
	for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
	{
		UObject* TestObject = SelectedObjects[ObjectIndex].Get();
		if (AActor* CurrentActor = Cast<AActor>(TestObject))
		{
			if (UPaperTileMapComponent* CurrentComponent = CurrentActor->FindComponentByClass<UPaperTileMapComponent>())
			{
				bEditingActor = true;
				TileComponent = CurrentComponent;
				TileMap = CurrentComponent->TileMap;
				break;
			}
		}
		else if (UPaperTileMapComponent* TestComponent = Cast<UPaperTileMapComponent>(TestObject))
		{
			TileComponent = TestComponent;
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
	TileMapComponentPtr = TileComponent;

	IDetailCategoryBuilder& TileMapCategory = DetailLayout.EditCategory("Tile Map");

	TAttribute<EVisibility> InternalInstanceVis = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FPaperTileMapDetailsCustomization::GetVisibilityForInstancedOnlyProperties));

	if (TileComponent != nullptr)
	{
		TileMapCategory
		.AddCustomRow(LOCTEXT( "TileMapInstancingControlsSearchText", "Edit New Promote Asset"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			.FillHeight(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				// Edit button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 2.0f, 0.0f )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.OnClicked(this, &FPaperTileMapDetailsCustomization::EnterTileMapEditingMode)
					.Visibility(this, &FPaperTileMapDetailsCustomization::GetNonEditModeVisibility)
					.Text( LOCTEXT("EditAsset", "Edit") )
					.ToolTipText( LOCTEXT("EditAssetToolTip", "Edit this tile map") )
				]

				// Create new tile map button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 2.0f, 0.0f )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.OnClicked(this, &FPaperTileMapDetailsCustomization::OnNewButtonClicked)
					.Visibility(this, &FPaperTileMapDetailsCustomization::GetNewButtonVisiblity)
					.Text(LOCTEXT("CreateNewInstancedMap", "New"))
					.ToolTipText( LOCTEXT("CreateNewInstancedMapToolTip", "Create a new tile map") )
				]

				// Promote to asset button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 2.0f, 0.0f )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.OnClicked(this, &FPaperTileMapDetailsCustomization::OnPromoteButtonClicked)
					.Visibility(this, &FPaperTileMapDetailsCustomization::GetVisibilityForInstancedOnlyProperties)
					.Text(LOCTEXT("PromoteToAsset", "Promote to asset"))
					.ToolTipText(LOCTEXT("PromoteToAssetToolTip", "Save this tile map as a reusable asset"))
				]
			]
		];

		TileMapCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UPaperTileMapComponent, TileMap));
	}

	// Add the layer browser
	if (TileMap != nullptr)
	{
		TAttribute<EVisibility> LayerBrowserVis;
		LayerBrowserVis.Set(EVisibility::Visible);
		if (TileComponent != nullptr)
		{
			LayerBrowserVis = InternalInstanceVis;
		}

		FText TileLayerListText = LOCTEXT("TileLayerList", "Tile layer list");
		TileMapCategory.AddCustomRow(TileLayerListText)
		.Visibility(LayerBrowserVis)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(DetailLayout.GetDetailFont())
				.Text(TileLayerListText)
			]
			+SVerticalBox::Slot()
			[
				SNew(STileLayerList, TileMap, NotifyHook)
			]
		];
	}

	// Add all of the properties from the inline tilemap
	if ((TileComponent != nullptr) && (TileComponent->OwnsTileMap()))
	{
		TArray<UObject*> ListOfTileMaps;
		ListOfTileMaps.Add(TileMap);

		for (TFieldIterator<UProperty> PropIt(UPaperTileMap::StaticClass()); PropIt; ++PropIt)
		{
			UProperty* TestProperty = *PropIt;

			if (TestProperty->HasAnyPropertyFlags(CPF_Edit))
			{
				FName CategoryName(*TestProperty->GetMetaData(TEXT("Category")));
				IDetailCategoryBuilder& Category = DetailLayout.EditCategory(CategoryName);

				if (IDetailPropertyRow* ExternalRow = Category.AddExternalProperty(ListOfTileMaps, TestProperty->GetFName()))
				{
					ExternalRow->Visibility(InternalInstanceVis);
				}
			}
		}
	}

	// Make sure the setup category is near the top
	DetailLayout.EditCategory("Setup");
}

FReply FPaperTileMapDetailsCustomization::EnterTileMapEditingMode()
{
	if (UPaperTileMapComponent* TileMapComponent = TileMapComponentPtr.Get())
	{
		if (TileMapComponent->OwnsTileMap())
		{
			GLevelEditorModeTools().ActivateMode(FEdModeTileMap::EM_TileMap);
		}
		else
		{
			FAssetEditorManager::Get().OpenEditorForAsset(TileMapComponent->TileMap);
		}
	}
	return FReply::Handled();
}

FReply FPaperTileMapDetailsCustomization::OnNewButtonClicked()
{
	if (UPaperTileMapComponent* TileMapComponent = TileMapComponentPtr.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("CreateNewTileMap", "New Tile Map"));
		TileMapComponent->Modify();
		TileMapComponent->CreateNewOwnedTileMap();

		MyDetailLayout->ForceRefreshDetails();
	}

	return FReply::Handled();
}

FReply FPaperTileMapDetailsCustomization::OnPromoteButtonClicked()
{
	if (UPaperTileMapComponent* TileMapComponent = TileMapComponentPtr.Get())
	{
		if (TileMapComponent->OwnsTileMap())
		{
			if (TileMapComponent->TileMap != nullptr)
			{
				// Try promoting the tile map to be an asset (prompts for a name&path, creates a package and then calls the factory, which renames the existing asset and sets RF_Public)
				UPaperTileMapPromotionFactory* PromotionFactory = NewObject<UPaperTileMapPromotionFactory>();
				PromotionFactory->AssetToRename = TileMapComponent->TileMap;

				FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(PromotionFactory->GetSupportedClass(), PromotionFactory);
			}
		}
	}

	return FReply::Handled();
}

EVisibility FPaperTileMapDetailsCustomization::GetNonEditModeVisibility() const
{
	return GLevelEditorModeTools().IsModeActive(FEdModeTileMap::EM_TileMap) ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FPaperTileMapDetailsCustomization::GetNewButtonVisiblity() const
{
	return (TileMapComponentPtr.Get() != nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FPaperTileMapDetailsCustomization::GetVisibilityForInstancedOnlyProperties() const
{
	if (UPaperTileMapComponent* TileMapComponent = TileMapComponentPtr.Get())
	{
		if (TileMapComponent->OwnsTileMap())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE