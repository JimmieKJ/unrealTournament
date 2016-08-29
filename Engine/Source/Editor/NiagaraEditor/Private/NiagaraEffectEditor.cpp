// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraEffect.h"
#include "NiagaraSequence.h"

#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "NiagaraEffectEditor"


const FName FNiagaraEffectEditor::UpdateTabId(TEXT("NiagaraEditor_Effect"));
const FName FNiagaraEffectEditor::ViewportTabID(TEXT("NiagaraEffectEditor_Viewport"));
const FName FNiagaraEffectEditor::EmitterEditorTabID(TEXT("NiagaraEffectEditor_EmitterEditor"));
const FName FNiagaraEffectEditor::DevEmitterEditorTabID(TEXT("NiagaraEffectEditor_DevEmitterEditor"));
const FName FNiagaraEffectEditor::CurveEditorTabID(TEXT("NiagaraEffectEditor_CurveEditor"));
const FName FNiagaraEffectEditor::SequencerTabID(TEXT("NiagaraEffectEditor_Sequencer"));

void FNiagaraEffectEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraEffectEditor", "Niagara Effect"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabID, FOnSpawnTab::CreateSP(this, &FNiagaraEffectEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("Preview", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(EmitterEditorTabID, FOnSpawnTab::CreateSP(this, &FNiagaraEffectEditor::SpawnTab_EmitterList))
		.SetDisplayName(LOCTEXT("Emitters", "Emitters"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DevEmitterEditorTabID, FOnSpawnTab::CreateSP(this, &FNiagaraEffectEditor::SpawnTab_DevEmitterList))
		.SetDisplayName(LOCTEXT("DevEmitters", "Emitters_Dev"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(CurveEditorTabID, FOnSpawnTab::CreateSP(this, &FNiagaraEffectEditor::SpawnTab_CurveEd))
		.SetDisplayName(LOCTEXT("Curves", "Curves"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SequencerTabID, FOnSpawnTab::CreateSP(this, &FNiagaraEffectEditor::SpawnTab_Sequencer))
		.SetDisplayName(LOCTEXT("Timeline", "Timeline"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());


	InTabManager->RegisterTabSpawner(UpdateTabId, FOnSpawnTab::CreateSP(this, &FNiagaraEffectEditor::SpawnTab))
		.SetDisplayName(LOCTEXT("NiagaraEffect", "Niagara Effect"))
		.SetGroup( WorkspaceMenuCategory.ToSharedRef() );


}

void FNiagaraEffectEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(UpdateTabId);
	InTabManager->UnregisterTabSpawner(ViewportTabID);
	InTabManager->UnregisterTabSpawner(EmitterEditorTabID);
	InTabManager->UnregisterTabSpawner(DevEmitterEditorTabID);
	InTabManager->UnregisterTabSpawner(CurveEditorTabID);
	InTabManager->UnregisterTabSpawner(SequencerTabID);
}




FNiagaraEffectEditor::~FNiagaraEffectEditor()
{

}


void FNiagaraEffectEditor::InitNiagaraEffectEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEffect* InEffect)
{
	Effect = InEffect;
	check(Effect != NULL);
	EffectInstance = MakeShareable(new FNiagaraEffectInstance(InEffect));

	const float InTime = -0.02f;
	const float OutTime = 3.2f;

	if (!Sequencer.IsValid())
	{
		NiagaraSequence = NewObject<UNiagaraSequence>(InEffect);
		NiagaraSequence->MovieScene = NewObject<UMovieScene>(NiagaraSequence, FName("Niagara Effect MovieScene"));
		NiagaraSequence->MovieScene->SetPlaybackRange(InTime, OutTime);

		FSequencerViewParams ViewParams(TEXT("NiagaraSequencerSettings"));
		{
			ViewParams.InitialScrubPosition = 0;
		}

		FSequencerInitParams SequencerInitParams;
		{
			SequencerInitParams.ViewParams = ViewParams;
			SequencerInitParams.RootSequence = NiagaraSequence;
			SequencerInitParams.bEditWithinLevelEditor = false;
			SequencerInitParams.ToolkitHost = nullptr;
		}

		ISequencerModule &SeqModule = FModuleManager::LoadModuleChecked< ISequencerModule >("Sequencer");
		FDelegateHandle CreateTrackEditorHandle = SeqModule.RegisterTrackEditor_Handle(FOnCreateTrackEditor::CreateStatic(&FNiagaraEffectEditor::CreateTrackEditor));
		Sequencer = SeqModule.CreateSequencer(SequencerInitParams);

		for (TSharedPtr<FNiagaraSimulation> Emitter : EffectInstance->GetEmitters())
		{
			UEmitterMovieSceneTrack *Track = NiagaraSequence->MovieScene->AddMasterTrack<UEmitterMovieSceneTrack>();
			Track->SetEmitter(Emitter);
		}
	}

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Niagara_Effect_Layout_v7")
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
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.1f)
					->AddTab(ViewportTabID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(EmitterEditorTabID, ETabState::OpenedTab)
					->AddTab(DevEmitterEditorTabID, ETabState::OpenedTab)
				)
			)
			->Split
			(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.3f)
			->AddTab(CurveEditorTabID, ETabState::OpenedTab)
			->AddTab(SequencerTabID, ETabState::OpenedTab)
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

void FNiagaraEffectEditor::NotifyPreChange(UProperty* PropertyAboutToChanged)
{
	Viewport->GetPreviewComponent()->UnregisterComponent();
}

void FNiagaraEffectEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	Viewport->GetPreviewComponent()->RegisterComponent();
}

