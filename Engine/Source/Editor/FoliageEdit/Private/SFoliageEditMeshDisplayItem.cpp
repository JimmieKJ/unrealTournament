// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "SFoliageEdit.h"
#include "FoliageEditActions.h"
#include "FoliageEdMode.h"
#include "Foliage/FoliageType.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"

#include "SFoliageEditMeshDisplayItem.h"
#include "ObjectEditorUtils.h"
#include "TutorialMetaData.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "FoliageEd_Mode"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SFoliageEditMeshDisplayItem::Construct(const FArguments& InArgs)
{
	BorderImage = FEditorStyle::GetBrush("FoliageEditMode.BubbleBorder");
	BorderBackgroundColor = TAttribute<FSlateColor>(this, &SFoliageEditMeshDisplayItem::GetBorderColor);

	FoliageEditPtr = InArgs._FoliageEditPtr;
	FoliageSettingsPtr = InArgs._FoliageSettingsPtr;
	Thumbnail = InArgs._AssetThumbnail;
	FoliageMeshUIInfo = InArgs._FoliageMeshUIInfo;

	ThumbnailWidget = Thumbnail->MakeThumbnailWidget();

	// Create the command list and bind the commands.
	UICommandList = MakeShareable(new FUICommandList);
	BindCommands();

	CurrentViewSettings = ECurrentViewSettings::ShowPaintSettings;

	// Create the details panels for the clustering tab.
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const FDetailsViewArgs DetailsViewArgs(false, false, true, false, true, this);
	ClusterSettingsDetails = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsObjectList.Add(FoliageSettingsPtr);
	ClusterSettingsDetails->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SFoliageEditMeshDisplayItem::IsPropertyVisible));
	ClusterSettingsDetails->SetObjects(DetailsObjectList);

	// Everything (or almost) uses this padding, change it to expand the padding.
	FMargin StandardPadding(2.0f, 2.0f, 2.0f, 2.0f);
	FMargin StandardSidePadding(2.0f, 0.0f, 2.0f, 0.0f);

	// Create the toolbar for the tab options.
	FToolBarBuilder Toolbar(UICommandList, FMultiBoxCustomization::None);
	{
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetNoSettings);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetPaintSettings);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetClusterSettings);
	}

	SAssignNew(ThumbnailBox, SBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.MeshImage"), "LevelEditorToolbox"))
		.WidthOverride(80).HeightOverride(80)
		[
			SAssignNew(ThumbnailWidgetBorder, SBorder)
			[
				ThumbnailWidget.ToSharedRef()
			]
		];

	const float MAIN_TITLE = 1.0f;
	const float SPINBOX_PREFIX = 0.3f;
	const float SPINBOX_SINGLE = 2.2f;

	TSharedRef<SHorizontalBox> DensityBox =
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)

			// Density
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(StandardPadding)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNotReapplySettingsVisible)
				.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.Density"), "LevelEditorToolbox"))
				.ToolTipText(LOCTEXT("Density_Tooltip", "Foliage instances will be placed at this density, specified in instances per 1000x1000 unit area."))

				+ SHorizontalBox::Slot()
				.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Density", "Density / 1Kuu"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DensitySuperscript", "2"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8))
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(SPINBOX_SINGLE)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.0f)
					.MaxValue(65536.0f)
					.MaxSliderValue(1000.0f)
					.Value(this, &SFoliageEditMeshDisplayItem::GetDensity)
					.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnDensityChanged)
				]
			]

			// Density Adjustment
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(StandardPadding)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
				.ToolTipText(LOCTEXT("DensityAdjust_Tooltip", "If checked, the density of foliage instances already placed will be adjusted. <1 will remove instances to reduce the density, >1 will place extra instances."))

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnDensityReapply)
					.IsChecked(this, &SFoliageEditMeshDisplayItem::IsDensityReapplyChecked)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DensityAdjust", "Density Adjust"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(SPINBOX_SINGLE)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.0f)
					.MaxValue(4.0f)
					.MaxSliderValue(2.0f)
					.Value(this, &SFoliageEditMeshDisplayItem::GetDensityReapply)
					.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnDensityReapplyChanged)
				]
			]

			// Radius
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(StandardPadding)
			[
				SNew(SHorizontalBox)
				.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.Radius"), "LevelEditorToolbox"))
				.ToolTipText(LOCTEXT("Radius_Tooltip", "The minimum distance between foliage instances."))

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
					.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnRadiusReapply)
					.IsChecked(this, &SFoliageEditMeshDisplayItem::IsRadiusReapplyChecked)
					.ToolTipText(LOCTEXT("ReapplyRadiusCheck_Tooltip", "If checked, foliage instances not meeting the new Radius constraint will be removed"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Radius", "Radius"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(SPINBOX_SINGLE)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.0f)
					.MaxValue(65536.0f)
					.MaxSliderValue(200.0f)
					.Value(this, &SFoliageEditMeshDisplayItem::GetRadius)
					.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnRadiusChanged)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(StandardPadding)
		[
			ThumbnailBox.ToSharedRef()
		];

	TSharedRef<SHorizontalBox> AlignToNormalBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.AlignToNormal"), "LevelEditorToolbox"))			
		.ToolTipText(LOCTEXT("AlignToNormal_Tooltip", "Whether foliage instances should have their angle adjusted away from vertical to match the normal of the surface they're painted on"))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnAlignToNormalReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsAlignToNormalReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyNormalCheck_Tooltip", "If checked, foliage instances will have their normal alignment adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnAlignToNormal)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsAlignToNormalChecked)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AlignToNormal", "Align to Normal"))
			]
		];

	TSharedRef<SHorizontalBox> MaxAngleBox =
		SNew(SHorizontalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsAlignToNormalVisible)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.MaxAngle"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("MaxAngle_Tooltip", "The maximum angle in degrees that foliage instances will be adjusted away from the vertical"))

		// Dummy Checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible_Dummy)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MaxAngle", "Max Angle +/-"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-180.f)
				.MaxValue(180.0f)
				.MinSliderValue(0.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetMaxAngle)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnMaxAngleChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> RandomYawBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.RandomYaw"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("RandomYaw_Tooltip", "If selected, foliage instances will have a random yaw rotation around their vertical axis applied"))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnRandomYawReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsRandomYawReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyRandomYawCheck_Tooltip", "If checked, foliage instances will have their yaw adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnRandomYaw)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsRandomYawChecked)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RandomYaw", "Random Yaw"))
			]
		];

	TSharedRef<SHorizontalBox> UniformScaleBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.UniformScale"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("UniformScale_Tooltip", "If selected, foliage instances will have unfiorm  X,Y and Z scales"))

		// Dummy Checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible_Dummy)
		]

		// Uniform Scale Checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnUniformScale)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsUniformScaleChecked)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UniformScale", "Uniform Scale"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]
		// "Lock" Non-uniform scaling checkbox.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Lock", "Lock"))
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible)
		];

	TSharedRef<SHorizontalBox> ScaleUniformBox =
		SNew(SHorizontalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsUniformScalingVisible)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.ScaleValues"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("ScaleUniformValues_Tooltip", "Specifies the range of scale, from minimum to maximum, to apply to a foliage instance's Scale property, applied uniformly to X, Y and Z."))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnScaleUniformReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsScaleUniformReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyScaleUnformCheck_Tooltip", "If checked, foliage instances will have their scale uniformly adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ScaleUniform", "Scale"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_PREFIX)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f) // empty slot to eat the space for the alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Min", "Min"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleUniformMin)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleUniformMinChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Max", "Max"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleUniformMax)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleUniformMaxChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> ScaleXBox =
		SNew(SHorizontalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible)
		.ToolTipText(LOCTEXT("ScaleXValues_Tooltip", "Specifies the range of scale, from minimum to maximum, to apply to a foliage instance's X Scale property"))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnScaleXReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsScaleXReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyScaleXCheck_Tooltip", "If checked, foliage instances will have their X scale adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ScaleX", "Scale X"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_PREFIX)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f) // empty slot to eat the space for the alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Min", "Min"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleXMin)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleXMinChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Max", "Max"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleXMax)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleXMaxChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnScaleXLocked)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsScaleXLockedChecked)
				.ToolTipText(LOCTEXT("ScaleXLocked_Tooltip", "Locks the X axis scale. All axes with the locked checkbox set will have the same scale applied."))
			]
		];

	TSharedRef<SHorizontalBox> ScaleYBox =
		SNew(SHorizontalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible)
		.ToolTipText(LOCTEXT("ScaleYValues_Tooltip", "Specifies the range of scale, from minimum to maximum, to apply to a foliage instance's Y Scale property"))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnScaleYReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsScaleYReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyScaleYCheck_Tooltip", "If checked, foliage instances will have their Y scale adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ScaleY", "Scale Y"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_PREFIX)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f) // empty slot to eat the space for the alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Min", "Min"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleYMin)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleYMinChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Max", "Max"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleYMax)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleYMaxChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnScaleYLocked)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsScaleYLockedChecked)
				.ToolTipText(LOCTEXT("ScaleYLocked_Tooltip", "Locks the Y axis scale. All axes with the locked checkbox set will have the same scale applied."))
			]
		];

	TSharedRef<SHorizontalBox> ScaleZBox =
		SNew(SHorizontalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible)
		.ToolTipText(LOCTEXT("ScaleZValues_Tooltip", "Specifies the range of scale, from minimum to maximum, to apply to a foliage instance's Z Scale property"))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnScaleZReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsScaleZReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyScaleZCheck_Tooltip", "If checked, foliage instances will have their Z scale adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ScaleZ", "Scale Z"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_PREFIX)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f) // empty slot to eat the space for the alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Min", "Min"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleZMin)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleZMinChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Max", "Max"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(100.0f)
				.MaxSliderValue(10.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetScaleZMax)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnScaleZMaxChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnScaleZLocked)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsScaleZLockedChecked)
				.ToolTipText(LOCTEXT("ScaleZLocked_Tooltip", "Locks the Z axis scale. All axes with the locked checkbox set will have the same scale applied."))
			]
		];

	TSharedRef<SHorizontalBox> ZOffsetBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.ZOffset"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("ZOffset_Tooltip", "Specifies a range from minimum to maximum of the offset to apply to a foliage instance's Z location"))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnZOffsetReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsZOffsetReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyZOffsetCheck_Tooltip", "If checked, foliage instances will have their Z offset adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ZOffset", "Z Offset"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_PREFIX)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f) // empty slot to eat the space for the alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Min", "Min"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-65536.0f)
				.MaxValue(65536.0f)
				.MinSliderValue(-100.0f)
				.MaxSliderValue(100.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetZOffsetMin)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnZOffsetMin)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Max", "Max"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-65536.0f)
				.MaxValue(65536.0f)
				.MinSliderValue(-100.0f)
				.MaxSliderValue(100.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetZOffsetMax)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnZOffsetMax)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> RandomPitchBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.RandomPitch"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("RandomPitch_Tooltip", "A random pitch adjustment can be applied to each instance, up to the specified angle in degrees, from the original vertical."))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnRandomPitchReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsRandomPitchReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyRandomPitch_Tooltip", "If checked, foliage instances will have their pitch adjusted by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RandomPitchAngle", "Random Pitch +/-"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-180.0f)
				.MinSliderValue(0.0f)
				.MaxValue(180.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetRandomPitch)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnRandomPitchChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> GroundSlopeBox = SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.GroundSlope"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("GroundSlope_Tooltip", "If non-zero, foliage instances will only be placed on surfaces sloping less than the angle specified from the horizontal. Negative values reverse the test, placing instances only on surfaces sloping greater than the specified angle."))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnGroundSlopeReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsGroundSlopeReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyGroundSlopeCheck_Tooltip", "If checked, foliage instances not meeting the ground slope condition will be removed by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GroundSlope", "Ground Slope"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-180.0f)
				.MinSliderValue(0.0f)
				.MaxValue(180.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetGroundSlope)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnGroundSlopeChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> HeightBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.Height"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("Height_Tooltip", "The valid altitude range where foliage instances will be placed, specified using minimum and maximum world coordinate Z values"))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnHeightReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsHeightReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyHeightCheck_Tooltip", "If checked, foliage instances not meeting the valid Z height condition will be removed by the Reapply tool"))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Height", "Height"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_PREFIX)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f) // empty slot to eat the space for the alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Min", "Min"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-262144.0f)
				.MaxValue(262144.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetHeightMin)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnHeightMinChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Max", "Max"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-262144.0f)
				.MaxValue(262144.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetHeightMax)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnHeightMaxChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> LandscapeLayerBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.LandscapeLayer"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("LandscapeLayer_Tooltip", "If a layer name is specified, painting on landscape will limit the foliage to areas of landscape with the specified layer painted."))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnLandscapeLayerReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsLandscapeLayerReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyLandscapeLayerCheck_Tooltip", "If checked, foliage instances painted on areas that do not have the appopriate landscape layer painted will be removed by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeLayer", "Landscape Layer"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text(this, &SFoliageEditMeshDisplayItem::GetLandscapeLayer)
				.OnTextChanged(this, &SFoliageEditMeshDisplayItem::OnLandscapeLayerChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> CollisionWithWorldBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.CollisionWorld"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("CollisionWorld_Tooltip", "If checked, an overlap test with existing world geometry is performed before each instance is placed."))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnCollisionWithWorldReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsCollisionWithWorldReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyCollisionWithWorldCheck_Tooltip", "If checked, foliage instances will have an overlap test with the world reapplied, and overlapping instances will be removed by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnCollisionWithWorld)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsCollisionWithWorldChecked)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CollisionWithWorld", "Collision with World"))
			]
		];

	TSharedRef<SHorizontalBox> CollisionScaleX =
		SNew(SHorizontalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsCollisionWithWorldVisible)
		.ToolTipText(LOCTEXT("CollisionWorldScale_Tooltip", "The foliage instance's collision bounding box will be scaled by the specified amount before performing the overlap check."))

		// Dummy Checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible_Dummy)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CollisionScale", "Collision Scale"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_PREFIX)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f) // empty slot to eat the space for the alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("X", "X"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.333333f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.01f)
				.MaxValue(5.0f)
				.MaxSliderValue(1.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetCollisionScaleX)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnCollisionScaleXChanged)
			]

			// Y
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Y", "Y"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.333333f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.01f)
				.MaxValue(5.0f)
				.MaxSliderValue(1.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetCollisionScaleY)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnCollisionScaleYChanged)
			]

			// Z
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(StandardSidePadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Z", "Z"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.333333f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.01f)
				.MaxValue(5.0f)
				.MaxSliderValue(1.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetCollisionScaleZ)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnCollisionScaleZChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Visibility(this, &SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy)
			]
		];

	TSharedRef<SHorizontalBox> VertexColorMaskBox =
		SNew(SHorizontalBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.VertexColorMask"), "LevelEditorToolbox"))
		.ToolTipText(LOCTEXT("VertexColorMask_Tooltip", "When painting on static meshes, foliage instance placement can be limited to areas where the static mesh has values in the selected vertex color channel(s). This allows a static mesh to mask out certain areas to prevent foliage from being placed there."))

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible)
			.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMaskReapply)
			.IsChecked(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskReapplyChecked)
			.ToolTipText(LOCTEXT("ReapplyVertexColorMaskCheck_Tooltip", "If checked, foliage instances no longer matching the vertex color constraint will be removed by the Reapply tool"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("VertexColorMask", "Vertex Color Mask"))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(StandardSidePadding)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMask, FOLIAGEVERTEXCOLORMASK_Disabled)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskChecked, FOLIAGEVERTEXCOLORMASK_Disabled)
				.ToolTipText(LOCTEXT("VertexColorDisabled_Tooltip", "Do not use a vertex color mask"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VertexColorDisabled", "Disabled"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(StandardSidePadding)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMask, FOLIAGEVERTEXCOLORMASK_Red)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskChecked, FOLIAGEVERTEXCOLORMASK_Red)
				.ToolTipText(LOCTEXT("VertexColorR_Tooltip", "Use the vertex color Red channel as a mask"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VertexColorR", "R"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(StandardSidePadding)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMask, FOLIAGEVERTEXCOLORMASK_Green)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskChecked, FOLIAGEVERTEXCOLORMASK_Green)
				.ToolTipText(LOCTEXT("VertexColorG_Tooltip", "Use the vertex color Green channel as a mask"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VertexColorG", "G"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(StandardSidePadding)
			.VAlign(VAlign_Center)
			[

				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMask, FOLIAGEVERTEXCOLORMASK_Blue)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskChecked, FOLIAGEVERTEXCOLORMASK_Blue)
				.ToolTipText(LOCTEXT("VertexColorB_Tooltip", "Use the vertex color Blue channel as a mask"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VertexColorB", "B"))
				]
			]


			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(StandardSidePadding)
			.VAlign(VAlign_Center)
			[

				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMask, FOLIAGEVERTEXCOLORMASK_Alpha)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskChecked, FOLIAGEVERTEXCOLORMASK_Alpha)
				.ToolTipText(LOCTEXT("VertexColorA_Tooltip", "Use the vertex color Alpha channel as a mask"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VertexColorA", "A"))
				]
			]
		];

	TSharedRef<SHorizontalBox> VertexColorMaskThresholdBox =
		SNew(SHorizontalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskThresholdVisible)
		.ToolTipText(LOCTEXT("VertexColorMaskThreshold_Tooltip", "Specifies the threshold value above which the static mesh vertex color value must be, in order for foliage instances to be placed in a specific area."))

		// Dummy Checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsReapplySettingsVisible_Dummy)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(MAIN_TITLE + SPINBOX_PREFIX)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("VertexColorMaskThreshold", "Mask Threshold"))
			.Visibility(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskThresholdVisible)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(SPINBOX_SINGLE)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(StandardSidePadding)
			.VAlign(VAlign_Center)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Value(this, &SFoliageEditMeshDisplayItem::GetVertexColorMaskThreshold)
				.OnValueChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMaskThresholdChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(StandardSidePadding)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnVertexColorMaskInvert)
				.IsChecked(this, &SFoliageEditMeshDisplayItem::IsVertexColorMaskInvertChecked)
				.ToolTipText(LOCTEXT("VertexColorMaskThresholdInvert_Tooltip", "When unchecked, foliage instances will be placed only when the vertex color in the specified channel(s) is above the threshold amount. When checked, the vertex color must be less than the threshold amount."))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VertexColorMaskThresholdInvert", "Invert"))
				.ToolTipText(LOCTEXT("VertexColorMaskThresholdInvert_Tooltip", "When unchecked, foliage instances will be placed only when the vertex color in the specified channel(s) is above the threshold amount. When checked, the vertex color must be less than the threshold amount."))
			]
		];

	TSharedRef<SVerticalBox> PaintSettings =
		SNew(SVerticalBox)
		.Visibility(this, &SFoliageEditMeshDisplayItem::IsPaintSettingsVisible)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			DensityBox
		]

		// Align to Normal Checkbox
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			AlignToNormalBox
		]

		// Max Angle
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			MaxAngleBox
		]

		// Random Yaw Checkbox
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			RandomYawBox
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			UniformScaleBox
		]

		// Uniform Scale
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			ScaleUniformBox
		]

		// Scale X
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			ScaleXBox
		]

		// Scale Y
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			ScaleYBox
		]

		// Scale Z
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			ScaleZBox
		]

		// Z Offset
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			ZOffsetBox
		]

		// Random Pitch +/-
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			RandomPitchBox
		]

		// Ground Slope
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			GroundSlopeBox
		]

		// Height
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			HeightBox
		]

		// Landscape Layer
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			LandscapeLayerBox
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			CollisionWithWorldBox
		]

		// Collision ScaleX
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			CollisionScaleX
		]

		// Vertex Color Mask
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			VertexColorMaskBox
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(StandardPadding)
		[
			VertexColorMaskThresholdBox
		];

	this->ChildSlot
	.Padding(6.0f)
	.VAlign(VAlign_Top)
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("FoliageEditMode.ItemBackground"))
		.Padding(0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
				.OnMouseButtonDown(this, &SFoliageEditMeshDisplayItem::OnMouseDownSelection)
				.BorderImage(FEditorStyle::GetBrush("FoliageEditMode.SelectionBackground"))
				.BorderBackgroundColor(TAttribute<FSlateColor>(this, &SFoliageEditMeshDisplayItem::GetBorderColor))
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolTip.Background"))
					.Padding(0.0f)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SFoliageEditMeshDisplayItem::IsSelected)
						.OnCheckStateChanged(this, &SFoliageEditMeshDisplayItem::OnSelectionChanged)
						.Padding(0.0f)
					]
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(StandardPadding)
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SFoliageEditMeshDisplayItem::IsSettingsVisible)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						Toolbar.MakeWidget()
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(StandardPadding)
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SFoliageEditMeshDisplayItem::IsSettingsVisible)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SFoliageEditMeshDisplayItem::GetStaticMeshname)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.OnClicked(this, &SFoliageEditMeshDisplayItem::OnReplace)
						.ToolTipText(NSLOCTEXT("FoliageEdMode", "Replace_Tooltip", "Replace all instances with the Static Mesh currently selected in the Content Browser."))
						.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.ReplaceInstances"), "LevelEditorToolbox"))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush(TEXT("ContentReference.UseSelectionFromContentBrowser")))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.OnClicked(this, &SFoliageEditMeshDisplayItem::OnSync)
						.ToolTipText(NSLOCTEXT("FoliageEdMode", "FindInContentBrowser_Tooltip", "Find this Static Mesh in the Content Browser."))
						.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.FindInBrowser"), "LevelEditorToolbox"))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush(TEXT("ContentReference.FindInContentBrowser")))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.OnClicked(this, &SFoliageEditMeshDisplayItem::OnRemove)
						.ToolTipText(NSLOCTEXT("FoliageEdMode", "Remove_Tooltip", "Delete all foliage instances of this Static Mesh."))
						.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.DeleteInstances"), "LevelEditorToolbox"))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush(TEXT("ContentReference.Clear")))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(StandardPadding)
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SFoliageEditMeshDisplayItem::IsSettingsVisible)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SFoliageEditMeshDisplayItem::GetSettingsLabelText)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.OnClicked(this, &SFoliageEditMeshDisplayItem::OnOpenSettings)
						.ToolTipText(NSLOCTEXT("FoliageEdMode", "OpenSettings_Tooltip", "Use the InstancedFoliageSettings currently selected in the Content Browser."))
						.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.OpenSettings"), "LevelEditorToolbox"))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush(TEXT("FoliageEditMode.OpenSettings")))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.OnClicked(this, &SFoliageEditMeshDisplayItem::OnSaveRemoveSettings)
						.ToolTipText(this, &SFoliageEditMeshDisplayItem::GetSaveRemoveSettingsTooltip)
						.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("Foliage.SaveSettings"), "LevelEditorToolbox"))
						[
							SNew(SImage)
							.Image(this, &SFoliageEditMeshDisplayItem::GetSaveSettingsBrush)
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SFoliageEditMeshDisplayItem::IsNoSettingsVisible)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						[
							Toolbar.MakeWidget()
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SFoliageEditMeshDisplayItem::GetStaticMeshname)
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						ThumbnailBox.ToSharedRef()
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					.Visibility(this, &SFoliageEditMeshDisplayItem::IsClusterSettingsVisible)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SFoliageEditMeshDisplayItem::GetInstanceCountString)
							]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(StandardPadding)
						[
							ThumbnailBox.ToSharedRef()
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						ClusterSettingsDetails.ToSharedRef()
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					PaintSettings
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SFoliageEditMeshDisplayItem::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	FoliageEditPtr.Pin()->GetFoliageEditMode()->ReallocateClusters(FoliageSettingsPtr);
}

