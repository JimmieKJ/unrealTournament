// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"

#include "Editor/CurveTableEditor/Public/CurveTableEditorModule.h"
#include "Editor/CurveTableEditor/Public/ICurveTableEditor.h"
#include "Engine/CurveTable.h"
#include "DesktopPlatformModule.h"
#include "IMainFrameModule.h"

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
			CurTable->AssetImportData->ExtractFilenames(ImportPaths);
		}
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CurveTable_ExportAsCSV", "Export as CSV"),
		LOCTEXT("CurveTable_ExportAsCSVTooltip", "Export the curve table as a file containing CSV data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CurveTable::ExecuteExportAsCSV, Tables ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CurveTable_ExportAsJSON", "Export as JSON"),
		LOCTEXT("CurveTable_ExportAsJSONTooltip", "Export the curve table as a file containing JSON data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CurveTable::ExecuteExportAsJSON, Tables ),
			FCanExecuteAction()
			)
		);

	TArray<FString> PotentialFileExtensions;
	PotentialFileExtensions.Add(TEXT(".xls"));
	PotentialFileExtensions.Add(TEXT(".xlsm"));
	PotentialFileExtensions.Add(TEXT(".csv"));
	PotentialFileExtensions.Add(TEXT(".json"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CurveTable_OpenSourceData", "Open Source Data"),
		LOCTEXT("CurveTable_OpenSourceDataTooltip", "Opens the curve table's source data file in an external editor. It will search using the following extensions: .xls/.xlsm/.csv/.json"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CurveTable::ExecuteFindSourceFileInExplorer, ImportPaths, PotentialFileExtensions ),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_CurveTable::CanExecuteFindSourceFileInExplorer, ImportPaths, PotentialFileExtensions)
			)
		);
}

void FAssetTypeActions_CurveTable::ExecuteExportAsCSV(TArray< TWeakObjectPtr<UObject> > Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	void* ParentWindowWindowHandle = nullptr;

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
	{
		ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
	}

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto CurTable = Cast<UCurveTable>((*ObjIt).Get());
		if (CurTable)
		{
			const FText Title = FText::Format(LOCTEXT("CurveTable_ExportCSVDialogTitle", "Export '{0}' as CSV..."), FText::FromString(*CurTable->GetName()));
			const FString CurrentFilename = CurTable->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Curve Table CSV (*.csv)|*.csv");

			TArray<FString> OutFilenames;
			DesktopPlatform->SaveFileDialog(
				ParentWindowWindowHandle,
				Title.ToString(),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetPath(CurrentFilename),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetBaseFilename(CurrentFilename) + TEXT(".csv"),
				FileTypes,
				EFileDialogFlags::None,
				OutFilenames
				);

			if (OutFilenames.Num() > 0)
			{
				FFileHelper::SaveStringToFile(CurTable->GetTableAsCSV(), *OutFilenames[0]);
			}
		}
	}
}

void FAssetTypeActions_CurveTable::ExecuteExportAsJSON(TArray< TWeakObjectPtr<UObject> > Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	void* ParentWindowWindowHandle = nullptr;

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
	{
		ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
	}

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto CurTable = Cast<UCurveTable>((*ObjIt).Get());
		if (CurTable)
		{
			const FText Title = FText::Format(LOCTEXT("CurveTable_ExportCSVDialogTitle", "Export '{0}' as JSON..."), FText::FromString(*CurTable->GetName()));
			const FString CurrentFilename = CurTable->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Curve Table JSON (*.json)|*.json");

			TArray<FString> OutFilenames;
			DesktopPlatform->SaveFileDialog(
				ParentWindowWindowHandle,
				Title.ToString(),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetPath(CurrentFilename),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetBaseFilename(CurrentFilename) + TEXT(".json"),
				FileTypes,
				EFileDialogFlags::None,
				OutFilenames
				);

			if (OutFilenames.Num() > 0)
			{
				FFileHelper::SaveStringToFile(CurTable->GetTableAsJSON(), *OutFilenames[0]);
			}
		}
	}
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
		CurveTable->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

#undef LOCTEXT_NAMESPACE