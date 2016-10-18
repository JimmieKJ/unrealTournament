// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "SNiagaraEffectEditorWidget.h"
#include "SNotificationList.h"
#include "SExpandableArea.h"
#include "SSplitter.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "NiagaraEffectEditor.h"
#include "NiagaraEmitterPropertiesDetailsCustomization.h"

#define LOCTEXT_NAMESPACE "NiagaraEffectEditor"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SNiagaraEffectEditorWidget::Construct(const FArguments& InArgs)
{
	EffectObj = InArgs._EffectObj;
	EffectInstance = InArgs._EffectInstance;
	EffectEditor = InArgs._EffectEditor;
	bForDev = InArgs._bForDev;

	check(EffectObj);
	check(EffectInstance);
	check(EffectEditor);
	
	if (bForDev)
	{
		ChildSlot
			.Padding(4)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.VAlign(VAlign_Fill)
				[
					SAssignNew(EmitterBox, SScrollBox)
					.Orientation(Orient_Horizontal)
				]
			];

		ConstructDevEmittersView();
	}
	else
	{
		TSharedPtr<SOverlay> OverlayWidget;
		this->ChildSlot
			.Padding(8)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.75f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					.Padding(0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SAssignNew(OverlayWidget, SOverlay)
								+ SOverlay::Slot()
								.Padding(10)
								.VAlign(VAlign_Fill)
								[
									SAssignNew(ListView, SListView<TSharedPtr<FNiagaraSimulation> >)
									.ItemHeight(256)
									.ListItemsSource(&(this->EffectInstance->GetEmitters()))
									.OnGenerateRow(this, &SNiagaraEffectEditorWidget::OnGenerateWidgetForList)
									.OnSelectionChanged(this, &SNiagaraEffectEditorWidget::OnSelectionChanged)
									//.SelectionMode(ESelectionMode::Single)
								]
							]
						]
					]
				]
			];
	}
}

void SNiagaraEffectEditorWidget::ConstructDevEmittersView()
{
	EmitterBox->ClearChildren();
	for (TSharedPtr<FNiagaraSimulation>& Emitter : EffectInstance->GetEmitters())
	{
		EmitterBox->AddSlot()
		.Padding(2.0f)
			[
				NGED_SECTION_DARKBORDER
				[
					SNew(SEmitterWidgetDev).Emitter(Emitter).Effect(EffectInstance).EffectEditor(EffectEditor)
				]
			];
	}
}

void SNiagaraTimeline::Construct(const FArguments& InArgs)
{
	Emitter = InArgs._Emitter;
	EffectInstance = InArgs._EffectInstance;
	Effect = InArgs._Effect;

	TSharedPtr<SOverlay> OverlayWidget;
	this->ChildSlot
	[
		SAssignNew(CurveEditor, SCurveEditor)
	];
}

void SEmitterWidgetBase::Construct(const FArguments& InArgs)
{
	Emitter = InArgs._Emitter;
	EffectInstance = InArgs._Effect;
	EffectEditor = InArgs._EffectEditor;
}

void SEmitterWidgetDev::Construct(const FArguments& InArgs)
{
	SEmitterWidgetBase::Construct(InArgs);

	BuildContents();
}

