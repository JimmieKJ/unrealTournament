// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "DetailItemNode.h"
#include "PropertyEditorHelpers.h"
#include "DetailCategoryGroupNode.h"
#include "PropertyHandleImpl.h"
#include "DetailGroup.h"
#include "DetailPropertyRow.h"
#include "DetailCustomBuilderRow.h"
#include "SDetailSingleItemRow.h"
#include "TutorialMetaData.h"



FDetailItemNode::FDetailItemNode(const FDetailLayoutCustomization& InCustomization, TSharedRef<FDetailCategoryImpl> InParentCategory, TAttribute<bool> InIsParentEnabled )
	: Customization( InCustomization )
	, ParentCategory( InParentCategory )
	, IsParentEnabled( InIsParentEnabled )
	, CachedItemVisibility( EVisibility::Visible )
	, bShouldBeVisibleDueToFiltering( false )
	, bShouldBeVisibleDueToChildFiltering( false )
	, bTickable( false )
	, bIsExpanded( InCustomization.HasCustomBuilder() ? !InCustomization.CustomBuilderRow->IsInitiallyCollapsed() : false )
	, bIsHighlighted( false )
{
	
}

void FDetailItemNode::Initialize()
{
	if( ( Customization.HasCustomWidget() && Customization.WidgetDecl->VisibilityAttr.IsBound() )
		|| ( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->RequiresTick() )
		|| ( Customization.HasPropertyNode() && Customization.PropertyRow->RequiresTick() )
		|| ( Customization.HasGroup() && Customization.DetailGroup->RequiresTick() ) )
	{
		// The node needs to be ticked because it has widgets that can dynamically come and go
		bTickable = true;
		ParentCategory.Pin()->AddTickableNode( *this );
	}

	if( Customization.HasPropertyNode() )
	{
		InitPropertyEditor();
	}
	else if( Customization.HasCustomBuilder() )
	{
		InitCustomBuilder();
	}
	else if( Customization.HasGroup() )
	{
		InitGroup();
	}

	if (Customization.PropertyRow.IsValid() && Customization.PropertyRow->GetForceAutoExpansion())
	{
		SetExpansionState(true);
	}

	// Cache the visibility of customizations that can set it
	if( Customization.HasCustomWidget() )
	{	
		CachedItemVisibility = Customization.WidgetDecl->VisibilityAttr.Get();
	}
	else if( Customization.HasPropertyNode() )
	{
		CachedItemVisibility = Customization.PropertyRow->GetPropertyVisibility();
	}
	else if( Customization.HasGroup() )
	{
		CachedItemVisibility = Customization.DetailGroup->GetGroupVisibility();
	}

	const bool bUpdateFilteredNodes = false;
	GenerateChildren( bUpdateFilteredNodes );
}

FDetailItemNode::~FDetailItemNode()
{
	if( bTickable && ParentCategory.IsValid() )
	{
		ParentCategory.Pin()->RemoveTickableNode( *this );
	}
}

void FDetailItemNode::InitPropertyEditor()
{
	if( Customization.GetPropertyNode()->GetProperty() && Customization.GetPropertyNode()->GetProperty()->IsA<UArrayProperty>() )
	{
		const bool bUpdateFilteredNodes = false;
		FSimpleDelegate OnRegenerateChildren = FSimpleDelegate::CreateSP( this, &FDetailItemNode::GenerateChildren, bUpdateFilteredNodes );

		Customization.GetPropertyNode()->SetOnRebuildChildren( OnRegenerateChildren );
	}

	Customization.PropertyRow->OnItemNodeInitialized( ParentCategory.Pin().ToSharedRef(), IsParentEnabled );
}

void FDetailItemNode::InitCustomBuilder()
{
	Customization.CustomBuilderRow->OnItemNodeInitialized( AsShared(), ParentCategory.Pin().ToSharedRef(), IsParentEnabled );

	// Restore saved expansion state
	FName BuilderName = Customization.CustomBuilderRow->GetCustomBuilderName();
	if( BuilderName != NAME_None )
	{
		bIsExpanded = ParentCategory.Pin()->GetSavedExpansionState( *this );
	}

}

