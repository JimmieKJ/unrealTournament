// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MergeActorsPrivatePCH.h"
#include "SMeshProxyDialog.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "SNumericEntryBox.h"
#include "STextComboBox.h"
#include "MeshProxyTool/MeshProxyTool.h"

#define LOCTEXT_NAMESPACE "SMeshProxyDialog"


void SMeshProxyDialog::Construct(const FArguments& InArgs, FMeshProxyTool* InTool)
{
	Tool = InTool;
	check(Tool != nullptr);

	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Z"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Z"))));

	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("64"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("128"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("256"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("512"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("1024"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("2048"))));

	CreateLayout();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMeshProxyDialog::CreateLayout()
{
	this->ChildSlot
	[
		SNew(SVerticalBox)
			
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(10)
		[
			// Simplygon logo
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("MeshProxy.SimplygonLogo"))
		]
			
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				// Proxy options
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OnScreenSizeLabel", "On Screen Size (pixels)"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(40)
							.MaxValue(1200)
							.MinSliderValue(40)
							.MaxSliderValue(1200)
							.AllowSpin(true)
							.Value(this, &SMeshProxyDialog::GetScreenSize)
							.OnValueChanged(this, &SMeshProxyDialog::ScreenSizeChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MergeDistanceLabel", "Merge Distance (pixels)"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(0)
							.MaxValue(300)
							.MinSliderValue(0)
							.MaxSliderValue(300)
							.AllowSpin(true)
							.Value(this, &SMeshProxyDialog::GetMergeDistance)
							.OnValueChanged(this, &SMeshProxyDialog::MergeDistanceChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextureResolutionLabel", "Texture Resolution"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextComboBox)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.OptionsSource(&TextureResolutionOptions)
						.InitiallySelectedItem(TextureResolutionOptions[3]) //512
						.OnSelectionChanged(this, &SMeshProxyDialog::SetTextureResolution)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetRecalculateNormals)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetRecalculateNormals)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RecalcNormalsLabel", "Recalculate Normals"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					.Padding(0.0, 0.0, 3.0, 0.0)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(30)
							[
								SNullWidget::NullWidget
							]
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.IsEnabled(this, &SMeshProxyDialog::HardAngleThresholdEnabled)
							.Text(LOCTEXT("HardAngleLabel", "Hard Angle"))
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						]
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<float>)
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(0.f)
							.MaxValue(180.f)
							.MinSliderValue(0.f)
							.MaxSliderValue(180.f)
							.AllowSpin(true)
							.Value(this, &SMeshProxyDialog::GetHardAngleThreshold)
							.OnValueChanged(this, &SMeshProxyDialog::HardAngleThresholdChanged)
							.IsEnabled(this, &SMeshProxyDialog::HardAngleThresholdEnabled)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetUseClippingPlane)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetUseClippingPlane)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ClippingPlaneLabel", "Use Clipping Plane"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(0.5f)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(30)
							[
								SNullWidget::NullWidget
							]
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.IsEnabled(this, &SMeshProxyDialog::UseClippingPlaneEnabled)
							.Text(LOCTEXT("ClippingAxisLabel", "Clipping Axis"))
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						]
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(8.0, 0.0, 8.0, 0.0)
					[
						SNew(STextComboBox)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.OptionsSource(&CuttingPlaneOptions)
						.InitiallySelectedItem(CuttingPlaneOptions[0])
						.OnSelectionChanged(this, &SMeshProxyDialog::SetClippingAxis)
						.IsEnabled(this, &SMeshProxyDialog::UseClippingPlaneEnabled)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					.Padding(0.0, 0.0, 3.0, 0.0)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(30)
							[
								SNullWidget::NullWidget
							]
						]
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PlaneLevelLabel", "Plane level"))
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.IsEnabled(this, &SMeshProxyDialog::UseClippingPlaneEnabled)
						]
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<float>)
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.Value(this, &SMeshProxyDialog::GetClippingLevel)
							.OnValueChanged(this, &SMeshProxyDialog::ClippingLevelChanged)
							.IsEnabled(this, &SMeshProxyDialog::UseClippingPlaneEnabled)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportNormalMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportNormalMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportNormalMapLabel", "Export Normal Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportMetallicMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportMetallicMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportMetallicMapLabel", "Export Metallic Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportRoughnessMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportRoughnessMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportRoughnessMapLabel", "Export Roughness Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportSpecularMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportSpecularMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportSpecularMapLabel", "Export Specular Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

