// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "STileLayerList.h"
#include "STileLayerItem.h"
#include "PaperStyle.h"

#include "ScopedTransaction.h"

#include "TileMapEditorCommands.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// STileLayerList

void STileLayerList::Construct(const FArguments& InArgs, UPaperTileMap* InTileMap, FNotifyHook* InNotifyHook)
{
	TileMapPtr = InTileMap;
	NotifyHook = InNotifyHook;

	FTileMapEditorCommands::Register();
	const FTileMapEditorCommands& Commands = FTileMapEditorCommands::Get();

	CommandList = MakeShareable(new FUICommandList);

	CommandList->MapAction(
		Commands.AddNewLayerAbove,
		FExecuteAction::CreateSP(this, &STileLayerList::AddNewLayerAbove));

	CommandList->MapAction(
		Commands.AddNewLayerBelow,
		FExecuteAction::CreateSP(this, &STileLayerList::AddNewLayerBelow));

	CommandList->MapAction(
		Commands.DeleteLayer,
		FExecuteAction::CreateSP(this, &STileLayerList::DeleteLayer),
		FCanExecuteAction::CreateSP(this, &STileLayerList::CanExecuteActionNeedingSelectedLayer));

	CommandList->MapAction(
		Commands.DuplicateLayer,
		FExecuteAction::CreateSP(this, &STileLayerList::DuplicateLayer),
		FCanExecuteAction::CreateSP(this, &STileLayerList::CanExecuteActionNeedingSelectedLayer));

	CommandList->MapAction(
		Commands.MergeLayerDown,
		FExecuteAction::CreateSP(this, &STileLayerList::MergeLayerDown),
		FCanExecuteAction::CreateSP(this, &STileLayerList::CanExecuteActionNeedingLayerBelow));

	CommandList->MapAction(
		Commands.MoveLayerUp,
		FExecuteAction::CreateSP(this, &STileLayerList::MoveLayerUp),
		FCanExecuteAction::CreateSP(this, &STileLayerList::CanExecuteActionNeedingLayerAbove));

	CommandList->MapAction(
		Commands.MoveLayerDown,
		FExecuteAction::CreateSP(this, &STileLayerList::MoveLayerDown),
		FCanExecuteAction::CreateSP(this, &STileLayerList::CanExecuteActionNeedingLayerBelow));
	
	FToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization("TileLayerBrowserToolbar"), TSharedPtr<FExtender>(), Orient_Horizontal, /*InForceSmallIcons=*/ true);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	ToolbarBuilder.AddToolBarButton(Commands.AddNewLayerAbove);
	ToolbarBuilder.AddToolBarButton(Commands.MoveLayerUp);
	ToolbarBuilder.AddToolBarButton(Commands.MoveLayerDown);
	ToolbarBuilder.AddToolBarButton(Commands.DuplicateLayer);
	ToolbarBuilder.AddToolBarButton(Commands.DeleteLayer);

	TSharedRef<SWidget> Toolbar = ToolbarBuilder.MakeWidget();

	RefreshMirrorList();

	ListViewWidget = SNew(SPaperLayerListView)
		.SelectionMode(ESelectionMode::Single)
		.ClearSelectionOnClick(false)
		.ListItemsSource(&MirrorList)
		.OnSelectionChanged(this, &STileLayerList::OnSelectionChanged)
		.OnGenerateRow(this, &STileLayerList::OnGenerateLayerListRow)
		.OnContextMenuOpening(this, &STileLayerList::OnConstructContextMenu);

	// Restore the selection
	InTileMap->ValidateSelectedLayerIndex();
	if (InTileMap->TileLayers.IsValidIndex(InTileMap->SelectedLayerIndex))
	{
		UPaperTileLayer* SelectedLayer = InTileMap->TileLayers[InTileMap->SelectedLayerIndex];
		SetSelectedLayer(SelectedLayer);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(SBox)
			.HeightOverride(115.0f)
			[
				ListViewWidget.ToSharedRef()
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			Toolbar
		]
	];
}