void FDetailItemNode::InitGroup()
{
	Customization.DetailGroup->OnItemNodeInitialized( AsShared(), ParentCategory.Pin().ToSharedRef(), IsParentEnabled );

	if (Customization.DetailGroup->ShouldStartExpanded())
	{
		bIsExpanded = true;
	}
	else
	{
		// Restore saved expansion state
		FName GroupName = Customization.DetailGroup->GetGroupName();
		if (GroupName != NAME_None)
		{
			bIsExpanded = ParentCategory.Pin()->GetSavedExpansionState(*this);
		}
	}
}

bool FDetailItemNode::HasMultiColumnWidget() const
{
	return ( Customization.HasCustomWidget() && Customization.WidgetDecl->HasColumns() )
		|| ( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->HasColumns() )
		|| ( Customization.HasGroup() && Customization.DetailGroup->HasColumns() )
		|| ( Customization.HasPropertyNode() && Customization.PropertyRow->HasColumns());
}

void FDetailItemNode::ToggleExpansion()
{
	SetExpansionState( !bIsExpanded );
}

void FDetailItemNode::SetExpansionState(bool bWantsExpanded)
{
	bIsExpanded = bWantsExpanded;

	// Expand the child after filtering if it wants to be expanded
	ParentCategory.Pin()->RequestItemExpanded(AsShared(), bIsExpanded);

	OnItemExpansionChanged(bIsExpanded);
}

TSharedRef< ITableRow > FDetailItemNode::GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities, bool bAllowFavoriteSystem)
{
	FTagMetaData TagMeta(TEXT("DetailRowItem"));
	if (ParentCategory.IsValid())
	{
		if (Customization.IsValidCustomization() && Customization.GetPropertyNode().IsValid())
		{
			TagMeta.Tag = *FString::Printf(TEXT("DetailRowItem.%s"), *Customization.GetPropertyNode()->GetDisplayName().ToString());
		}
		else if (Customization.HasCustomWidget() )
		{
			TagMeta.Tag = Customization.GetWidgetRow().RowTagName;
		}
	}
	if( Customization.HasPropertyNode() && Customization.GetPropertyNode()->AsCategoryNode() )
	{
		return
			SNew(SDetailCategoryTableRow, AsShared(), OwnerTable)
			.DisplayName(Customization.GetPropertyNode()->GetDisplayName())
			.AddMetaData<FTagMetaData>(TagMeta)
			.InnerCategory( true );
	}
	else
	{
		return
			SNew(SDetailSingleItemRow, &Customization, HasMultiColumnWidget(), AsShared(), OwnerTable )
			.AddMetaData<FTagMetaData>(TagMeta)
			.ColumnSizeData(ColumnSizeData)
			.AllowFavoriteSystem(bAllowFavoriteSystem);
	}
}

void FDetailItemNode::GetChildren( FDetailNodeList& OutChildren )
{
	for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		TSharedRef<IDetailTreeNode>& Child = Children[ChildIndex];

		ENodeVisibility ChildVisibility = Child->GetVisibility();

		// Report the child if the child is visible or we are visible due to filtering and there were no filtered children.  
		// If we are visible due to filtering and so is a child, we only show that child.  
		// If we are visible due to filtering and no child is visible, we show all children

		if( ChildVisibility == ENodeVisibility::Visible ||
			( !bShouldBeVisibleDueToChildFiltering && bShouldBeVisibleDueToFiltering && ChildVisibility != ENodeVisibility::ForcedHidden ) )
		{
			if( Child->ShouldShowOnlyChildren() )
			{
				Child->GetChildren( OutChildren );
			}
			else
			{
				OutChildren.Add( Child );
			}
		}
	}
}

void FDetailItemNode::GenerateChildren( bool bUpdateFilteredNodes )
{
	Children.Empty();

	if (!ParentCategory.IsValid())
	{
		return;
	}

	if( Customization.HasPropertyNode() )
	{
		Customization.PropertyRow->OnGenerateChildren( Children );
	}
	else if( Customization.HasCustomBuilder() )
	{
		Customization.CustomBuilderRow->OnGenerateChildren( Children );

		// Need to refresh the tree for custom builders as we could be regenerating children at any point
		ParentCategory.Pin()->RefreshTree( bUpdateFilteredNodes );
	}
	else if( Customization.HasGroup() )
	{
		Customization.DetailGroup->OnGenerateChildren( Children );
	}
}


