// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealEd.h"
#include "UObjectGlobals.h"
#include "FbxImporter.h"
#include "FbxSceneOptionWindow.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "IDocumentation.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "SDockTab.h"
#include "TabManager.h"
#include "STextComboBox.h"

#define LOCTEXT_NAMESPACE "FBXOption"

SFbxSceneOptionWindow::SFbxSceneOptionWindow()
	: SceneInfo(NULL)
	, GlobalImportSettings(NULL)
	, SceneImportOptionsDisplay(NULL)
	, SceneImportOptionsStaticMeshDisplay(NULL)
	, SceneMeshOverrideOptions(NULL)
	, SceneImportOptionsSkeletalMeshDisplay(NULL)
	, SceneImportOptionsAnimationDisplay(NULL)
	, SceneImportOptionsMaterialDisplay(NULL)
	, bShouldImport(false)
{
}

SFbxSceneOptionWindow::~SFbxSceneOptionWindow()
{
	if (FbxSceneImportTabManager.IsValid())
	{
		FbxSceneImportTabManager->UnregisterAllTabSpawners();
	}
	FbxSceneImportTabManager = NULL;
	Layout = NULL;

	SceneTabTreeview = NULL;
	SceneTabDetailsView = NULL;

	SceneInfo = NULL;
	GlobalImportSettings = NULL;
	SceneImportOptionsDisplay = NULL;
	SceneImportOptionsStaticMeshDisplay = NULL;
	SceneMeshOverrideOptions = NULL;
	SceneImportOptionsSkeletalMeshDisplay = NULL;
	SceneImportOptionsAnimationDisplay = NULL;
	SceneImportOptionsMaterialDisplay = NULL;
	OwnerWindow = NULL;
	
}

TSharedRef<SDockTab> SFbxSceneOptionWindow::SpawnSceneTab(const FSpawnTabArgs& Args)
{
	//Create the treeview
	SceneTabTreeview = SNew(SFbxSceneTreeView)
		.SceneInfo(SceneInfo);

	TSharedPtr<SBox> InspectorBox;
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("WidgetFbxSceneActorTab", "Scene"))
		.ToolTipText(LOCTEXT("WidgetFbxSceneActorTabTextToolTip", "Switch to the scene tab."))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.4)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				[
					SNew(SUniformGridPanel)
					.SlotPadding(2)
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SCheckBox)
							.HAlign(HAlign_Center)
							.OnCheckStateChanged(SceneTabTreeview.Get(), &SFbxSceneTreeView::OnToggleSelectAll)
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(0.0f, 3.0f, 6.0f, 3.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FbxOptionWindow_Scene_All", "All"))
						]
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("FbxOptionWindow_Scene_ExpandAll", "Expand All"))
						.OnClicked(SceneTabTreeview.Get(), &SFbxSceneTreeView::OnExpandAll)
					]
					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("FbxOptionWindow_Scene_CollapseAll", "Collapse All"))
						.OnClicked(SceneTabTreeview.Get(), &SFbxSceneTreeView::OnCollapseAll)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBox)
					[
						SceneTabTreeview.ToSharedRef()
					]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.6)
			[
				SAssignNew(InspectorBox, SBox)
			]
		];
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	SceneTabDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBox->SetContent(SceneTabDetailsView->AsShared());
	SceneTabDetailsView->SetObject(SceneImportOptionsDisplay);
	return DockTab;
}

TSharedRef<SDockTab> SFbxSceneOptionWindow::SpawnStaticMeshTab(const FSpawnTabArgs& Args)
{
	//Create the static mesh listview
	StaticMeshTabListView = SNew(SFbxSceneStaticMeshListView)
		.SceneInfo(SceneInfo)
		.GlobalImportSettings(GlobalImportSettings)
		.SceneMeshOverrideOptions(SceneMeshOverrideOptions)
		.SceneImportOptionsStaticMeshDisplay(SceneImportOptionsStaticMeshDisplay);

	TSharedPtr<SBox> InspectorBox;
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("WidgetFbxSceneStaticMeshTab", "Static Meshes"))
		.ToolTipText(LOCTEXT("WidgetFbxSceneStaticMeshTabTextToolTip", "Switch to the static meshes tab."))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.4)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SCheckBox)
								.HAlign(HAlign_Center)
								.OnCheckStateChanged(StaticMeshTabListView.Get(), &SFbxSceneStaticMeshListView::OnToggleSelectAll)
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(0.0f, 3.0f, 6.0f, 3.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("FbxOptionWindow_Scene_All", "All"))
							]
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SBox)
						[
							StaticMeshTabListView.ToSharedRef()
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.6)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							StaticMeshTabListView->CreateOverrideOptionComboBox().ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("FbxOptionWindow_SM_CreateOverride", "Create Override"))
							.OnClicked(StaticMeshTabListView.Get(), &SFbxSceneStaticMeshListView::OnCreateOverrideOptions)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(InspectorBox, SBox)
					]
				]
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
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("FbxOptionWindow_SM_Select_asset_using", "Select Asset Using"))
					.OnClicked(StaticMeshTabListView.Get(), &SFbxSceneStaticMeshListView::OnSelectAssetUsing)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("FbxOptionWindow_SM_Delete", "Delete"))
					.IsEnabled(StaticMeshTabListView.Get(), &SFbxSceneStaticMeshListView::CanDeleteOverride)
					.OnClicked(StaticMeshTabListView.Get(), &SFbxSceneStaticMeshListView::OnDeleteOverride)
				]
			]
		];
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	SceneTabDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBox->SetContent(SceneTabDetailsView->AsShared());
	SceneTabDetailsView->SetObject(SceneImportOptionsStaticMeshDisplay);
	SceneTabDetailsView->OnFinishedChangingProperties().AddSP(StaticMeshTabListView.Get(), &SFbxSceneStaticMeshListView::OnFinishedChangingProperties);
	return DockTab;
}

