// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class FMeshMergingTool;

/*-----------------------------------------------------------------------------
   SMeshMergingDialog
-----------------------------------------------------------------------------*/
class SMeshMergingDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMeshMergingDialog)
	{
	}

	SLATE_END_ARGS()

public:
	/** **/
	SMeshMergingDialog();

	/** SWidget functions */
	void Construct(const FArguments& InArgs, FMeshMergingTool* InTool);

private:

	/** Called when the Merge button is clicked */
	FReply OnMergeClicked();

	/**  */
	ECheckBoxState GetGenerateLightmapUV() const;
	void SetGenerateLightmapUV(ECheckBoxState NewValue);

	/** Target lightmap channel */
	bool IsLightmapChannelEnabled() const;
	void SetTargetLightMapChannel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void SetTargetLightMapResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/**  */
	ECheckBoxState GetExportSpecificLODEnabled() const;
	void SetExportSpecificLODEnabled(ECheckBoxState NewValue);
	bool IsExportSpecificLODEnabled() const;
	void SetExportSpecificLODIndex(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
		
	/**  */
	ECheckBoxState GetImportVertexColors() const;
	void SetImportVertexColors(ECheckBoxState NewValue);

	/**  */
	ECheckBoxState GetPivotPointAtZero() const;
	void SetPivotPointAtZero(ECheckBoxState NewValue);

	ECheckBoxState GetPlaceInWorld() const;
	void SetPlaceInWorld(ECheckBoxState NewValue);

	/** Material merging */
	bool IsMaterialMergingEnabled() const;
	ECheckBoxState GetMergeMaterials() const;
	void SetMergeMaterials(ECheckBoxState NewValue);

	ECheckBoxState GetExportNormalMap() const;
	void SetExportNormalMap(ECheckBoxState NewValue);

	ECheckBoxState GetExportMetallicMap() const;
	void SetExportMetallicMap(ECheckBoxState NewValue);

	ECheckBoxState GetExportRoughnessMap() const;
	void SetExportRoughnessMap(ECheckBoxState NewValue);

	ECheckBoxState GetExportSpecularMap() const;
	void SetExportSpecularMap(ECheckBoxState NewValue);

	void SetMergedMaterialAtlasResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	
private:

	FMeshMergingTool* Tool;

	TArray<TSharedPtr<FString>>	ExportLODOptions;
	TArray<TSharedPtr<FString>>	LightMapResolutionOptions;
	TArray<TSharedPtr<FString>>	LightMapChannelOptions;
	TArray<TSharedPtr<FString>>	MergedMaterialResolutionOptions;
};
