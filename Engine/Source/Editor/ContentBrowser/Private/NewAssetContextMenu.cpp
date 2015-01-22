// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "NewAssetContextMenu.h"
#include "IDocumentation.h"
#include "EditorClassUtils.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

struct FFactoryItem
{
	UFactory* Factory;
	FText DisplayName;

	FFactoryItem(UFactory* InFactory, const FText& InDisplayName)
		: Factory(InFactory), DisplayName(InDisplayName)
	{}
};

TArray<FFactoryItem> FindFactoriesInCategory(EAssetTypeCategories::Type AssetTypeCategory)
{
	TArray<FFactoryItem> FactoriesInThisCategory;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UFactory::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			UFactory* Factory = Class->GetDefaultObject<UFactory>();
			if (Factory->ShouldShowInNewMenu() && ensure(!Factory->GetDisplayName().IsEmpty()))
			{
				uint32 FactoryCategories = Factory->GetMenuCategories();

				if (FactoryCategories & AssetTypeCategory)
				{
					new(FactoriesInThisCategory)FFactoryItem(Factory, Factory->GetDisplayName());
				}
			}
		}
	}

	return FactoriesInThisCategory;
}

class SFactoryMenuEntry : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SFactoryMenuEntry )
		: _Width(32)
		, _Height(32)
		{}
		SLATE_ARGUMENT( uint32, Width )
		SLATE_ARGUMENT( uint32, Height )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	Factory				The factory this menu entry represents
	 */
	void Construct( const FArguments& InArgs, UFactory* Factory )
	{
		// Since we are only displaying classes, we need no thumbnail pool.
		Thumbnail = MakeShareable( new FAssetThumbnail( Factory->GetSupportedClass(), InArgs._Width, InArgs._Height, TSharedPtr<FAssetThumbnailPool>() ) );

		FName ClassThumbnailBrushOverride = Factory->GetNewAssetThumbnailOverride();
		TSharedPtr<SWidget> ThumbnailWidget;
		if ( ClassThumbnailBrushOverride != NAME_None )
		{
			const bool bAllowFadeIn = false;
			const bool bForceGenericThumbnail = true;
			const bool bAllowHintText = false;
			ThumbnailWidget = Thumbnail->MakeThumbnailWidget(bAllowFadeIn, bForceGenericThumbnail, EThumbnailLabel::AssetName, FText(), FLinearColor(0,0,0,0), bAllowHintText, ClassThumbnailBrushOverride);
		}
		else
		{
			ThumbnailWidget = Thumbnail->MakeThumbnailWidget();
		}

		ChildSlot
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 4, 0, 0, 0 )
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride( Thumbnail->GetSize().X + 4 )
				.HeightOverride( Thumbnail->GetSize().Y + 4 )
				[
					ThumbnailWidget.ToSharedRef()
				]
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 4, 0)
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.Padding(0, 0, 0, 1)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle("LevelViewportContextMenu.AssetLabel.Text.Font") )
					.Text( Factory->GetDisplayName() )
				]
			]
		];

		
		SetToolTip(IDocumentation::Get()->CreateToolTip(Factory->GetToolTip(), nullptr, Factory->GetToolTipDocumentationPage(), Factory->GetToolTipDocumentationExcerpt()));
	}

private:
	TSharedPtr< FAssetThumbnail > Thumbnail;
};

