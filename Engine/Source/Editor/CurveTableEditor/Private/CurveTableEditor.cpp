// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CurveTableEditorPrivatePCH.h"

#include "CurveTableEditor.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"
#include "Engine/CurveTable.h"
 
#define LOCTEXT_NAMESPACE "CurveTableEditor"

const FName FCurveTableEditor::CurveTableTabId("CurveTableEditor_CurveTable");
const FName FCurveTableEditor::RowNameColumnId("RowName");

class SCurveTableListViewRow : public SMultiColumnTableRow<FCurveTableEditorRowListViewDataPtr>
{
public:
	SLATE_BEGIN_ARGS(SCurveTableListViewRow) {}
		/** The widget that owns the tree.  We'll only keep a weak reference to it. */
		SLATE_ARGUMENT(TSharedPtr<FCurveTableEditor>, CurveTableEditor)
		/** The list item for this row */
		SLATE_ARGUMENT(FCurveTableEditorRowListViewDataPtr, Item)
	SLATE_END_ARGS()

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		CurveTableEditor = InArgs._CurveTableEditor;
		Item = InArgs._Item;
		SMultiColumnTableRow<FCurveTableEditorRowListViewDataPtr>::Construct(
			FSuperRowType::FArguments()
				.Style(FEditorStyle::Get(), "DataTableEditor.CellListViewRow"), 
			InOwnerTableView
			);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		TSharedPtr<FCurveTableEditor> CurveTableEditorPtr = CurveTableEditor.Pin();
		return (CurveTableEditorPtr.IsValid())
			? CurveTableEditorPtr->MakeCellWidget(Item, IndexInList, ColumnName)
			: SNullWidget::NullWidget;
	}

private:
	/** Weak reference to the curve table editor that owns our list */
	TWeakPtr<FCurveTableEditor> CurveTableEditor;
	/** The item associated with this row of data */
	FCurveTableEditorRowListViewDataPtr Item;
};


void FCurveTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_CurveTableEditor", "Curve Table Editor"));

	TabManager->RegisterTabSpawner( CurveTableTabId, FOnSpawnTab::CreateSP(this, &FCurveTableEditor::SpawnTab_CurveTable) )
		.SetDisplayName( LOCTEXT("CurveTableTab", "Curve Table") )
		.SetGroup( WorkspaceMenuCategory.ToSharedRef() );
}


void FCurveTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	TabManager->UnregisterTabSpawner( CurveTableTabId );
}


FCurveTableEditor::~FCurveTableEditor()
{
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
}


void FCurveTableEditor::InitCurveTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCurveTable* Table )
{
	const TSharedRef< FTabManager::FLayout > StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_CurveTableEditor_Layout")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split
		(
			FTabManager::NewStack()
			->AddTab( CurveTableTabId, ETabState::OpenedTab )
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FCurveTableEditorModule::CurveTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table );
	
	FCurveTableEditorModule& CurveTableEditorModule = FModuleManager::LoadModuleChecked<FCurveTableEditorModule>( "CurveTableEditor" );
	AddMenuExtender(CurveTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	FReimportManager::Instance()->OnPostReimport().AddSP(this, &FCurveTableEditor::OnPostReimport);

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( CurveTableTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	// NOTE: Could fill in asset editor commands here!
}

FName FCurveTableEditor::GetToolkitFName() const
{
	return FName("CurveTableEditor");
}

FText FCurveTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "CurveTable Editor" );
}


FString FCurveTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "CurveTable ").ToString();
}


FLinearColor FCurveTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}


const UCurveTable* FCurveTableEditor::GetCurveTable() const
{
	return Cast<const UCurveTable>(GetEditingObject());
}

