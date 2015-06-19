// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "Runtime/Slate/Private/Framework/Docking/DockingPrivate.h"
#include "Runtime/Slate/Private/Framework/Docking/FDockingDragOperation.h"

class SHelperTabContent : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHelperTabContent)
	{}
	SLATE_ARGUMENT(TSharedPtr<FTabManager::FLayout>, InLayout)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, TSharedRef<SDockTab> InParentTab, TSharedRef<FTabManager> InTabManager)
	{
		ParentTab = InParentTab;
		TabManager = InTabManager;

		Layout = InArgs._InLayout;

		ChildSlot
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(SButton)
				.OnClicked(this, &SHelperTabContent::OnSplitClicked, EOrientation::Orient_Horizontal)

			]
			+SHorizontalBox::Slot()
			[
				SNew(SButton)
				.OnClicked(this, &SHelperTabContent::OnSplitClicked, EOrientation::Orient_Vertical)
			]
		];
	}

	TWeakPtr<SDockTab> ParentTab;
	TWeakPtr<FTabManager> TabManager;
	TSharedPtr<FTabManager::FLayout> Layout;

	FReply OnSplitClicked(EOrientation InOrientation)
	{
		if (!TabManager.Pin().IsValid())
		{
			return FReply::Unhandled();
		}

		if (Layout.IsValid())
		{
			Layout->AddArea(
				FTabManager::NewPrimaryArea()
				->Split
				(
				FTabManager::NewStack()
				->SetHideTabWell(true)
				->AddTab("TabId", ETabState::OpenedTab)
				));
		}

		ParentTab.Pin()->SetContent(TabManager.Pin()->RestoreFrom(Layout.ToSharedRef(), ParentTab.Pin()->GetParentWindow()).ToSharedRef());



		return FReply::Handled();
	}
};


class SLayoutCreator : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLayoutCreator)
	{}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, TSharedPtr<SWindow> ParentWindow)
	{
		DefaultLayout = FTabManager::NewLayout("LayoutCreator_Default")
		->AddArea
		(
			FTabManager::NewArea(720, 600)
			->SetWindow(FVector2D(420, 10), false)
			->Split
			(
				FTabManager::NewStack()
				->AddTab("MainTab", ETabState::OpenedTab)
			)
		);

		FGlobalTabmanager::Get()->RegisterTabSpawner("MainTab", FOnSpawnTab::CreateRaw(this, &SLayoutCreator::SpawnMainTab));

		
		FGlobalTabmanager::Get()->RestoreFrom(DefaultLayout.ToSharedRef(), TSharedPtr<SWindow>());
	}

	~SLayoutCreator()
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner("MainTab");
		TabManager->UnregisterAllTabSpawners();
		TabManager.Reset();
	}

	TSharedRef< SDockTab > SpawnMainTab(const FSpawnTabArgs& Args)
	{
		TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("LayoutCreator_FirstLayout")
		->AddArea
		(
			// The primary area will be restored and returned as a widget.
			// Unlike other areas it will not get its own window.
			// This allows the caller to explicitly place the primary area somewhere in the widget hierarchy.
			FTabManager::NewPrimaryArea()
			->Split
			(
				FTabManager::NewStack()
				->SetHideTabWell(true)
				->AddTab("FirstTab", ETabState::OpenedTab)
			)
		);

		ExtendableLayout = Layout;
		//Layout->GetPrimaryArea().Pin()->spl


		SAssignNew(MainTab, SDockTab)
			.TabRole(ETabRole::MajorTab)
			.Label(FText::FromString("MainTab"));

		TabManager = FGlobalTabmanager::Get()->NewTabManager(MainTab.ToSharedRef());

		TabManager->RegisterTabSpawner("FirstTab", FOnSpawnTab::CreateRaw(this, &SLayoutCreator::SpawnFristTab));

		MainTab->SetContent(
			TabManager->RestoreFrom(Layout, Args.GetOwnerWindow()).ToSharedRef()
				);

		return MainTab.ToSharedRef();
	}

	TSharedRef< SDockTab > SpawnFristTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.Label(FText::FromString("FirstTab"))
			[
				SNew(SHelperTabContent, MainTab.ToSharedRef(), TabManager.ToSharedRef())
				.InLayout(ExtendableLayout)
			];
	}

	TSharedPtr<SDockTab> MainTab;
	TSharedPtr<FTabManager::FLayout> DefaultLayout;
	TSharedPtr<FTabManager::FLayout> ExtendableLayout;
	
	TSharedPtr<FTabManager> TabManager;
};