// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginsEditorPrivatePCH.h"
#include "PluginStyle.h"
#include "SPluginsEditor.h"
#include "SPluginList.h"
#include "SPluginCategories.h"
#include "SSearchBox.h"

#define LOCTEXT_NAMESPACE "PluginsEditor"


// @todo plugedit: Intro anim!!!


void SPluginsEditor::Construct( const FArguments& Args )
{
	struct Local
	{
		static void PluginStatusToStringArray( const FPluginStatus& PluginStatus, OUT TArray< FString >& StringArray )
		{
			// NOTE: Only the friendly name is searchable for now.  We don't display the actual plugin name in the UI.
			StringArray.Add( PluginStatus.FriendlyName );
		}
	};

	// @todo plugedit: Should we save/restore category selection and other view settings?  Splitter position, etc.

	bBreadcrumbNeedsRefresh = true;

	// Setup text filtering
	PluginTextFilter = MakeShareable( new FPluginTextFilter( FPluginTextFilter::FItemToStringArray::CreateStatic( &Local::PluginStatusToStringArray ) ) );

	const float PaddingAmount = 2.0f;


	PluginCategories = SNew( SPluginCategories, SharedThis( this ) );


	ChildSlot
	[ 
		SNew( SHorizontalBox )

		+SHorizontalBox::Slot()
		.Padding( PaddingAmount )
		.AutoWidth()	// @todo plugedit: Probably want a splitter here (w/ saved layout)
		[
			PluginCategories.ToSharedRef()
		]

		+SHorizontalBox::Slot()
		.Padding( PaddingAmount )
		[
			SNew( SVerticalBox )

			+SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, PaddingAmount))
			.AutoHeight()
			[
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.Padding( PaddingAmount )
				[
					SAssignNew( BreadcrumbTrail, SPluginCategoryBreadcrumbTrail )
					.DelimiterImage( FPluginStyle::Get()->GetBrush( "Plugins.BreadcrumbArrow" ) ) 
					.ShowLeadingDelimiter( true )
					.OnCrumbClicked( this, &SPluginsEditor::BreadcrumbTrail_OnCrumbClicked )
				]
	
				+SHorizontalBox::Slot()
				.Padding( PaddingAmount )
				[
					SNew( SSearchBox )
					.OnTextChanged( this, &SPluginsEditor::SearchBox_OnPluginSearchTextChanged )
				]
			]

			+SVerticalBox::Slot()
			[
				SAssignNew( PluginList, SPluginList, SharedThis( this ) )
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(PaddingAmount, PaddingAmount, PaddingAmount, 0.0f))
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor::Yellow)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.0f)
				.Visibility(this, &SPluginsEditor::HandleRestartEditorNoticeVisibility)
				[
					SNew( SHorizontalBox )

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
					[
						SNew(SImage)
						.Image(FPluginStyle::Get()->GetBrush("Plugins.Warning"))
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PluginSettingsRestartNotice", "Unreal Editor must be restarted for the plugin changes to take effect."))
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					[
						SNew(SButton)
						.Text(LOCTEXT("PluginSettingsRestartEditor", "Restart Now"))
						.OnClicked(this, &SPluginsEditor::HandleRestartEditorButtonClicked)
					]
				]
			]
		]
	];
}


EVisibility SPluginsEditor::HandleRestartEditorNoticeVisibility() const
{
	return IProjectManager::Get().IsRestartRequired() ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply SPluginsEditor::HandleRestartEditorButtonClicked() const
{
	const bool bWarn = false;
	FUnrealEdMisc::Get().RestartEditor(bWarn);
	return FReply::Handled();
}


void SPluginsEditor::SearchBox_OnPluginSearchTextChanged( const FText& NewText )
{
	PluginTextFilter->SetRawFilterText( NewText );
}


TSharedPtr< FPluginCategoryTreeItem > SPluginsEditor::GetSelectedCategory() const
{
	return PluginCategories->GetSelectedCategory();
}


void SPluginsEditor::OnCategorySelectionChanged()
{
	if( PluginList.IsValid() )
	{
		PluginList->SetNeedsRefresh();
	}

	// Breadcrumbs will need to be refreshed
	bBreadcrumbNeedsRefresh = true;
}


void SPluginsEditor::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Call parent implementation
	SCompoundWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );

	if( bBreadcrumbNeedsRefresh )
	{
		RefreshBreadcrumbTrail();
	}
}


void SPluginsEditor::RefreshBreadcrumbTrail()
{
	// Update breadcrumb trail
	if( BreadcrumbTrail.IsValid() )
	{
		// Build up the list of categories, starting at the selected node and working our way backwards.
		TArray< FPluginCategoryTreeItemPtr > CategoryItemPath;
		const auto& SelectedCategory = PluginCategories->GetSelectedCategory();
		if( SelectedCategory.IsValid() )
		{
			for( auto NextCategory = SelectedCategory; NextCategory.IsValid(); NextCategory = NextCategory->GetParentCategory() )
			{
				CategoryItemPath.Insert( NextCategory, 0 );
			}
		}


		// Fill in the crumbs
		BreadcrumbTrail->ClearCrumbs();
		for( auto CategoryIt( CategoryItemPath.CreateConstIterator() ); CategoryIt; ++CategoryIt )
		{
			const auto& Category = *CategoryIt;

			FPluginCategoryBreadcrumbPtr NewBreadcrumb( new FPluginCategoryBreadcrumb( Category ) );
			BreadcrumbTrail->PushCrumb( Category->GetCategoryDisplayName(), NewBreadcrumb );
		}
	}

	bBreadcrumbNeedsRefresh = false;
}


void SPluginsEditor::BreadcrumbTrail_OnCrumbClicked( const FPluginCategoryBreadcrumbPtr& CrumbData )
{
	if( PluginCategories.IsValid() )
	{
		PluginCategories->SelectCategory( CrumbData->GetCategoryItem() );
	}
}



#undef LOCTEXT_NAMESPACE