TSharedRef<ITableRow> STileLayerList::OnGenerateLayerListRow(FMirrorEntry Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	typedef STableRow<FMirrorEntry> RowType;

	TSharedRef<RowType> NewRow = SNew(RowType, OwnerTable)
		.Style(&FPaperStyle::Get()->GetWidgetStyle<FTableRowStyle>("TileMapEditor.LayerBrowser.TableViewRow"));

	FIsSelected IsSelectedDelegate = FIsSelected::CreateSP(NewRow, &RowType::IsSelectedExclusively);
	NewRow->SetContent(SNew(STileLayerItem, *Item, TileMapPtr.Get(), IsSelectedDelegate));

	return NewRow;
}

UPaperTileLayer* STileLayerList::GetSelectedLayer() const
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		if (ListViewWidget->GetNumItemsSelected() > 0)
		{
			FMirrorEntry SelectedItem = ListViewWidget->GetSelectedItems()[0];
			const int32 SelectedIndex = *SelectedItem;
			if (TileMap->TileLayers.IsValidIndex(SelectedIndex))
			{
				return TileMap->TileLayers[SelectedIndex];
			}
		}
	}

	return nullptr;
}

FText STileLayerList::GenerateDuplicatedLayerName(const FString& InputNameRaw, UPaperTileMap* TileMap)
{
	// Create a set of existing names
	TSet<FString> ExistingNames;
	for (UPaperTileLayer* ExistingLayer : TileMap->TileLayers)
	{
		ExistingNames.Add(ExistingLayer->LayerName.ToString());
	}

	FString BaseName = InputNameRaw;
	int32 TestIndex = 0;
	bool bAddNumber = false;

	// See if this is the result of a previous duplication operation, and change the desired name accordingly
	int32 SpaceIndex;
	if (InputNameRaw.FindLastChar(' ', /*out*/ SpaceIndex))
	{
		FString PossibleDuplicationSuffix = InputNameRaw.Mid(SpaceIndex + 1);

		if (PossibleDuplicationSuffix == TEXT("copy"))
		{
			bAddNumber = true;
			BaseName = InputNameRaw.Left(SpaceIndex);
			TestIndex = 2;
		}
		else
		{
			int32 ExistingIndex = FCString::Atoi(*PossibleDuplicationSuffix);

			const FString TestSuffix = FString::Printf(TEXT(" copy %d"), ExistingIndex);

			if (InputNameRaw.EndsWith(TestSuffix))
			{
				bAddNumber = true;
				BaseName = InputNameRaw.Left(InputNameRaw.Len() - TestSuffix.Len());
				TestIndex = ExistingIndex + 1;
			}
		}
	}

	// Find a good name
	FString TestLayerName = BaseName + TEXT(" copy");

	if (bAddNumber || ExistingNames.Contains(TestLayerName))
	{
		do
		{
			TestLayerName = FString::Printf(TEXT("%s copy %d"), *BaseName, TestIndex++);
		} while (ExistingNames.Contains(TestLayerName));
	}

	return FText::FromString(TestLayerName);
}

UPaperTileLayer* STileLayerList::AddLayer(bool bCollisionLayer, int32 InsertionIndex)
{
	UPaperTileLayer* NewLayer = nullptr;

	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("TileMapAddLayer", "Add New Layer"));
		TileMap->SetFlags(RF_Transactional);
		TileMap->Modify();

		NewLayer = TileMap->AddNewLayer(bCollisionLayer, InsertionIndex);

		PostEditNotfications();

		// Change the selection set to select it
		SetSelectedLayer(NewLayer);
	}

	return NewLayer;
}