TSharedRef<SDockTab> FCurveTableEditor::SpawnTab_CurveTable( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == CurveTableTabId );

	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(8.0f, 8.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(8.0f, 8.0f));

	TSharedRef<SHeaderRow> RowNamesHeaderRow = SNew(SHeaderRow);
	RowNamesHeaderRow->AddColumn(
		SHeaderRow::Column(RowNameColumnId)
		.DefaultLabel(FText::GetEmpty())
		);

	ColumnNamesHeaderRow = SNew(SHeaderRow);

	RowNamesListView = SNew(SListView<FCurveTableEditorRowListViewDataPtr>)
		.ListItemsSource(&AvailableRows)
		.HeaderRow(RowNamesHeaderRow)
		.OnGenerateRow(this, &FCurveTableEditor::MakeRowNameWidget)
		.OnListViewScrolled(this, &FCurveTableEditor::OnRowNamesListViewScrolled)
		.ScrollbarVisibility(EVisibility::Collapsed)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.SelectionMode(ESelectionMode::None)
		.AllowOverscroll(EAllowOverscroll::No);

	CellsListView = SNew(SListView<FCurveTableEditorRowListViewDataPtr>)
		.ListItemsSource(&AvailableRows)
		.HeaderRow(ColumnNamesHeaderRow)
		.OnGenerateRow(this, &FCurveTableEditor::MakeRowWidget)
		.OnListViewScrolled(this, &FCurveTableEditor::OnCellsListViewScrolled)
		.ExternalScrollbar(VerticalScrollBar)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.SelectionMode(ESelectionMode::None)
		.AllowOverscroll(EAllowOverscroll::No);

	RefreshCachedCurveTable();

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("CurveTableEditor.Tabs.Properties") )
		.Label( LOCTEXT("CurveTableTitle", "Curve Table") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(this, &FCurveTableEditor::GetRowNameColumnWidth)
						[
							RowNamesListView.ToSharedRef()
						]
					]
					+SHorizontalBox::Slot()
					[
						SNew(SScrollBox)
						.Orientation(Orient_Horizontal)
						.ExternalScrollbar(HorizontalScrollBar)
						+SScrollBox::Slot()
						[
							CellsListView.ToSharedRef()
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						VerticalScrollBar
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(this, &FCurveTableEditor::GetRowNameColumnWidth)
						[
							SNullWidget::NullWidget
						]
					]
					+SHorizontalBox::Slot()
					[
						HorizontalScrollBar
					]
				]
			]
		];
}


void FCurveTableEditor::RefreshCachedCurveTable()
{
	CacheDataTableForEditing();

	ColumnNamesHeaderRow->ClearColumns();
	for (int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FCurveTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];

		ColumnNamesHeaderRow->AddColumn(
			SHeaderRow::Column(ColumnData->ColumnId)
			.DefaultLabel(ColumnData->DisplayName)
			.FixedWidth(ColumnData->DesiredColumnWidth)
			);
	}

	RowNamesListView->RequestListRefresh();
	CellsListView->RequestListRefresh();
}


