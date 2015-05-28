// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "SNiagaraEffectEditorWidget.h"
#include "SNotificationList.h"
#include "SExpandableArea.h"
#include "SSplitter.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"


#define LOCTEXT_NAMESPACE "NiagaraEffectEditor"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SNiagaraEffectEditorWidget::Construct(const FArguments& InArgs)
{
	EffectObj = InArgs._EffectObj;
	if (InArgs._EffectInstance)
	{
		EffectInstance = InArgs._EffectInstance;
	}
	else
	{
		EffectInstance = new FNiagaraEffectInstance(EffectObj);
	}

	//bNeedsRefresh = false;


	Viewport = SNew(SNiagaraEffectEditorViewport);


	TSharedPtr<SOverlay> OverlayWidget;
	this->ChildSlot
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.75f)
			[

			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[

				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(512)
						.HeightOverride(512)
						[
							Viewport.ToSharedRef()
						]
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(OverlayWidget, SOverlay)

						+SOverlay::Slot()
						.Padding(10)
						.VAlign(VAlign_Fill)
						[
							SAssignNew(ListView, SListView<TSharedPtr<FNiagaraSimulation> >)
							.ItemHeight(256)
							.ListItemsSource(&(this->EffectInstance->GetEmitters()))
							.OnGenerateRow(this, &SNiagaraEffectEditorWidget::OnGenerateWidgetForList)
							.OnSelectionChanged(this, &SNiagaraEffectEditorWidget::OnSelectionChanged)
							//.SelectionMode(ESelectionMode::None)
						]

						// Bottom-right corner text indicating the type of tool
						+ SOverlay::Slot()
							.Padding(10)
							.VAlign(VAlign_Bottom)
							.HAlign(HAlign_Right)
							[
								SNew(STextBlock)
								.Visibility(EVisibility::HitTestInvisible)
								.TextStyle(FEditorStyle::Get(), "Graph.CornerText")
								.Text(LOCTEXT("NiagaraEditorLabel", "Niagara Effect"))
							]

						// Top-right corner text indicating read only when not simulating
						+ SOverlay::Slot()
							.Padding(20)
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Right)
							[
								SNew(STextBlock)
								.Visibility(this, &SNiagaraEffectEditorWidget::ReadOnlyVisibility)
								.TextStyle(FEditorStyle::Get(), "Graph.CornerText")
								.Text(LOCTEXT("ReadOnlyLabel", "Read Only"))
							]

						// Bottom-right corner text for notification list position
						+ SOverlay::Slot()
							.Padding(15.f)
							.VAlign(VAlign_Bottom)
							.HAlign(HAlign_Right)
							[
								SAssignNew(NotificationListPtr, SNotificationList)
								.Visibility(EVisibility::HitTestInvisible)
							]
					]
			]

			]
			+ SSplitter::Slot()
			.Value(0.25f)
				[
					NGED_SECTION_BORDER
					[
						SNew(SBox)
						.MinDesiredHeight(400).MinDesiredWidth(1920)
						[
							SAssignNew(TimeLine, SNiagaraTimeline)
						]
					]
				]
		];

	Viewport->SetPreviewEffect(EffectInstance);
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