/** Create new tab for the supplied graph - don't call this directly, call SExplorer->FindTabForGraph.*/
TSharedRef<SNiagaraEffectEditorWidget> FNiagaraEffectEditor::CreateEditorWidget(UNiagaraEffect* InEffect)
{
	check(InEffect != NULL);
	EffectInstance = MakeShareable(new FNiagaraEffectInstance(InEffect));
	
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
	return SNew(SNiagaraEffectEditorWidget).EffectObj(InEffect).EffectInstance(EffectInstance.Get()).EffectObj(Effect).EffectEditor(this);
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




TSharedRef<SDockTab> FNiagaraEffectEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == ViewportTabID);

	Viewport = SNew(SNiagaraEffectEditorViewport);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			Viewport.ToSharedRef()
		];

	Viewport->SetPreviewEffect(EffectInstance);
	Viewport->OnAddedToTab(SpawnedTab);

	return SpawnedTab;
}



TSharedRef<SDockTab> FNiagaraEffectEditor::SpawnTab_EmitterList(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == EmitterEditorTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SAssignNew(EmitterEditorWidget, SNiagaraEffectEditorWidget).EffectInstance(EffectInstance.Get()).EffectEditor(this).EffectObj(Effect)
		];


	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraEffectEditor::SpawnTab_DevEmitterList(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == DevEmitterEditorTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SAssignNew(DevEmitterEditorWidget, SNiagaraEffectEditorWidget).EffectInstance(EffectInstance.Get()).EffectEditor(this).EffectObj(Effect).bForDev(true)
		];


	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraEffectEditor::SpawnTab_CurveEd(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == CurveEditorTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SAssignNew(TimeLine, SNiagaraTimeline)
		];

	return SpawnedTab;
}



TSharedRef<SDockTab> FNiagaraEffectEditor::SpawnTab_Sequencer(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SequencerTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			Sequencer->GetSequencerWidget()
		];

	return SpawnedTab;
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

	UNiagaraEmitterProperties *Props = Effect->AddEmitterProperties();
	TSharedPtr<FNiagaraSimulation> NewEmitter = EffectInstance->AddEmitter(Props);
	Effect->CreateEffectRendererProps(NewEmitter);
	Viewport->SetPreviewEffect(EffectInstance);
	EmitterEditorWidget->UpdateList();
	DevEmitterEditorWidget->UpdateList();
	
	Effect->MarkPackageDirty();

	return FReply::Handled();
}

FReply FNiagaraEffectEditor::OnDuplicateEmitterClicked(TSharedPtr<FNiagaraSimulation> Emitter)
{
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");

	if (UNiagaraEmitterProperties* ToDupe = Emitter->GetProperties().Get())
	{
		UNiagaraEmitterProperties *Props = CastChecked<UNiagaraEmitterProperties>(StaticDuplicateObject(ToDupe,Effect));
		Effect->AddEmitterProperties(Props);
		TSharedPtr<FNiagaraSimulation> NewEmitter = EffectInstance->AddEmitter(Props);
		Effect->CreateEffectRendererProps(NewEmitter);
		Viewport->SetPreviewEffect(EffectInstance);
		EmitterEditorWidget->UpdateList();
		DevEmitterEditorWidget->UpdateList();

		Effect->MarkPackageDirty();
	}
	return FReply::Handled();
}

FReply FNiagaraEffectEditor::OnDeleteEmitterClicked(TSharedPtr<FNiagaraSimulation> Emitter)
{
	if (UNiagaraEmitterProperties* ToDelete = Emitter->GetProperties().Get())
	{
		Effect->DeleteEmitterProperties(ToDelete);
		EffectInstance->DeleteEmitter(Emitter);
		Viewport->SetPreviewEffect(EffectInstance);
		EmitterEditorWidget->UpdateList();
		DevEmitterEditorWidget->UpdateList();

		Effect->MarkPackageDirty();
	}
	return FReply::Handled();
}


FReply FNiagaraEffectEditor::OnEmitterSelected(TSharedPtr<FNiagaraSimulation> SelectedItem, ESelectInfo::Type SelType)
{
	return FReply::Handled();
}


void FNiagaraEffectEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	if ( NiagaraSequence != nullptr )
	{
		Collector.AddReferencedObject( NiagaraSequence );
	}
}


#undef LOCTEXT_NAMESPACE