void FNewAssetContextMenu::MakeContextMenu(FMenuBuilder& MenuBuilder, const FString& InPath, const FOnNewAssetRequested& InOnNewAssetRequested, const FOnNewFolderRequested& InOnNewFolderRequested)
{
	if(InOnNewFolderRequested.IsBound() && GetDefault<UContentBrowserSettings>()->DisplayFolders)
	{
		MenuBuilder.BeginSection("ContentBrowserNewFolder", LOCTEXT("FolderMenuHeading", "Folder") );
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("NewFolderLabel", "New Folder"),
				LOCTEXT("NewFolderTooltip", "Create a new folder at this location."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.NewFolderIcon"),
				FUIAction(FExecuteAction::CreateStatic(&FNewAssetContextMenu::ExecuteNewFolder, InPath, InOnNewFolderRequested))
				);
		}
		MenuBuilder.EndSection(); //ContentBrowserNewFolder
	}

	// The new style menu
	MenuBuilder.BeginSection( "ContentBrowserCreateAsset", LOCTEXT( "ImportAssetMenuHeading", "Import Asset" ) );
	{
		// Get the import label
		FText ImportAssetLabel;
		if ( !InPath.IsEmpty() )
		{
			ImportAssetLabel = FText::Format( LOCTEXT( "ImportAsset", "Import to {0}..." ), FText::FromString( InPath ) );
		}
		else
		{
			ImportAssetLabel = LOCTEXT( "ImportAsset_NoPath", "Import" );
		}

		MenuBuilder.AddMenuEntry(
			ImportAssetLabel,
			LOCTEXT( "ImportAssetTooltip", "Imports an asset from file to this folder." ),
			FSlateIcon( FEditorStyle::GetStyleSetName(), "ContentBrowser.ImportIcon" ),
			FUIAction(
			FExecuteAction::CreateStatic( &FNewAssetContextMenu::ExecuteImportAsset, InPath ),
			FCanExecuteAction::CreateStatic( &FNewAssetContextMenu::IsAssetPathSelected, InPath )
			)
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ContentBrowserNewBasicAsset", LOCTEXT("BasicAssetsMenuHeading", "Create Basic Asset") );
	{
		CreateNewAssetMenuCategory(MenuBuilder, EAssetTypeCategories::Basic, InPath, InOnNewAssetRequested);
	}
	MenuBuilder.EndSection(); //ContentBrowserNewBasicAsset

	MenuBuilder.BeginSection("ContentBrowserNewAdvancedAsset", LOCTEXT("AdvancedAssetsMenuHeading", "Create Advanced Asset"));
	{
		{
			TArray<FFactoryItem> AnimationFactories = FindFactoriesInCategory(EAssetTypeCategories::Animation);
			if (AnimationFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("AnimationAssetCategory", "Animation"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::Animation, InPath, InOnNewAssetRequested)
					);
			}
		}

		{
			TArray<FFactoryItem> BlueprintFactories = FindFactoriesInCategory(EAssetTypeCategories::Blueprint);
			if (BlueprintFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("BlueprintAssetCategory", "Blueprints"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::Blueprint, InPath, InOnNewAssetRequested)
					);
			}
		}

		{
			TArray<FFactoryItem> MaterialFactories = FindFactoriesInCategory(EAssetTypeCategories::MaterialsAndTextures);
			if (MaterialFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("MaterialAssetCategory", "Materials & Textures"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::MaterialsAndTextures, InPath, InOnNewAssetRequested)
					);
			}
	}

		{
			TArray<FFactoryItem> SoundFactories = FindFactoriesInCategory(EAssetTypeCategories::Sounds);
			if (SoundFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("SoundAssetCategory", "Sounds"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::Sounds, InPath, InOnNewAssetRequested)
					);
			}
		}

		{
			TArray<FFactoryItem> PhysicsFactories = FindFactoriesInCategory(EAssetTypeCategories::Physics);
			if (PhysicsFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("PhysicsAssetCategory", "Physics"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::Physics, InPath, InOnNewAssetRequested)
					);
			}
		}

		{
			TArray<FFactoryItem> UIFactories = FindFactoriesInCategory(EAssetTypeCategories::UI);
			if (UIFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("UserInterfaceAssetCategory", "User Interface"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::UI, InPath, InOnNewAssetRequested)
					);
			}
		}

		{
			TArray<FFactoryItem> MiscFactories = FindFactoriesInCategory(EAssetTypeCategories::Misc);
			if (MiscFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("MiscellaneousAssetCategory", "Miscellaneous"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::Misc, InPath, InOnNewAssetRequested)
					);
			}
		}

		{
			TArray<FFactoryItem> GameplayFactories = FindFactoriesInCategory(EAssetTypeCategories::Gameplay);
			if (GameplayFactories.Num() > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("GameplayAssetCategory", "Gameplay"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&FNewAssetContextMenu::CreateNewAssetMenuCategory, EAssetTypeCategories::Gameplay, InPath, InOnNewAssetRequested)
					);
			}
		}
	}
	MenuBuilder.EndSection(); //ContentBrowserNewAdvancedAsset
}

void FNewAssetContextMenu::CreateNewAssetMenuCategory(FMenuBuilder& MenuBuilder, EAssetTypeCategories::Type AssetTypeCategory, FString InPath, FOnNewAssetRequested InOnNewAssetRequested)
{
	// Find UFactory classes that can create new objects in this category.
	TArray<FFactoryItem> FactoriesInThisCategory = FindFactoriesInCategory(AssetTypeCategory);

	// Sort the list
	struct FCompareFactoryDisplayNames
	{
		FORCEINLINE bool operator()( const FFactoryItem& A, const FFactoryItem& B ) const
		{
			return A.DisplayName.CompareToCaseIgnored(B.DisplayName) < 0;
		}
	};
	FactoriesInThisCategory.Sort( FCompareFactoryDisplayNames() );

	// Add menu entries for each one
	for ( auto FactoryIt = FactoriesInThisCategory.CreateConstIterator(); FactoryIt; ++FactoryIt )
	{
		UFactory* Factory = (*FactoryIt).Factory;
		TWeakObjectPtr<UClass> WeakFactoryClass = Factory->GetClass();

		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateStatic( &FNewAssetContextMenu::ExecuteNewAsset, InPath, WeakFactoryClass, InOnNewAssetRequested ),
				FCanExecuteAction::CreateStatic( &FNewAssetContextMenu::IsAssetPathSelected, InPath )
				),
			SNew( SFactoryMenuEntry, Factory )
			);
	}
}

bool FNewAssetContextMenu::IsAssetPathSelected(FString InPath)
{
	return !InPath.IsEmpty();
}

void FNewAssetContextMenu::ExecuteImportAsset( FString InPath )
{
	if ( ensure( !InPath.IsEmpty() ) )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
		AssetToolsModule.Get().ImportAssets( InPath );
	}
}

void FNewAssetContextMenu::ExecuteNewAsset(FString InPath, TWeakObjectPtr<UClass> FactoryClass, FOnNewAssetRequested InOnNewAssetRequested)
{
	if ( ensure(FactoryClass.IsValid()) && ensure(!InPath.IsEmpty()) )
	{
		InOnNewAssetRequested.ExecuteIfBound(InPath, FactoryClass);
	}
}

void FNewAssetContextMenu::ExecuteNewFolder(FString InPath, FOnNewFolderRequested InOnNewFolderRequested)
{
	if(ensure(!InPath.IsEmpty()) )
	{
		InOnNewFolderRequested.ExecuteIfBound(InPath);
	}
}

#undef LOCTEXT_NAMESPACE
