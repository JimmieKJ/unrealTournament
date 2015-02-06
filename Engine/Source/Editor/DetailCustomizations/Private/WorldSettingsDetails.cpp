// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "WorldSettingsDetails.h"
#include "SAssetDropTarget.h"
#include "AssetThumbnail.h"

#define LOCTEXT_NAMESPACE "WorldSettingsDetails"


/* IDetailCustomization overrides
 *****************************************************************************/

void FWorldSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("GameMode");
	CustomizeGameInfoProperty("DefaultGameMode", DetailBuilder, Category);

	AddLightmapCustomization(DetailBuilder);
}


/* FWorldSettingsDetails implementation
 *****************************************************************************/


void FWorldSettingsDetails::CustomizeGameInfoProperty( const FName& PropertyName, IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& CategoryBuilder )
{
	// Get the object that we are viewing details of. Expect to only edit one WorldSettings object at a time!
	TArray< TWeakObjectPtr<UObject> > ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);
	UObject* ObjectCustomized = (ObjectsCustomized.Num() > 0) ? ObjectsCustomized[0].Get() : NULL;

	// Allocate customizer object
	GameInfoModeCustomizer = MakeShareable(new FGameModeInfoCustomizer(ObjectCustomized, PropertyName));

	// Then use it to customize
	GameInfoModeCustomizer->CustomizeGameModeSetting(DetailBuilder, CategoryBuilder);
}


void FWorldSettingsDetails::AddLightmapCustomization( IDetailLayoutBuilder& DetailBuilder )
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Lightmass");

	TSharedRef<FLightmapCustomNodeBuilder> LightMapGroupBuilder = MakeShareable(new FLightmapCustomNodeBuilder(DetailBuilder.GetThumbnailPool()));
	const bool bForAdvanced = true;
	Category.AddCustomBuilder(LightMapGroupBuilder, bForAdvanced);
}



FLightmapCustomNodeBuilder::FLightmapCustomNodeBuilder(const TSharedPtr<FAssetThumbnailPool>& InThumbnailPool)
{
	ThumbnailPool = InThumbnailPool;
}


FLightmapCustomNodeBuilder::~FLightmapCustomNodeBuilder()
{
	FEditorDelegates::OnLightingBuildKept.RemoveAll(this);
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
}


void FLightmapCustomNodeBuilder::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;

	FEditorDelegates::OnLightingBuildKept.AddSP(this, &FLightmapCustomNodeBuilder::HandleLightingBuildKept);
	FEditorDelegates::MapChange.AddSP(this, &FLightmapCustomNodeBuilder::HandleMapChanged);
	FEditorDelegates::NewCurrentLevel.AddSP(this, &FLightmapCustomNodeBuilder::HandleNewCurrentLevel);
}


void FLightmapCustomNodeBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	NodeRow.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("LightmapHeaderRowContent", "Lightmaps") )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	];

	NodeRow.ValueContent()
	[
		SNew( STextBlock )
		.Text( this, &FLightmapCustomNodeBuilder::GetLightmapCountText )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	];
}


void FLightmapCustomNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	RefreshLightmapItems();

	ChildrenBuilder.AddChildContent(LOCTEXT("LightMapsFilter", "Lightmaps"))
	.ValueContent()
	.HAlign(HAlign_Fill)
	[
		SAssignNew(LightmapListView, SListView<TSharedPtr<FLightmapItem>>)
		.ListItemsSource(&LightmapItems)
		.OnGenerateRow(this, &FLightmapCustomNodeBuilder::MakeLightMapListViewWidget)
		.OnContextMenuOpening(this, &FLightmapCustomNodeBuilder::OnGetLightMapContextMenuContent)
		.OnMouseButtonDoubleClick(this, &FLightmapCustomNodeBuilder::OnLightMapListMouseButtonDoubleClick)
	];
}


FText FLightmapCustomNodeBuilder::GetLightmapCountText() const
{
	return FText::Format( LOCTEXT("LightmapHeaderRowCount", "{0} Lightmap(s)"), FText::AsNumber(LightmapItems.Num()) );
}


void FLightmapCustomNodeBuilder::HandleLightingBuildKept()
{
	OnRegenerateChildren.ExecuteIfBound();
}


void FLightmapCustomNodeBuilder::HandleMapChanged(uint32 MapChangeFlags)
{
	OnRegenerateChildren.ExecuteIfBound();
}


void FLightmapCustomNodeBuilder::HandleNewCurrentLevel()
{
	OnRegenerateChildren.ExecuteIfBound();
}


