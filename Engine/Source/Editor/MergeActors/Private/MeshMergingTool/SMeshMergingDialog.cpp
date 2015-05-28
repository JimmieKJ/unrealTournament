// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MergeActorsPrivatePCH.h"
#include "SMeshMergingDialog.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "STextComboBox.h"
#include "RawMesh.h"
#include "MeshMergingTool/MeshMergingTool.h"

#define LOCTEXT_NAMESPACE "SMeshMergingDialog"

//////////////////////////////////////////////////////////////////////////
// SMeshMergingDialog

SMeshMergingDialog::SMeshMergingDialog()
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMeshMergingDialog::Construct(const FArguments& InArgs, FMeshMergingTool* InTool)
{
	Tool = InTool;
	check(Tool != nullptr);

	// Setup available resolutions for lightmap and merged materials
	const int32 MinTexResolution = 1 << FTextureLODGroup().MinLODMipCount;
	const int32 MaxTexResolution = 1 << FTextureLODGroup().MaxLODMipCount;
	for (int32 TexRes = MinTexResolution; TexRes <= MaxTexResolution; TexRes*=2)
	{
		LightMapResolutionOptions.Add(MakeShareable(new FString(TTypeToString<int32>::ToString(TexRes))));
		MergedMaterialResolutionOptions.Add(MakeShareable(new FString(TTypeToString<int32>::ToString(TexRes))));
	}

	Tool->MergingSettings.TargetLightMapResolution = FMath::Clamp(Tool->MergingSettings.TargetLightMapResolution, MinTexResolution, MaxTexResolution);
	Tool->MergingSettings.MergedMaterialAtlasResolution = FMath::Clamp(Tool->MergingSettings.MergedMaterialAtlasResolution, MinTexResolution, MaxTexResolution);
		
	// Setup available UV channels for an atlased lightmap
	for (int32 Index = 0; Index < MAX_MESH_TEXTURE_COORDS; Index++)
	{
		LightMapChannelOptions.Add(MakeShareable(new FString(TTypeToString<int32>::ToString(Index))));
	}

	for (int32 Index = 0; Index < MAX_STATIC_MESH_LODS; Index++)
	{
		ExportLODOptions.Add(MakeShareable(new FString(TTypeToString<int32>::ToString(Index))));
	}

	// Create widget layout
	this->ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0,2,0,0)
		[
			// Lightmap settings
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
									
				// Enable atlasing
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetGenerateLightmapUV)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetGenerateLightmapUV)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AtlasLightmapUVLabel", "Generate Lightmap UVs"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
					
				// Target lightmap channel / Max lightmap resolution
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.IsEnabled(this, &SMeshMergingDialog::IsLightmapChannelEnabled)
						.Text(LOCTEXT("TargetLightMapChannelLabel", "Target Channel:"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4,0,4,0)
					[
						SNew(STextComboBox)
						.IsEnabled( this, &SMeshMergingDialog::IsLightmapChannelEnabled )
						.OptionsSource(&LightMapChannelOptions)
						.InitiallySelectedItem(LightMapChannelOptions[Tool->MergingSettings.TargetLightMapUVChannel])
						.OnSelectionChanged(this, &SMeshMergingDialog::SetTargetLightMapChannel)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.IsEnabled(this, &SMeshMergingDialog::IsLightmapChannelEnabled)
						.Text(LOCTEXT("TargetLightMapResolutionLabel", "Target Resolution:"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
												
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4,0,4,0)
					[
						SNew(STextComboBox)
						.IsEnabled( this, &SMeshMergingDialog::IsLightmapChannelEnabled )
						.OptionsSource(&LightMapResolutionOptions)
						.InitiallySelectedItem(LightMapResolutionOptions[FMath::FloorLog2(Tool->MergingSettings.TargetLightMapResolution)])
						.OnSelectionChanged(this, &SMeshMergingDialog::SetTargetLightMapResolution)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
			]
		]

		// Other merging settings
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0,2,0,0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				// LOD to export
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.Type(ESlateCheckBoxType::CheckBox)
						.IsChecked(this, &SMeshMergingDialog::GetExportSpecificLODEnabled)
						.OnCheckStateChanged(this, &SMeshMergingDialog::SetExportSpecificLODEnabled)
						.Content()
						[
							SNew(STextBlock)
							.IsEnabled(this, &SMeshMergingDialog::IsExportSpecificLODEnabled)
							.Text(LOCTEXT("TargetMeshLODIndexLabel", "Export specific LOD:"))
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						]
					]
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4,0,4,0)
					[
						SNew(STextComboBox)
						.IsEnabled(this, &SMeshMergingDialog::IsExportSpecificLODEnabled)
						.OptionsSource(&ExportLODOptions)
						.InitiallySelectedItem(ExportLODOptions[Tool->ExportLODIndex])
						.OnSelectionChanged(this, &SMeshMergingDialog::SetExportSpecificLODIndex)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
					
				// Vertex colors
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetImportVertexColors)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetImportVertexColors)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ImportVertexColorsLabel", "Import Vertex Colors"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				// Pivot at zero
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetPivotPointAtZero)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetPivotPointAtZero)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PivotPointAtZeroLabel", "Pivot Point At (0,0,0)"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				// Place in world
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetPlaceInWorld)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetPlaceInWorld)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PlaceInWorldLabel", "Place In World"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				// Merge materials
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetMergeMaterials)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetMergeMaterials)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MergeMaterialsLabel", "Merge Materials"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
				// Export normal map
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetExportNormalMap)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetExportNormalMap)
					.IsEnabled(this, &SMeshMergingDialog::IsMaterialMergingEnabled)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportNormalMapLabel", "Export Normal Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
				// Export metallic map
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetExportMetallicMap)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetExportMetallicMap)
					.IsEnabled(this, &SMeshMergingDialog::IsMaterialMergingEnabled)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportMetallicMapLabel", "Export Metallic Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
				// Export roughness map
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetExportRoughnessMap)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetExportRoughnessMap)
					.IsEnabled(this, &SMeshMergingDialog::IsMaterialMergingEnabled)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportRoughnessMapLabel", "Export Roughness Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
				// Export specular map
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshMergingDialog::GetExportSpecularMap)
					.OnCheckStateChanged(this, &SMeshMergingDialog::SetExportSpecularMap)
					.IsEnabled(this, &SMeshMergingDialog::IsMaterialMergingEnabled)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportSpecularMapLabel", "Export Specular Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
				
				// Merged texture size
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.IsEnabled(this, &SMeshMergingDialog::IsMaterialMergingEnabled)
						.Text(LOCTEXT("MergedMaterialAtlasResolutionLabel", "Merged Material Atlas Resolution:"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
												
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4,0,4,0)
					[
						SNew(STextComboBox)
						.IsEnabled(this, &SMeshMergingDialog::IsMaterialMergingEnabled)
						.OptionsSource(&MergedMaterialResolutionOptions)
						.InitiallySelectedItem(MergedMaterialResolutionOptions[FMath::FloorLog2(Tool->MergingSettings.MergedMaterialAtlasResolution)])
						.OnSelectionChanged(this, &SMeshMergingDialog::SetMergedMaterialAtlasResolution)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]														
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

ECheckBoxState SMeshMergingDialog::GetGenerateLightmapUV() const
{
	return (Tool->MergingSettings.bGenerateLightMapUV ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetGenerateLightmapUV(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bGenerateLightMapUV = (ECheckBoxState::Checked == NewValue);
}

bool SMeshMergingDialog::IsLightmapChannelEnabled() const
{
	return Tool->MergingSettings.bGenerateLightMapUV;
}

void SMeshMergingDialog::SetTargetLightMapChannel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	TTypeFromString<int32>::FromString(Tool->MergingSettings.TargetLightMapUVChannel, **NewSelection);
}

void SMeshMergingDialog::SetTargetLightMapResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	TTypeFromString<int32>::FromString(Tool->MergingSettings.TargetLightMapResolution, **NewSelection);
}

ECheckBoxState SMeshMergingDialog::GetExportSpecificLODEnabled() const
{
	return (Tool->bExportSpecificLOD ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetExportSpecificLODEnabled(ECheckBoxState NewValue)
{
	Tool->bExportSpecificLOD = (NewValue == ECheckBoxState::Checked);
}

bool SMeshMergingDialog::IsExportSpecificLODEnabled() const
{
	return Tool->bExportSpecificLOD;
}

void SMeshMergingDialog::SetExportSpecificLODIndex(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	TTypeFromString<int32>::FromString(Tool->ExportLODIndex, **NewSelection);
}

ECheckBoxState SMeshMergingDialog::GetImportVertexColors() const
{
	return (Tool->MergingSettings.bImportVertexColors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetImportVertexColors(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bImportVertexColors = (ECheckBoxState::Checked == NewValue);
}

ECheckBoxState SMeshMergingDialog::GetPivotPointAtZero() const
{
	return (Tool->MergingSettings.bPivotPointAtZero ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetPivotPointAtZero(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bPivotPointAtZero = (ECheckBoxState::Checked == NewValue);
}

ECheckBoxState SMeshMergingDialog::GetPlaceInWorld() const
{
	return (Tool->bPlaceInWorld ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetPlaceInWorld(ECheckBoxState NewValue)
{
	Tool->bPlaceInWorld = (ECheckBoxState::Checked == NewValue);
}

bool SMeshMergingDialog::IsMaterialMergingEnabled() const
{
	return Tool->MergingSettings.bMergeMaterials;
}

ECheckBoxState SMeshMergingDialog::GetMergeMaterials() const
{
	return (Tool->MergingSettings.bMergeMaterials ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetMergeMaterials(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bMergeMaterials = (ECheckBoxState::Checked == NewValue);
}

ECheckBoxState SMeshMergingDialog::GetExportNormalMap() const
{
	return (Tool->MergingSettings.bExportNormalMap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetExportNormalMap(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bExportNormalMap = (ECheckBoxState::Checked == NewValue);
}

ECheckBoxState SMeshMergingDialog::GetExportMetallicMap() const
{
	return (Tool->MergingSettings.bExportMetallicMap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetExportMetallicMap(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bExportMetallicMap = (ECheckBoxState::Checked == NewValue);
}

ECheckBoxState SMeshMergingDialog::GetExportRoughnessMap() const
{
	return (Tool->MergingSettings.bExportRoughnessMap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetExportRoughnessMap(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bExportRoughnessMap = (ECheckBoxState::Checked == NewValue);
}

ECheckBoxState SMeshMergingDialog::GetExportSpecularMap() const
{
	return (Tool->MergingSettings.bExportSpecularMap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetExportSpecularMap(ECheckBoxState NewValue)
{
	Tool->MergingSettings.bExportSpecularMap = (ECheckBoxState::Checked == NewValue);
}

void SMeshMergingDialog::SetMergedMaterialAtlasResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	TTypeFromString<int32>::FromString(Tool->MergingSettings.MergedMaterialAtlasResolution, **NewSelection);
}


#undef LOCTEXT_NAMESPACE