void SFoliageEditMeshDisplayItem::BindCommands()
{
	const FFoliageEditCommands& Commands = FFoliageEditCommands::Get();

	UICommandList->MapAction(
		Commands.SetNoSettings,
		FExecuteAction::CreateSP(this, &SFoliageEditMeshDisplayItem::OnNoSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEditMeshDisplayItem::IsNoSettingsEnabled));

	UICommandList->MapAction(
		Commands.SetPaintSettings,
		FExecuteAction::CreateSP(this, &SFoliageEditMeshDisplayItem::OnPaintSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEditMeshDisplayItem::IsPaintSettingsEnabled));

	UICommandList->MapAction(
		Commands.SetClusterSettings,
		FExecuteAction::CreateSP(this, &SFoliageEditMeshDisplayItem::OnClusterSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SFoliageEditMeshDisplayItem::IsClusterSettingsEnabled));
}

ECurrentViewSettings::Type SFoliageEditMeshDisplayItem::GetCurrentDisplayStatus() const
{
	return CurrentViewSettings;
}

void SFoliageEditMeshDisplayItem::SetCurrentDisplayStatus(ECurrentViewSettings::Type InDisplayStatus)
{
	CurrentViewSettings = InDisplayStatus;
}

void SFoliageEditMeshDisplayItem::Replace(UFoliageType* InFoliageSettingsPtr, TSharedPtr<FAssetThumbnail> InAssetThumbnail, TSharedPtr<FFoliageMeshUIInfo> InFoliageMeshUIInfo)
{
	FoliageSettingsPtr = InFoliageSettingsPtr;
	FoliageMeshUIInfo = InFoliageMeshUIInfo;
	Thumbnail = MoveTemp(InAssetThumbnail);
	ThumbnailWidget = Thumbnail.Get()->MakeThumbnailWidget();

	ThumbnailWidgetBorder->SetContent(ThumbnailWidget.ToSharedRef());
}