void FCurveTableEditor::CacheDataTableForEditing()
{
	RowNameColumnWidth = 10.0f;

	const UCurveTable* Table = GetCurveTable();
	if (!Table || Table->RowMap.Num() == 0)
	{
		AvailableColumns.Empty();
		AvailableRows.Empty();
		return;
	}

	TArray<FName> Names;
	TArray<FRichCurve*> Curves;
		
	// get the row names and curves they represent
	Table->RowMap.GenerateKeyArray(Names);
	Table->RowMap.GenerateValueArray(Curves);

	// Determine the curve with the longest set of data, for the column names
	int32 LongestCurveIndex = 0;
	for (int32 CurvesIdx = 1; CurvesIdx < Curves.Num(); ++CurvesIdx)
	{
		if (Curves[CurvesIdx]->GetNumKeys() > Curves[LongestCurveIndex]->GetNumKeys())
		{
			LongestCurveIndex = CurvesIdx;
		}
	}

	TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FTextBlockStyle& CellTextStyle = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("DataTableEditor.CellText");
	static const float CellPadding = 10.0f;

	// Column titles, taken from the longest curve
	AvailableColumns.Reset(Curves[LongestCurveIndex]->GetNumKeys());
	for (auto It(Curves[LongestCurveIndex]->GetKeyIterator()); It; ++It)
	{
		const FText ColumnText = FText::AsNumber(It->Time);

		FCurveTableEditorColumnHeaderDataPtr CachedColumnData = MakeShareable(new FCurveTableEditorColumnHeaderData());
		CachedColumnData->ColumnId = *ColumnText.ToString();
		CachedColumnData->DisplayName = ColumnText;
		CachedColumnData->DesiredColumnWidth = FontMeasure->Measure(CachedColumnData->DisplayName, CellTextStyle.Font).X + CellPadding;

		AvailableColumns.Add(CachedColumnData);
	}

	// Each curve is a row entry
	AvailableRows.Reset(Curves.Num());
	for (int32 CurvesIdx = 0; CurvesIdx < Curves.Num(); ++CurvesIdx)
	{
		const FName& CurveName = Names[CurvesIdx];

		FCurveTableEditorRowListViewDataPtr CachedRowData = MakeShareable(new FCurveTableEditorRowListViewData());
		CachedRowData->RowId = CurveName;
		CachedRowData->DisplayName = FText::FromName(CurveName);

		const float RowNameWidth = FontMeasure->Measure(CachedRowData->DisplayName, CellTextStyle.Font).X + CellPadding;
		RowNameColumnWidth = FMath::Max(RowNameColumnWidth, RowNameWidth);

		CachedRowData->CellData.Reserve(Curves[CurvesIdx]->GetNumKeys());
		{
			int32 ColumnIndex = 0;
			for (auto It(Curves[CurvesIdx]->GetKeyIterator()); It; ++It)
			{
				FCurveTableEditorColumnHeaderDataPtr CachedColumnData = AvailableColumns[ColumnIndex++];

				const FText CellText = FText::AsNumber(It->Value);
				CachedRowData->CellData.Add(CellText);

				const float CellWidth = FontMeasure->Measure(CellText, CellTextStyle.Font).X + CellPadding;
				CachedColumnData->DesiredColumnWidth = FMath::Max(CachedColumnData->DesiredColumnWidth, CellWidth);
			}
		}

		AvailableRows.Add(CachedRowData);
	}
}


TSharedRef<ITableRow> FCurveTableEditor::MakeRowNameWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow<FCurveTableEditorRowListViewDataPtr>, OwnerTable)
		.Style(FEditorStyle::Get(), "DataTableEditor.NameListViewRow")
		[
			SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SNew(STextBlock)
				.Text(InRowDataPtr->DisplayName)
			]
		];
}


TSharedRef<ITableRow> FCurveTableEditor::MakeRowWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(SCurveTableListViewRow, OwnerTable)
		.CurveTableEditor(SharedThis(this))
		.Item(InRowDataPtr);
}


TSharedRef<SWidget> FCurveTableEditor::MakeCellWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex, const FName& InColumnId)
{
	int32 ColumnIndex = 0;
	for (; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FCurveTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];
		if (ColumnData->ColumnId == InColumnId)
		{
			break;
		}
	}

	// Valid column ID?
	if (AvailableColumns.IsValidIndex(ColumnIndex))
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "DataTableEditor.CellText")
				.Text(InRowDataPtr->CellData[ColumnIndex])
			];
	}

	return SNullWidget::NullWidget;
}


void FCurveTableEditor::OnRowNamesListViewScrolled(double InScrollOffset)
{
	// Synchronize the list views
	CellsListView->SetScrollOffset(InScrollOffset);
}


void FCurveTableEditor::OnCellsListViewScrolled(double InScrollOffset)
{
	// Synchronize the list views
	RowNamesListView->SetScrollOffset(InScrollOffset);
}


FOptionalSize FCurveTableEditor::GetRowNameColumnWidth() const
{
	return FOptionalSize(RowNameColumnWidth);
}

void FCurveTableEditor::OnPostReimport(UObject* InObject, bool)
{
	const UCurveTable* Table = GetCurveTable();
	if (Table && Table == InObject)
	{
		RefreshCachedCurveTable();
	}
}

#undef LOCTEXT_NAMESPACE