TSharedRef<SDockTab> SFbxSceneOptionWindow::SpawnSkeletalMeshTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("WidgetFbxSceneSkeletalMeshTab", "Skeletal Meshes"))
		.ToolTipText(LOCTEXT("WidgetFbxSceneSkeletalMeshTabTextToolTip", "Switch to the skeletal meshes tab."))
		[
			SNew(STextBlock).Text(LOCTEXT("SkeletalMeshTextBoxPlaceHolder", "Skeletal Mesh Import options placeholder"))
		];
}

TSharedRef<SDockTab> SFbxSceneOptionWindow::SpawnMaterialTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("WidgetFbxSceneMaterialTab", "Materials"))
		.ToolTipText(LOCTEXT("WidgetFbxSceneMaterialTabTextToolTip", "Switch to the materials tab."))
		[
			SNew(STextBlock).Text(LOCTEXT("MaterialTextBoxPlaceHolder", "Material Import options placeholder"))
		];
}

TSharedPtr<SWidget> SFbxSceneOptionWindow::SpawnDockTab()
{
	return FbxSceneImportTabManager->RestoreFrom(Layout.ToSharedRef(), OwnerWindow).ToSharedRef();
}

void SFbxSceneOptionWindow::InitAllTabs()
{
	Layout = FTabManager::NewLayout("FbxSceneImportUI_Layout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->Split
			(
				FTabManager::NewStack()
				->AddTab("Scene", ETabState::OpenedTab)
				->AddTab("StaticMeshes", ETabState::OpenedTab)
				->AddTab("SkeletalMeshes", ETabState::OpenedTab)
				->AddTab("Materials", ETabState::OpenedTab)
			)
		);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab);

	FbxSceneImportTabManager = FGlobalTabmanager::Get()->NewTabManager(DockTab);

	FbxSceneImportTabManager->RegisterTabSpawner("Scene", FOnSpawnTab::CreateSP(this, &SFbxSceneOptionWindow::SpawnSceneTab));
	FbxSceneImportTabManager->RegisterTabSpawner("StaticMeshes", FOnSpawnTab::CreateSP(this, &SFbxSceneOptionWindow::SpawnStaticMeshTab));
	FbxSceneImportTabManager->RegisterTabSpawner("SkeletalMeshes", FOnSpawnTab::CreateSP(this, &SFbxSceneOptionWindow::SpawnSkeletalMeshTab));
	FbxSceneImportTabManager->RegisterTabSpawner("Materials", FOnSpawnTab::CreateSP(this, &SFbxSceneOptionWindow::SpawnMaterialTab));
}