void STileLayerList::ChangeLayerOrdering(int32 OldIndex, int32 NewIndex)
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		if (TileMap->TileLayers.IsValidIndex(OldIndex) && TileMap->TileLayers.IsValidIndex(NewIndex))
		{
			const FScopedTransaction Transaction(LOCTEXT("TileMapReorderLayer", "Reorder Layer"));
			TileMap->SetFlags(RF_Transactional);
			TileMap->Modify();

			UPaperTileLayer* LayerToMove = TileMap->TileLayers[OldIndex];
			TileMap->TileLayers.RemoveAt(OldIndex);
			TileMap->TileLayers.Insert(LayerToMove, NewIndex);

			if (TileMap->SelectedLayerIndex == OldIndex)
			{
				TileMap->SelectedLayerIndex = NewIndex;
				SetSelectedLayer(LayerToMove);
			}

			PostEditNotfications();
		}
	}
}

void STileLayerList::AddNewLayerAbove()
{
	AddLayer(/*bCollisionLayer=*/ false, GetSelectionIndex());
}

void STileLayerList::AddNewLayerBelow()
{
	AddLayer(/*bCollisionLayer=*/ false, GetSelectionIndex() + 1);
}

int32 STileLayerList::GetSelectionIndex() const
{
	int32 SelectionIndex = INDEX_NONE;

	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		if (UPaperTileLayer* SelectedLayer = GetSelectedLayer())
		{
			TileMap->TileLayers.Find(SelectedLayer, /*out*/ SelectionIndex);
		}
		else
		{
			SelectionIndex = TileMap->TileLayers.Num() - 1;
		}
	}

	return SelectionIndex;
}

void STileLayerList::DeleteLayer()
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		const int32 DeleteIndex = GetSelectionIndex();
		if (DeleteIndex != INDEX_NONE)
		{
			const FScopedTransaction Transaction(LOCTEXT("TileMapDeleteLayer", "Delete Layer"));
			TileMap->SetFlags(RF_Transactional);
			TileMap->Modify();

			TileMap->TileLayers.RemoveAt(DeleteIndex);

			PostEditNotfications();

			// Select the item below the one that just got deleted
			const int32 NewSelectionIndex = FMath::Min<int32>(DeleteIndex, TileMap->TileLayers.Num() - 1);
			if (TileMap->TileLayers.IsValidIndex(NewSelectionIndex))
			{
				SetSelectedLayer(TileMap->TileLayers[NewSelectionIndex]);
			}
		}
	}
}

void STileLayerList::DuplicateLayer()
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		const int32 DuplicateIndex = GetSelectionIndex();
		if (DuplicateIndex != INDEX_NONE)
		{
			const FScopedTransaction Transaction(LOCTEXT("TileMapDuplicateLayer", "Duplicate Layer"));
			TileMap->SetFlags(RF_Transactional);
			TileMap->Modify();

			UPaperTileLayer* NewLayer = DuplicateObject<UPaperTileLayer>(TileMap->TileLayers[DuplicateIndex], TileMap);
			TileMap->TileLayers.Insert(NewLayer, DuplicateIndex);
			NewLayer->LayerName = GenerateDuplicatedLayerName(NewLayer->LayerName.ToString(), TileMap);

			PostEditNotfications();

			// Select the duplicated layer
			SetSelectedLayer(NewLayer);
		}
	}
}

void STileLayerList::MergeLayerDown()
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		const int32 SourceIndex = GetSelectionIndex();
		const int32 TargetIndex = SourceIndex + 1;
		if ((SourceIndex != INDEX_NONE) && (TargetIndex != INDEX_NONE))
		{
			const FScopedTransaction Transaction(LOCTEXT("TileMapMergeLayerDown", "Merge Layer Down"));
			TileMap->SetFlags(RF_Transactional);
			TileMap->Modify();

			UPaperTileLayer* SourceLayer = TileMap->TileLayers[SourceIndex];
			UPaperTileLayer* TargetLayer = TileMap->TileLayers[TargetIndex];

			TargetLayer->SetFlags(RF_Transactional);
			TargetLayer->Modify();

			// Copy the non-empty tiles from the source to the target layer
			for (int32 Y = 0; Y < SourceLayer->LayerWidth; ++Y)
			{
				for (int32 X = 0; X < SourceLayer->LayerWidth; ++X)
				{
					FPaperTileInfo TileInfo = SourceLayer->GetCell(X, Y);
					if (TileInfo.IsValid())
					{
						TargetLayer->SetCell(X, Y, TileInfo);
					}
				}
			}

			// Remove the source layer
			TileMap->TileLayers.RemoveAt(SourceIndex);

			// Update viewers
			PostEditNotfications();
		}
	}
}