void FDetailItemNode::OnItemExpansionChanged( bool bInIsExpanded )
{
	bIsExpanded = bInIsExpanded;
	if( Customization.HasPropertyNode() )
	{
		Customization.GetPropertyNode()->SetNodeFlags( EPropertyNodeFlags::Expanded, bInIsExpanded );
	}

	if( ParentCategory.IsValid() && ( ( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->GetCustomBuilderName() != NAME_None )
		 || ( Customization.HasGroup() && Customization.DetailGroup->GetGroupName() != NAME_None ) ) )
	{
		ParentCategory.Pin()->SaveExpansionState( *this );
	}
}

bool FDetailItemNode::ShouldBeExpanded() const
{
	bool bShouldBeExpanded = bIsExpanded || bShouldBeVisibleDueToChildFiltering;
	if( Customization.HasPropertyNode() )
	{
		FPropertyNode& PropertyNode = *Customization.GetPropertyNode();
		bShouldBeExpanded = PropertyNode.HasNodeFlags( EPropertyNodeFlags::Expanded ) != 0;
		bShouldBeExpanded |= PropertyNode.HasNodeFlags( EPropertyNodeFlags::IsSeenDueToChildFiltering ) != 0;
	}
	return bShouldBeExpanded;
}

ENodeVisibility FDetailItemNode::GetVisibility() const
{
	const bool bHasAnythingToShow = Customization.IsValidCustomization();

	const bool bIsForcedHidden = 
		!bHasAnythingToShow 
		|| (Customization.HasCustomWidget() && Customization.WidgetDecl->VisibilityAttr.Get() != EVisibility::Visible )
		|| (Customization.HasPropertyNode() && Customization.PropertyRow->GetPropertyVisibility() != EVisibility::Visible );

	ENodeVisibility Visibility;
	if( bIsForcedHidden )
	{
		Visibility = ENodeVisibility::ForcedHidden;
	}
	else
	{
		Visibility = (bShouldBeVisibleDueToFiltering || bShouldBeVisibleDueToChildFiltering) ? ENodeVisibility::Visible : ENodeVisibility::HiddenDueToFiltering;
	}

	return Visibility;
}

static bool PassesAllFilters( const FDetailLayoutCustomization& InCustomization, const FDetailFilter& InFilter, const FString& InCategoryName )
{	
	struct Local
	{
		static bool StringPassesFilter(const FDetailFilter& InDetailFilter, const FString& InString)
		{
			// Make sure the passed string matches all filter strings
			if( InString.Len() > 0 )
			{
				for (int32 TestNameIndex = 0; TestNameIndex < InDetailFilter.FilterStrings.Num(); ++TestNameIndex)
				{
					const FString& TestName = InDetailFilter.FilterStrings[TestNameIndex];
					if ( !InString.Contains(TestName) ) 
					{
						return false;
					}
				}
				return true;
			}
			return false;
		}
	};

	bool bPassesAllFilters = true;

	if( InFilter.FilterStrings.Num() > 0 || InFilter.bShowOnlyModifiedProperties == true || InFilter.bShowOnlyDiffering == true )
	{
		const bool bSearchFilterIsEmpty = InFilter.FilterStrings.Num() == 0;

		TSharedPtr<FPropertyNode> PropertyNodePin = InCustomization.GetPropertyNode();

		const bool bPassesCategoryFilter = !bSearchFilterIsEmpty && InFilter.bShowAllChildrenIfCategoryMatches ? Local::StringPassesFilter(InFilter, InCategoryName) : false;

		bPassesAllFilters = false;
		if( PropertyNodePin.IsValid() && !PropertyNodePin->AsCategoryNode() )
		{
			
			const bool bIsNotBeingFiltered = PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::IsBeingFiltered) == 0;
			const bool bIsSeenDueToFiltering = PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering) != 0;
			const bool bIsParentSeenDueToFiltering = PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::IsParentSeenDueToFiltering) != 0;

			const bool bPassesSearchFilter = bSearchFilterIsEmpty || ( bIsNotBeingFiltered || bIsSeenDueToFiltering || bIsParentSeenDueToFiltering );
			const bool bPassesModifiedFilter = bPassesSearchFilter && ( InFilter.bShowOnlyModifiedProperties == false || PropertyNodePin->GetDiffersFromDefault() == true );
			const bool bPassesDifferingFilter = InFilter.bShowOnlyDiffering ? InFilter.WhitelistedProperties.Find(*FPropertyNode::CreatePropertyPath(PropertyNodePin.ToSharedRef())) != nullptr : true;

			// The property node is visible (note categories are never visible unless they have a child that is visible )
			bPassesAllFilters = (bPassesSearchFilter && bPassesModifiedFilter && bPassesDifferingFilter) || bPassesCategoryFilter;
		}
		else if( InCustomization.HasCustomWidget() )
		{
			const bool bPassesTextFilter = Local::StringPassesFilter(InFilter, InCustomization.WidgetDecl->FilterTextString.ToString());

			bPassesAllFilters = bPassesTextFilter || bPassesCategoryFilter;
		}
	}

	return bPassesAllFilters;
}

