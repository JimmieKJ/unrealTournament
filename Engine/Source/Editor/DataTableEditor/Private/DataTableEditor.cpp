// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DataTableEditorPrivatePCH.h"

#include "DataTableEditor.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SSearchBox.h"
#include "SDockTab.h"
#include "SRowEditor.h"
#include "Engine/DataTable.h"
 
#define LOCTEXT_NAMESPACE "DataTableEditor"

const FName FDataTableEditor::DataTableTabId( TEXT( "DataTableEditor_DataTable" ) );
const FName FDataTableEditor::RowEditorTabId(TEXT("DataTableEditor_RowEditor"));

void FDataTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_Data Table Editor", "Data Table Editor"));

	TabManager->RegisterTabSpawner( DataTableTabId, FOnSpawnTab::CreateSP(this, &FDataTableEditor::SpawnTab_DataTable) )
		.SetDisplayName( LOCTEXT("DataTableTab", "Data Table") )
		.SetGroup( WorkspaceMenuCategory.ToSharedRef() );

	TabManager->RegisterTabSpawner(RowEditorTabId, FOnSpawnTab::CreateSP(this, &FDataTableEditor::SpawnTab_RowEditor))
		.SetDisplayName(LOCTEXT("RowEditorTab", "Row Editor"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FDataTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	TabManager->UnregisterTabSpawner( DataTableTabId );
	TabManager->UnregisterTabSpawner(RowEditorTabId);
}

FDataTableEditor::FDataTableEditor()
{
}

FDataTableEditor::~FDataTableEditor()
{
}

void FDataTableEditor::PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
}

void FDataTableEditor::PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	if (Struct && DataTable.IsValid() && (DataTable->RowStruct == Struct))
	{
		CachedDataTable.Empty();
		ReloadVisibleData();
	}
}

void FDataTableEditor::PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
}

void FDataTableEditor::PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
	FStringAssetReference::InvalidateTag(); // Should be removed after UE-5615 is fixed
	if (Changed == DataTable.Get())
	{
		CachedDataTable.Empty();
		ReloadVisibleData();
	}
}

void FDataTableEditor::InitDataTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDataTable* Table )
{
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_DataTableEditor_Layout" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->AddTab( DataTableTabId, ETabState::OpenedTab )
		)
		->Split
		(
			FTabManager::NewStack()
			->AddTab(RowEditorTabId, ETabState::OpenedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FDataTableEditorModule::DataTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table );
	
	FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>( "DataTableEditor" );
	AddMenuExtender(DataTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( DataTableTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	// NOTE: Could fill in asset editor commands here!
}

FName FDataTableEditor::GetToolkitFName() const
{
	return FName("DataTableEditor");
}

FText FDataTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "DataTable Editor" );
}


FString FDataTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DataTable ").ToString();
}


FLinearColor FDataTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FSlateColor FDataTableEditor::GetRowColor(FName RowName) const
{
	if (RowName == HighlightedRowName)
	{
		return FSlateColor(FColorList::Orange);
	}
	return FSlateColor::UseForeground();
}

FReply FDataTableEditor::OnRowClicked(const FGeometry&, const FPointerEvent&, FName RowName)
{
	if (HighlightedRowName != RowName)
	{
		SetHighlightedRow(RowName);
		CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
	}

	return FReply::Handled();
}