void SFoliageEditMeshDisplayItem::OnNoSettings()
{
	CurrentViewSettings = ECurrentViewSettings::ShowNone;
}

bool SFoliageEditMeshDisplayItem::IsNoSettingsEnabled() const
{
	return CurrentViewSettings == ECurrentViewSettings::ShowNone;
}

void SFoliageEditMeshDisplayItem::OnPaintSettings()
{
	CurrentViewSettings = ECurrentViewSettings::ShowPaintSettings;
}

bool SFoliageEditMeshDisplayItem::IsPaintSettingsEnabled() const
{
	return CurrentViewSettings == ECurrentViewSettings::ShowPaintSettings;
}

void SFoliageEditMeshDisplayItem::OnClusterSettings()
{
	CurrentViewSettings = ECurrentViewSettings::ShowClusterSettings;
}

bool SFoliageEditMeshDisplayItem::IsClusterSettingsEnabled() const
{
	return CurrentViewSettings == ECurrentViewSettings::ShowClusterSettings;
}

FText SFoliageEditMeshDisplayItem::GetStaticMeshname() const
{
	UStaticMesh* Mesh = FoliageSettingsPtr->GetStaticMesh();
	return FText::FromName(Mesh ? Mesh->GetFName() : NAME_None);
}

FText SFoliageEditMeshDisplayItem::GetSettingsLabelText() const
{
	return FoliageSettingsPtr->GetOuter()->IsA(UPackage::StaticClass()) ? FText::FromString(FoliageSettingsPtr->GetPathName()) : LOCTEXT("NotUsingSharedSettings", "(not using shared settings)");
}