void STileLayerList::MoveLayerUp()
{
	const int32 SelectedIndex = GetSelectionIndex();
	const int32 NewIndex = SelectedIndex - 1;
	ChangeLayerOrdering(SelectedIndex, NewIndex);
}

void STileLayerList::MoveLayerDown()
{
	const int32 SelectedIndex = GetSelectionIndex();
	const int32 NewIndex = SelectedIndex + 1;
	ChangeLayerOrdering(SelectedIndex, NewIndex);
}

int32 STileLayerList::GetNumLayers() const
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		return TileMap->TileLayers.Num();
	}
	return 0;
}

bool STileLayerList::CanExecuteActionNeedingLayerAbove() const
{
	return GetSelectionIndex() > 0;
}

bool STileLayerList::CanExecuteActionNeedingLayerBelow() const
{
	const int32 SelectedLayer = GetSelectionIndex();
	return (SelectedLayer != INDEX_NONE) && (SelectedLayer + 1 < GetNumLayers());
}

bool STileLayerList::CanExecuteActionNeedingSelectedLayer() const
{
	return GetSelectionIndex() != INDEX_NONE;
}

void STileLayerList::SetSelectedLayer(UPaperTileLayer* SelectedLayer)
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		int32 NewIndex;
		if (TileMap->TileLayers.Find(SelectedLayer, /*out*/ NewIndex))
		{
			if (MirrorList.IsValidIndex(NewIndex))
			{
				ListViewWidget->SetSelection(MirrorList[NewIndex]);
			}
		}
	}
}

void STileLayerList::OnSelectionChanged(FMirrorEntry ItemChangingState, ESelectInfo::Type SelectInfo)
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		TileMap->SelectedLayerIndex = GetSelectionIndex();

		PostEditNotfications();
	}
}

TSharedPtr<SWidget> STileLayerList::OnConstructContextMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	const FTileMapEditorCommands& Commands = FTileMapEditorCommands::Get();

 	MenuBuilder.BeginSection("BasicOperations");
 	{
		MenuBuilder.AddMenuEntry(Commands.DuplicateLayer);
		MenuBuilder.AddMenuEntry(Commands.DeleteLayer);
		MenuBuilder.AddMenuEntry(Commands.MergeLayerDown);
 	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void STileLayerList::PostEditNotfications()
{
	RefreshMirrorList();

	ListViewWidget->RequestListRefresh();

	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		TileMap->PostEditChange();
	}

	if (NotifyHook != nullptr)
	{
		UProperty* TileMapProperty = FindFieldChecked<UProperty>(UPaperTileMapComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UPaperTileMapComponent, TileMap));
		NotifyHook->NotifyPreChange(TileMapProperty);
		NotifyHook->NotifyPostChange(FPropertyChangedEvent(TileMapProperty), TileMapProperty);
	}
}

void STileLayerList::RefreshMirrorList()
{
	if (UPaperTileMap* TileMap = TileMapPtr.Get())
	{
		const int32 NumEntriesToAdd = TileMap->TileLayers.Num() - MirrorList.Num();
		if (NumEntriesToAdd < 0)
		{
			MirrorList.RemoveAt(TileMap->TileLayers.Num(), -NumEntriesToAdd);
		}
		else if (NumEntriesToAdd > 0)
		{
			for (int32 Count = 0; Count < NumEntriesToAdd; ++Count)
			{
				TSharedPtr<int32> NewEntry = MakeShareable(new int32);
				*NewEntry = MirrorList.Num();
				MirrorList.Add(NewEntry);
			}
		}
	}
	else
	{
		MirrorList.Empty();
	}
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE