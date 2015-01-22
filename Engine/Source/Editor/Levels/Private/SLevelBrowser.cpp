// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelsPrivatePCH.h"

#include "SLevelBrowser.h"
#include "SSearchBox.h"

#define LOCTEXT_NAMESPACE "LevelBrowser"

void SLevelBrowser::Construct(const FArguments& InArgs)
{
	Mode = ELevelBrowserMode::Levels;

	LevelCollectionViewModel = FLevelCollectionViewModel::Create(GEditor);
	SelectedLevelsFilter = MakeShareable(new FActorsAssignedToSpecificLevelsFilter());
	SelectedLevelViewModel = FLevelViewModel::Create(NULL, NULL, GEditor); //We'll set the datasource for this viewmodel later

	SearchBoxLevelFilter = MakeShareable(new LevelTextFilter(LevelTextFilter::FItemToStringArray::CreateSP(this, &SLevelBrowser::TransformLevelToString)));

	LevelCollectionViewModel->AddFilter(SearchBoxLevelFilter.ToSharedRef());
	LevelCollectionViewModel->OnLevelsChanged().AddSP(this, &SLevelBrowser::OnLevelsChanged);
	LevelCollectionViewModel->OnSelectionChanged().AddSP(this, &SLevelBrowser::UpdateSelectedLevel);


	//////////////////////////////////////////////////////////////////////////
	//	Levels View Section
	SAssignNew(LevelsSection, SBorder)
		.Padding(5)
		.BorderImage(FEditorStyle::GetBrush("NoBrush"))
		.Content()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				//Shift Up Button
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("ShiftUpHint", "Shift Streaming Level Up").ToString())
					.ContentPadding(0)
					.OnClicked(this, &SLevelBrowser::OnClickUp)
					.IsEnabled(this, &SLevelBrowser::CanShiftSelection)
					[
						SNew(SImage).Image(FEditorStyle::GetBrush("Level.ArrowUp"))
					]
				]
				//Shift Down Button
				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("ShiftDownHint", "Shift Streaming Level Down").ToString())
						.ContentPadding(0)
						.OnClicked(this, &SLevelBrowser::OnClickDown)
						.IsEnabled(this, &SLevelBrowser::CanShiftSelection)
						[
							SNew(SImage).Image(FEditorStyle::GetBrush("Level.ArrowDown"))
						]
					]
				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.f)
					[
						SNew(SSearchBox)
						.ToolTipText(LOCTEXT("FilterSearchToolTip", "Type here to search Levels").ToString())
						.HintText(LOCTEXT("FilterSearchHint", "Search Levels"))
						.OnTextChanged(SearchBoxLevelFilter.Get(), &LevelTextFilter::SetRawFilterText)
					]
			]
			+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SLevelsView, LevelCollectionViewModel.ToSharedRef())
					.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
					.ConstructContextMenu(FOnContextMenuOpening::CreateSP(this, &SLevelBrowser::ConstructLevelContextMenu))
					.HighlightText(SearchBoxLevelFilter.Get(), &LevelTextFilter::GetRawFilterText)
				]
		];

	ChildSlot
		[
			SAssignNew(ContentAreaBox, SVerticalBox)
		];


	SetupLevelsMode();
}

#undef LOCTEXT_NAMESPACE