EVisibility  SFoliageEditMeshDisplayItem::IsNoSettingsVisible() const
{
	return CurrentViewSettings == ECurrentViewSettings::ShowNone ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility  SFoliageEditMeshDisplayItem::IsSettingsVisible() const
{
	return CurrentViewSettings == ECurrentViewSettings::ShowNone ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SFoliageEditMeshDisplayItem::IsPaintSettingsVisible() const
{
	return CurrentViewSettings == ECurrentViewSettings::ShowPaintSettings ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SFoliageEditMeshDisplayItem::IsClusterSettingsVisible() const
{
	return CurrentViewSettings == ECurrentViewSettings::ShowClusterSettings ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SFoliageEditMeshDisplayItem::OnReplace()
{
	// Get current selection from content browser
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	USelection* SelectedSet = GEditor->GetSelectedSet(UStaticMesh::StaticClass());
	UStaticMesh* SelectedStaticMesh = Cast<UStaticMesh>(SelectedSet->GetTop(UStaticMesh::StaticClass()));
	if (SelectedStaticMesh != NULL)
	{
		FEdModeFoliage* Mode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);

		bool bMeshMerged = false;
		if (Mode->ReplaceStaticMesh(FoliageSettingsPtr, SelectedStaticMesh, bMeshMerged))
		{
			// If they were merged, simply remove the current item. Otherwise replace it.
			if (bMeshMerged)
			{
				FoliageEditPtr.Pin()->RemoveItemFromScrollbox(SharedThis(this));
			}
			else
			{
				FoliageEditPtr.Pin()->ReplaceItem(SharedThis(this), SelectedStaticMesh);
			}
		}
	}

	return FReply::Handled();
}

FSlateColor SFoliageEditMeshDisplayItem::GetBorderColor() const
{
	if (FoliageSettingsPtr->IsSelected)
	{
		return FSlateColor(FLinearColor(0.828f, 0.364f, 0.003f));
	}

	return FSlateColor(FLinearColor(0.25f, 0.25f, 0.25f));
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsSelected() const
{
	return FoliageSettingsPtr->IsSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnSelectionChanged(ECheckBoxState InType)
{
	FoliageSettingsPtr->IsSelected = InType == ECheckBoxState::Checked;
}

FReply SFoliageEditMeshDisplayItem::OnSync()
{
	TArray<UObject*> Objects;

	Objects.Add(FoliageSettingsPtr->GetStaticMesh());

	GEditor->SyncBrowserToObjects(Objects);

	return FReply::Handled();
}

FReply SFoliageEditMeshDisplayItem::OnRemove()
{
	FEdModeFoliage* Mode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);

	if (Mode->RemoveFoliageMesh(FoliageSettingsPtr))
	{
		FoliageEditPtr.Pin()->RemoveItemFromScrollbox(SharedThis(this));
	}

	return FReply::Handled();
}

FReply SFoliageEditMeshDisplayItem::OnSaveRemoveSettings()
{
	FEdModeFoliage* Mode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);

	if (FoliageSettingsPtr->GetOuter()->IsA(UPackage::StaticClass()))
	{
		UFoliageType* NewSettings = NULL;
		NewSettings = Mode->CopySettingsObject(FoliageSettingsPtr);

		// Do not replace the current one if NULL is returned, just keep the old one.
		if (NewSettings)
		{
			FoliageSettingsPtr = NewSettings;
		}
	}
	else
	{
		// Build default settings asset name and path
		FString DefaultAsset = FPackageName::GetLongPackagePath(FoliageSettingsPtr->GetStaticMesh()->GetOutermost()->GetName()) + TEXT("/") + FoliageSettingsPtr->GetStaticMesh()->GetName() + TEXT("_settings");

		TSharedRef<SDlgPickAssetPath> SettingDlg =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("SettingsDialogTitle", "Choose Location for Foliage Settings Asset"))
			.DefaultAssetPath(FText::FromString(DefaultAsset));

		if (SettingDlg->ShowModal() != EAppReturnType::Cancel)
		{
			UFoliageType* NewSettings = NULL;

			NewSettings = Mode->SaveSettingsObject(SettingDlg->GetFullAssetPath(), FoliageSettingsPtr);

			// Do not replace the current one if NULL is returned, just keep the old one.
			if (NewSettings)
			{
				FoliageSettingsPtr = NewSettings;
			}
		}
	}

	return FReply::Handled();
}

FReply SFoliageEditMeshDisplayItem::OnOpenSettings()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	USelection* SelectedSet = GEditor->GetSelectedSet(UFoliageType::StaticClass());
	UFoliageType* SelectedSettings = SelectedSet->GetTop<UFoliageType>();
	if (SelectedSettings != nullptr)
	{
		FEdModeFoliage* Mode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);
		Mode->ReplaceSettingsObject(FoliageSettingsPtr, SelectedSettings);
		FoliageSettingsPtr = SelectedSettings;
	}

	return FReply::Handled();
}