void SEmitterWidgetDev::BuildContents()
{
	//Register details customization.	
	if (UNiagaraEmitterProperties* PinnedProps = Emitter->GetProperties().Get())
	{
		if (!Details.IsValid())
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			FDetailsViewArgs DetailsViewArgs(false, false, true, FDetailsViewArgs::HideNameArea, true, this);
			Details = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
			FOnGetDetailCustomizationInstance LayoutEmitterDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraEmitterPropertiesDetails::MakeInstance, Emitter->GetProperties());
			Details->RegisterInstancedCustomPropertyLayout(UNiagaraEmitterProperties::StaticClass(), LayoutEmitterDetails);

			ChildSlot
				.Padding(4)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0)
					[
						NGED_SECTION_BORDER
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Fill)
							[
								// name and status line
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Left)
								.AutoWidth()
								.Padding(2)
								[
									SNew(SCheckBox)
									.OnCheckStateChanged(this, &SEmitterWidgetBase::OnEmitterEnabledChanged)
									.IsChecked(this, &SEmitterWidgetBase::IsEmitterEnabled)
									.ToolTipText(FText::FromString("Toggles whether this emitter is enabled. Disabled emitters don't simulate or render."))
								]
								+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(2)
									.HAlign(HAlign_Left)
									[
										SNew(SEditableText).Text(this, &SEmitterWidgetBase::GetEmitterName)
										.MinDesiredWidth(200)
										.Font(FEditorStyle::GetFontStyle("ContentBrowser.AssetListViewNameFontDirty"))
										.OnTextChanged(this, &SEmitterWidgetBase::OnEmitterNameChanged)
									]
								+ SHorizontalBox::Slot()
									.HAlign(HAlign_Right)
									.AutoWidth()
									[
										SNew(SButton)
										.VAlign(VAlign_Center)
										.HAlign(HAlign_Right)
										.Text(LOCTEXT("DeleteEmitter", "Delete"))
										.ToolTipText(LOCTEXT("DeleteEmitter_Tooltip", "Deletes this emitter from the effect."))
										.OnClicked(EffectEditor, &FNiagaraEffectEditor::OnDeleteEmitterClicked, Emitter)
									]
								+ SHorizontalBox::Slot()
									.HAlign(HAlign_Right)
									.AutoWidth()
									[
										SNew(SButton)
										.VAlign(VAlign_Center)
										.HAlign(HAlign_Right)
										.Text(LOCTEXT("DuplicateEmitter", "Duplicate"))
										.ToolTipText(LOCTEXT("DuplicateEmitter_Tooltip", "Duplicate this emitter."))
										.OnClicked(EffectEditor, &FNiagaraEffectEditor::OnDuplicateEmitterClicked, Emitter)
									]
							]
							+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Fill)
								.Padding(2)
								[
									SNew(STextBlock).Text(this, &SEmitterWidgetBase::GetStatsText)
								]
						]
					]
					+ SVerticalBox::Slot()
						.VAlign(VAlign_Fill)
						.Padding(0)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.MaxWidth(400)
							[
								Details.ToSharedRef()
							]
						]
				];
		}

		Details->SetObject(PinnedProps, true);
	}
}

void SEmitterWidgetDev::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	//Changes to some properties will require the properties to be reinitialized and the UI to be rebuilt.
	if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("Script"))
	{
		Emitter->GetProperties()->Init();

		BuildContents();
	}

	//Always refresh the emitter on a property change.
	Emitter->GetParentEffectInstance()->ReInit();
}