void FDetailItemNode::Tick( float DeltaTime )
{
	if( ensure( bTickable ) )
	{
		if( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->RequiresTick() ) 
		{
			Customization.CustomBuilderRow->Tick( DeltaTime );
		}

		// Recache visibility
		EVisibility NewVisibility;
		if( Customization.HasCustomWidget() )
		{	
			NewVisibility = Customization.WidgetDecl->VisibilityAttr.Get();
		}
		else if( Customization.HasPropertyNode() )
		{
			NewVisibility = Customization.PropertyRow->GetPropertyVisibility();
		}
		else if( Customization.HasGroup() )
		{
			NewVisibility = Customization.DetailGroup->GetGroupVisibility();
		}
	
		if( CachedItemVisibility != NewVisibility )
		{
			// The visibility of a node in the tree has changed.  We must refresh the tree to remove the widget
			CachedItemVisibility = NewVisibility;
			const bool bRefilterCategory = true;
			ParentCategory.Pin()->RefreshTree( bRefilterCategory );
		}
	}
}

bool FDetailItemNode::ShouldShowOnlyChildren() const
{
	// Show only children of this node if there is no content for custom details or the property node requests that only children be shown
	return ( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->ShowOnlyChildren() )
		 || (Customization.HasPropertyNode() && Customization.PropertyRow->ShowOnlyChildren() );
}

FName FDetailItemNode::GetNodeName() const
{
	if( Customization.HasCustomBuilder() )
	{
		return Customization.CustomBuilderRow->GetCustomBuilderName();
	}
	else if( Customization.HasGroup() )
	{
		return Customization.DetailGroup->GetGroupName();
	}

	return NAME_None;
}

FPropertyPath FDetailItemNode::GetPropertyPath() const
{
	FPropertyPath Ret;
	auto PropertyNode = Customization.GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		Ret = *FPropertyNode::CreatePropertyPath( PropertyNode.ToSharedRef() );
	}
	return Ret;
}

void FDetailItemNode::FilterNode( const FDetailFilter& InFilter )
{
	bShouldBeVisibleDueToFiltering = PassesAllFilters( Customization, InFilter, ParentCategory.Pin()->GetDisplayName().ToString() );

	bShouldBeVisibleDueToChildFiltering = false;

	// Filter each child
	for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		TSharedRef<IDetailTreeNode>& Child = Children[ChildIndex];

		// If the parent is visible, we pass an empty filter to all children so that they resume their
		// default expansion.  This is a lot safer method, otherwise customized details panels tend to be
		// filtered incorrectly because they have no means of discovering if their parents were filtered.
		if ( bShouldBeVisibleDueToFiltering )
		{
			Child->FilterNode(FDetailFilter());

			// The child should be visible, but maybe something else has it hidden, check if it's
			// visible just for safety reasons.
			if ( Child->GetVisibility() == ENodeVisibility::Visible )
			{
				// Expand the child after filtering if it wants to be expanded
				ParentCategory.Pin()->RequestItemExpanded(Child, Child->ShouldBeExpanded());
			}
		}
		else
		{
			Child->FilterNode(InFilter);

			if ( Child->GetVisibility() == ENodeVisibility::Visible )
			{
				if ( !InFilter.IsEmptyFilter() )
				{
					// The child is visible due to filtering so we must also be visible
					bShouldBeVisibleDueToChildFiltering = true;
				}

				// Expand the child after filtering if it wants to be expanded
				ParentCategory.Pin()->RequestItemExpanded(Child, Child->ShouldBeExpanded());
			}
		}
	}
}