FText SFoliageEditMeshDisplayItem::GetSaveRemoveSettingsTooltip() const
{
	// Remove Settings tooltip.
	if (FoliageSettingsPtr->GetOuter()->IsA(UPackage::StaticClass()))
	{
		return NSLOCTEXT("FoliageEdMode", "RemoveSettings_Tooltip", "Do not store the foliage settings in a shared InstancedFoliageSettings object.");
	}

	// Save settings tooltip.
	return NSLOCTEXT("FoliageEdMode", "SaveSettings_Tooltip", "Save these settings as an InstancedFoliageSettings object stored in a package.");
}

bool SFoliageEditMeshDisplayItem::IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	const FString Category = FObjectEditorUtils::GetCategory(&PropertyAndParent.Property);
	return Category == TEXT("Clustering") || Category == TEXT("Culling") || Category == TEXT("Lighting") || Category == TEXT("Collision") || Category == TEXT("Rendering");
}

FText SFoliageEditMeshDisplayItem::GetInstanceCountString() const
{
	return FText::Format(LOCTEXT("InstanceCount_Value", "Instance Count: {0}"), FText::AsNumber(FoliageMeshUIInfo->MeshInfo ? FoliageMeshUIInfo->MeshInfo->GetInstanceCount() : 0 ));
}

