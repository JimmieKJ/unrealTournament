// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginsEditorPrivatePCH.h"
#include "SPluginCategoryTreeItem.h"
#include "PluginCategoryTreeItem.h"
#include "SPluginCategories.h"
#include "PluginStyle.h"


#define LOCTEXT_NAMESPACE "PluginCategoryTreeItem"


void SPluginCategoryTreeItem::Construct( const FArguments& Args, const TSharedRef< SPluginCategories > Owner, const TSharedRef< class FPluginCategoryTreeItem >& Item )
{
	OwnerWeak = Owner;
	ItemData = Item;

	const float CategoryIconSize = FPluginStyle::Get()->GetFloat("CategoryTreeItem.IconSize");
	const float PaddingAmount = FPluginStyle::Get()->GetFloat("CategoryTreeItem.PaddingAmount");;

	// @todo plugedit: Show a badge when at least one plugin in this category is not at the latest version

	// Figure out which font size to use
	const auto bIsRootItem = !Item->GetParentCategory().IsValid();

	ChildSlot
	[ 
		SNew( SBorder )
		.BorderImage( FPluginStyle::Get()->GetBrush(bIsRootItem ? "CategoryTreeItem.Root.BackgroundBrush" : "CategoryTreeItem.BackgroundBrush") )
		.Padding(FPluginStyle::Get()->GetMargin(bIsRootItem ? "CategoryTreeItem.Root.BackgroundPadding" : "CategoryTreeItem.BackgroundPadding") )
		[
			SNew( SHorizontalBox )

			// Icon image
			+SHorizontalBox::Slot()
			.Padding( PaddingAmount )
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew( SBox )
				.WidthOverride( CategoryIconSize )
				.HeightOverride( CategoryIconSize )
				[
					SNew( SImage )
					.Image( this, &SPluginCategoryTreeItem::GetIconBrush )
				]
			]

			// Category name
			+SHorizontalBox::Slot()
			.Padding( PaddingAmount )
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text( Item->GetCategoryDisplayName() )
				.TextStyle( FPluginStyle::Get(), bIsRootItem ? "CategoryTreeItem.Root.Text" : "CategoryTreeItem.Text" )
			]
			
			// Plugin count
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( PaddingAmount )
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )

				// Only display if at there is least one plugin is in this category
				.Visibility( Item->GetPlugins().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed )

				.Text( FText::Format( LOCTEXT( "NumberOfPluginsWrapper", "({0})" ), FText::AsNumber( Item->GetPlugins().Num() ) ) )
				.TextStyle( FPluginStyle::Get(), bIsRootItem ? "CategoryTreeItem.Root.PluginCountText" : "CategoryTreeItem.PluginCountText" )
			]
		]
	];
}


const FSlateBrush* SPluginCategoryTreeItem::GetIconBrush() const
{
	// @todo plugedit: Need support for styles embedded into plugins!
	const FSlateBrush* IconBrush;

	if ( ItemData->GetCategoryPath() == TEXT("BuiltIn") )
	{
		IconBrush = FPluginStyle::Get()->GetBrush( "CategoryTreeItem.BuiltIn" );
	}
	else if ( ItemData->GetCategoryPath() == TEXT("Installed") )
	{
		IconBrush = FPluginStyle::Get()->GetBrush( "CategoryTreeItem.Installed" );
	}
	else
	{
		const bool bIsLeafItemWithPlugins = ( ItemData->GetSubCategories().Num() == 0 ) && ( ItemData->GetPlugins().Num() > 0 );
		if( bIsLeafItemWithPlugins )
		{
			IconBrush = FPluginStyle::Get()->GetBrush( "CategoryTreeItem.LeafItemWithPlugin" );
		}
		else
		{
			const bool bIsExpanded = OwnerWeak.Pin()->IsItemExpanded( ItemData );
			IconBrush = bIsExpanded ? FPluginStyle::Get()->GetBrush( "CategoryTreeItem.ExpandedCategory" ) : FPluginStyle::Get()->GetBrush( "CategoryTreeItem.Category" );
		}
	}

	return IconBrush;
}


#undef LOCTEXT_NAMESPACE