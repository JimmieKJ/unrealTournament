// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SEditorViewport.h"
#include "SAnimationEditorViewport.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"

class FMenuBuilder;

/**
 * A level viewport toolbar widget that is placed in a viewport
 */
class SAnimViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SAnimViewportToolBar ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SAnimationEditorViewportTabBody> InViewport, TSharedPtr<class SEditorViewport> InRealViewport);

private:
	/**
	 * Generates the toolbar view menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	TSharedRef<SWidget> GenerateViewMenu() const;

	/**
	 * Generates the toolbar show menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	TSharedRef<SWidget> GenerateShowMenu() const;

	/**
	 * Generates the Show -> Scene sub menu content
	 */
	void FillShowSceneMenu(FMenuBuilder& MenuBuilder) const;

	/**
	 * Generates the Show -> Advanced sub menu content
	 */
	void FillShowAdvancedMenu(FMenuBuilder& MenuBuilder) const;

	/**
	* Generates the Show -> Bone sub menu content
	*/
	void FillShowBoneDrawMenu(FMenuBuilder& MenuBuilder) const;

	/**
	* Generates the Show -> Overlay sub menu content
	*/
	void FillShowOverlayDrawMenu(FMenuBuilder& MenuBuilder) const;

	/**
	* Generates the Show -> Clothing sub menu content
	*/
	void FillShowClothingMenu(FMenuBuilder& MenuBuilder) const;

	/**
	* Generates the Show -> Display Info sub menu content
	*/
	void FillShowDisplayInfoMenu(FMenuBuilder& MenuBuilder) const;

	/**
	 * Generates the toolbar LOD menu content 
	 *
	 * @return The widget containing the LOD menu content based on LOD model count
	 */
	TSharedRef<SWidget> GenerateLODMenu() const;

	/**
	 * Returns the label for the "LOD" tool bar menu, which changes depending on the current LOD selection
	 *
	 * @return	Label to use for this LOD label
	 */
	FText GetLODMenuLabel() const;

	/**
	 * Generates the toolbar viewport type menu content 
	 *
	 * @return The widget containing the viewport type menu content
	 */
	TSharedRef<SWidget> GenerateViewportTypeMenu() const;

	/**
	 * Generates the toolbar playback menu content 
	 *
	 * @return The widget containing the playback menu content
	 */
	TSharedRef<SWidget> GeneratePlaybackMenu() const;

	/** Generate the turntable menu entries */
	void GenerateTurnTableMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the scene setup menu */
	void GenerateSceneSetupMenu(FMenuBuilder& MenuBuilder);

	/** Customize the details of the scene setup object */
	TSharedRef<class IDetailCustomization> CustomizePreviewSceneDescription();

	/** Customize a preview mesh collection entry */
	TSharedRef<class IPropertyTypeCustomization> CustomizePreviewMeshCollectionEntry();

	/**
	* Generate color of the text on the top
	*/ 
	FSlateColor GetFontColor() const;

	/**
	 * Returns the label for the Playback tool bar menu, which changes depending on the current playback speed
	 *
	 * @return	Label to use for this Menu
	 */
	FText GetPlaybackMenuLabel() const;

	/**
	 * Returns the label for the Viewport type tool bar menu, which changes depending on the current selected type
	 *
	 * @return	Label to use for this Menu
	 */
	FText GetCameraMenuLabel() const;
	const FSlateBrush* GetCameraMenuLabelIcon() const;

	/** Called by the FOV slider in the perspective viewport to get the FOV value */
	float OnGetFOVValue() const;
	/** Called when the FOV slider is adjusted in the perspective viewport */
	void OnFOVValueChanged( float NewValue );
	/** Called when a value is entered into the FOV slider/box in the perspective viewport */
	void OnFOVValueCommitted( float NewValue, ETextCommit::Type CommitInfo );

	/** Called by the floor offset slider in the perspective viewport to get the offset value */
	TOptional<float> OnGetFloorOffset() const;
	/** Called when the floor offset slider is adjusted in the perspective viewport */
	void OnFloorOffsetChanged( float NewValue );

	// Called to determine if the gizmos can be used in the current preview
	EVisibility GetTransformToolbarVisibility() const;

private:
	/** The viewport that we are in */
	TWeakPtr<class SAnimationEditorViewportTabBody> Viewport;
};