EVisibility SFoliageEditMeshDisplayItem::IsReapplySettingsVisible() const
{
	FEdModeFoliage* Mode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);
	return Mode->UISettings.GetReapplyToolSelected() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SFoliageEditMeshDisplayItem::IsReapplySettingsVisible_Dummy() const
{
	FEdModeFoliage* Mode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);
	return Mode->UISettings.GetReapplyToolSelected() ? EVisibility::Hidden : EVisibility::Collapsed;
}

EVisibility SFoliageEditMeshDisplayItem::IsNotReapplySettingsVisible() const
{
	FEdModeFoliage* Mode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);
	return Mode->UISettings.GetReapplyToolSelected() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SFoliageEditMeshDisplayItem::IsUniformScalingVisible() const
{
	return FoliageSettingsPtr->UniformScale ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible() const
{
	return FoliageSettingsPtr->UniformScale ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SFoliageEditMeshDisplayItem::IsNonUniformScalingVisible_Dummy() const
{
	return FoliageSettingsPtr->UniformScale ? EVisibility::Collapsed : EVisibility::Hidden;
}

void SFoliageEditMeshDisplayItem::OnDensityReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyDensity = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsDensityReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyDensity ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnDensityChanged(float InValue)
{
	FoliageSettingsPtr->Density = InValue;
}

float SFoliageEditMeshDisplayItem::GetDensity() const
{
	return FoliageSettingsPtr->Density;
}

void SFoliageEditMeshDisplayItem::OnDensityReapplyChanged(float InValue)
{
	FoliageSettingsPtr->ReapplyDensityAmount = InValue;
}

float SFoliageEditMeshDisplayItem::GetDensityReapply() const
{
	return FoliageSettingsPtr->ReapplyDensityAmount;
}

void SFoliageEditMeshDisplayItem::OnRadiusReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyRadius = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsRadiusReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyRadius ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnRadiusChanged(float InValue)
{
	FoliageSettingsPtr->Radius = InValue;
}

float SFoliageEditMeshDisplayItem::GetRadius() const
{
	return FoliageSettingsPtr->Radius;
}

void SFoliageEditMeshDisplayItem::OnAlignToNormalReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyAlignToNormal = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsAlignToNormalReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyAlignToNormal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnAlignToNormal(ECheckBoxState InState)
{
	if (InState == ECheckBoxState::Checked)
	{
		FoliageSettingsPtr->AlignToNormal = true;
	}
	else
	{
		FoliageSettingsPtr->AlignToNormal = false;
	}
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsAlignToNormalChecked() const
{
	return FoliageSettingsPtr->AlignToNormal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SFoliageEditMeshDisplayItem::IsAlignToNormalVisible() const
{
	return FoliageSettingsPtr->AlignToNormal ? EVisibility::Visible : EVisibility::Collapsed;
}

void SFoliageEditMeshDisplayItem::OnMaxAngleChanged(float InValue)
{
	FoliageSettingsPtr->AlignMaxAngle = InValue;
}

float SFoliageEditMeshDisplayItem::GetMaxAngle() const
{
	return FoliageSettingsPtr->AlignMaxAngle;
}

void SFoliageEditMeshDisplayItem::OnRandomYawReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyRandomYaw = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsRandomYawReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyRandomYaw ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnRandomYaw(ECheckBoxState InState)
{
	if (InState == ECheckBoxState::Checked)
	{
		FoliageSettingsPtr->RandomYaw = true;
	}
	else
	{
		FoliageSettingsPtr->RandomYaw = false;
	}
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsRandomYawChecked() const
{
	return FoliageSettingsPtr->RandomYaw ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnUniformScale(ECheckBoxState InState)
{
	if (InState == ECheckBoxState::Checked)
	{
		FoliageSettingsPtr->UniformScale = true;

		FoliageSettingsPtr->ScaleMinY = FoliageSettingsPtr->ScaleMinX;
		FoliageSettingsPtr->ScaleMinZ = FoliageSettingsPtr->ScaleMinX;
	}
	else
	{
		FoliageSettingsPtr->UniformScale = false;
	}
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsUniformScaleChecked() const
{
	return FoliageSettingsPtr->UniformScale ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleUniformReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyScaleX = InState == ECheckBoxState::Checked ? true : false;
	FoliageSettingsPtr->ReapplyScaleY = InState == ECheckBoxState::Checked ? true : false;
	FoliageSettingsPtr->ReapplyScaleZ = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsScaleUniformReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyScaleX ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleUniformMinChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMinX = InValue;
	FoliageSettingsPtr->ScaleMinY = InValue;
	FoliageSettingsPtr->ScaleMinZ = InValue;

	FoliageSettingsPtr->ScaleMaxX = FMath::Max(FoliageSettingsPtr->ScaleMaxX, InValue);
	FoliageSettingsPtr->ScaleMaxY = FoliageSettingsPtr->ScaleMaxX;
	FoliageSettingsPtr->ScaleMaxZ = FoliageSettingsPtr->ScaleMaxX;
}

float SFoliageEditMeshDisplayItem::GetScaleUniformMin() const
{
	return FoliageSettingsPtr->ScaleMinX;
}

void SFoliageEditMeshDisplayItem::OnScaleUniformMaxChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMaxX = InValue;
	FoliageSettingsPtr->ScaleMaxY = InValue;
	FoliageSettingsPtr->ScaleMaxZ = InValue;

	FoliageSettingsPtr->ScaleMinX = FMath::Min(FoliageSettingsPtr->ScaleMinX, InValue);
	FoliageSettingsPtr->ScaleMinY = FoliageSettingsPtr->ScaleMinX;
	FoliageSettingsPtr->ScaleMinZ = FoliageSettingsPtr->ScaleMinX;
}

float SFoliageEditMeshDisplayItem::GetScaleUniformMax() const
{
	return FoliageSettingsPtr->ScaleMaxX;
}

void SFoliageEditMeshDisplayItem::OnScaleXReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyScaleX = InState == ECheckBoxState::Checked;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsScaleXReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyScaleX ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleXMinChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMinX = InValue;

	FoliageSettingsPtr->ScaleMaxX = FMath::Max(FoliageSettingsPtr->ScaleMaxX, InValue);
}

float SFoliageEditMeshDisplayItem::GetScaleXMin() const
{
	return FoliageSettingsPtr->ScaleMinX;
}

void SFoliageEditMeshDisplayItem::OnScaleXMaxChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMaxX = InValue;

	FoliageSettingsPtr->ScaleMinX = FMath::Min(FoliageSettingsPtr->ScaleMinX, InValue);
}

float SFoliageEditMeshDisplayItem::GetScaleXMax() const
{
	return FoliageSettingsPtr->ScaleMaxX;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsScaleXLockedChecked() const
{
	return FoliageSettingsPtr->LockScaleX ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleXLocked(ECheckBoxState InState)
{
	FoliageSettingsPtr->LockScaleX = InState == ECheckBoxState::Checked;
}

void SFoliageEditMeshDisplayItem::OnScaleYReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyScaleY = InState == ECheckBoxState::Checked;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsScaleYReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyScaleY ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleYMinChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMinY = InValue;

	FoliageSettingsPtr->ScaleMaxY = FMath::Max(FoliageSettingsPtr->ScaleMaxY, InValue);
}

float SFoliageEditMeshDisplayItem::GetScaleYMin() const
{
	return FoliageSettingsPtr->ScaleMinY;
}

void SFoliageEditMeshDisplayItem::OnScaleYMaxChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMaxY = InValue;

	FoliageSettingsPtr->ScaleMinY = FMath::Min(FoliageSettingsPtr->ScaleMinY, InValue);
}

float SFoliageEditMeshDisplayItem::GetScaleYMax() const
{
	return FoliageSettingsPtr->ScaleMaxY;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsScaleYLockedChecked() const
{
	return FoliageSettingsPtr->LockScaleY ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleYLocked(ECheckBoxState InState)
{
	FoliageSettingsPtr->LockScaleY = InState == ECheckBoxState::Checked;
}

void SFoliageEditMeshDisplayItem::OnScaleZReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyScaleZ = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsScaleZReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyScaleZ ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleZMinChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMinZ = InValue;

	FoliageSettingsPtr->ScaleMaxZ = FMath::Max(FoliageSettingsPtr->ScaleMaxZ, InValue);
}

float SFoliageEditMeshDisplayItem::GetScaleZMin() const
{
	return FoliageSettingsPtr->ScaleMinZ;
}

void SFoliageEditMeshDisplayItem::OnScaleZMaxChanged(float InValue)
{
	FoliageSettingsPtr->ScaleMaxZ = InValue;

	FoliageSettingsPtr->ScaleMinZ = FMath::Min(FoliageSettingsPtr->ScaleMinZ, InValue);
}

float SFoliageEditMeshDisplayItem::GetScaleZMax() const
{
	return FoliageSettingsPtr->ScaleMaxZ;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsScaleZLockedChecked() const
{
	return FoliageSettingsPtr->LockScaleZ ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnScaleZLocked(ECheckBoxState InState)
{
	FoliageSettingsPtr->LockScaleZ = InState == ECheckBoxState::Checked;
}

void SFoliageEditMeshDisplayItem::OnZOffsetReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyZOffset = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsZOffsetReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyZOffset ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnZOffsetMin(float InValue)
{
	FoliageSettingsPtr->ZOffsetMin = InValue;

	FoliageSettingsPtr->ZOffsetMax = FMath::Max(FoliageSettingsPtr->ZOffsetMax, InValue);
}

float SFoliageEditMeshDisplayItem::GetZOffsetMin() const
{
	return FoliageSettingsPtr->ZOffsetMin;
}

void SFoliageEditMeshDisplayItem::OnZOffsetMax(float InValue)
{
	FoliageSettingsPtr->ZOffsetMax = InValue;

	FoliageSettingsPtr->ZOffsetMin = FMath::Min(FoliageSettingsPtr->ZOffsetMin, InValue);
}

float SFoliageEditMeshDisplayItem::GetZOffsetMax() const
{
	return FoliageSettingsPtr->ZOffsetMax;
}

void SFoliageEditMeshDisplayItem::OnRandomPitchReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyRandomPitchAngle = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsRandomPitchReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyRandomPitchAngle ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnRandomPitchChanged(float InValue)
{
	FoliageSettingsPtr->RandomPitchAngle = InValue;
}

float SFoliageEditMeshDisplayItem::GetRandomPitch() const
{
	return FoliageSettingsPtr->RandomPitchAngle;
}

void SFoliageEditMeshDisplayItem::OnGroundSlopeReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyGroundSlope = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsGroundSlopeReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyGroundSlope ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnGroundSlopeChanged(float InValue)
{
	FoliageSettingsPtr->GroundSlope = InValue;
}

float SFoliageEditMeshDisplayItem::GetGroundSlope() const
{
	return FoliageSettingsPtr->GroundSlope;
}

void SFoliageEditMeshDisplayItem::OnHeightReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyHeight = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsHeightReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyHeight ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnHeightMinChanged(float InValue)
{
	FoliageSettingsPtr->HeightMin = InValue;

	FoliageSettingsPtr->HeightMax = FMath::Max(FoliageSettingsPtr->HeightMax, InValue);
}

float SFoliageEditMeshDisplayItem::GetHeightMin() const
{
	return FoliageSettingsPtr->HeightMin;
}

void SFoliageEditMeshDisplayItem::OnHeightMaxChanged(float InValue)
{
	FoliageSettingsPtr->HeightMax = InValue;

	FoliageSettingsPtr->HeightMin = FMath::Min(FoliageSettingsPtr->HeightMin, InValue);
}

float SFoliageEditMeshDisplayItem::GetHeightMax() const
{
	return FoliageSettingsPtr->HeightMax;
}

void SFoliageEditMeshDisplayItem::OnLandscapeLayerReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyLandscapeLayer = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsLandscapeLayerReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyLandscapeLayer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnLandscapeLayerChanged(const FText& InValue)
{
	FoliageSettingsPtr->LandscapeLayer = FName(*InValue.ToString());
}

FText SFoliageEditMeshDisplayItem::GetLandscapeLayer() const
{
	return FText::FromName(FoliageSettingsPtr->LandscapeLayer);
}

void SFoliageEditMeshDisplayItem::OnCollisionWithWorld(ECheckBoxState InState)
{
	if (InState == ECheckBoxState::Checked)
	{
		FoliageSettingsPtr->CollisionWithWorld = true;
	}
	else
	{
		FoliageSettingsPtr->CollisionWithWorld = false;
	}
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsCollisionWithWorldChecked() const
{
	return FoliageSettingsPtr->CollisionWithWorld ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SFoliageEditMeshDisplayItem::IsCollisionWithWorldVisible() const
{
	return FoliageSettingsPtr->CollisionWithWorld ? EVisibility::Visible : EVisibility::Collapsed;
}

void SFoliageEditMeshDisplayItem::OnCollisionWithWorldReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyCollisionWithWorld = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsCollisionWithWorldReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyCollisionWithWorld ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnCollisionScaleXChanged(float InValue)
{
	FoliageSettingsPtr->CollisionScale.X = InValue;
}

void SFoliageEditMeshDisplayItem::OnCollisionScaleYChanged(float InValue)
{
	FoliageSettingsPtr->CollisionScale.Y = InValue;
}

void SFoliageEditMeshDisplayItem::OnCollisionScaleZChanged(float InValue)
{
	FoliageSettingsPtr->CollisionScale.Z = InValue;
}

float SFoliageEditMeshDisplayItem::GetCollisionScaleX() const
{
	return FoliageSettingsPtr->CollisionScale.X;
}

float SFoliageEditMeshDisplayItem::GetCollisionScaleY() const
{
	return FoliageSettingsPtr->CollisionScale.Y;
}

float SFoliageEditMeshDisplayItem::GetCollisionScaleZ() const
{
	return FoliageSettingsPtr->CollisionScale.Z;
}

void SFoliageEditMeshDisplayItem::OnVertexColorMask(ECheckBoxState InState, FoliageVertexColorMask Mask)
{
	FoliageSettingsPtr->VertexColorMask = Mask;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsVertexColorMaskChecked(FoliageVertexColorMask Mask) const
{
	return FoliageSettingsPtr->VertexColorMask == Mask ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnVertexColorMaskThresholdChanged(float InValue)
{
	FoliageSettingsPtr->VertexColorMaskThreshold = InValue;
}

float SFoliageEditMeshDisplayItem::GetVertexColorMaskThreshold() const
{
	return FoliageSettingsPtr->VertexColorMaskThreshold;
}

EVisibility SFoliageEditMeshDisplayItem::IsVertexColorMaskThresholdVisible() const
{
	return (FoliageSettingsPtr->VertexColorMask == FOLIAGEVERTEXCOLORMASK_Disabled) ? EVisibility::Collapsed : EVisibility::Visible;
}

void SFoliageEditMeshDisplayItem::OnVertexColorMaskInvert(ECheckBoxState InState)
{
	FoliageSettingsPtr->VertexColorMaskInvert = (InState == ECheckBoxState::Checked);
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsVertexColorMaskInvertChecked() const
{
	return FoliageSettingsPtr->VertexColorMaskInvert ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEditMeshDisplayItem::OnVertexColorMaskReapply(ECheckBoxState InState)
{
	FoliageSettingsPtr->ReapplyVertexColorMask = InState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SFoliageEditMeshDisplayItem::IsVertexColorMaskReapplyChecked() const
{
	return FoliageSettingsPtr->ReapplyVertexColorMask ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FReply SFoliageEditMeshDisplayItem::OnMouseDownSelection(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FoliageSettingsPtr->IsSelected = !FoliageSettingsPtr->IsSelected;
	return FReply::Handled();
}

const FSlateBrush* SFoliageEditMeshDisplayItem::GetSaveSettingsBrush() const
{
	if (FoliageSettingsPtr->GetOuter()->IsA(UPackage::StaticClass()))
	{
		return FEditorStyle::GetBrush(TEXT("FoliageEditMode.DeleteItem"));
	}

	return FEditorStyle::GetBrush(TEXT("FoliageEditMode.SaveSettings"));
}

#undef LOCTEXT_NAMESPACE