void SFbxSceneOptionWindow::Construct(const FArguments& InArgs)
{
	SceneInfo = InArgs._SceneInfo;
	GlobalImportSettings = InArgs._GlobalImportSettings;
	SceneImportOptionsDisplay = InArgs._SceneImportOptionsDisplay;
	SceneImportOptionsStaticMeshDisplay = InArgs._SceneImportOptionsStaticMeshDisplay;
	SceneMeshOverrideOptions = InArgs._SceneMeshOverrideOptions;
	SceneImportOptionsSkeletalMeshDisplay = InArgs._SceneImportOptionsSkeletalMeshDisplay;
	SceneImportOptionsAnimationDisplay = InArgs._SceneImportOptionsAnimationDisplay;
	SceneImportOptionsMaterialDisplay = InArgs._SceneImportOptionsMaterialDisplay;
	OwnerWindow = InArgs._OwnerWindow;

	check(SceneInfo.IsValid());
	check(GlobalImportSettings != NULL);
	check(SceneImportOptionsDisplay != NULL);
	check(SceneImportOptionsStaticMeshDisplay != NULL);
	check(SceneMeshOverrideOptions != NULL)
	check(SceneImportOptionsSkeletalMeshDisplay != NULL);
	check(SceneImportOptionsAnimationDisplay != NULL);
	check(SceneImportOptionsMaterialDisplay != NULL);
	check(OwnerWindow.IsValid());

	InitAllTabs();

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
					.Text(LOCTEXT("FbxSceneImport_CurrentPath", "Import Asset Path: "))
				]
				+SHorizontalBox::Slot()
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
		.FillHeight(1.0f)
		.Padding(2)
		[
			SpawnDockTab().ToSharedRef()
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
				IDocumentation::Get()->CreateAnchor(FString("Engine/Content/FBX/ImportOptions"))
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("FbxOptionWindow_Import", "Import"))
				.IsEnabled(this, &SFbxSceneOptionWindow::CanImport)
				.OnClicked(this, &SFbxSceneOptionWindow::OnImport)
			]
			+ SUniformGridPanel::Slot(2, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("FbxOptionWindow_Cancel", "Cancel"))
				.ToolTipText(LOCTEXT("FbxOptionWindow_Cancel_ToolTip", "Cancels importing this FBX file"))
				.OnClicked(this, &SFbxSceneOptionWindow::OnCancel)
			]
		]
	];

	//By default we want to see the Scene tab
	FbxSceneImportTabManager->InvokeTab(FTabId("Scene"));
}

bool SFbxSceneOptionWindow::CanImport()  const
{
	return true;
}

void SFbxSceneOptionWindow::CopyFbxOptionsToFbxOptions(UnFbx::FBXImportOptions *SourceOptions, UnFbx::FBXImportOptions *DestinationOptions)
{
	FMemory::BigBlockMemcpy(DestinationOptions, SourceOptions, sizeof(UnFbx::FBXImportOptions));
}

void SFbxSceneOptionWindow::CopyStaticMeshOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, UFbxSceneImportOptionsStaticMesh* StaticMeshOptions)
{
	ImportSettings->bAutoGenerateCollision = StaticMeshOptions->bAutoGenerateCollision;
	ImportSettings->bBuildAdjacencyBuffer = StaticMeshOptions->bBuildAdjacencyBuffer;
	ImportSettings->bBuildReversedIndexBuffer = StaticMeshOptions->bBuildReversedIndexBuffer;
	ImportSettings->bGenerateLightmapUVs = StaticMeshOptions->bGenerateLightmapUVs;
	ImportSettings->bOneConvexHullPerUCX = StaticMeshOptions->bOneConvexHullPerUCX;
	ImportSettings->bRemoveDegenerates = StaticMeshOptions->bRemoveDegenerates;
	ImportSettings->bTransformVertexToAbsolute = StaticMeshOptions->bTransformVertexToAbsolute;
	ImportSettings->StaticMeshLODGroup = StaticMeshOptions->StaticMeshLODGroup;
	switch (StaticMeshOptions->VertexColorImportOption)
	{
	case EFbxSceneVertexColorImportOption::Type::Replace:
		ImportSettings->VertexColorImportOption = EVertexColorImportOption::Type::Replace;
		break;
	case EFbxSceneVertexColorImportOption::Type::Override:
		ImportSettings->VertexColorImportOption = EVertexColorImportOption::Type::Override;
		break;
	case EFbxSceneVertexColorImportOption::Type::Ignore:
		ImportSettings->VertexColorImportOption = EVertexColorImportOption::Type::Ignore;
		break;
	default:
		ImportSettings->VertexColorImportOption = EVertexColorImportOption::Type::Replace;
	}
	ImportSettings->VertexOverrideColor = StaticMeshOptions->VertexOverrideColor;
	switch (StaticMeshOptions->NormalImportMethod)
	{
		case EFBXSceneNormalImportMethod::FBXSceneNIM_ComputeNormals:
			ImportSettings->NormalImportMethod = FBXNIM_ComputeNormals;
			break;
		case EFBXSceneNormalImportMethod::FBXSceneNIM_ImportNormals:
			ImportSettings->NormalImportMethod = FBXNIM_ImportNormals;
			break;
		case EFBXSceneNormalImportMethod::FBXSceneNIM_ImportNormalsAndTangents:
			ImportSettings->NormalImportMethod = FBXNIM_ImportNormalsAndTangents;
			break;
	}
	switch (StaticMeshOptions->NormalGenerationMethod)
	{
	case EFBXSceneNormalGenerationMethod::BuiltIn:
		ImportSettings->NormalGenerationMethod = EFBXNormalGenerationMethod::BuiltIn;
		break;
	case EFBXSceneNormalGenerationMethod::MikkTSpace:
		ImportSettings->NormalGenerationMethod = EFBXNormalGenerationMethod::MikkTSpace;
		break;
	}
}

