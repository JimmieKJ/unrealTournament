// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FFoliageMeshUIInfo;
typedef TSharedPtr<FFoliageMeshUIInfo> FFoliageMeshUIInfoPtr; //should match typedef in FoliageEdMode.h

class SFoliageEdit : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFoliageEdit) {}
	SLATE_END_ARGS()

public:
	/** SCompoundWidget functions */
	void Construct(const FArguments& InArgs);

	/** Does a full refresh on the list. */
	void RefreshFullList();

	/** Notifies the widget that the mesh assigned to a foliage type in the list has changed */
	void NotifyFoliageTypeMeshChanged(UFoliageType* FoliageType);

	/** Gets FoliageEditMode. Used by the cluster details to notify changes */
	class FEdModeFoliage* GetFoliageEditMode() const { return FoliageEditMode; }

private:
	/** Creates the toolbar. */
	TSharedRef<SWidget> BuildToolBar();

	/** Checks if the tool mode is Paint. */
	bool IsPaintTool() const;

	/** Checks if the tool mode is Reapply Settings. */
	bool IsReapplySettingsTool() const;

	/** Checks if the tool mode is Select. */
	bool IsSelectTool() const;

	bool IsLassoSelectTool() const;

	/** Checks if the tool mode is Paint Bucket. */
	bool IsPaintFillTool() const;

	FText GetActiveToolName() const;

private:	// BRUSH SETTINGS
	/** Sets the brush Radius for the brush. */
	void SetRadius(float InRadius);

	/** Retrieves the brush Radius for the brush. */
	TOptional<float> GetRadius() const;

	/** Sets the Paint Density for the brush. */
	void SetPaintDensity(float InDensity);

	/** Retrieves the Paint Density for the brush. */
	TOptional<float> GetPaintDensity() const;

	/** Sets the Erase Density for the brush. */
	void SetEraseDensity(float InDensity);

	/** Retrieves the Erase Density for the brush. */
	TOptional<float> GetEraseDensity() const;

	/** Retrieves the text for the filters option */
	FText GetFilterText() const;

	/** Sets the filter settings for if painting will occur on Landscapes. */
	void OnCheckStateChanged_Landscape(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on Landscapes. */
	ECheckBoxState GetCheckState_Landscape() const;

	/** Retrieves the tooltip text for the landscape filter */
	FText GetTooltipText_Landscape() const;

	/** Sets the filter settings for if painting will occur on Static Meshes. */
	void OnCheckStateChanged_StaticMesh(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on Static Meshes. */
	ECheckBoxState GetCheckState_StaticMesh() const;

	/** Retrieves the tooltip text for the static mesh filter */
	FText GetTooltipText_StaticMesh() const;

	/** Sets the filter settings for if painting will occur on BSPs. */
	void OnCheckStateChanged_BSP(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on BSPs. */
	ECheckBoxState GetCheckState_BSP() const;

	/** Retrieves the tooltip text for the BSP filter */
	FText GetTooltipText_BSP() const;

	/** Sets the filter settings for if painting will occur on translucent meshes. */
	void OnCheckStateChanged_Translucent(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on translucent meshes. */
	ECheckBoxState GetCheckState_Translucent() const;

	/** Retrieves the tooltip text for the translucent filter */
	FText GetTooltipText_Translucent() const;

	/** Checks if the radius spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_Radius() const;

	/** Checks if the paint density spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_PaintDensity() const;

	/** Checks if the erase density spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_EraseDensity() const;

	/** Checks if the filters should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_Filters() const;

	/** Checks if the text in the empty list overlay should appear. If the list is has items but the the drag and drop override is true, it will return EVisibility::Visible. */
	EVisibility GetVisibility_EmptyList() const;

	/** @return Whether selecting instances is currently possible */
	EVisibility GetVisibility_SelectionOptions() const;

private:	// SELECTION

	/** Handler for 'Select All' command  */
	FReply OnSelectAllInstances();

	/** Handler for 'Select Invalid Instances' command  */
	FReply OnSelectInvalidInstances();

	/** Handler for 'Deselect All' command  */
	FReply OnDeselectAllInstances();

	/** Tooltip text for 'Instance Count" column */
	FText GetTotalInstanceCountTooltipText() const;

	/** Handler to trigger a refresh of the details view when the active tool changes */
	void HandleOnToolChanged();

	
private:
	/** Palette of available foliage types */
	TSharedPtr<class SFoliagePalette> FoliagePalette;

	/** Pointer to the foliage edit mode. */
	FEdModeFoliage*					FoliageEditMode;
};