void SEmitterWidget::Construct(const FArguments& InArgs)
{
	SEmitterWidgetBase::Construct(InArgs);

	CurMaterial = nullptr;

	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(1, /*InAreRealTileThumbnailsAllowed=*/false));
	// Create the thumbnail handle
	MaterialThumbnail = MakeShareable(new FAssetThumbnail(CurMaterial, 64, 64, ThumbnailPool));
	ThumbnailWidget = MaterialThumbnail->MakeThumbnailWidget();
	MaterialThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render

	RenderModuleList.Add(MakeShareable(new FString("Sprites")));
	RenderModuleList.Add(MakeShareable(new FString("Ribbon")));
	RenderModuleList.Add(MakeShareable(new FString("Trails")));
	RenderModuleList.Add(MakeShareable(new FString("Meshes")));

	static TArray<TSharedPtr<EditorExposedVectorConstant>> DummyArray;
	static TArray<TSharedPtr<EditorExposedVectorCurveConstant>> DummyCurveArray;
	TArray<TSharedPtr<EditorExposedVectorConstant>> *EditorExposedConstants = &DummyArray;
	TArray<TSharedPtr<EditorExposedVectorCurveConstant>> *EditorExposedCurveConstants = &DummyCurveArray;
	if (Emitter->GetProperties()->UpdateScriptProps.Script && Emitter->GetProperties()->UpdateScriptProps.Script->Source)
	{
		EditorExposedConstants = &Emitter->GetProperties()->UpdateScriptProps.Script->Source->ExposedVectorConstants;
		EditorExposedCurveConstants = &Emitter->GetProperties()->UpdateScriptProps.Script->Source->ExposedVectorCurveConstants;
	}

	TArray<TSharedPtr<EditorExposedVectorConstant>> *EditorExposedSpawnConstants = &DummyArray;
	TArray<TSharedPtr<EditorExposedVectorCurveConstant>> *EditorExposedSpawnCurveConstants = &DummyCurveArray;
	if (Emitter->GetProperties()->SpawnScriptProps.Script && Emitter->GetProperties()->SpawnScriptProps.Script->Source)
	{
		EditorExposedSpawnConstants = &Emitter->GetProperties()->SpawnScriptProps.Script->Source->ExposedVectorConstants;
		EditorExposedSpawnCurveConstants = &Emitter->GetProperties()->SpawnScriptProps.Script->Source->ExposedVectorCurveConstants;
	}


	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = true;
	ViewArgs.bShowActorLabel = false;
	TSharedRef<IDetailsView> RendererPropertiesViewRef = PropertyModule.CreateDetailView(ViewArgs);
	RendererPropertiesView = RendererPropertiesViewRef;
	RendererPropertiesView->SetObject(Emitter->GetProperties()->RendererProperties);

	
	ChildSlot
		.Padding(4)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.LightGroupBorder"))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					NGED_SECTION_BORDER
					[
						// name and status line
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SEmitterWidgetBase::OnEmitterEnabledChanged)
							.IsChecked(this, &SEmitterWidgetBase::IsEmitterEnabled)
							.ToolTipText(FText::FromString("Toggles whether this emitter is enabled. Disabled emitters don't simulate or render."))
						]
						+ SHorizontalBox::Slot()
							.FillWidth(0.3)
							.HAlign(HAlign_Left)
							[
								SNew(SEditableText).Text(this, &SEmitterWidgetBase::GetEmitterName)
								.MinDesiredWidth(250)
								.Font(FEditorStyle::GetFontStyle("ContentBrowser.AssetListViewNameFontDirty"))
								.OnTextChanged(this, &SEmitterWidgetBase::OnEmitterNameChanged)
							]
						+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.FillWidth(0.7)
							[
								SNew(STextBlock).Text(this, &SEmitterWidgetBase::GetStatsText)
								.MinDesiredWidth(500)
							]
							+ SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								.AutoWidth()
								[
									SNew(SButton)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Right)
									.Text(LOCTEXT("DeleteEmitter", "Delete"))
									.ToolTipText(LOCTEXT("DeleteEmitter_Tooltip", "Deletes this emitter from the effect."))
									.OnClicked(EffectEditor, &FNiagaraEffectEditor::OnDeleteEmitterClicked, Emitter)
								]
							+ SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								.AutoWidth()
								[
									SNew(SButton)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Right)
									.Text(LOCTEXT("DuplicateEmitter", "Duplicate"))
									.ToolTipText(LOCTEXT("DuplicateEmitter_Tooltip", "Duplicate this emitter."))
									.OnClicked(EffectEditor, &FNiagaraEffectEditor::OnDuplicateEmitterClicked, Emitter)
								]
					]
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						//------------------------------------------------------------------------------------
						// horizontal arrangement of property widgets
						SNew(SHorizontalBox)


						// Update and spawn script selectors
						//
						+SHorizontalBox::Slot()
						.Padding(4.0f)
						.FillWidth(950)
						.HAlign(HAlign_Left)
						[
							NGED_SECTION_BORDER
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(4)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.Padding(2)
									.AutoHeight()
									[
										NGED_SECTION_DARKBORDER
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.VAlign(VAlign_Center)
											.AutoWidth()
											.Padding(2)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("UpdateScriptLabel", "Update Script"))
											]
											+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.VAlign(VAlign_Center)
											[
												SAssignNew(UpdateScriptSelector, SContentReference)
												.AllowedClass(UNiagaraScript::StaticClass())
												.AssetReference(this, &SEmitterWidget::GetUpdateScript)
												.AllowSelectingNewAsset(true)
												.AllowClearingReference(true)
												.OnSetReference(this, &SEmitterWidget::OnUpdateScriptSelectedFromPicker)
												.WidthOverride(130)
											]
										]
									]

									+ SVerticalBox::Slot()
									.Padding(2)
									.AutoHeight()
									.Expose(UpdateScriptConstantListSlot)
									[
										SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(LOCTEXT("ConstantsLabel", "Constants"))
										.BodyContent()
										[
											SAssignNew(UpdateScriptConstantList, SListView<TSharedPtr<EditorExposedVectorConstant>>)
											.ItemHeight(20).ListItemsSource(EditorExposedConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateUpdateConstantListRow)
											.SelectionMode(ESelectionMode::None)
										]
									]

									+ SVerticalBox::Slot()
										.Padding(2)
										.AutoHeight()
										.Expose(UpdateScriptCurveConstantListSlot)
										[
											SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(LOCTEXT("CurvesLabel", "Curves"))
											.BodyContent()
											[
												SNew(SListView<TSharedPtr<EditorExposedVectorCurveConstant>>)
												.ItemHeight(20).ListItemsSource(EditorExposedCurveConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateUpdateCurveConstantListRow)
												.SelectionMode(ESelectionMode::None)
											]
										]
								]

								+ SHorizontalBox::Slot()
								.Padding(4)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.Padding(2)
									.AutoHeight()
									[
										NGED_SECTION_DARKBORDER
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.VAlign(VAlign_Center)
											.AutoWidth()
											.Padding(4)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("SpawnScriptLabel", "Spawn Script"))
											]
											+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.VAlign(VAlign_Center)
											[
												SAssignNew(SpawnScriptSelector, SContentReference)
												.AllowedClass(UNiagaraScript::StaticClass())
												.AssetReference(this, &SEmitterWidget::GetSpawnScript)
												.AllowSelectingNewAsset(true)
												.AllowClearingReference(true)
												.OnSetReference(this, &SEmitterWidget::OnSpawnScriptSelectedFromPicker)
												.WidthOverride(130)
											]
										]
									]

									+ SVerticalBox::Slot()
										.Padding(2)
										.AutoHeight()
										.Expose(SpawnScriptConstantListSlot)
										[
											SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(LOCTEXT("ConstantsLabel", "Constants"))
											.BodyContent()
											[
												SAssignNew(SpawnScriptConstantList, SListView<TSharedPtr<EditorExposedVectorConstant>>)
												.ItemHeight(20).ListItemsSource(EditorExposedSpawnConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateSpawnConstantListRow)
												.SelectionMode(ESelectionMode::None)
											]
										]
								]
							]
						]


						// Spawn rate and other emitter properties
						+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.Padding(4)
							[
								NGED_SECTION_BORDER
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.Padding(4)
									.AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										.AutoWidth()
										.Padding(4)
										[
											SNew(STextBlock)
											.Text(LOCTEXT("SpawnRateLabel", "Spawn Rate"))
										]
										+ SHorizontalBox::Slot()
											[
												SNew(SEditableTextBox).OnTextChanged(this, &SEmitterWidget::OnSpawnRateChanged)
												.Text(this, &SEmitterWidget::GetSpawnRateText)
											]
									]
									+ SVerticalBox::Slot()
									.Padding(4)
									.AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										.AutoWidth()
										.Padding(4)
										[
											SNew(STextBlock)
											.Text(LOCTEXT("LoopsLabel", "Loops"))
										]
										+ SHorizontalBox::Slot()
											[
												SNew(SEditableTextBox).OnTextChanged(this, &SEmitterWidget::OnNumLoopsChanged)
												.Text(this, &SEmitterWidget::GetLoopsText)
											]
									]

								]
							]


						// Material
						+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.Padding(4)
							[
								NGED_SECTION_BORDER
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoHeight()
									.Padding(4)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.Padding(4)
										.AutoWidth()
										[
											SNew(STextBlock)
											.Text(LOCTEXT("MaterialLabel", "Material"))
										]
										+ SHorizontalBox::Slot()
											.Padding(4)
											.AutoWidth()
											[
												SNew(SVerticalBox)
												+ SVerticalBox::Slot()
												.HAlign(HAlign_Left)
												.VAlign(VAlign_Center)
												.AutoHeight()
												.Padding(4)
												[
													SNew(SBox)
													.WidthOverride(64)
													.HeightOverride(64)
													[
														ThumbnailWidget.ToSharedRef()
													]
												]
												+ SVerticalBox::Slot()
													.HAlign(HAlign_Left)
													.VAlign(VAlign_Center)
													.AutoHeight()
													.Padding(4)
													[
														SNew(SContentReference)
														.AllowedClass(UMaterial::StaticClass())
														.AssetReference(this, &SEmitterWidget::GetMaterial)
														.AllowSelectingNewAsset(true)
														.AllowClearingReference(true)
														.OnSetReference(this, &SEmitterWidget::OnMaterialSelected)
														.WidthOverride(150)
													]
											]
									]
									+ SVerticalBox::Slot()
										.AutoHeight()
										.VAlign(VAlign_Center)
										.Padding(4)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.Padding(4)
											.AutoWidth()
											[
												SNew(STextBlock)
												.Text(LOCTEXT("RenderAsLabel", "Render as"))
											]
											+ SHorizontalBox::Slot()
											.Padding(4)
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												// session combo box
												SNew(SComboBox<TSharedPtr<FString> >)
												.ContentPadding(FMargin(6.0f, 2.0f))
												.OptionsSource(&RenderModuleList)
												.OnSelectionChanged(this, &SEmitterWidget::OnRenderModuleChanged)
												//										.ToolTipText(LOCTEXT("SessionComboBoxToolTip", "Select the game session to interact with.").ToString())
												.OnGenerateWidget(this, &SEmitterWidget::GenerateRenderModuleComboWidget)
												[
													SNew(STextBlock)
													.Text(this, &SEmitterWidget::GetRenderModuleText)
												]
											]
										]
									+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(4)
										[
											RendererPropertiesViewRef
										]

								]
							]
					]
			]
		];

}




