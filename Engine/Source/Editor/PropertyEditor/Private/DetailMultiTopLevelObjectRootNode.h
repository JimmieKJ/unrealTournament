// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SDetailTableRowBase.h"

class SDetailMultiTopLevelObjectTableRow : public SDetailTableRowBase
{
public:
	SLATE_BEGIN_ARGS(SDetailMultiTopLevelObjectTableRow)
		: _DisplayName()
		, _ShowExpansionArrow(false)
	{}
		SLATE_ARGUMENT( FText, DisplayName )
		SLATE_ARGUMENT( bool, ShowExpansionArrow )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<IDetailTreeNode> InOwnerTreeNode, const TSharedRef<SWidget>& InCustomizedWidgetContents, const TSharedRef<STableViewBase>& InOwnerTableView );
private:
	const FSlateBrush* GetBackgroundImage() const;
private:
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
private:
	bool bShowExpansionArrow;
};


class FDetailMultiTopLevelObjectRootNode : public IDetailTreeNode, public TSharedFromThis<FDetailMultiTopLevelObjectRootNode>
{
public:
	FDetailMultiTopLevelObjectRootNode( const FDetailNodeList& InChildNodes, const TSharedPtr<IDetailRootObjectCustomization>& RootObjectCustomization, IDetailsViewPrivate& InDetailsView, const UObject& InRootObject );

private:
	virtual IDetailsViewPrivate& GetDetailsView() const override{ return DetailsView; }
	virtual void OnItemExpansionChanged( bool bIsExpanded ) override {}
	virtual bool ShouldBeExpanded() const override { return true; }
	virtual ENodeVisibility GetVisibility() const override;
	virtual TSharedRef< ITableRow > GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities, bool bAllowFavoriteSystem) override;
	virtual void GetChildren( TArray< TSharedRef<IDetailTreeNode> >& OutChildren )  override;
	virtual void FilterNode( const FDetailFilter& InFilter ) override;
	virtual void Tick( float DeltaTime ) override {}
	virtual bool ShouldShowOnlyChildren() const override;
	virtual FName GetNodeName() const override { return NodeName; }
private:
	FDetailNodeList ChildNodes;
	IDetailsViewPrivate& DetailsView;
	TWeakPtr<IDetailRootObjectCustomization> RootObjectCustomization;
	const TWeakObjectPtr<UObject> RootObject;
	FName NodeName;
	bool bShouldBeVisible;
};