TSharedPtr<SUniformGridPanel> FDataTableEditor::CreateGridPanel()
{
	TSharedPtr<SUniformGridPanel> GridPanel = SNew(SUniformGridPanel).SlotPadding( FMargin( 2.0f ) );

	if (DataTable.IsValid())
	{
		if (CachedDataTable.Num() == 0)
		{
			CachedDataTable = DataTable->GetTableData();

			RowsVisibility.SetNum(CachedDataTable.Num());
			for (int32 i = 0; i < RowsVisibility.Num(); ++i)
			{
				RowsVisibility[i] = true;
			}
		}

		check(CachedDataTable.Num() > 0 && CachedDataTable.Num() == RowsVisibility.Num());

		int32 RowIndex = 0;
		TArray<FString>& ColumnTitles = CachedDataTable[0];
		for(int i = 0;i<CachedDataTable.Num();++i)
		{
			if (RowsVisibility[i])
			{
				const bool bIsHeader = (i == 0);
				const FLinearColor RowColor = (RowIndex % 2 == 0) ? FLinearColor::Gray : FLinearColor::Black;

				TArray<FString>& Row = CachedDataTable[i];
				FName RowName(*Row[0]);
				TAttribute<FSlateColor> ForegroundColor = bIsHeader
					? FSlateColor::UseForeground()
					: TAttribute<FSlateColor>::Create(
					TAttribute<FSlateColor>::FGetter::CreateSP(this, &FDataTableEditor::GetRowColor, RowName));

				auto RowClickCallback = bIsHeader
					? FPointerEventHandler()
					: FPointerEventHandler::CreateSP(this, &FDataTableEditor::OnRowClicked, RowName);

				for(int Column = 0;Column<Row.Num();++Column)
				{
					GridPanel->AddSlot(Column, RowIndex)
					[
						SNew(SBorder)
						.Padding(1)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						.BorderBackgroundColor(RowColor)
						.ForegroundColor(ForegroundColor)
						.OnMouseButtonDown(RowClickCallback)
						[
							SNew(STextBlock)
							.Text(Row[Column])
							.ToolTipText(bIsHeader 
							?	(FString::Printf(TEXT("Column '%s"), *ColumnTitles[Column])) 
							:	(FString::Printf(TEXT("%s: %s"), *ColumnTitles[Column], *Row[Column])))
						]
					];
				}

				++RowIndex;
			}
		}
	}
	return GridPanel;
}

void FDataTableEditor::OnSearchTextChanged(const FText& SearchText)
{
	FString SearchFor = SearchText.ToString();
	if (!SearchFor.IsEmpty())
	{
		check(CachedDataTable.Num() == RowsVisibility.Num());

		// starting from index 1, because 0 is header
		for (int32 RowIdx = 1; RowIdx < CachedDataTable.Num(); ++RowIdx)
		{
			RowsVisibility[RowIdx] = false;

			for (int32 i = 0; i < CachedDataTable[RowIdx].Num(); ++i)
			{
				if (SearchFor.Len() < CachedDataTable[RowIdx][i].Len())
				{
					if (CachedDataTable[RowIdx][i].Contains(SearchFor))
					{
						RowsVisibility[RowIdx] = true;
						break;
					}
				}
			}
		}
	}
	else
	{
		for (int32 RowIdx = 0; RowIdx < RowsVisibility.Num(); ++RowIdx)
		{
			RowsVisibility[RowIdx] = true;
		}
	}

	ReloadVisibleData();
}

void FDataTableEditor::ReloadVisibleData()
{
	if (ScrollBoxWidget.IsValid())
	{
		ScrollBoxWidget->ClearChildren();
		ScrollBoxWidget->AddSlot()
			[
				CreateGridPanel().ToSharedRef()
			];
	}
}

TSharedRef<SVerticalBox> FDataTableEditor::CreateContentBox()
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(SearchBox, SSearchBox)
			.OnTextChanged(this, &FDataTableEditor::OnSearchTextChanged)
		]
		+SVerticalBox::Slot()
		[
			SAssignNew(ScrollBoxWidget, SScrollBox)
			+SScrollBox::Slot()
			[
				CreateGridPanel().ToSharedRef()
			]
		];
}

TSharedRef<SWidget> FDataTableEditor::CreateRowEditorBox()
{
	auto RowEditor = SNew(SRowEditor, DataTable.Get());
	RowEditor->RowSelectedCallback.BindSP(this, &FDataTableEditor::SetHighlightedRow);
	CallbackOnRowHighlighted.BindSP(RowEditor, &SRowEditor::SelectRow);
	return RowEditor;
}

TSharedRef<SDockTab> FDataTableEditor::SpawnTab_RowEditor(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == RowEditorTabId);

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("DataTableEditor.Tabs.Properties"))
		.Label(LOCTEXT("RowEditorTitle", "Row Editor"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				CreateRowEditorBox()
			]
		];
}


TSharedRef<SDockTab> FDataTableEditor::SpawnTab_DataTable( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == DataTableTabId );

	DataTable = Cast<UDataTable>(GetEditingObject());

	TSharedRef<SVerticalBox> ContentBox = CreateContentBox();

	GridPanelOwner = 
		SNew(SBorder)
		.Padding(2)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
		[
			ContentBox
		];

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("DataTableEditor.Tabs.Properties") )
		.Label( LOCTEXT("DataTableTitle", "Data Table") )
		.TabColorScale( GetTabColorScale() )
		[
			GridPanelOwner.ToSharedRef()
		];
}

void FDataTableEditor::SetHighlightedRow(FName Name)
{
	HighlightedRowName = Name;
}

#undef LOCTEXT_NAMESPACE