void SEmitterWidget::Construct(const FArguments& InArgs)
{
	CurSpawnScript = nullptr;
	CurUpdateScript = nullptr;
	Emitter = InArgs._Emitter;
	EffectInstance = InArgs._Effect;
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
	if (Emitter->GetProperties()->UpdateScript && Emitter->GetProperties()->UpdateScript->Source)
	{
		EditorExposedConstants = &Emitter->GetProperties()->UpdateScript->Source->ExposedVectorConstants;
		EditorExposedCurveConstants = &Emitter->GetProperties()->UpdateScript->Source->ExposedVectorCurveConstants;
	}

	TArray<TSharedPtr<EditorExposedVectorConstant>> *EditorExposedSpawnConstants = &DummyArray;
	TArray<TSharedPtr<EditorExposedVectorCurveConstant>> *EditorExposedSpawnCurveConstants = &DummyCurveArray;
	if (Emitter->GetProperties()->SpawnScript && Emitter->GetProperties()->SpawnScript->Source)
	{
		EditorExposedSpawnConstants = &Emitter->GetProperties()->SpawnScript->Source->ExposedVectorConstants;
		EditorExposedSpawnCurveConstants = &Emitter->GetProperties()->SpawnScript->Source->ExposedVectorCurveConstants;
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
							.OnCheckStateChanged(this, &SEmitterWidget::OnEmitterEnabledChanged)
							.IsChecked(this, &SEmitterWidget::IsEmitterEnabled)
							.ToolTipText(FText::FromString("Toggles whether this emitter is enabled. Disabled emitters don't simulate or render."))
						]
						+ SHorizontalBox::Slot()
							.FillWidth(0.3)
							.HAlign(HAlign_Left)
							[
								SNew(SEditableText).Text(this, &SEmitterWidget::GetEmitterName)
								.MinDesiredWidth(250)
								.Font(FEditorStyle::GetFontStyle("ContentBrowser.AssetListViewNameFontDirty"))
								.OnTextChanged(this, &SEmitterWidget::OnEmitterNameChanged)
							]
						+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.FillWidth(0.7)
							[
								SNew(STextBlock).Text(this, &SEmitterWidget::GetStatsText)
								.MinDesiredWidth(500)
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
											.ItemHeight(20).ListItemsSource(EditorExposedConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateConstantListRow)
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
												.ItemHeight(20).ListItemsSource(EditorExposedCurveConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateCurveConstantListRow)
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
												.ItemHeight(20).ListItemsSource(EditorExposedSpawnConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateConstantListRow)
												.SelectionMode(ESelectionMode::None)
											]
										]
								]
							]
						]


						// Spawn rate
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
	TComponentReregisterContext<UNiagaraComponent> ComponentReregisterContext;

	UObject *Props = Emitter->GetProperties()->RendererProperties;
	RendererPropertiesView->SetObject(Props);
}




void SEmitterWidget::OnUpdateScriptSelectedFromPicker(UObject *Asset)
{
	// Close the asset picker
	//AssetPickerAnchor->SetIsOpen(false);

	// Set the object found from the asset picker
	CurUpdateScript = Cast<UNiagaraScript>(Asset);
	Emitter->SetUpdateScript(CurUpdateScript);

	UpdateScriptConstantListSlot->DetachWidget();
	(*UpdateScriptConstantListSlot)
		[
			SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(LOCTEXT("ConstantsLabel", "Constants"))
			.BodyContent()
			[
				SAssignNew(UpdateScriptConstantList, SListView<TSharedPtr<EditorExposedVectorConstant>>)
				.ItemHeight(20).ListItemsSource(&CurUpdateScript->Source->ExposedVectorConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateConstantListRow)
			]
		];
}

void SEmitterWidget::OnSpawnScriptSelectedFromPicker(UObject *Asset)
{
	// Close the asset picker
	//AssetPickerAnchor->SetIsOpen(false);

	// Set the object found from the asset picker
	CurSpawnScript = Cast<UNiagaraScript>(Asset);
	Emitter->SetSpawnScript(CurSpawnScript);

	SpawnScriptConstantListSlot->DetachWidget();
	(*SpawnScriptConstantListSlot)
		[
			SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(LOCTEXT("ConstantsLabel", "Constants"))
			.BodyContent()
			[
				SAssignNew(SpawnScriptConstantList, SListView<TSharedPtr<EditorExposedVectorConstant>>)
				.ItemHeight(20).ListItemsSource(&CurSpawnScript->Source->ExposedVectorConstants).OnGenerateRow(this, &SEmitterWidget::OnGenerateConstantListRow)
			]
		];
}


END_SLATE_FUNCTION_BUILD_OPTIMIZATION