//Screen size
TOptional<int32> SMeshProxyDialog::GetScreenSize() const
{
	return Tool->ProxySettings.ScreenSize;
}

void SMeshProxyDialog::ScreenSizeChanged(int32 NewValue)
{
	Tool->ProxySettings.ScreenSize = NewValue;
}

//Recalculate normals
ECheckBoxState SMeshProxyDialog::GetRecalculateNormals() const
{
	return Tool->ProxySettings.bRecalculateNormals ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetRecalculateNormals(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bRecalculateNormals = (NewValue == ECheckBoxState::Checked);
}

//Hard Angle Threshold
bool SMeshProxyDialog::HardAngleThresholdEnabled() const
{
	if(Tool->ProxySettings.bRecalculateNormals)
	{
		return true;
	}

	return false;
}

TOptional<float> SMeshProxyDialog::GetHardAngleThreshold() const
{
	return Tool->ProxySettings.HardAngleThreshold;
}

void SMeshProxyDialog::HardAngleThresholdChanged(float NewValue)
{
	Tool->ProxySettings.HardAngleThreshold = NewValue;
}

//Merge Distance
TOptional<int32> SMeshProxyDialog::GetMergeDistance() const
{
	return Tool->ProxySettings.MergeDistance;
}

void SMeshProxyDialog::MergeDistanceChanged(int32 NewValue)
{
	Tool->ProxySettings.MergeDistance = NewValue;
}

//Clipping Plane
ECheckBoxState SMeshProxyDialog::GetUseClippingPlane() const
{
	return Tool->ProxySettings.bUseClippingPlane ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetUseClippingPlane(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bUseClippingPlane = (NewValue == ECheckBoxState::Checked);
}

bool SMeshProxyDialog::UseClippingPlaneEnabled() const
{
	if(Tool->ProxySettings.bUseClippingPlane)
	{
		return true;
	}
	return false;
}

void SMeshProxyDialog::SetClippingAxis(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo )
{
	//This is a ugly hack, but it solves the problem for now
	int32 AxisIndex = CuttingPlaneOptions.Find(NewSelection);
	//FMessageDialog::Debugf(*FString::Printf(TEXT("%d"), AxisIndex ));
	if(AxisIndex < 3)
	{
		Tool->ProxySettings.bPlaneNegativeHalfspace = false;
		Tool->ProxySettings.AxisIndex = AxisIndex;
		return;
	}
	else
	{
		Tool->ProxySettings.bPlaneNegativeHalfspace = true;
		Tool->ProxySettings.AxisIndex = AxisIndex - 3;
		return;
	}
}

TOptional<float> SMeshProxyDialog::GetClippingLevel() const
{
	return Tool->ProxySettings.ClippingLevel;
}

void SMeshProxyDialog::ClippingLevelChanged(float NewValue)
{
	Tool->ProxySettings.ClippingLevel = NewValue;
}

//Texture Resolution
void SMeshProxyDialog::SetTextureResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Resolution = 512;
	TTypeFromString<int32>::FromString(Resolution, **NewSelection);
	
	Tool->ProxySettings.TextureWidth	= Resolution;
	Tool->ProxySettings.TextureHeight = Resolution;
}

ECheckBoxState SMeshProxyDialog::GetExportNormalMap() const
{
	return Tool->ProxySettings.bExportNormalMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportNormalMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bExportNormalMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SMeshProxyDialog::GetExportMetallicMap() const
{
	return Tool->ProxySettings.bExportMetallicMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportMetallicMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bExportMetallicMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SMeshProxyDialog::GetExportRoughnessMap() const
{
	return Tool->ProxySettings.bExportRoughnessMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportRoughnessMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bExportRoughnessMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SMeshProxyDialog::GetExportSpecularMap() const
{
	return Tool->ProxySettings.bExportSpecularMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportSpecularMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bExportSpecularMap = (NewValue == ECheckBoxState::Checked);
}


#undef LOCTEXT_NAMESPACE