void SFbxSceneOptionWindow::CopyFbxOptionsToStaticMeshOptions(UnFbx::FBXImportOptions *ImportSettings, UFbxSceneImportOptionsStaticMesh* StaticMeshOptions)
{
	StaticMeshOptions->bAutoGenerateCollision = ImportSettings->bAutoGenerateCollision;
	StaticMeshOptions->bBuildAdjacencyBuffer = ImportSettings->bBuildAdjacencyBuffer;
	StaticMeshOptions->bBuildReversedIndexBuffer = ImportSettings->bBuildReversedIndexBuffer;
	StaticMeshOptions->bGenerateLightmapUVs = ImportSettings->bGenerateLightmapUVs;
	StaticMeshOptions->bOneConvexHullPerUCX = ImportSettings->bOneConvexHullPerUCX;
	StaticMeshOptions->bRemoveDegenerates = ImportSettings->bRemoveDegenerates;
	StaticMeshOptions->bTransformVertexToAbsolute = ImportSettings->bTransformVertexToAbsolute;
	StaticMeshOptions->StaticMeshLODGroup = ImportSettings->StaticMeshLODGroup;
	switch (ImportSettings->VertexColorImportOption)
	{
	case EVertexColorImportOption::Type::Replace:
		StaticMeshOptions->VertexColorImportOption = EFbxSceneVertexColorImportOption::Type::Replace;
		break;
	case EVertexColorImportOption::Type::Override:
		StaticMeshOptions->VertexColorImportOption = EFbxSceneVertexColorImportOption::Type::Override;
		break;
	case EVertexColorImportOption::Type::Ignore:
		StaticMeshOptions->VertexColorImportOption = EFbxSceneVertexColorImportOption::Type::Ignore;
		break;
	default:
		StaticMeshOptions->VertexColorImportOption = EFbxSceneVertexColorImportOption::Type::Replace;
	}
	StaticMeshOptions->VertexOverrideColor = ImportSettings->VertexOverrideColor;
	switch (ImportSettings->NormalImportMethod)
	{
	case FBXNIM_ComputeNormals:
		StaticMeshOptions->NormalImportMethod = EFBXSceneNormalImportMethod::FBXSceneNIM_ComputeNormals;
		break;
	case FBXNIM_ImportNormals:
		StaticMeshOptions->NormalImportMethod = EFBXSceneNormalImportMethod::FBXSceneNIM_ImportNormals;
		break;
	case FBXNIM_ImportNormalsAndTangents:
		StaticMeshOptions->NormalImportMethod = EFBXSceneNormalImportMethod::FBXSceneNIM_ImportNormalsAndTangents;
		break;
	}
	switch (ImportSettings->NormalGenerationMethod)
	{
	case EFBXNormalGenerationMethod::BuiltIn:
		StaticMeshOptions->NormalGenerationMethod = EFBXSceneNormalGenerationMethod::BuiltIn;
		break;
	case EFBXNormalGenerationMethod::MikkTSpace:
		StaticMeshOptions->NormalGenerationMethod = EFBXSceneNormalGenerationMethod::MikkTSpace;
		break;
	}
}

void SFbxSceneOptionWindow::CopySkeletalMeshOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, UFbxSceneImportOptionsSkeletalMesh* SkeletalMeshOptions)
{
}

void SFbxSceneOptionWindow::CopyFbxOptionsToSkeletalMeshOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsSkeletalMesh* SkeletalMeshOptions)
{
}

void SFbxSceneOptionWindow::CopyAnimationOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, UFbxSceneImportOptionsAnimation* AnimationOptions)
{
}

void SFbxSceneOptionWindow::CopyFbxOptionsToAnimationOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsAnimation* AnimationOptions)
{
}

void SFbxSceneOptionWindow::CopyMaterialOptionsToFbxOptions(UnFbx::FBXImportOptions *ImportSettings, UFbxSceneImportOptionsMaterial* MaterialOptions)
{
}

void SFbxSceneOptionWindow::CopyFbxOptionsToMaterialOptions(UnFbx::FBXImportOptions *ImportSettings, class UFbxSceneImportOptionsMaterial* MaterialOptions)
{
}

#undef LOCTEXT_NAMESPACE
