// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "DetailGroup.h"
#include "PropertyHandleImpl.h"
#include "DetailPropertyRow.h"
#include "DetailItemNode.h"

FDetailGroup::FDetailGroup( const FName InGroupName, TSharedRef<FDetailCategoryImpl> InParentCategory, const FText& InLocalizedDisplayName, const bool bInStartExpanded )
	: ParentCategory( InParentCategory )
	, LocalizedDisplayName( InLocalizedDisplayName )
	, GroupName( InGroupName )
	, bStartExpanded( bInStartExpanded )
{

}

FDetailWidgetRow& FDetailGroup::HeaderRow()
{
	HeaderCustomization = MakeShareable( new FDetailLayoutCustomization );
	HeaderCustomization->WidgetDecl = MakeShareable( new FDetailWidgetRow );

	return *HeaderCustomization->WidgetDecl;
}

IDetailPropertyRow& FDetailGroup::HeaderProperty( TSharedRef<IPropertyHandle> PropertyHandle )
{
	check( PropertyHandle->IsValidHandle() );

	PropertyHandle->MarkHiddenByCustomization();

	HeaderCustomization = MakeShareable( new FDetailLayoutCustomization );
	HeaderCustomization->PropertyRow = MakeShareable( new FDetailPropertyRow( StaticCastSharedRef<FPropertyHandleBase>( PropertyHandle )->GetPropertyNode(), ParentCategory.Pin().ToSharedRef() ) );

	return *HeaderCustomization->PropertyRow;
}

FDetailWidgetRow& FDetailGroup::AddWidgetRow()
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.WidgetDecl = MakeShareable( new FDetailWidgetRow );

	GroupChildren.Add( NewCustomization );

	return *NewCustomization.WidgetDecl;
}

IDetailPropertyRow& FDetailGroup::AddPropertyRow( TSharedRef<IPropertyHandle> PropertyHandle )
{
	check( PropertyHandle->IsValidHandle() );

	PropertyHandle->MarkHiddenByCustomization();

	FDetailLayoutCustomization NewCustomization;
	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( StaticCastSharedRef<FPropertyHandleBase>( PropertyHandle )->GetPropertyNode(), ParentCategory.Pin().ToSharedRef() ) );

	GroupChildren.Add( NewCustomization );

	return *NewCustomization.PropertyRow;
}

void FDetailGroup::ToggleExpansion( bool bExpand )
{
	if( ParentCategory.IsValid() && OwnerTreeNode.IsValid() )
	{
		ParentCategory.Pin()->RequestItemExpanded( OwnerTreeNode.Pin().ToSharedRef(), bExpand );
	}
}

bool FDetailGroup::GetExpansionState() const
{
	if (ParentCategory.IsValid() && OwnerTreeNode.IsValid())
	{
		return ParentCategory.Pin()->GetSavedExpansionState(*OwnerTreeNode.Pin().Get());
	}
	return false;
}

bool FDetailGroup::HasColumns() const
{
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() && HeaderCustomization->PropertyRow->HasColumns() )
	{
		return HeaderCustomization->PropertyRow->HasColumns();
	}
	else if( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		return HeaderCustomization->WidgetDecl->HasColumns();
	}
	
	return true;
}

bool FDetailGroup::RequiresTick() const
{
	bool bRequiresTick = false;
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		bRequiresTick = HeaderCustomization->PropertyRow->RequiresTick();
	}
	else if ( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		bRequiresTick = HeaderCustomization->WidgetDecl->VisibilityAttr.IsBound();
	}

	return bRequiresTick;
}

EVisibility FDetailGroup::GetGroupVisibility() const
{
	EVisibility Visibility = EVisibility::Visible;
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		Visibility = HeaderCustomization->PropertyRow->GetPropertyVisibility();
	}
	else if ( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		Visibility = HeaderCustomization->WidgetDecl->VisibilityAttr.Get();
	}

	return Visibility;
}

FDetailWidgetRow FDetailGroup::GetWidgetRow()
{
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		return HeaderCustomization->PropertyRow->GetWidgetRow();
	}
	else if ( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		return *HeaderCustomization->WidgetDecl;
	}
	else
	{
		FDetailWidgetRow Row;

		Row.NameContent()
		[
			MakeNameWidget()
		];

		return Row;
	}
}

void FDetailGroup::OnItemNodeInitialized( TSharedRef<FDetailItemNode> InTreeNode, TSharedRef<FDetailCategoryImpl> InParentCategory, const TAttribute<bool>& InIsParentEnabled )
{
	OwnerTreeNode = InTreeNode;
	ParentCategory = InParentCategory;
	IsParentEnabled = InIsParentEnabled;

	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		HeaderCustomization->PropertyRow->OnItemNodeInitialized( InParentCategory, InIsParentEnabled );
	}
}

void FDetailGroup::OnGenerateChildren( FDetailNodeList& OutChildren )
{
	for( int32 ChildIndex = 0; ChildIndex < GroupChildren.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailItemNode> NewNode = MakeShareable( new FDetailItemNode( GroupChildren[ChildIndex], ParentCategory.Pin().ToSharedRef(), IsParentEnabled ) );
		NewNode->Initialize();
		OutChildren.Add( NewNode );
	}
}

FReply FDetailGroup::OnNameClicked()
{
	OwnerTreeNode.Pin()->ToggleExpansion();

	return FReply::Handled();
}

TSharedRef<SWidget> FDetailGroup::MakeNameWidget()
{
	return
		SNew( SButton )
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ContentPadding(2)
		.OnClicked( this, &FDetailGroup::OnNameClicked )				
		.ForegroundColor( FSlateColor::UseForeground() )
		.Content()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LocalizedDisplayName )
		];
}
