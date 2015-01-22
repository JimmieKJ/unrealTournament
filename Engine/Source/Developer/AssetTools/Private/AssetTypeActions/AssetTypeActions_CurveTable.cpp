// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"

#include "Editor/CurveTableEditor/Public/CurveTableEditorModule.h"
#include "Editor/CurveTableEditor/Public/ICurveTableEditor.h"
#include "Engine/CurveTable.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_CurveTable::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Tables = GetTypedWeakObjectPtrs<UObject>(InObjects);

	TArray<FString> ImportPaths;
	for (auto TableIter = Tables.CreateConstIterator(); TableIter; ++TableIter)
	{
		const UCurveTable* CurTable = Cast<UCurveTable>((*TableIter).Get());
		if (CurTable)
		{
			ImportPaths.Add(FReimportManager::ResolveImportFilename(CurTable->ImportPath, CurTable));
		}
	}

	TArray<FString> XLSExtensions;
	XLSExtensions.Add(TEXT(".xls"));
	XLSExtensions.Add(TEXT(".xlsm"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CurveTable_OpenSourceXLS", "Open Source (.xls/.xlsm)"),
		LOCTEXT("CurveTable_OpenSourceXLSTooltip", "Opens the curve table's source XLS/XLSM file in an external editor."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CurveTable::ExecuteFindExcelFileInExplorer, ImportPaths, XLSExtensions ),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_CurveTable::CanExecuteFindExcelFileInExplorer, ImportPaths, XLSExtensions)
			)
		);
}

void FAssetTypeActions_CurveTable::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Table = Cast<UCurveTable>(*ObjIt);
		if (Table != NULL)
		{
			FCurveTableEditorModule& CurveTableEditorModule = FModuleManager::LoadModuleChecked<FCurveTableEditorModule>( "CurveTableEditor" );
			TSharedRef< ICurveTableEditor > NewCurveTableEditor = CurveTableEditorModule.CreateCurveTableEditor( EToolkitMode::Standalone, EditWithinLevelEditor, Table );
		}
	}
}

void FAssetTypeActions_CurveTable::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto CurveTable = CastChecked<UCurveTable>(Asset);
		OutSourceFilePaths.Add(FReimportManager::ResolveImportFilename(CurveTable->ImportPath, CurveTable));
	}
}

#undef LOCTEXT_NAMESPACE