// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraEffect.h"

#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SNiagaraEffectEditorWidget.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "NiagaraEffectEditor"


const FName FNiagaraEffectEditor::UpdateTabId(TEXT("NiagaraEditor_Effect"));

void FNiagaraEffectEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraEffectEditor", "Niagara Effect"));

	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	TabManager->RegisterTabSpawner(UpdateTabId, FOnSpawnTab::CreateSP(this, &FNiagaraEffectEditor::SpawnTab))
		.SetDisplayName(LOCTEXT("NiagaraEffect", "Niagara Effect"))
		.SetGroup( WorkspaceMenuCategory.ToSharedRef() );
}

void FNiagaraEffectEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner(UpdateTabId);
}

FNiagaraEffectEditor::~FNiagaraEffectEditor()
{

}


void FNiagaraEffectEditor::InitNiagaraEffectEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEffect* InEffect)
{
	Effect = InEffect;
	check(Effect != NULL);

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Niagara_Effect_Layout_v2")
		->AddArea
		(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
		FTabManager::NewStack()
		->SetSizeCoefficient(0.1f)
		->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		->SetHideTabWell(true)
		)
		->Split
		(
		FTabManager::NewStack()
		->AddTab(UpdateTabId, ETabState::OpenedTab)
		)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Effect);
	
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddMenuExtender(NiagaraEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

FName FNiagaraEffectEditor::GetToolkitFName() const
{
	return FName("Niagara");
}

FText FNiagaraEffectEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Niagara");
}

FString FNiagaraEffectEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Niagara ").ToString();
}


FLinearColor FNiagaraEffectEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}


/** Create new tab for the supplied graph - don't call this directly, call SExplorer->FindTabForGraph.*/
TSharedRef<SNiagaraEffectEditorWidget> FNiagaraEffectEditor::CreateEditorWidget(UNiagaraEffect* InEffect)
{
	check(InEffect != NULL);
	EffectInstance = new FNiagaraEffectInstance(InEffect);
	
	if (!EditorCommands.IsValid())
	{
		EditorCommands = MakeShareable(new FUICommandList);
	}

	// Create the appearance info
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "NIAGARA");

	// Make title bar
	TSharedRef<SWidget> TitleBarWidget =
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush(TEXT("Graph.TitleBackground")))
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EffectLabel", "Niagara Effect"))
				.TextStyle(FEditorStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
			]
		];

		
		
	// Make the effect editor widget
	return SNew(SNiagaraEffectEditorWidget).TitleBar(TitleBarWidget).EffectObj(InEffect).EffectInstance(EffectInstance);
}


TSharedRef<SDockTab> FNiagaraEffectEditor::SpawnTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == UpdateTabId);

	TSharedRef<SNiagaraEffectEditorWidget> Editor = CreateEditorWidget(Effect);

	UpdateEditorPtr = Editor; // Keep pointer to editor

	return SNew(SDockTab)
		.Label(LOCTEXT("NiagaraEffect", "Niagara Effect"))
		.TabColorScale(GetTabColorScale())
		[
			Editor
		];
}

void FNiagaraEffectEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedRef<SWidget> AddEmitterBox)
		{
			ToolbarBuilder.BeginSection("AddEmitter");
			{
				ToolbarBuilder.AddWidget(AddEmitterBox);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	TSharedRef<SWidget> AddEmitterBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4)
		[
			SNew(SButton)
			.OnClicked(this, &FNiagaraEffectEditor::OnAddEmitterClicked)
			.Content()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("LevelEditor.Add"))
					]
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NiagaraToolbar_AddEmitter", "Add Emitter"))
					]
			]
		];

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, AddEmitterBox)
		);

	AddToolbarExtender(ToolbarExtender);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddToolbarExtender(NiagaraEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}



FReply FNiagaraEffectEditor::OnAddEmitterClicked()
{
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");

	FNiagaraEmitterProperties *Props = Effect->AddEmitterProperties();
	TSharedPtr<FNiagaraSimulation> NewEmitter = EffectInstance->AddEmitter(Props);
	Effect->CreateEffectRendererProps(NewEmitter);
	UpdateEditorPtr.Pin()->GetViewport()->SetPreviewEffect(EffectInstance);
	UpdateEditorPtr.Pin()->UpdateList();
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
