// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AlembicImporterPrivatePCH.h"
#include "AlembicImportOptions.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "AbcImportSettings.h"

#include "STrackSelectionTableRow.h"

#define LOCTEXT_NAMESPACE "AlembicImportOptions"

void SAlembicImportOptions::Construct(const FArguments& InArgs)
{
	ImportSettings = InArgs._ImportSettings;
	WidgetWindow = InArgs._WidgetWindow;
	PolyMeshes = InArgs._PolyMeshes;

	check(ImportSettings);

	TSharedPtr<SBox> InspectorBox;
	this->ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
						.Text(LOCTEXT("Import_CurrentFileTitle", "Current File: "))
					]
					+ SHorizontalBox::Slot()
					.Padding(5, 0, 0, 0)
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
						.Text(InArgs._FullPath)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SBox)				
					.MinDesiredWidth(512.0f)
					.MaxDesiredHeight(350.0f)
					[
						SNew(SListView<FAbcPolyMeshObjectPtr>)
						.ItemHeight(24)						
						.ScrollbarVisibility(EVisibility::Visible)
						.ListItemsSource(&PolyMeshes)
						.OnMouseButtonDoubleClick(this, &SAlembicImportOptions::OnItemDoubleClicked)
						.OnGenerateRow(this, &SAlembicImportOptions::OnGenerateWidgetForList)
						.HeaderRow
						(
							SNew(SHeaderRow)

							+ SHeaderRow::Column("ShouldImport")
							.DefaultLabel(FText::FromString(TEXT("Should Import")))
							.FillWidth(0.2f)

							+ SHeaderRow::Column("TrackName")
							.DefaultLabel(LOCTEXT("TrackNameHeader", "Track Name"))
							.FillWidth(0.4f)

							+ SHeaderRow::Column("TrackInformation")
							.DefaultLabel(LOCTEXT("TrackInformationHeader", "Track Information"))
							.FillWidth(0.4f)
						)
					]
				]
			]


			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SAssignNew(InspectorBox, SBox)
				.MinDesiredWidth(512.0f)
				.MaxDesiredHeight(350.0f)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SAssignNew(ImportButton, SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("AlembicOptionWindow_Import", "Import"))
					.IsEnabled(this, &SAlembicImportOptions::CanImport)
					.OnClicked(this, &SAlembicImportOptions::OnImport)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("AlembicOptionWindow_Cancel", "Cancel"))
					.ToolTipText(LOCTEXT("AlembicOptionWindow_Cancel_ToolTip", "Cancels importing this Alembic file"))
					.OnClicked(this, &SAlembicImportOptions::OnCancel)
				]
			]
		];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InspectorBox->SetContent(DetailsView->AsShared());
	DetailsView->SetObject(ImportSettings);
}

TSharedRef<ITableRow> SAlembicImportOptions::OnGenerateWidgetForList(FAbcPolyMeshObjectPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STrackSelectionTableRow, OwnerTable)
		.PolyMesh(InItem);
}

bool SAlembicImportOptions::CanImport()  const
{
	return true;
}

void SAlembicImportOptions::OnItemDoubleClicked(FAbcPolyMeshObjectPtr ClickedItem)
{
	for (FAbcPolyMeshObjectPtr Item : PolyMeshes)
	{
		Item->bShouldImport = (Item == ClickedItem);
	}
}

#undef LOCTEXT_NAMESPACE

