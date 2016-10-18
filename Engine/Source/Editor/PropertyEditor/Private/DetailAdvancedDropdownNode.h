// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class FAdvancedDropdownNode : public IDetailTreeNode, public TSharedFromThis<FAdvancedDropdownNode>
{
public:
	FAdvancedDropdownNode( FDetailCategoryImpl& InParentCategory, const TAttribute<bool>& InExpanded, const TAttribute<bool>& InEnabled, bool bInShouldShowAdvancedButton, bool bInDisplayShowAdvancedMessage, bool bInShowSplitter )
		: ParentCategory( InParentCategory )
		, IsEnabled( InEnabled )
		, IsExpanded( InExpanded )
		, bShouldShowAdvancedButton( bInShouldShowAdvancedButton )
		, bIsTopNode( false )
		, bDisplayShowAdvancedMessage( bInDisplayShowAdvancedMessage )
		, bShowSplitter( bInShowSplitter )
	{}

	FAdvancedDropdownNode( FDetailCategoryImpl& InParentCategory, bool bInIsTopNode )
		: ParentCategory( InParentCategory )
		, bShouldShowAdvancedButton( false )
		, bIsTopNode( bInIsTopNode )
		, bDisplayShowAdvancedMessage( false  )
		, bShowSplitter( false )
	{}
private:
	/** IDetailTreeNode Interface */
	virtual IDetailsViewPrivate& GetDetailsView() const override{ return ParentCategory.GetDetailsView(); }
	virtual TSharedRef< ITableRow > GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities, bool bAllowFavoriteSystem) override;
	virtual void GetChildren( TArray< TSharedRef<IDetailTreeNode> >& OutChildren )  override {}
	virtual void OnItemExpansionChanged( bool bIsExpanded ) override {}
	virtual bool ShouldBeExpanded() const override { return false; }
	virtual ENodeVisibility GetVisibility() const override { return ENodeVisibility::Visible; }
	virtual void FilterNode( const FDetailFilter& InFilter ) override {}
	virtual void Tick( float DeltaTime ) override {}
	virtual bool ShouldShowOnlyChildren() const override { return false; }
	virtual FName GetNodeName() const override { return NAME_None; }

	/** Called when the advanced drop down arrow is clicked */
	FReply OnAdvancedDropDownClicked();
private:
	FDetailCategoryImpl& ParentCategory;
	TAttribute<bool> IsEnabled;
	TAttribute<bool> IsExpanded;
	bool bShouldShowAdvancedButton;
	bool bIsTopNode;
	bool bDisplayShowAdvancedMessage;
	bool bShowSplitter;
};
