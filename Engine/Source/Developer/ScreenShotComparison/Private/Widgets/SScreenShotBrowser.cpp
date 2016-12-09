// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotBrowser.cpp: Implements the SScreenShotBrowser class.
=============================================================================*/

#include "Widgets/SScreenShotBrowser.h"
#include "HAL/FileManager.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/SScreenComparisonRow.h"
#include "Models/ScreenComparisonModel.h"
#include "Misc/FeedbackContext.h"

#define LOCTEXT_NAMESPACE "ScreenshotComparison"

void SScreenShotBrowser::Construct( const FArguments& InArgs,  IScreenShotManagerRef InScreenShotManager  )
{
	ScreenShotManager = InScreenShotManager;
	ComparisonRoot = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() / TEXT("Exported"));


	ChildSlot
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SDirectoryPicker)
				.Directory(ComparisonRoot)
				.OnDirectoryChanged(this, &SScreenShotBrowser::OnDirectoryChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RunComparison", "Run Compare"))
				.OnClicked_Lambda(
					[&]()
				{
					GWarn->BeginSlowTask(LOCTEXT("ComparingScreenshots", "Comparing Screenshots..."), true);
					ScreenShotManager->CompareScreensotsAsync().Wait();
					ScreenShotManager->ExportScreensotsAsync().Wait();
					GWarn->EndSlowTask();

					RebuildTree();

					return FReply::Handled();
				})
			]
		]

		+SVerticalBox::Slot()
		.FillHeight( 1.0f )
		[
			SAssignNew(ComparisonView, SListView< TSharedPtr<FScreenComparisonModel> >)
			.ListItemsSource(&ComparisonList)
			.OnGenerateRow(this, &SScreenShotBrowser::OnGenerateWidgetForScreenResults)
			.SelectionMode(ESelectionMode::None)
			.HeaderRow
			(
				SNew(SHeaderRow)

				+ SHeaderRow::Column("Name")
				.DefaultLabel(LOCTEXT("ColumnHeader_Name", "Name"))
				.FillWidth(1.0f)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Delta")
				.DefaultLabel(LOCTEXT("ColumnHeader_Delta", "Local | Global Delta"))
				.FixedWidth(120)
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Preview")
				.DefaultLabel(LOCTEXT("ColumnHeader_Preview", "Preview"))
				.FixedWidth(500)
				.HAlignHeader(HAlign_Left)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)
			)
		]
	];

	// Register for callbacks
	ScreenShotManager->RegisterScreenShotUpdate(ScreenShotDelegate);

	RebuildTree();
}

void SScreenShotBrowser::OnDirectoryChanged(const FString& Directory)
{
	ComparisonRoot = Directory;

	RebuildTree();
}

TSharedRef<ITableRow> SScreenShotBrowser::OnGenerateWidgetForScreenResults(TSharedPtr<FScreenComparisonModel> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	// Create the row widget.
	return
		SNew(SScreenComparisonRow, OwnerTable)
		.ScreenshotManager(ScreenShotManager)
		.ComparisonDirectory(ComparisonDirectory)
		.Comparisons(CurrentComparisons)
		.ComparisonResult(InItem);
}

void SScreenShotBrowser::RebuildTree()
{
	TArray<FString> Changelists;
	FString SearchDirectory = ComparisonRoot / TEXT("*");
	IFileManager::Get().FindFiles(Changelists, *SearchDirectory, false, true);

	ComparisonList.Reset();

	if ( Changelists.Num() > 0 )
	{
		Changelists.StableSort();

		ComparisonDirectory = ComparisonRoot / Changelists.Top();
		CurrentComparisons = ScreenShotManager->ImportScreensots(ComparisonDirectory);

		if ( CurrentComparisons.IsValid() )
		{
			// Copy the comparisons to an array as shared pointers the list view can use.
			for ( FString& Added : CurrentComparisons->Added )
			{
				TSharedPtr<FScreenComparisonModel> Model = MakeShared<FScreenComparisonModel>(EComparisonResultType::Added);
				Model->Folder = Added;
				ComparisonList.Add(Model);
			}

			for ( FString& Missing : CurrentComparisons->Missing )
			{
				TSharedPtr<FScreenComparisonModel> Model = MakeShared<FScreenComparisonModel>(EComparisonResultType::Missing);
				Model->Folder = Missing;
				ComparisonList.Add(Model);
			}

			// Copy the comparisons to an array as shared pointers the list view can use.
			for ( FImageComparisonResult& Result : CurrentComparisons->Comparisons )
			{
				TSharedPtr<FScreenComparisonModel> Model = MakeShared<FScreenComparisonModel>(EComparisonResultType::Compared);
				Model->ComparisonResult = Result;

				ComparisonList.Add(Model);
			}
		}
	}

	ComparisonView->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
