// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forward declarations
class IDetailsView;
struct FPropertyAndParent;

namespace ECurrentViewSettings
{
	enum Type 
	{ 
		ShowNone = 0, 
		ShowPaintSettings, 
		ShowClusterSettings 
	};
}

class SFoliageEditMeshDisplayItem : public SBorder, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SFoliageEditMeshDisplayItem)
		: _FoliageSettingsPtr(NULL)
		{}

		SLATE_ARGUMENT(UFoliageType*, FoliageSettingsPtr)
		SLATE_ARGUMENT(TSharedPtr<FAssetThumbnail>, AssetThumbnail)
		SLATE_ARGUMENT(TWeakPtr<SFoliageEdit>, FoliageEditPtr)
		SLATE_ARGUMENT(TSharedPtr<FFoliageMeshUIInfo>, FoliageMeshUIInfo)
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Called when the replace button is pressed, simply updates it's current data with the passed in data.
	 *
	 * @param InFoliageSettingsPtr			Pointer to the settings for this item.
	 * @param InAssetThumbnail				The thumbnail to display.
	 * @param InFoliageMeshUIInfo			The UI info for this item.
	 */
	void Replace(UFoliageType* InFoliageSettingsPtr, TSharedPtr<FAssetThumbnail> InAssetThumbnail, TSharedPtr<FFoliageMeshUIInfo> InFoliageMeshUIInfo);

	/** Used to cache and return the display status of items when undoing. */
	ECurrentViewSettings::Type GetCurrentDisplayStatus() const;

	/**
	 * Sets the display status of the item.
	 *
	 * @param InDisplayStatus		The status to set the item's display to.
	 */
	void SetCurrentDisplayStatus(ECurrentViewSettings::Type InDisplayStatus);

	/** Handles clicking on the selection area for the item. */
	FReply OnMouseDownSelection(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Returns the current brush for the Save/Remove settings button. */
	const FSlateBrush* GetSaveSettingsBrush() const;

private:
	/** Callback to get the border color for the item. */
	FSlateColor GetBorderColor() const;

	/** Returns true if this item is selected. */
	ECheckBoxState IsSelected() const;

	void OnSelectionChanged(ECheckBoxState InType);

	/** Bind commands used by this class for the toolbar. */
	void BindCommands();

	/** Delegate functions handling the display status. */
	void OnNoSettings();

	bool IsNoSettingsEnabled() const;

	void OnPaintSettings();

	bool IsPaintSettingsEnabled() const;

	void OnClusterSettings();

	bool IsClusterSettingsEnabled() const;

	/** General UI delegate functions. */
	FText GetStaticMeshname() const;

	FText GetSettingsLabelText() const;

	EVisibility  IsNoSettingsVisible() const;

	EVisibility  IsSettingsVisible() const;

	EVisibility IsPaintSettingsVisible() const;

	EVisibility IsClusterSettingsVisible() const;

	/** Button delegate functions. */
	FReply OnReplace();

	FReply OnSync();

	FReply OnRemove();

	FReply OnSaveRemoveSettings();

	FReply OnOpenSettings();

	/** Returns the tool-tip to use for the save settings button. */
	FText GetSaveRemoveSettingsTooltip() const;

	/** Determines what properties to display in the details panel. */
	bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const;

	/** Cluster Accessors */
	FText GetInstanceCountString() const;

	/** General functions for delegates. */
	EVisibility IsReapplySettingsVisible() const;
	EVisibility IsReapplySettingsVisible_Dummy() const;
	EVisibility IsNotReapplySettingsVisible() const;

	EVisibility IsUniformScalingVisible() const;
	EVisibility IsNonUniformScalingVisible() const;
	EVisibility IsNonUniformScalingVisible_Dummy() const;


	/** Density */
	void OnDensityReapply(ECheckBoxState InState);

	ECheckBoxState IsDensityReapplyChecked() const;

	void OnDensityChanged(float InValue);

	float GetDensity() const;

	void OnDensityReapplyChanged(float InValue);

	float GetDensityReapply() const;

	int32 GetSeedsPerComponent() const;
	void OnSeedsPerComponentChanged(int32 InValue);

	int32 GetSimulationSteps() const;
	void OnSimulationStepsChanged(int32 InValue);

	float GetMaxRadius() const;
	void OnMaxRadiusChanged(float InValue);

	float GetSpreadRadius() const;
	void OnSpreadRadiusChanged(float InValue);

	float GetGrowthRadius() const;
	void OnGrowthRadiusChanged(float InValue);

	/** Radius */
	void OnRadiusReapply(ECheckBoxState InState);

	ECheckBoxState IsRadiusReapplyChecked() const;

	void OnRadiusChanged(float InValue);

	float GetRadius() const;

	/** Align to Normal */
	void OnAlignToNormalReapply(ECheckBoxState InState);

	ECheckBoxState IsAlignToNormalReapplyChecked() const;

	void OnAlignToNormal(ECheckBoxState InState);

	ECheckBoxState IsAlignToNormalChecked() const;

	EVisibility IsAlignToNormalVisible() const;

	/** Align to Normal */
	void OnMaxAngleChanged(float InValue);

	float GetMaxAngle() const;

	/** Random Yaw */
	void OnRandomYawReapply(ECheckBoxState InState);

	ECheckBoxState IsRandomYawReapplyChecked() const;

	void OnRandomYaw(ECheckBoxState InState);

	ECheckBoxState IsRandomYawChecked() const;

	/** Uniform Scale Checkbox */
	void OnUniformScale(ECheckBoxState InState);

	ECheckBoxState IsUniformScaleChecked() const;

	/** Uniform Scaling Min/Max */
	void OnScaleUniformReapply(ECheckBoxState InState);

	ECheckBoxState IsScaleUniformReapplyChecked() const;

	void OnScaleUniformMinChanged(float InValue);

	float GetScaleUniformMin() const;

	void OnScaleUniformMaxChanged(float InValue);

	float GetScaleUniformMax() const;

	/** X-Axis Scaling Min/Max */
	void OnScaleXReapply(ECheckBoxState InState);

	ECheckBoxState IsScaleXReapplyChecked() const;

	void OnScaleXMinChanged(float InValue);

	float GetScaleXMin() const;

	void OnScaleXMaxChanged(float InValue);

	float GetScaleXMax() const;

	ECheckBoxState IsScaleXLockedChecked() const;

	void OnScaleXLocked(ECheckBoxState InState);

	/** Y-Axis Scaling Min/Max */
	void OnScaleYReapply(ECheckBoxState InState);

	ECheckBoxState IsScaleYReapplyChecked() const;

	void OnScaleYMinChanged(float InValue);

	float GetScaleYMin() const;

	void OnScaleYMaxChanged(float InValue);

	float GetScaleYMax() const;

	ECheckBoxState IsScaleYLockedChecked() const;

	void OnScaleYLocked(ECheckBoxState InState);

	/** Z-Axis Scaling Min/Max */
	void OnScaleZReapply(ECheckBoxState InState);

	ECheckBoxState IsScaleZReapplyChecked() const;

	void OnScaleZMinChanged(float InValue);

	float GetScaleZMin() const;

	void OnScaleZMaxChanged(float InValue);

	float GetScaleZMax() const;

	ECheckBoxState IsScaleZLockedChecked() const;

	void OnScaleZLocked(ECheckBoxState InState);

	/** Z-Offset Min/Max */
	void OnZOffsetReapply(ECheckBoxState InState);

	ECheckBoxState IsZOffsetReapplyChecked() const;

	void OnZOffsetMin(float InValue);

	float GetZOffsetMin() const;

	void OnZOffsetMax(float InValue);

	float GetZOffsetMax() const;

	/** Random Pitch +/- */
	void OnRandomPitchReapply(ECheckBoxState InState);

	ECheckBoxState IsRandomPitchReapplyChecked() const;

	void OnRandomPitchChanged(float InValue);

	float GetRandomPitch() const;

	/** Ground Slope */
	void OnGroundSlopeReapply(ECheckBoxState InState);

	ECheckBoxState IsGroundSlopeReapplyChecked() const;

	void OnGroundSlopeChanged(float InValue);

	float GetGroundSlope() const;

	/** Height Min/Max */
	void OnHeightReapply(ECheckBoxState InState);

	ECheckBoxState IsHeightReapplyChecked() const;

	void OnHeightMinChanged(float InValue);

	float GetHeightMin() const;

	void OnHeightMaxChanged(float InValue);

	float GetHeightMax() const;

	float GetShadeRadius() const;

	void OnShadeRadiusChanged(float InValue);

	float GetShadeAffinity() const;

	void OnShadeAffinityChanged(float InValue);


	float GetSimulationPriority() const;

	void OnSimulationPriorityChanged(float InValue);

	/** Landscape Layer */
	void OnLandscapeLayerReapply(ECheckBoxState InState);

	ECheckBoxState IsLandscapeLayerReapplyChecked() const;

	void OnLandscapeLayerChanged(const FText& InValue);

	FText GetLandscapeLayer() const;

	/** Collision with World */
	void OnCollisionWithWorld(ECheckBoxState InState);

	ECheckBoxState IsCollisionWithWorldChecked() const;

	EVisibility IsCollisionWithWorldVisible() const;

	void OnCollisionWithWorldReapply(ECheckBoxState InState);

	ECheckBoxState IsCollisionWithWorldReapplyChecked() const;

	void OnCollisionScaleXChanged(float InValue);
	void OnCollisionScaleYChanged(float InValue);
	void OnCollisionScaleZChanged(float InValue);

	float GetCollisionScaleX() const;
	float GetCollisionScaleY() const;
	float GetCollisionScaleZ() const;

	/** Vertex Color Mask */
	void OnVertexColorMaskReapply(ECheckBoxState InState);

	ECheckBoxState IsVertexColorMaskReapplyChecked() const;

	void OnVertexColorMask(ECheckBoxState InState, FoliageVertexColorMask Mask);

	ECheckBoxState IsVertexColorMaskChecked(FoliageVertexColorMask Mask) const;

	/** Vertex Color Threshold */
	void OnVertexColorMaskThresholdChanged(float InValue);

	float GetVertexColorMaskThreshold() const;

	EVisibility IsVertexColorMaskThresholdVisible() const;

	void OnVertexColorMaskInvert(ECheckBoxState InState);

	ECheckBoxState IsVertexColorMaskInvertChecked() const;

private:
	/** Pointer back to the foliage edit mode containing this. */
	TWeakPtr<SFoliageEdit> FoliageEditPtr;

	/** The settings for placing foliage. */
	UFoliageType* FoliageSettingsPtr;

	/** Contains the thumbnail widget, needed to recreate the thumbnail when replacing. */
	TSharedPtr<SBorder> ThumbnailWidgetBorder;

	TSharedPtr<SBox> ThumbnailBox;

	/** The actual widget for the thumbnail. */
	TSharedPtr<SWidget> ThumbnailWidget;

	/** The thumbnail asset from the thumbnail pool. */
	TSharedPtr<FAssetThumbnail> Thumbnail;

	/** Info for the static mesh, used to pull instanced information. */
	TSharedPtr<FFoliageMeshUIInfo> FoliageMeshUIInfo;

	/** The current view settings this item is set to. */
	ECurrentViewSettings::Type CurrentViewSettings;

	/** Details panel for cluster settings. */
	TSharedPtr<IDetailsView> ClusterSettingsDetails;

	/** Holds the UFoliageType object for the details panel. */
	TArray<UObject*> DetailsObjectList;

	/** Command list for binding functions to for the toolbar. */
	TSharedPtr<FUICommandList> UICommandList;
};
