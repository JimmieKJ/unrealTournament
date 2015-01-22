// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/SceneOutliner/Public/ISceneOutlinerColumn.h"

/**
 * A custom column for the SceneOutliner which allows the user to remove actors from
 * the contents of a layer with a single click
 */
class FSceneOutlinerLayerContentsColumn : public ISceneOutlinerColumn
{

public:

	/**
	 *	Constructor
	 */
	FSceneOutlinerLayerContentsColumn( const TSharedRef< class FLayerViewModel >& InViewModel );

	virtual ~FSceneOutlinerLayerContentsColumn() {}
	
	static FName GetID();

	//////////////////////////////////////////////////////////////////////////
	// Begin ISceneOutlinerColumn Implementation

	virtual FName GetColumnID() override;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;

	virtual const TSharedRef< SWidget > ConstructRowWidget( SceneOutliner::FTreeItemRef TreeItem, const STableRow<SceneOutliner::FTreeItemPtr>& Row ) override
	{
		FColumnGenerator Generator(*this);
		TreeItem->Visit(Generator);
		return Generator.Widget.ToSharedRef();
	}

	// End ISceneOutlinerColumn Implementation
	//////////////////////////////////////////////////////////////////////////

private:

	FReply OnRemoveFromLayerClicked( const TWeakObjectPtr< AActor > Actor );

	TSharedRef<SWidget> ConstructRowWidget(const TWeakObjectPtr< AActor >& Actor );
	
	struct FColumnGenerator : SceneOutliner::FColumnGenerator
	{
		FSceneOutlinerLayerContentsColumn& Column;
		FColumnGenerator(FSceneOutlinerLayerContentsColumn& InColumn) : Column(InColumn) {}

		virtual TSharedRef<SWidget> GenerateWidget(SceneOutliner::FActorTreeItem& ActorItem) const override
		{
			return Column.ConstructRowWidget(ActorItem.Actor);
		}
	};
	friend FColumnGenerator;
private:

	/**	 */
	const TSharedRef< FLayerViewModel > ViewModel;
};