TSharedRef<ITableRow> FLightmapCustomNodeBuilder::MakeLightMapListViewWidget(TSharedPtr<FLightmapItem> LightMapItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(LightMapItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FLightmapItem>>, OwnerTable );
	}

	const uint32 ThumbnailResolution = 64;
	const uint32 ThumbnailBoxPadding = 4;
	UObject* LightMapObject = FindObject<UObject>(NULL, *LightMapItem->ObjectPath);
	FAssetData LightMapAssetData(LightMapObject);

	FAssetThumbnailConfig ThumbnailConfig;
	ThumbnailConfig.bAllowFadeIn = true;

	return SNew( STableRow<TSharedPtr<FLightmapItem>>, OwnerTable )
		.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow")
		[
			SNew(SHorizontalBox)

			// Viewport
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew( SBox )
				.WidthOverride( ThumbnailResolution + ThumbnailBoxPadding * 2 )
				.HeightOverride( ThumbnailResolution + ThumbnailBoxPadding * 2 )
				[
					// Drop shadow border
					SNew(SBorder)
					.Padding(ThumbnailBoxPadding)
					.BorderImage( FEditorStyle::GetBrush("ContentBrowser.ThumbnailShadow") )
					[
						LightMapItem->Thumbnail->MakeThumbnailWidget(ThumbnailConfig)
					]
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 1)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("ContentBrowser.AssetTileViewNameFont"))
					.Text( FText::FromName(LightMapAssetData.AssetName) )
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 1)
				[
					// Class
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("ContentBrowser.AssetListViewClassFont"))
					.Text(FText::FromName(LightMapAssetData.AssetClass))
				]
			]
		];
}


TSharedPtr<SWidget> FLightmapCustomNodeBuilder::OnGetLightMapContextMenuContent()
{
	TArray<TSharedPtr<FLightmapItem>> SelectedLightmaps = LightmapListView->GetSelectedItems();
	
	TSharedPtr<FLightmapItem> SelectedLightmap;
	if ( SelectedLightmaps.Num() > 0 )
	{
		SelectedLightmap = SelectedLightmaps[0];
	}

	if ( SelectedLightmap.IsValid() )
	{
		FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL);

		MenuBuilder.BeginSection("LightMapsContextMenuSection", LOCTEXT("LightMapsContextMenuHeading", "Options") );
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ViewLightmapLabel", "View"),
				LOCTEXT("ViewLightmapTooltip", "Opens the texture editor with this lightmap."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FLightmapCustomNodeBuilder::ExecuteViewLightmap, SelectedLightmap->ObjectPath))
				);
		}
		MenuBuilder.EndSection(); //LightMapsContextMenuSection

		return MenuBuilder.MakeWidget();
	}
	
	return NULL;
}


void FLightmapCustomNodeBuilder::OnLightMapListMouseButtonDoubleClick(TSharedPtr<FLightmapItem> SelectedLightmap)
{
	if ( ensure(SelectedLightmap.IsValid()) )
	{
		ExecuteViewLightmap(SelectedLightmap->ObjectPath);
	}
}


void FLightmapCustomNodeBuilder::ExecuteViewLightmap(FString SelectedLightmapPath)
{
	UObject* LightMapObject = FindObject<UObject>(NULL, *SelectedLightmapPath);
	if ( LightMapObject )
	{
		FAssetEditorManager::Get().OpenEditorForAsset(LightMapObject);
	}
}

void FLightmapCustomNodeBuilder::RefreshLightmapItems()
{
	LightmapItems.Empty();

	FWorldContext& Context = GEditor->GetEditorWorldContext();
	UWorld* World = Context.World();
	if ( World )
	{
		TArray<UTexture2D*> LightMapsAndShadowMaps;
		World->GetLightMapsAndShadowMaps(World->GetCurrentLevel(), LightMapsAndShadowMaps);

		for ( auto ObjIt = LightMapsAndShadowMaps.CreateConstIterator(); ObjIt; ++ObjIt )
		{
			UTexture2D* CurrentObject = *ObjIt;
			if (CurrentObject)
			{
				FAssetData AssetData = FAssetData(CurrentObject);
				const uint32 ThumbnailResolution = 64;
				TSharedPtr<FAssetThumbnail> LightMapThumbnail = MakeShareable( new FAssetThumbnail( AssetData, ThumbnailResolution, ThumbnailResolution, ThumbnailPool ) );
				TSharedPtr<FLightmapItem> NewItem = MakeShareable( new FLightmapItem(CurrentObject->GetPathName(), LightMapThumbnail) );
				LightmapItems.Add(NewItem);
			}
		}
	}

	if ( LightmapListView.IsValid() )
	{
		LightmapListView->RequestListRefresh();
	}
}


#undef LOCTEXT_NAMESPACE