void SEmitterWidget::OnRenderModuleChanged(TSharedPtr<FString> ModName, ESelectInfo::Type SelectInfo)
{
	EEmitterRenderModuleType Type = RMT_Sprites;
	for (int32 i = 0; i < RenderModuleList.Num(); i++)
	{
		if (*RenderModuleList[i] == *ModName)
		{
			Type = (EEmitterRenderModuleType)(i + 1);
			break;
		}
	}


	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FChangeNiagaraRenderModule,
		FNiagaraEffectInstance*, InEffect, this->EffectInstance,
		TSharedPtr<FNiagaraSimulation>, InEmitter, this->Emitter,
		EEmitterRenderModuleType, InType, Type,
		{
		InEmitter->SetRenderModuleType(InType, InEffect->GetComponent()->GetWorld()->FeatureLevel);
		InEmitter->GetProperties()->RenderModuleType = InType;
		InEffect->RenderModuleupdate();
	});
	FlushRenderingCommands();

	UMaterial *Material = UMaterial::GetDefaultMaterial(MD_Surface);
	Emitter->GetEffectRenderer()->SetMaterial(Material, EffectInstance->GetComponent()->GetWorld()->FeatureLevel);
	{
		TComponentReregisterContext<UNiagaraComponent> ComponentReregisterContext;
	}
	UObject *Props = Emitter->GetProperties()->RendererProperties;
	RendererPropertiesView->SetObject(Props);
}




