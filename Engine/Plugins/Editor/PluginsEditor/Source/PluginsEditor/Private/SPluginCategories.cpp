// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginsEditorPrivatePCH.h"
#include "SPluginCategories.h"
#include "PluginCategoryTreeItem.h"
#include "IPluginManager.h"
#include "SPluginCategoryTreeItem.h"
#include "SPluginsEditor.h"

#define LOCTEXT_NAMESPACE "PluginCategories"



void SPluginCategories::Construct( const FArguments& Args, const TSharedRef< SPluginsEditor > Owner )
{
	OwnerWeak = Owner;

	bNeedsRefresh = false;
	RebuildAndFilterCategoryTree();

	PluginCategoryTreeView =
		SNew( SPluginCategoryTreeView )

		// For now we only support selecting a single folder in the tree
		.SelectionMode( ESelectionMode::Single )
		.ClearSelectionOnClick( false )		// Don't allow user to select nothing.  We always expect a category to be selected!

		.TreeItemsSource( &RootPluginCategories )
		.OnGenerateRow( this, &SPluginCategories::PluginCategoryTreeView_OnGenerateRow ) 
		.OnGetChildren( this, &SPluginCategories::PluginCategoryTreeView_OnGetChildren )

		.OnSelectionChanged( this, &SPluginCategories::PluginCategoryTreeView_OnSelectionChanged )
		;

	// Expand the root categories by default
	for( auto RootCategoryIt( RootPluginCategories.CreateConstIterator() ); RootCategoryIt; ++RootCategoryIt )
	{
		const auto& Category = *RootCategoryIt;
		PluginCategoryTreeView->SetItemExpansion( Category, true );
	}

	// Select the first item by default
	if( RootPluginCategories.Num() > 0 )
	{
		PluginCategoryTreeView->SetSelection( RootPluginCategories[ 0 ] );
	}

	ChildSlot.AttachWidget( PluginCategoryTreeView.ToSharedRef() );
}


SPluginCategories::~SPluginCategories()
{
}


/** @return Gets the owner of this list */
SPluginsEditor& SPluginCategories::GetOwner()
{
	return *OwnerWeak.Pin();
}


