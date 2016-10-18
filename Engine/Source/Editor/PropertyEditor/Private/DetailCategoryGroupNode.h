// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SDetailTableRowBase.h"

class SDetailCategoryTableRow : public SDetailTableRowBase
{
public:
	SLATE_BEGIN_ARGS( SDetailCategoryTableRow )
		: _InnerCategory( false )
	{}
		SLATE_ARGUMENT( FText, DisplayName )
		SLATE_ARGUMENT( bool, InnerCategory )
		SLATE_ARGUMENT( TSharedPtr<SWidget>, HeaderContent )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<IDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView );
private:
	EVisibility IsSeparatorVisible() const;
	const FSlateBrush* GetBackgroundImage() const;
private:
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
private:
	bool bIsInnerCategory;
};


class FDetailCategoryGroupNode : public IDetailTreeNode, public TSharedFromThis<FDetailCategoryGroupNode>
{
public:
	FDetailCategoryGroupNode( const FDetailNodeList& InChildNodes, FName InGroupName, FDetailCategoryImpl& InParentCategory );

private:
	virtual IDetailsViewPrivate& GetDetailsView() const override{ return ParentCategory.GetDetailsView(); }
	virtual void OnItemExpansionChanged( bool bIsExpanded ) override {}
	virtual bool ShouldBeExpanded() const override { return true; }
	virtual ENodeVisibility GetVisibility() const override { return bShouldBeVisible ? ENodeVisibility::Visible : ENodeVisibility::HiddenDueToFiltering; }
	virtual TSharedRef< ITableRow > GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities, bool bAllowFavoriteSystem) override;
	virtual void GetChildren( TArray< TSharedRef<IDetailTreeNode> >& OutChildren )  override;
	virtual void FilterNode( const FDetailFilter& InFilter ) override;
	virtual void Tick( float DeltaTime ) override {}
	virtual bool ShouldShowOnlyChildren() const override { return false; }
	virtual FName GetNodeName() const override { return NAME_None; }
private:
	FDetailNodeList ChildNodes;
	FDetailCategoryImpl& ParentCategory;
	FName GroupName;
	bool bShouldBeVisible;
};
