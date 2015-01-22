// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// STileLayerList

class STileLayerList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STileLayerList) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UPaperTileMap* TileMap, FNotifyHook* InNotifyHook);

protected:
	typedef SListView<class UPaperTileLayer*> SPaperLayerListView;

protected:
	TSharedPtr<SPaperLayerListView> ListViewWidget;
	TSharedPtr<class FUICommandList> CommandList;
	TWeakObjectPtr<class UPaperTileMap> TileMapPtr;
	FNotifyHook* NotifyHook;
protected:
	TSharedRef<ITableRow> OnGenerateLayerListRow(class UPaperTileLayer* Item, const TSharedRef<STableViewBase>& OwnerTable);

	class UPaperTileLayer* GetSelectedLayer() const;

	// Returns the selected index if anything is selected, or the top item otherwise (only returns INDEX_NONE if there are no layers)
	int32 GetSelectionIndex() const;

	static FText GenerateDuplicatedLayerName(const FString& InputNameRaw, UPaperTileMap* TileMap);

	class UPaperTileLayer* AddLayer(bool bCollisionLayer, int32 InsertionIndex = INDEX_NONE);

	// Moves a layer from OldIndex to NewIndex if both are valid, otherwise it does nothing silently
	void ChangeLayerOrdering(int32 OldIndex, int32 NewIndex);

	void AddNewLayerAbove();
	void AddNewLayerBelow();
	void DeleteLayer();
	void DuplicateLayer();
	void MergeLayerDown();
	void MoveLayerUp();
	void MoveLayerDown();

	int32 GetNumLayers() const;
	bool CanExecuteActionNeedingLayerAbove() const;
	bool CanExecuteActionNeedingLayerBelow() const;
	bool CanExecuteActionNeedingSelectedLayer() const;

	void SetSelectedLayer(UPaperTileLayer* SelectedLayer);

	void OnSelectionChanged(UPaperTileLayer* ItemChangingState, ESelectInfo::Type SelectInfo);
	TSharedPtr<SWidget> OnConstructContextMenu();

	// Called after edits are finished
	void PostEditNotfications();
};
