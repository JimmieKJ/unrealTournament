// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"

#include "Editor/DataTableEditor/Public/DataTableEditorModule.h"
#include "Editor/DataTableEditor/Public/IDataTableEditor.h"
#include "Engine/DataTable.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_DataTable::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Tables = GetTypedWeakObjectPtrs<UObject>(InObjects);
	
	TArray<FString> ImportPaths;
	for (auto TableIter = Tables.CreateConstIterator(); TableIter; ++TableIter)
	{
		const UDataTable* CurTable = Cast<UDataTable>((*TableIter).Get());
		if (CurTable)
		{
			ImportPaths.Add(FReimportManager::ResolveImportFilename(CurTable->ImportPath, CurTable));
		}
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DataTable_JSON", "JSON"),
		LOCTEXT("DataTable_JSONTooltip", "Creates a JSON version of the data table in the lock."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_DataTable::ExecuteJSON, Tables ),
			FCanExecuteAction()
			)
		);

	TArray<FString> XLSExtensions;
	XLSExtensions.Add(TEXT(".xls"));
	XLSExtensions.Add(TEXT(".xlsm"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DataTable_OpenSourceXLS", "Open Source (.xls/.xlsm)"),
		LOCTEXT("DataTable_OpenSourceXLSTooltip", "Opens the data table's source XLS/XLSM file in an external editor."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_DataTable::ExecuteFindExcelFileInExplorer, ImportPaths, XLSExtensions ),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_DataTable::CanExecuteFindExcelFileInExplorer, ImportPaths, XLSExtensions)
			)
		);
}

void FAssetTypeActions_DataTable::ExecuteJSON(TArray< TWeakObjectPtr<UObject> > Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Table = Cast<UDataTable>((*ObjIt).Get());
		if (Table != NULL)
		{
			UE_LOG(LogDataTable, Log,  TEXT("JSON DataTable : %s"), *Table->GetTableAsJSON());
		}
	}
}

void FAssetTypeActions_DataTable::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Table = Cast<UDataTable>(*ObjIt);
		if (Table != NULL)
		{
			FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>( "DataTableEditor" );
			TSharedRef< IDataTableEditor > NewDataTableEditor = DataTableEditorModule.CreateDataTableEditor( EToolkitMode::Standalone, EditWithinLevelEditor, Table );
		}
	}
}

void FAssetTypeActions_DataTable::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto DataTable = CastChecked<UDataTable>(Asset);
		OutSourceFilePaths.Add(FReimportManager::ResolveImportFilename(DataTable->ImportPath, DataTable));
	}
}

#undef LOCTEXT_NAMESPACE