void SPluginCategories::RebuildAndFilterCategoryTree()
{
	{
		RootPluginCategories.Reset();

		// Add the root level categories
		TSharedRef<FPluginCategoryTreeItem> BuiltInCategory = MakeShareable(new FPluginCategoryTreeItem(NULL, TEXT("BuiltIn"), TEXT("Built-in"), LOCTEXT("BuiltInCategoryName", "Built-in")));
		RootPluginCategories.Add( BuiltInCategory );
		TSharedRef<FPluginCategoryTreeItem> InstalledCategory = MakeShareable(new FPluginCategoryTreeItem(NULL, TEXT("Installed"), TEXT("Installed"), LOCTEXT("InstalledCategoryName", "Installed")));
		RootPluginCategories.Add( InstalledCategory );


		const TArray< FPluginStatus > Plugins = IPluginManager::Get().QueryStatusForAllPlugins();
		for( auto PluginIt( Plugins.CreateConstIterator() ); PluginIt; ++PluginIt )
		{
			const auto& PluginStatus = *PluginIt;


			// Figure out which base category this plugin belongs in
			FPluginCategoryTreeItemPtr CategoryForPlugin;
			if( PluginStatus.bIsBuiltIn )
			{
				CategoryForPlugin = BuiltInCategory;
			}
			else
			{
				CategoryForPlugin = InstalledCategory;
			}
			check( CategoryForPlugin.IsValid() );
			FString ItemCategoryPath = CategoryForPlugin->GetCategoryPath();


			const FString& CategoryPath = PluginStatus.CategoryPath;
			{
				// We're expecting the category string to be in the "A.B.C" format.  We'll split up the string here and form
				// a proper hierarchy in the UI
				TArray< FString > SplitCategories;
				CategoryPath.ParseIntoArray( &SplitCategories, TEXT( "." ), true /* bCullEmpty */ );

				// Make sure all of the categories exist
				for( auto SplitCategoryIt( SplitCategories.CreateConstIterator() ); SplitCategoryIt; ++SplitCategoryIt )
				{
					const auto& SplitCategory = *SplitCategoryIt;

					if( !ItemCategoryPath.IsEmpty() )
					{
						ItemCategoryPath += TEXT( '.' );
					}
					ItemCategoryPath += SplitCategory;

					// Locate this category at the level we're at in the hierarchy
					FPluginCategoryTreeItemPtr FoundCategory = NULL;
					TArray< FPluginCategoryTreeItemPtr >& TestCategoryList = CategoryForPlugin.IsValid() ? CategoryForPlugin->AccessSubCategories() : RootPluginCategories;
					for( auto TestCategoryIt( TestCategoryList.CreateConstIterator() ); TestCategoryIt; ++TestCategoryIt )
					{
						const auto& TestCategory = *TestCategoryIt;
						if( TestCategory->GetCategoryName() == SplitCategory )
						{
							// Found it!
							FoundCategory = TestCategory;
							break;
						}
					}

					if( !FoundCategory.IsValid() )
					{
						// OK, this is a new category name for us, so add it now!
						const auto ParentCategory = CategoryForPlugin;

						//@todo Allow for properly localized category names [3/7/2014 justin.sargent]
						FoundCategory = MakeShareable(new FPluginCategoryTreeItem(ParentCategory, ItemCategoryPath, SplitCategory, FText::FromString(SplitCategory)));
						TestCategoryList.Add( FoundCategory );
					}

					// Descend the hierarchy for the next category
					CategoryForPlugin = FoundCategory;
				}
			}

			check( CategoryForPlugin.IsValid() );
			
			// Associate the plugin with the category
			// PERFORMANCE NOTE: Copying FPluginStats by value here
			CategoryForPlugin->AddPlugin( MakeShareable( new FPluginStatus( PluginStatus ) ) );
		}


		// Sort every single category alphabetically
		{
			struct FPluginCategoryTreeItemSorter
			{
				bool operator()( const FPluginCategoryTreeItemPtr& A, const FPluginCategoryTreeItemPtr& B ) const
				{
					return A->GetCategoryDisplayName().CompareTo( B->GetCategoryDisplayName() ) == -1;
				}

				static void SortCategoriesRecursively( TArray< FPluginCategoryTreeItemPtr >& Categories )
				{
					// Sort the categories
					Categories.Sort( FPluginCategoryTreeItemSorter() );

					// Sort sub-categories
					for( auto SubCategoryIt( Categories.CreateConstIterator() ); SubCategoryIt; ++SubCategoryIt )
					{
						const auto& SubCategory = *SubCategoryIt;
						SortCategoriesRecursively( SubCategory->AccessSubCategories() );
					}
				}
			};

			FPluginCategoryTreeItemSorter::SortCategoriesRecursively( RootPluginCategories );
		}
	}

	if( PluginCategoryTreeView.IsValid() )
	{
		PluginCategoryTreeView->RequestTreeRefresh();
	}

	bNeedsRefresh = false;
}


void SPluginCategories::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Call parent implementation
	SCompoundWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );

	if( bNeedsRefresh )
	{
		RebuildAndFilterCategoryTree();
	}
}


TSharedRef<ITableRow> SPluginCategories::PluginCategoryTreeView_OnGenerateRow( FPluginCategoryTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable )
{
	return
		SNew( STableRow< FPluginCategoryTreeItemPtr >, OwnerTable )
		[
			SNew( SPluginCategoryTreeItem, SharedThis( this ), Item.ToSharedRef() )
		];
}


void SPluginCategories::PluginCategoryTreeView_OnGetChildren( FPluginCategoryTreeItemPtr Item, TArray< FPluginCategoryTreeItemPtr >& OutChildren )
{
	const auto& SubCategories = Item->GetSubCategories();
	OutChildren.Append( SubCategories );
}


void SPluginCategories::PluginCategoryTreeView_OnSelectionChanged( FPluginCategoryTreeItemPtr Item, ESelectInfo::Type SelectInfo )
{
	// Selection changed, which may affect which plugins are displayed in the list.  We need to invalidate the list.
	OwnerWeak.Pin()->OnCategorySelectionChanged();
}


FPluginCategoryTreeItemPtr SPluginCategories::GetSelectedCategory() const
{
	if( PluginCategoryTreeView.IsValid() )
	{
		auto SelectedItems = PluginCategoryTreeView->GetSelectedItems();
		if( SelectedItems.Num() > 0 )
		{
			const auto& SelectedCategoryItem = SelectedItems[ 0 ];
			return SelectedCategoryItem;
		}
	}

	return NULL;
}


void SPluginCategories::SelectCategory( const FPluginCategoryTreeItemPtr& CategoryToSelect )
{
	if( ensure( PluginCategoryTreeView.IsValid() ) )
	{
		PluginCategoryTreeView->SetSelection( CategoryToSelect );
	}
}


bool SPluginCategories::IsItemExpanded( const FPluginCategoryTreeItemPtr Item ) const
{
	return PluginCategoryTreeView->IsItemExpanded( Item );
}


#undef LOCTEXT_NAMESPACE