void SEmitterWidget::OnUpdateScriptSelectedFromPicker(UObject *Asset)
{
	// Close the asset picker
	//AssetPickerAnchor->SetIsOpen(false);

	// Set the object found from the asset picker
	UNiagaraScript* CurUpdateScript = Cast<UNiagaraScript>(Asset);
	UNiagaraEmitterProperties* PinnedProps = Emitter->GetProperties().Get();
	if (PinnedProps)
	{
		PinnedProps->UpdateScriptProps.Script = CurUpdateScript;
		PinnedProps->UpdateScriptProps.Init(PinnedProps);
		Emitter->Init();

		UpdateScriptConstantListSlot->DetachWidget();
		(*UpdateScriptConstantListSlot)
			[
				SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(LOCTEXT("ConstantsLabel", "Constants"))
				.BodyContent()
				[
					SAssignNew(UpdateScriptConstantList, SListView<TSharedPtr<EditorExposedVectorConstant>>)
					.ItemHeight(20).ListItemsSource(&CurUpdateScript->Source->ExposedVectorConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateUpdateConstantListRow)
				]
			];
	}
}

void SEmitterWidget::OnSpawnScriptSelectedFromPicker(UObject *Asset)
{
	// Close the asset picker
	//AssetPickerAnchor->SetIsOpen(false);

	// Set the object found from the asset picker
	UNiagaraScript* CurSpawnScript = Cast<UNiagaraScript>(Asset);
	UNiagaraEmitterProperties* PinnedProps = Emitter->GetProperties().Get();
	if (PinnedProps)
	{
		PinnedProps->SpawnScriptProps.Script = CurSpawnScript;
		PinnedProps->SpawnScriptProps.Init(PinnedProps);
		Emitter->Init();


		SpawnScriptConstantListSlot->DetachWidget();
		(*SpawnScriptConstantListSlot)
			[
				SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(LOCTEXT("ConstantsLabel", "Constants"))
				.BodyContent()
				[
					SAssignNew(SpawnScriptConstantList, SListView<TSharedPtr<EditorExposedVectorConstant>>)
					.ItemHeight(20).ListItemsSource(&CurSpawnScript->Source->ExposedVectorConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateSpawnConstantListRow)
				]
			];
	}
}


void SNiagaraEffectEditorWidget::OnSelectionChanged(TSharedPtr<FNiagaraSimulation> SelectedItem, ESelectInfo::Type SelType)
{
	EffectEditor->OnEmitterSelected(SelectedItem, SelType);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE